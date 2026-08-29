/*
 * SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
 * SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.0
 */

use std::collections::{HashMap, VecDeque};

use indexmap::IndexSet;

use crate::hir::{declarations, type_check::Type};
use crate::mono::{GenericInstance, TypeArgument, create_mono_plan};

use super::*;

#[derive(Clone)]
pub(super) struct Template
{
    pub(super) tree: usize,
    pub(super) node: usize,
    pub(super) name: String,
    pub(super) parameters: Vec<Parameter>,
    pub(super) signature: declarations::Function,
    pub(super) ordinal: usize,
}

#[derive(Clone)]
pub(super) struct Parameter
{
    name: String,
    constraints: Vec<declarations::TypeRef>,
}

#[derive(Clone, Debug, Eq, PartialEq)]
pub(super) struct Use
{
    pub(super) name: String,
    pub(super) arguments: Vec<declarations::TypeRef>,
}

pub(super) struct Instance
{
    pub(super) tree: usize,
    pub(super) node: usize,
    pub(super) key: String,
    pub(super) function: declarations::Function,
    pub(super) substitutions: HashMap<String, Type>,
}

struct PendingUse
{
    generic_use: Use,
    ancestry: Vec<(usize, Vec<declarations::TypeRef>)>,
}

pub(super) fn parameters(tree: &SyntaxTree, function: &SyntaxNode) -> Option<Vec<Parameter>>
{
    let parameters = function
        .children
        .iter()
        .filter_map(|index| tree.nodes.get(*index))
        .filter(|child| child.kind == GENERIC_PARAMETER)
        .map(|parameter| {
            let name = first_child_kind(tree, parameter, IDENTIFIER)?;
            let constraints = parameter
                .children
                .iter()
                .filter_map(|index| tree.nodes.get(*index))
                .filter(|child| child.kind != IDENTIFIER)
                .map(|constraint| lower_type(tree, constraint))
                .collect();
            Some(Parameter {
                name: name.text.clone(),
                constraints,
            })
        })
        .collect::<Option<Vec<_>>>()?;
    (!parameters.is_empty()).then_some(parameters)
}

pub(super) fn call_use(tree: &SyntaxTree, callee: &SyntaxNode) -> Option<Use>
{
    if callee.kind != EXPR_GENERIC_QUALIFIER || callee.children.len() < 2
    {
        return None;
    }
    let base = tree.nodes.get(callee.children[0])?;
    if base.kind != PATH
    {
        return None;
    }
    let type_nodes = callee.children[1..]
        .iter()
        .filter_map(|index| tree.nodes.get(*index))
        .collect::<Vec<_>>();
    if type_nodes.len() + 1 != callee.children.len() || type_nodes.iter().any(|argument| !is_type_node(argument.kind))
    {
        return None;
    }
    let arguments = type_nodes
        .iter()
        .map(|argument| lower_type(tree, argument))
        .collect::<Vec<_>>();
    (arguments.len() + 1 == callee.children.len()).then(|| Use {
        name: path_text(tree, base),
        arguments,
    })
}

pub(super) fn collect_uses(trees: &[SyntaxTree]) -> Vec<Use>
{
    let mut uses = Vec::new();
    for tree in trees
    {
        for (call_index, call) in tree.nodes.iter().enumerate().filter(|(_, node)| node.kind == EXPR_CALL)
        {
            if belongs_to_generic_template(tree, call_index)
            {
                continue;
            }
            let Some(callee) = call.children.first().and_then(|index| tree.nodes.get(*index))
            else
            {
                continue;
            };
            if let Some(call_use) = call_use(tree, callee) &&
                !uses.contains(&call_use)
            {
                uses.push(call_use);
            }
        }
    }
    uses
}

pub(super) fn discover_instances(
    trees: &[SyntaxTree],
    templates: &[Template],
    nominal_types: &[declarations::NominalType],
    module: &str,
) -> Result<Vec<Instance>, LoweringError>
{
    let mut pending = collect_uses(trees)
        .into_iter()
        .map(|generic_use| PendingUse {
            generic_use,
            ancestry: Vec::new(),
        })
        .collect::<VecDeque<_>>();
    let mut instances = Vec::<Instance>::new();
    let mut instance_keys = IndexSet::<String>::new();
    while let Some(request) = pending.pop_front()
    {
        for template in templates.iter().filter(|template| {
            template.name == request.generic_use.name &&
                template.parameters.len() == request.generic_use.arguments.len()
        })
        {
            if request
                .ancestry
                .iter()
                .any(|(ordinal, arguments)| *ordinal == template.ordinal && arguments != &request.generic_use.arguments)
            {
                return Err(LoweringError::ExpandingGenericRecursion);
            }
            let Some((function, substitutions, key)) = specialization(template, &request.generic_use.arguments, module)
            else
            {
                continue;
            };
            validate_constraints(template, &request.generic_use.arguments, nominal_types)?;
            let identity = format!(
                "{key}({})",
                function
                    .parameters
                    .iter()
                    .map(|parameter| type_ref_key(&parameter.ty))
                    .collect::<Vec<_>>()
                    .join(",")
            );
            if !instance_keys.insert(identity)
            {
                continue;
            }
            let type_refs = template
                .parameters
                .iter()
                .map(|parameter| parameter.name.clone())
                .zip(request.generic_use.arguments.iter().cloned())
                .collect::<HashMap<_, _>>();
            let mut ancestry = request.ancestry.clone();
            ancestry.push((template.ordinal, request.generic_use.arguments.clone()));
            pending.extend(
                collect_node_uses(&trees[template.tree], template.node)
                    .into_iter()
                    .map(|nested| PendingUse {
                        generic_use: specialize_use(nested, &type_refs),
                        ancestry: ancestry.clone(),
                    }),
            );
            instances.push(Instance {
                tree: template.tree,
                node: template.node,
                key,
                function,
                substitutions,
            });
        }
    }
    Ok(instances)
}

fn validate_constraints(
    template: &Template,
    arguments: &[declarations::TypeRef],
    nominal_types: &[declarations::NominalType],
) -> Result<(), LoweringError>
{
    let definitions = nominal_types
        .iter()
        .map(|nominal| (nominal.name.as_str(), nominal))
        .collect::<HashMap<_, _>>();
    for (parameter, argument) in template.parameters.iter().zip(arguments)
    {
        for constraint in &parameter.constraints
        {
            let declarations::TypeRef::Named(constraint_name) = constraint
            else
            {
                return Err(LoweringError::ConstraintIsNotInterface(type_ref_key(constraint)));
            };
            if definitions
                .get(constraint_name.as_str())
                .is_none_or(|nominal| nominal.kind != declarations::NominalKind::Interface)
            {
                return Err(LoweringError::ConstraintIsNotInterface(constraint_name.clone()));
            }
            let declarations::TypeRef::Named(argument_name) = argument
            else
            {
                return Err(LoweringError::UnsatisfiedGenericConstraint {
                    argument: type_ref_key(argument),
                    constraint: constraint_name.clone(),
                });
            };
            if !implements_interface(argument_name, constraint_name, &definitions, &mut Vec::new())
            {
                return Err(LoweringError::UnsatisfiedGenericConstraint {
                    argument: argument_name.clone(),
                    constraint: constraint_name.clone(),
                });
            }
        }
    }
    Ok(())
}

fn implements_interface(
    name: &str,
    constraint: &str,
    definitions: &HashMap<&str, &declarations::NominalType>,
    visiting: &mut Vec<String>,
) -> bool
{
    if name == constraint
    {
        return true;
    }
    if visiting.iter().any(|current| current == name)
    {
        return false;
    }
    let Some(nominal) = definitions.get(name)
    else
    {
        return false;
    };
    visiting.push(name.to_string());
    let satisfied = nominal.bases.iter().any(|base| {
        let declarations::TypeRef::Named(base_name) = &base.ty
        else
        {
            return false;
        };
        implements_interface(base_name, constraint, definitions, visiting)
    });
    visiting.pop();
    satisfied
}

fn collect_node_uses(tree: &SyntaxTree, root: usize) -> Vec<Use>
{
    let mut uses = Vec::new();
    let mut pending = vec![root];
    while let Some(index) = pending.pop()
    {
        let Some(node) = tree.nodes.get(index)
        else
        {
            continue;
        };
        pending.extend(node.children.iter().rev().copied());
        if node.kind != EXPR_CALL
        {
            continue;
        }
        let Some(callee) = node.children.first().and_then(|child| tree.nodes.get(*child))
        else
        {
            continue;
        };
        if let Some(generic_use) = call_use(tree, callee) &&
            !uses.contains(&generic_use)
        {
            uses.push(generic_use);
        }
    }
    uses
}

fn specialize_use(mut generic_use: Use, substitutions: &HashMap<String, declarations::TypeRef>) -> Use
{
    generic_use.arguments = generic_use
        .arguments
        .iter()
        .map(|argument| specialize_ref(argument, substitutions))
        .collect();
    generic_use
}

fn is_type_node(kind: u32) -> bool
{
    (TYPE_NAMED..=TYPE_UNIT).contains(&kind) || kind == TYPE_MAP
}

fn belongs_to_generic_template(tree: &SyntaxTree, mut node_index: usize) -> bool
{
    while let Some(parent) = tree.nodes.get(node_index).and_then(|node| node.parent)
    {
        let Some(node) = tree.nodes.get(parent)
        else
        {
            return false;
        };
        if node.kind == DECL_FUNCTION
        {
            return parameters(tree, node).is_some();
        }
        node_index = parent;
    }
    false
}

pub(super) fn specialize_ref(
    value: &declarations::TypeRef,
    substitutions: &HashMap<String, declarations::TypeRef>,
) -> declarations::TypeRef
{
    match value
    {
        declarations::TypeRef::Named(name) => substitutions
            .get(name)
            .cloned()
            .unwrap_or_else(|| declarations::TypeRef::Named(substitute_named_ref(name, substitutions))),
        declarations::TypeRef::Optional {
            element,
        } => declarations::TypeRef::Optional {
            element: Box::new(specialize_ref(element, substitutions)),
        },
        declarations::TypeRef::Result {
            success,
            error,
        } => declarations::TypeRef::Result {
            success: Box::new(specialize_ref(success, substitutions)),
            error: Box::new(specialize_ref(error, substitutions)),
        },
        declarations::TypeRef::Reference {
            referent,
            mutable,
        } => declarations::TypeRef::Reference {
            referent: Box::new(specialize_ref(referent, substitutions)),
            mutable: *mutable,
        },
        declarations::TypeRef::Array {
            element,
            length,
        } => declarations::TypeRef::Array {
            element: Box::new(specialize_ref(element, substitutions)),
            length: *length,
        },
        declarations::TypeRef::Map {
            key,
            value,
        } => declarations::TypeRef::Map {
            key: Box::new(specialize_ref(key, substitutions)),
            value: Box::new(specialize_ref(value, substitutions)),
        },
        declarations::TypeRef::Tuple {
            fields,
        } => declarations::TypeRef::Tuple {
            fields: fields
                .iter()
                .map(|field| declarations::TupleFieldRef {
                    name: field.name.clone(),
                    ty: specialize_ref(&field.ty, substitutions),
                })
                .collect(),
        },
        declarations::TypeRef::Unit | declarations::TypeRef::Primitive(_) => value.clone(),
    }
}

fn substitute_named_ref(name: &str, substitutions: &HashMap<String, declarations::TypeRef>) -> String
{
    substitute_identifiers(name, |identifier| substitutions.get(identifier).map(type_ref_key))
}

fn substitute_identifiers(name: &str, mut replacement: impl FnMut(&str) -> Option<String>) -> String
{
    let mut output = String::with_capacity(name.len());
    let mut start = 0;
    for (index, character) in name.char_indices()
    {
        if character == '_' || character.is_alphanumeric()
        {
            continue;
        }
        if start < index
        {
            let identifier = &name[start..index];
            output.push_str(&replacement(identifier).unwrap_or_else(|| identifier.to_string()));
        }
        output.push(character);
        start = index + character.len_utf8();
    }
    if start < name.len()
    {
        let identifier = &name[start..];
        output.push_str(&replacement(identifier).unwrap_or_else(|| identifier.to_string()));
    }
    output
}

pub(super) fn specialization(
    template: &Template,
    arguments: &[declarations::TypeRef],
    module: &str,
) -> Option<(declarations::Function, HashMap<String, Type>, String)>
{
    if template.parameters.len() != arguments.len()
    {
        return None;
    }
    let substitutions = template
        .parameters
        .iter()
        .map(|parameter| parameter.name.clone())
        .zip(arguments.iter().cloned())
        .collect::<HashMap<_, _>>();
    let mut function = template.signature.clone();
    function.name = symbol(module, &template.name, template.ordinal, arguments);
    for parameter in &mut function.parameters
    {
        parameter.ty = specialize_ref(&parameter.ty, &substitutions);
    }
    function.return_type = specialize_ref(&function.return_type, &substitutions);
    let checked = substitutions
        .into_iter()
        .map(|(name, ty)| Some((name, checked_type(&ty)?)))
        .collect::<Option<HashMap<_, _>>>()?;
    let key = call_key(
        &template.name,
        &arguments.iter().map(checked_type).collect::<Option<Vec<_>>>()?,
    );
    Some((function, checked, key))
}

pub(super) fn specialized_call_key(
    tree: &SyntaxTree,
    callee: &SyntaxNode,
    substitutions: &HashMap<String, Type>,
) -> Option<String>
{
    let call = call_use(tree, callee)?;
    let arguments = call
        .arguments
        .iter()
        .map(|argument| {
            let mut checked = checked_type(argument)?;
            substitute_checked_type(&mut checked, substitutions);
            Some(checked)
        })
        .collect::<Option<Vec<_>>>()?;
    Some(call_key(&call.name, &arguments))
}

pub(super) fn call_key(name: &str, arguments: &[Type]) -> String
{
    let arguments = arguments.iter().map(type_key).collect::<Vec<_>>().join(",");
    format!("{name}::<{arguments}>")
}

pub(super) fn substitute_checked_type(value: &mut Type, substitutions: &HashMap<String, Type>)
{
    match value
    {
        Type::Named(name) =>
        {
            if let Some(replacement) = substitutions.get(name)
            {
                *value = replacement.clone();
            }
            else
            {
                *name = substitute_identifiers(name, |identifier| substitutions.get(identifier).map(type_key));
            }
        }
        Type::Optional {
            element,
        } => substitute_checked_type(element, substitutions),
        Type::Result {
            success,
            error,
        } =>
        {
            substitute_checked_type(success, substitutions);
            substitute_checked_type(error, substitutions);
        }
        Type::Reference {
            referent, ..
        } => substitute_checked_type(referent, substitutions),
        Type::Array {
            element, ..
        } |
        Type::Set {
            element,
        } => substitute_checked_type(element, substitutions),
        Type::Map {
            key,
            value,
        } =>
        {
            substitute_checked_type(key, substitutions);
            substitute_checked_type(value, substitutions);
        }
        Type::Tuple {
            fields,
        } =>
        {
            for field in fields
            {
                substitute_checked_type(&mut field.ty, substitutions);
            }
        }
        Type::Unit | Type::Primitive(_) =>
        {}
    }
}

pub(super) fn checked_type_in_context(value: &declarations::TypeRef, context: &LoweringContext) -> Option<Type>
{
    let mut checked = checked_type(value)?;
    substitute_checked_type(&mut checked, &context.type_substitutions);
    Some(checked)
}

fn type_key(value: &Type) -> String
{
    match value
    {
        Type::Unit => "unit".to_string(),
        Type::Primitive(value) => format!("{value:?}"),
        Type::Named(name) => name.clone(),
        Type::Optional {
            element,
        } => format!("optional<{}>", type_key(element)),
        Type::Result {
            success,
            error,
        } => format!("result<{},{}>", type_key(success), type_key(error)),
        Type::Reference {
            referent,
            mutable,
        } => format!(
            "&{}{}",
            if *mutable
            {
                "mut "
            }
            else
            {
                ""
            },
            type_key(referent)
        ),
        Type::Array {
            element,
            length,
        } => match length
        {
            Some(length) => format!("[{};{length}]", type_key(element)),
            None => format!("[{}]", type_key(element)),
        },
        Type::Set {
            element,
        } => format!("set<{}>", type_key(element)),
        Type::Map {
            key,
            value,
        } => format!("map<{},{}>", type_key(key), type_key(value)),
        Type::Tuple {
            fields,
        } => format!(
            "({})",
            fields
                .iter()
                .map(|field| type_key(&field.ty))
                .collect::<Vec<_>>()
                .join(",")
        ),
    }
}

fn symbol(module: &str, name: &str, ordinal: usize, arguments: &[declarations::TypeRef]) -> String
{
    let instance = GenericInstance {
        module_path: module.to_string(),
        function_name: format!("{name}$G{ordinal}"),
        type_arguments: arguments
            .iter()
            .map(|argument| TypeArgument {
                name: type_ref_key(argument),
            })
            .collect(),
    };
    create_mono_plan([instance])
        .entries
        .into_iter()
        .next()
        .map_or_else(|| format!("xs$mono${name}${ordinal}"), |entry| entry.symbol_name)
}

fn type_ref_key(value: &declarations::TypeRef) -> String
{
    match value
    {
        declarations::TypeRef::Unit => "unit".to_string(),
        declarations::TypeRef::Primitive(value) => format!("{value:?}"),
        declarations::TypeRef::Named(name) => name.clone(),
        declarations::TypeRef::Optional {
            element,
        } => format!("optional<{}>", type_ref_key(element)),
        declarations::TypeRef::Result {
            success,
            error,
        } => format!("result<{},{}>", type_ref_key(success), type_ref_key(error)),
        declarations::TypeRef::Reference {
            referent,
            mutable,
        } =>
        {
            format!(
                "&{}{}",
                if *mutable
                {
                    "mut "
                }
                else
                {
                    ""
                },
                type_ref_key(referent)
            )
        }
        declarations::TypeRef::Array {
            element,
            length,
        } => match length
        {
            Some(length) => format!("[{};{length}]", type_ref_key(element)),
            None => format!("[{}]", type_ref_key(element)),
        },
        declarations::TypeRef::Map {
            key,
            value,
        } => format!("[{}:{}]", type_ref_key(key), type_ref_key(value)),
        declarations::TypeRef::Tuple {
            fields,
        } => format!(
            "({})",
            fields
                .iter()
                .map(|field| type_ref_key(&field.ty))
                .collect::<Vec<_>>()
                .join(",")
        ),
    }
}

#[cfg(test)]
mod tests
{
    use super::*;

    #[test]
    fn substitutes_generic_parameters_inside_named_generic_types()
    {
        let substitutions = HashMap::from([
            ("T".to_string(), declarations::TypeRef::Primitive(PrimitiveType::Long)),
            ("U".to_string(), declarations::TypeRef::Named("Worker".to_string())),
        ]);
        assert_eq!(
            substitute_named_ref("std::pair::Pair<T, U>", &substitutions),
            "std::pair::Pair<Long, Worker>"
        );
        assert_eq!(substitute_named_ref("Type", &substitutions), "Type");
    }
}
