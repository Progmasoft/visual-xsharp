/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 */

use std::collections::{HashMap, HashSet};

use super::declarations::{self, NominalKind, NominalType, TypeRef};
use super::type_check::{Block, Expression, Statement, Type as HirType};
use crate::xlil::Type;

#[derive(Clone, Debug, Eq, PartialEq)]
pub(crate) struct AggregateLayout
{
  pub name: String,
  pub value_type: Type,
  pub fields: Vec<Type>,
}

#[derive(Clone, Debug, Eq, PartialEq)]
pub(crate) struct EnumDataVariantLayout
{
  pub owner: String,
  pub name: String,
  pub tag: u32,
  pub field: Option<u32>,
  pub payload_type: Option<Type>,
}

#[derive(Clone, Debug, Eq, PartialEq)]
pub(crate) struct EnumDataLayout
{
  pub name: String,
  pub value_type: Type,
  pub variants: Vec<EnumDataVariantLayout>,
}

impl EnumDataLayout
{
  pub(crate) fn variant(&self, owner: &str, name: &str, tag: u32) -> Option<&EnumDataVariantLayout>
  {
    self.variants
        .iter()
        .find(|variant| variant.owner == owner && variant.name == name && variant.tag == tag)
  }
}

#[derive(Clone, Debug, Default, Eq, PartialEq)]
pub(crate) struct AggregateRegistry
{
  pub layouts: Vec<AggregateLayout>,
  pub types: HashMap<String, Type>,
  pub tuples: Vec<(HirType, Type)>,
  pub optionals: Vec<(HirType, Type)>,
  pub results: Vec<(HirType, Type)>,
  pub enum_data: Vec<EnumDataLayout>,
}

pub(crate) fn build_module(module: &declarations::Module) -> Option<AggregateRegistry>
{
  let mut registry = build(&module.nominal_types)?;
  let definitions = module.nominal_types
                          .iter()
                          .filter(|declaration| declaration.kind == NominalKind::Data)
                          .map(|declaration| (declaration.name.as_str(), declaration))
                          .collect::<HashMap<_, _>>();
  for function in &module.functions
  {
    let _ = visit_type_ref(&function.return_type, &definitions, &mut HashSet::new(), &mut registry);
    for parameter in &function.parameters
    {
      let _ = visit_type_ref(&parameter.ty, &definitions, &mut HashSet::new(), &mut registry);
    }
    if let Some(body) = &function.body
    {
      visit_statements(body, &definitions, &mut registry)?;
    }
  }
  Some(registry)
}

pub(crate) fn build_functions_with_nominals(nominal_types: &[NominalType],
                                            functions: &[super::type_check::Function])
                                            -> Option<AggregateRegistry>
{
  let definitions = nominal_types.iter()
                                 .filter(|declaration| declaration.kind == NominalKind::Data)
                                 .map(|declaration| (declaration.name.as_str(), declaration))
                                 .collect::<HashMap<_, _>>();
  let mut registry = build(nominal_types)?;
  for function in functions
  {
    if let Some(return_type) = &function.return_type
    {
      let _ = visit_checked_type(return_type, &definitions, &mut HashSet::new(), &mut registry);
    }
    for local in &function.locals
    {
      let _ = visit_checked_type(&local.ty, &definitions, &mut HashSet::new(), &mut registry);
    }
    visit_statements(&function.body, &definitions, &mut registry)?;
  }
  Some(registry)
}

fn visit_statements(statements: &[Statement],
                    definitions: &HashMap<&str, &NominalType>,
                    registry: &mut AggregateRegistry)
                    -> Option<()>
{
  for statement in statements
  {
    match statement
    {
      Statement::Let { local, .. } =>
      {
        let _ = visit_checked_type(&local.ty, definitions, &mut HashSet::new(), registry);
        if let Statement::Let { initializer: Some(initializer),
                                .. } = statement
        {
          visit_expression(initializer, definitions, registry)?;
        }
      }
      Statement::If { then_block,
                      else_block,
                      condition,
                      .. } =>
      {
        visit_expression(condition, definitions, registry)?;
        visit_statements(&then_block.statements, definitions, registry)?;
        if let Some(block) = else_block
        {
          visit_statements(&block.statements, definitions, registry)?;
        }
      }
      Statement::While { condition,
                         body,
                         .. } =>
      {
        visit_expression(condition, definitions, registry)?;
        visit_statements(&body.statements, definitions, registry)?
      }
      Statement::ForEach { iterable,
                           iterable_type,
                           body,
                           .. } =>
      {
        let _ = visit_checked_type(iterable_type, definitions, &mut HashSet::new(), registry);
        visit_expression(iterable, definitions, registry)?;
        visit_statements(&body.statements, definitions, registry)?
      }
      Statement::For { initializer,
                       condition,
                       update,
                       body,
                       .. } =>
      {
        if let Some(initializer) = initializer
        {
          visit_statements(std::slice::from_ref(initializer), definitions, registry)?;
        }
        if let Some(condition) = condition
        {
          visit_expression(condition, definitions, registry)?;
        }
        if let Some(update) = update
        {
          visit_expression(update, definitions, registry)?;
        }
        visit_statements(&body.statements, definitions, registry)?;
      }
      Statement::Match { selector,
                         selector_type,
                         arms,
                         .. } =>
      {
        let _ = visit_checked_type(selector_type, definitions, &mut HashSet::new(), registry);
        visit_expression(selector, definitions, registry)?;
        for arm in arms
        {
          visit_statements(&arm.body.statements, definitions, registry)?;
        }
      }
      Statement::AssignIndex { index,
                               value,
                               element_type,
                               .. } =>
      {
        let _ = visit_checked_type(element_type, definitions, &mut HashSet::new(), registry);
        visit_expression(index, definitions, registry)?;
        visit_expression(value, definitions, registry)?;
      }
      Statement::AssignTupleElement { value,
                                      tuple_type,
                                      .. } =>
      {
        let _ = visit_checked_type(tuple_type, definitions, &mut HashSet::new(), registry);
        visit_expression(value, definitions, registry)?;
      }
      Statement::Expr(expression) => visit_expression(expression, definitions, registry)?,
      Statement::Return { value: Some(value), .. } => visit_expression(value, definitions, registry)?,
      _ =>
      {}
    }
  }
  Some(())
}

fn visit_block(block: &Block, definitions: &HashMap<&str, &NominalType>, registry: &mut AggregateRegistry)
               -> Option<()>
{
  visit_statements(&block.statements, definitions, registry)?;
  if let Some(tail) = &block.tail
  {
    visit_expression(tail, definitions, registry)?;
  }
  Some(())
}

fn visit_expression(expression: &Expression,
                    definitions: &HashMap<&str, &NominalType>,
                    registry: &mut AggregateRegistry)
                    -> Option<()>
{
  let mut visit_type = |ty: &HirType| {
    let _ = visit_checked_type(ty, definitions, &mut HashSet::new(), registry);
  };
  match expression
  {
    Expression::Literal { .. } |
    Expression::Local { .. } |
    Expression::Field { .. } |
    Expression::ArrayLength { .. } =>
    {}
    Expression::Object { nominal_type,
                         fields,
                         .. } =>
    {
      visit_type(&HirType::Named(nominal_type.clone()));
      for field in fields
      {
        visit_expression(&field.value, definitions, registry)?;
      }
    }
    Expression::EnumData { enum_type,
                           payload,
                           payload_type,
                           .. } =>
    {
      visit_type(&HirType::Named(enum_type.clone()));
      if let Some(payload_type) = payload_type
      {
        visit_type(payload_type);
      }
      if let Some(payload) = payload
      {
        visit_expression(payload, definitions, registry)?;
      }
    }
    Expression::Array { elements, .. } | Expression::Set { elements, .. } =>
    {
      for element in elements
      {
        visit_expression(element, definitions, registry)?;
      }
    }
    Expression::Map { entries, .. } =>
    {
      for entry in entries
      {
        visit_expression(&entry.key, definitions, registry)?;
        visit_expression(&entry.value, definitions, registry)?;
      }
    }
    Expression::Tuple { fields,
                        tuple_type,
                        .. } =>
    {
      visit_type(tuple_type);
      for field in fields
      {
        visit_expression(&field.value, definitions, registry)?;
      }
    }
    Expression::Member { receiver,
                         field_type,
                         .. } =>
    {
      visit_type(field_type);
      visit_expression(receiver, definitions, registry)?;
    }
    Expression::TupleElement { tuple,
                               element_type,
                               .. } =>
    {
      visit_type(element_type);
      visit_expression(tuple, definitions, registry)?;
    }
    Expression::Index { collection,
                        index,
                        element_type,
                        .. } =>
    {
      visit_type(element_type);
      visit_expression(collection, definitions, registry)?;
      visit_expression(index, definitions, registry)?;
    }
    Expression::Assign { value, .. } |
    Expression::AssignField { value, .. } |
    Expression::OptionalCoalesceAssign { value, .. } =>
    {
      visit_expression(value, definitions, registry)?;
    }
    Expression::Update { .. } =>
    {}
    Expression::Binary { left,
                         right,
                         .. } =>
    {
      visit_expression(left, definitions, registry)?;
      visit_expression(right, definitions, registry)?;
    }
    Expression::Unary { operand, .. } => visit_expression(operand, definitions, registry)?,
    Expression::OptionalUnwrap { value,
                                 element_type,
                                 .. } =>
    {
      visit_type(&HirType::Optional { element: element_type.clone() });
      visit_expression(value, definitions, registry)?;
    }
    Expression::OptionalMember { receiver,
                                 field_type,
                                 result_type,
                                 .. } =>
    {
      visit_type(field_type);
      visit_type(result_type);
      visit_expression(receiver, definitions, registry)?;
    }
    Expression::ResultPropagation { value, .. } => visit_expression(value, definitions, registry)?,
    Expression::Call { arguments,
                       parameter_types,
                       return_type,
                       .. } =>
    {
      for ty in parameter_types
      {
        visit_type(ty);
      }
      visit_type(return_type);
      for argument in arguments
      {
        visit_expression(argument, definitions, registry)?;
      }
    }
    Expression::If { condition,
                     then_block,
                     else_block,
                     result_type,
                     .. } =>
    {
      visit_type(result_type);
      visit_expression(condition, definitions, registry)?;
      visit_block(then_block, definitions, registry)?;
      visit_block(else_block, definitions, registry)?;
    }
    Expression::Match { selector,
                        selector_type,
                        arms,
                        result_type,
                        .. } =>
    {
      visit_type(selector_type);
      visit_type(result_type);
      visit_expression(selector, definitions, registry)?;
      for arm in arms
      {
        visit_block(&arm.body, definitions, registry)?;
      }
    }
  }
  Some(())
}

fn visit_type_ref(value: &TypeRef,
                  definitions: &HashMap<&str, &NominalType>,
                  visiting: &mut HashSet<String>,
                  registry: &mut AggregateRegistry)
                  -> Option<Type>
{
  visit_checked_type(&declarations::type_ref_to_checked(value)?,
                     definitions,
                     visiting,
                     registry)
}

fn visit_checked_type(value: &HirType,
                      definitions: &HashMap<&str, &NominalType>,
                      visiting: &mut HashSet<String>,
                      registry: &mut AggregateRegistry)
                      -> Option<Type>
{
  match value
  {
    HirType::Primitive(primitive) => super::mir_lowering::primitive_to_xlil(*primitive),
    HirType::Named(name) =>
    {
      registry.types
              .get(name)
              .copied()
              .or_else(|| visit(definitions.get(name.as_str())?, definitions, visiting, registry))
    }
    HirType::Reference { referent,
                         mutable: false, }
      if **referent == HirType::Primitive(crate::hir::type_check::PrimitiveType::Str) =>
    {
      Some(Type::STR)
    }
    HirType::Optional { element } =>
    {
      if let Some((_, value_type)) = registry.optionals.iter().find(|(source, _)| source == value)
      {
        return Some(*value_type);
      }
      let element_type = visit_checked_type(element, definitions, visiting, registry)?;
      let value_type = Type::aggregate(registry.layouts.len() as u32);
      registry.layouts
              .push(AggregateLayout { name: format!("optional.{}", registry.optionals.len()),
                                      value_type,
                                      fields: vec![Type::BOOL, element_type] });
      registry.optionals.push((value.clone(), value_type));
      Some(value_type)
    }
    HirType::Result { success,
                      error, } =>
    {
      if let Some((_, value_type)) = registry.results.iter().find(|(source, _)| source == value)
      {
        return Some(*value_type);
      }
      let success_type = result_payload_type(success, definitions, visiting, registry)?;
      let error_type = result_payload_type(error, definitions, visiting, registry)?;
      let value_type = Type::aggregate(registry.layouts.len() as u32);
      registry.layouts
              .push(AggregateLayout { name: format!("result.{}", registry.results.len()),
                                      value_type,
                                      fields: vec![Type::BOOL, success_type, error_type] });
      registry.results.push((value.clone(), value_type));
      Some(value_type)
    }
    HirType::Reference { referent, .. } =>
    {
      let _ = visit_checked_type(referent, definitions, visiting, registry);
      None
    }
    HirType::Tuple { fields } =>
    {
      if let Some((_, value_type)) = registry.tuples.iter().find(|(source, _)| source == value)
      {
        return Some(*value_type);
      }
      let field_types = fields.iter()
                              .map(|field| visit_checked_type(&field.ty, definitions, visiting, registry))
                              .collect::<Option<Vec<_>>>()?;
      let value_type = Type::aggregate(registry.layouts.len() as u32);
      registry.layouts
              .push(AggregateLayout { name: format!("tuple.{}", registry.tuples.len()),
                                      value_type,
                                      fields: field_types });
      registry.tuples.push((value.clone(), value_type));
      Some(value_type)
    }
    HirType::Array { element, .. } | HirType::Set { element } =>
    {
      let _ = visit_checked_type(element, definitions, visiting, registry);
      None
    }
    HirType::Map { key,
                   value, } =>
    {
      let _ = visit_checked_type(key, definitions, visiting, registry);
      let _ = visit_checked_type(value, definitions, visiting, registry);
      None
    }
    HirType::Unit => None,
  }
}

fn result_payload_type(value: &HirType,
                       definitions: &HashMap<&str, &NominalType>,
                       visiting: &mut HashSet<String>,
                       registry: &mut AggregateRegistry)
                       -> Option<Type>
{
  if matches!(value, HirType::Unit)
  {
    Some(Type::BOOL)
  }
  else
  {
    visit_checked_type(value, definitions, visiting, registry)
  }
}

pub(crate) fn build(declarations: &[NominalType]) -> Option<AggregateRegistry>
{
  let owned_definitions = declarations.iter()
                                      .cloned()
                                      .map(|value| (value.name.clone(), value))
                                      .collect::<HashMap<_, _>>();
  for declaration in declarations.iter()
                                 .filter(|declaration| declaration.kind == NominalKind::Data)
  {
    super::declarations::resolved_fields(declaration, &owned_definitions).ok()?;
  }
  let definitions = declarations.iter()
                                .filter(|declaration| declaration.kind == NominalKind::Data)
                                .map(|declaration| (declaration.name.as_str(), declaration))
                                .collect::<HashMap<_, _>>();
  let mut registry = AggregateRegistry::default();
  let mut visiting = HashSet::new();
  for declaration in declarations.iter()
                                 .filter(|declaration| declaration.kind == NominalKind::Data)
  {
    visit(declaration, &definitions, &mut visiting, &mut registry)?;
  }
  for declaration in declarations.iter()
                                 .filter(|declaration| declaration.kind == NominalKind::Enum)
  {
    if !declaration.bases.is_empty() || !declaration.fields.is_empty() || registry.types.contains_key(&declaration.name)
    {
      return None;
    }
    let value_type = Type::aggregate(registry.layouts.len() as u32);
    registry.types.insert(declaration.name.clone(), value_type);
    registry.layouts.push(AggregateLayout { name: declaration.name.clone(),
                                            value_type,
                                            fields: vec![Type::I32] });
  }
  let enum_data = super::enum_data::EnumDataRegistry::build(declarations);
  if !enum_data.is_valid()
  {
    return None;
  }
  for declaration in declarations.iter()
                                 .filter(|declaration| declaration.kind == NominalKind::EnumData)
  {
    let value_type = Type::aggregate(registry.layouts.len() as u32);
    let mut fields = vec![Type::I32];
    let mut variants = Vec::new();
    for variant in enum_data.variants(&declaration.name).ok()?
    {
      let payload_type =
        variant.payload
               .as_ref()
               .and_then(declarations::type_ref_to_checked)
               .and_then(|payload| visit_checked_type(&payload, &definitions, &mut HashSet::new(), &mut registry));
      if variant.payload.is_some() && payload_type.is_none()
      {
        return None;
      }
      let field = payload_type.and_then(|payload_type| {
                                fields.push(payload_type);
                                u32::try_from(fields.len() - 1).ok()
                              });
      variants.push(EnumDataVariantLayout { owner: variant.owner.clone(),
                                            name: variant.name.clone(),
                                            tag: variant.tag,
                                            field,
                                            payload_type });
    }
    registry.types.insert(declaration.name.clone(), value_type);
    registry.layouts.push(AggregateLayout { name: declaration.name.clone(),
                                            value_type,
                                            fields });
    registry.enum_data.push(EnumDataLayout { name: declaration.name.clone(),
                                             value_type,
                                             variants });
  }
  Some(registry)
}

fn visit(declaration: &NominalType,
         definitions: &HashMap<&str, &NominalType>,
         visiting: &mut HashSet<String>,
         registry: &mut AggregateRegistry)
         -> Option<Type>
{
  if let Some(value_type) = registry.types.get(&declaration.name)
  {
    return Some(*value_type);
  }
  if !visiting.insert(declaration.name.clone())
  {
    return None;
  }
  let mut fields = Vec::new();
  for base in &declaration.bases
  {
    let TypeRef::Named(name) = &base.ty
    else
    {
      return None;
    };
    let base = *definitions.get(name.as_str())?;
    if base.kind != declaration.kind
    {
      return None;
    }
    let base_type = visit(base, definitions, visiting, registry)?;
    fields.extend(registry.layouts
                          .get(base_type.registry_id as usize)?
                          .fields
                          .iter()
                          .copied());
  }
  for field in &declaration.fields
  {
    fields.push(match &field.ty
          {
            TypeRef::Primitive(primitive) => crate::hir::mir_lowering::primitive_to_xlil(*primitive)?,
            TypeRef::Named(name) => visit(definitions.get(name.as_str())?, definitions, visiting, registry)?,
            TypeRef::Unit => return None,
            TypeRef::Array { .. } |
            TypeRef::Map { .. } |
            TypeRef::Tuple { .. } |
            TypeRef::Optional { .. } |
            TypeRef::Result { .. } |
            TypeRef::Reference { .. } => return None,
          });
  }
  visiting.remove(&declaration.name);
  let value_type = Type::aggregate(registry.layouts.len() as u32);
  registry.types.insert(declaration.name.clone(), value_type);
  registry.layouts.push(AggregateLayout { name: declaration.name.clone(),
                                          value_type,
                                          fields });
  Some(value_type)
}
