/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 */

use std::collections::HashMap;

use crate::hir::declarations::{Module as HirModule, NominalKind, NominalType, TypeRef};
use crate::mir;
use crate::xlil::{self, Function, Module, Type};

fn lower_type(
    value: &TypeRef,
    aggregates: &crate::hir::aggregate_registry::AggregateRegistry,
    collections: &crate::hir::collection_registry::CollectionRegistry,
) -> Option<Type>
{
    match value
    {
        TypeRef::Unit => Some(Type::VOID),
        TypeRef::Primitive(value) => crate::hir::mir_lowering::primitive_to_xlil(*value),
        TypeRef::Named(name) => aggregates.types.get(name).copied(),
        TypeRef::Reference {
            referent,
            mutable: false,
        } if **referent == TypeRef::Primitive(crate::hir::type_check::PrimitiveType::Str) => Some(Type::STR),
        TypeRef::Optional {
            ..
        } =>
        {
            let checked = crate::hir::declarations::type_ref_to_checked(value)?;
            aggregates
                .optionals
                .iter()
                .find(|(source, _)| source == &checked)
                .map(|(_, ty)| *ty)
        }
        TypeRef::Result {
            ..
        } =>
        {
            let checked = crate::hir::declarations::type_ref_to_checked(value)?;
            aggregates
                .results
                .iter()
                .find(|(source, _)| source == &checked)
                .map(|(_, ty)| *ty)
        }
        TypeRef::Reference {
            ..
        } => None,
        TypeRef::Tuple {
            ..
        } =>
        {
            let checked = crate::hir::declarations::type_ref_to_checked(value)?;
            aggregates
                .tuples
                .iter()
                .find(|(source, _)| source == &checked)
                .map(|(_, ty)| *ty)
        }
        TypeRef::Array {
            ..
        } => collections.value_type(&crate::hir::declarations::type_ref_to_checked(value)?),
        TypeRef::Map {
            ..
        } => None,
    }
}

fn flatten_parameter(
    module: &HirModule,
    definitions: &HashMap<String, NominalType>,
    value: &TypeRef,
    aggregates: &crate::hir::aggregate_registry::AggregateRegistry,
    collections: &crate::hir::collection_registry::CollectionRegistry,
    visiting: &mut Vec<String>,
    parameters: &mut Vec<Type>,
) -> Option<()>
{
    match value
    {
        TypeRef::Primitive(value) => parameters.push(crate::hir::mir_lowering::primitive_to_xlil(*value)?),
        TypeRef::Named(name) =>
        {
            if visiting.contains(name)
            {
                return None;
            }
            let definition = module
                .nominal_types
                .iter()
                .find(|definition| definition.name == *name)?;
            if matches!(definition.kind, NominalKind::Enum | NominalKind::EnumData)
            {
                parameters.push(lower_type(value, aggregates, collections)?);
                return Some(());
            }
            if definition.kind != NominalKind::Data
            {
                return None;
            }
            visiting.push(name.clone());
            for field in &crate::hir::declarations::resolved_fields(definition, definitions).ok()?
            {
                flatten_parameter(
                    module,
                    definitions,
                    &field.ty,
                    aggregates,
                    collections,
                    visiting,
                    parameters,
                )?;
            }
            visiting.pop();
        }
        TypeRef::Reference {
            ..
        } => parameters.push(lower_type(value, aggregates, collections)?),
        TypeRef::Optional {
            ..
        } |
        TypeRef::Result {
            ..
        } => parameters.push(lower_type(value, aggregates, collections)?),
        TypeRef::Tuple {
            ..
        } |
        TypeRef::Array {
            ..
        } => parameters.push(lower_type(value, aggregates, collections)?),
        TypeRef::Unit => return None,
        TypeRef::Map {
            ..
        } => return None,
    }
    Some(())
}

fn signature(
    module: &HirModule,
    aggregates: &crate::hir::aggregate_registry::AggregateRegistry,
    collections: &crate::hir::collection_registry::CollectionRegistry,
    function: &crate::hir::declarations::Function,
) -> Option<(Type, Vec<Type>)>
{
    let return_type = lower_type(&function.return_type, aggregates, collections)?;
    let definitions = module
        .nominal_types
        .iter()
        .cloned()
        .map(|definition| (definition.name.clone(), definition))
        .collect::<HashMap<_, _>>();
    let mut parameters = Vec::new();
    for parameter in &function.parameters
    {
        flatten_parameter(
            module,
            &definitions,
            &parameter.ty,
            aggregates,
            collections,
            &mut Vec::new(),
            &mut parameters,
        )?;
    }
    Some((return_type, parameters))
}

fn matches_signature(function: &mir::Function, name: &str, return_type: Type, parameters: &[Type]) -> bool
{
    function.name == name &&
        function.return_type == return_type &&
        function
            .parameters
            .iter()
            .map(|parameter| parameter.value_type)
            .eq(parameters.iter().copied())
}

fn supports_source_native_subset(
    module: &HirModule,
    aggregates: &crate::hir::aggregate_registry::AggregateRegistry,
    collections: &crate::hir::collection_registry::CollectionRegistry,
) -> bool
{
    module
        .functions
        .iter()
        .all(|function| signature(module, aggregates, collections, function).is_some())
}

pub(super) fn lower_module(declarations: &HirModule, mir_functions: &[mir::Function]) -> Option<Module>
{
    let aggregates = crate::hir::aggregate_registry::build_module(declarations)?;
    let collections = crate::hir::collection_registry::build(declarations, &aggregates);
    if !supports_source_native_subset(declarations, &aggregates, &collections)
    {
        return None;
    }
    let mut module = Module::new(declarations.name.clone().unwrap_or_else(|| "root".to_string()));
    for layout in &aggregates.layouts
    {
        if module.add_aggregate_type(layout.name.clone(), layout.fields.clone()) != Some(layout.value_type)
        {
            return None;
        }
    }
    for layout in &collections.arrays
    {
        let registered = match layout.length
        {
            Some(length) => module.add_array_type(layout.element_type, length),
            None => module.add_dynamic_array_type(layout.element_type),
        };
        if registered != Some(layout.value_type)
        {
            return None;
        }
    }
    let mut used_mir = vec![false; mir_functions.len()];
    for declaration in &declarations.functions
    {
        let (return_type, parameters) = signature(declarations, &aggregates, &collections, declaration)?;
        if declaration.body_present
        {
            let index = mir_functions.iter().enumerate().position(|(index, function)| {
                !used_mir[index] && matches_signature(function, &declaration.name, return_type, &parameters)
            })?;
            used_mir[index] = true;
            module.add_function(
                xlil::lowering::MirToXlilLowerer::new()
                    .lower_function(&mir_functions[index])
                    .ok()?,
            );
        }
        else
        {
            module.add_function(Function::declaration(declaration.name.clone(), return_type, parameters));
        }
    }
    if used_mir.iter().filter(|used| **used).count() != mir_functions.len() || !xlil::verify_module(&module).is_empty()
    {
        return None;
    }
    Some(module)
}
