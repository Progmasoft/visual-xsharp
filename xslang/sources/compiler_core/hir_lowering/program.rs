/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 */

use super::*;

struct FunctionLocation
{
    tree: usize,
    node: usize,
    function: usize,
    constructor_owner: Option<String>,
    call_name: Option<String>,
    method_owner: Option<String>,
    type_substitutions: HashMap<String, Type>,
}

fn module_name(tree: &SyntaxTree, root: &SyntaxNode) -> Option<String>
{
    root.children
        .iter()
        .filter_map(|index| tree.nodes.get(*index))
        .find(|child| child.kind == DECL_MODULE)
        .and_then(|module| first_child_kind(tree, module, PATH))
        .map(|path| path_text(tree, path))
}

pub(super) fn lower_program(trees: &[SyntaxTree]) -> Result<declarations::Module, LoweringError>
{
    let mut program_name: Option<Option<String>> = None;
    let mut functions = Vec::new();
    let mut nominal_types = Vec::new();
    let mut locations = Vec::new();
    let mut generic_templates = Vec::new();
    for (tree_index, tree) in trees.iter().enumerate()
    {
        let root = node(tree, tree.root)?;
        if root.kind != FILE
        {
            return Err(LoweringError::InvalidRoot);
        }
        let current_name = module_name(tree, root);
        if let Some(expected) = &program_name
        {
            if expected != &current_name
            {
                return Err(LoweringError::ModuleMismatch);
            }
        }
        else
        {
            program_name = Some(current_name);
        }
        for node_index in root.children.iter().copied()
        {
            let Some(declaration) = tree.nodes.get(node_index)
            else
            {
                continue;
            };
            if matches!(declaration.kind, DECL_CLASS | DECL_INTERFACE | DECL_DATA | DECL_ENUM)
            {
                let nominal = nominal::lower_nominal_type(tree, declaration)?;
                if nominal.kind == declarations::NominalKind::Data
                {
                    for (ordinal, (constructor_index, constructor)) in declaration
                        .children
                        .iter()
                        .filter_map(|index| tree.nodes.get(*index).map(|node| (*index, node)))
                        .filter(|(_, child)| child.kind == constructor::CLASS_CONSTRUCTOR)
                        .enumerate()
                    {
                        let function = constructor::lower_signature(tree, constructor, &nominal.name, ordinal)?;
                        locations.push(FunctionLocation {
                            tree: tree_index,
                            node: constructor_index,
                            function: functions.len(),
                            constructor_owner: Some(nominal.name.clone()),
                            call_name: None,
                            method_owner: None,
                            type_substitutions: HashMap::new(),
                        });
                        functions.push(function);
                    }
                    for (ordinal, (method_index, method_node)) in declaration
                        .children
                        .iter()
                        .filter_map(|index| tree.nodes.get(*index).map(|node| (*index, node)))
                        .filter(|(_, child)| child.kind == DECL_FUNCTION && child.flags & OPERATOR == 0)
                        .enumerate()
                    {
                        let (function, source_name, is_static) =
                            method::lower_signature(tree, method_node, &nominal.name, ordinal)?;
                        locations.push(FunctionLocation {
                            tree: tree_index,
                            node: method_index,
                            function: functions.len(),
                            constructor_owner: None,
                            call_name: Some(
                                if is_static
                                {
                                    format!("{}::{source_name}", nominal.name)
                                }
                                else
                                {
                                    source_name
                                },
                            ),
                            method_owner: (!is_static).then(|| nominal.name.clone()),
                            type_substitutions: HashMap::new(),
                        });
                        functions.push(function);
                    }
                }
                nominal_types.push(nominal);
                continue;
            }
            let Some(function_node) = (declaration.kind == DECL_FUNCTION).then_some(declaration)
            else
            {
                continue;
            };
            let function = signature::lower_function(tree, function_node)?;
            if let Some(parameters) = generic::parameters(tree, function_node)
            {
                generic_templates.push(generic::Template {
                    tree: tree_index,
                    node: node_index,
                    name: function.name.clone(),
                    parameters,
                    signature: function,
                    ordinal: generic_templates.len(),
                });
                continue;
            }
            locations.push(FunctionLocation {
                tree: tree_index,
                node: node_index,
                function: functions.len(),
                constructor_owner: None,
                call_name: Some(function.name.clone()),
                method_owner: None,
                type_substitutions: HashMap::new(),
            });
            functions.push(function);
        }
    }
    let module_for_symbols = program_name.as_ref().and_then(|name| name.as_deref()).unwrap_or("root");
    let mut generic_instances = Vec::<(String, usize)>::new();
    for instance in generic::discover_instances(trees, &generic_templates, &nominal_types, module_for_symbols)?
    {
        let function_index = functions.len();
        locations.push(FunctionLocation {
            tree: instance.tree,
            node: instance.node,
            function: function_index,
            constructor_owner: None,
            call_name: None,
            method_owner: None,
            type_substitutions: instance.substitutions,
        });
        functions.push(instance.function);
        generic_instances.push((instance.key, function_index));
    }
    let mut top_level_overloads = HashMap::<String, Vec<usize>>::new();
    for location in &locations
    {
        if location.constructor_owner.is_none() &&
            location.method_owner.is_none() &&
            location.call_name.as_deref() == Some(functions[location.function].name.as_str())
        {
            top_level_overloads
                .entry(functions[location.function].name.clone())
                .or_default()
                .push(location.function);
        }
    }
    for (name, overloads) in top_level_overloads.iter().filter(|(_, overloads)| overloads.len() > 1)
    {
        if name == "main"
        {
            return Err(LoweringError::DuplicateCallable);
        }
        for (ordinal, function) in overloads.iter().enumerate()
        {
            functions[*function].name = format!("xs$fn${name}${ordinal}");
        }
    }
    let mut calls = HashMap::<String, Vec<CallSignature>>::new();
    let mut generic_calls = HashMap::<String, Vec<CallSignature>>::new();
    let mut methods = HashMap::<(String, String), Vec<CallSignature>>::new();
    for location in &locations
    {
        let Some(name) = &location.call_name
        else
        {
            continue;
        };
        let Some(signature) = call::signature(&functions[location.function])
        else
        {
            continue;
        };
        if let Some(owner) = &location.method_owner
        {
            methods
                .entry((owner.clone(), name.clone()))
                .or_default()
                .push(signature);
        }
        else
        {
            calls.entry(name.clone()).or_default().push(signature);
        }
    }
    if call::has_duplicate_signatures(&calls) || call::has_duplicate_method_signatures(&methods)
    {
        return Err(LoweringError::DuplicateCallable);
    }
    for (key, function) in generic_instances
    {
        let Some(signature) = call::signature(&functions[function])
        else
        {
            continue;
        };
        generic_calls.entry(key).or_default().push(signature);
    }
    if call::has_duplicate_signatures(&generic_calls)
    {
        return Err(LoweringError::DuplicateCallable);
    }
    let enum_data = crate::hir::enum_data::EnumDataRegistry::build(&nominal_types);
    if let Some(diagnostic) = enum_data.diagnostics().first()
    {
        return Err(LoweringError::InvalidEnumData(format!(
            "{}: {}",
            diagnostic.type_name, diagnostic.error
        )));
    }
    let context = LoweringContext {
        calls,
        generic_calls,
        constructors: functions
            .iter()
            .filter(|function| function.name.starts_with("xs$ctor$"))
            .filter_map(|function| {
                let declarations::TypeRef::Named(owner) = &function.return_type
                else
                {
                    return None;
                };
                let parameters = function
                    .parameters
                    .iter()
                    .map(|parameter| checked_type(&parameter.ty))
                    .collect::<Option<Vec<_>>>()?;
                Some((owner.clone(), CallSignature {
                    symbol: function.name.clone(),
                    parameters,
                    return_type: checked_type(&function.return_type)?,
                }))
            })
            .fold(
                HashMap::<String, Vec<CallSignature>>::new(),
                |mut constructors, (owner, signature)| {
                    constructors.entry(owner).or_default().push(signature);
                    constructors
                },
            ),
        methods,
        nominal_types: nominal_types.iter().cloned().map(|ty| (ty.name.clone(), ty)).collect(),
        enum_data,
        type_substitutions: HashMap::new(),
    };
    for location in locations
    {
        let tree = &trees[location.tree];
        let syntax = &tree.nodes[location.node];
        let function = &mut functions[location.function];
        let mut function_context = context.clone();
        function_context.type_substitutions = location.type_substitutions;
        let parameters = function.parameters.clone();
        let return_type = checked_type(&function.return_type);
        function.body = if let Some(owner) = &location.constructor_owner
        {
            constructor::lower_body(tree, syntax, &function_context, &parameters, owner)
        }
        else
        {
            lower_body(tree, syntax, &function_context, &parameters, return_type.as_ref())
        };
    }
    Ok(declarations::Module {
        name: program_name.unwrap_or(None),
        nominal_types,
        functions,
    })
}
