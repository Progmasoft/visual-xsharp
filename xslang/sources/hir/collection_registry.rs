/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 */

use super::aggregate_registry::AggregateRegistry;
use super::declarations;
use super::type_check::{Block, Statement, Type};
use crate::xlil;

#[derive(Clone, Debug, Eq, PartialEq)]
pub(crate) struct ArrayLayout
{
  pub source_type: Type,
  pub value_type: xlil::Type,
  pub element_type: xlil::Type,
  pub length: Option<u64>,
}

#[derive(Clone, Debug, Default, Eq, PartialEq)]
pub(crate) struct CollectionRegistry
{
  pub arrays: Vec<ArrayLayout>,
}

impl CollectionRegistry
{
  #[must_use]
  pub fn value_type(&self, source_type: &Type) -> Option<xlil::Type>
  {
    self.arrays
        .iter()
        .find(|layout| &layout.source_type == source_type)
        .map(|layout| layout.value_type)
  }

  fn visit_type(&mut self, source_type: &Type, aggregates: &AggregateRegistry) -> Option<xlil::Type>
  {
    match source_type
    {
      Type::Primitive(primitive) => super::mir_lowering::primitive_to_xlil(*primitive),
      Type::Named(name) => aggregates.types.get(name).copied(),
      Type::Tuple { .. } => aggregates.tuples
                                      .iter()
                                      .find(|(source, _)| source == source_type)
                                      .map(|(_, value_type)| *value_type),
      Type::Array { length,
                    element, } =>
      {
        if let Some(value_type) = self.value_type(source_type)
        {
          return Some(value_type);
        }
        let element_type = self.visit_type(element, aggregates)?;
        let value_type = xlil::Type::array(u32::try_from(self.arrays.len()).ok()?);
        self.arrays.push(ArrayLayout { source_type: source_type.clone(),
                                       value_type,
                                       element_type,
                                       length: *length });
        Some(value_type)
      }
      Type::Unit | Type::Set { .. } | Type::Map { .. } | Type::Optional { .. } | Type::Reference { .. } => None,
    }
  }
}

pub(crate) fn build(module: &declarations::Module, aggregates: &AggregateRegistry) -> CollectionRegistry
{
  let mut registry = CollectionRegistry::default();
  for declaration in &module.functions
  {
    if let Some(function) = declaration.as_type_checked_input()
    {
      if let Some(return_type) = &function.return_type
      {
        let _ = registry.visit_type(return_type, aggregates);
      }
      for local in &function.locals
      {
        let _ = registry.visit_type(&local.ty, aggregates);
      }
      visit_statements(&mut registry, &function.body, aggregates);
    }
  }
  for nominal in &module.nominal_types
  {
    for field in &nominal.fields
    {
      if let Some(field_type) = declarations::type_ref_to_checked(&field.ty)
      {
        let _ = registry.visit_type(&field_type, aggregates);
      }
    }
  }
  registry
}

pub(crate) fn build_functions_with_nominals(nominal_types: &[super::declarations::NominalType],
                                            functions: &[super::type_check::Function],
                                            aggregates: &AggregateRegistry)
                                            -> CollectionRegistry
{
  let mut registry = CollectionRegistry::default();
  for function in functions
  {
    if let Some(return_type) = &function.return_type
    {
      let _ = registry.visit_type(return_type, aggregates);
    }
    for local in &function.locals
    {
      let _ = registry.visit_type(&local.ty, aggregates);
    }
    visit_statements(&mut registry, &function.body, aggregates);
  }
  for nominal in nominal_types
  {
    for field in &nominal.fields
    {
      if let Some(field_type) = declarations::type_ref_to_checked(&field.ty)
      {
        let _ = registry.visit_type(&field_type, aggregates);
      }
    }
  }
  registry
}

fn visit_statements(registry: &mut CollectionRegistry, statements: &[Statement], aggregates: &AggregateRegistry)
{
  for statement in statements
  {
    match statement
    {
      Statement::Let { local, .. } =>
      {
        let _ = registry.visit_type(&local.ty, aggregates);
      }
      Statement::If { then_block,
                      else_block,
                      .. } =>
      {
        visit_block(registry, then_block, aggregates);
        if let Some(else_block) = else_block
        {
          visit_block(registry, else_block, aggregates);
        }
      }
      Statement::While { body, .. } => visit_block(registry, body, aggregates),
      Statement::For { initializer,
                       body,
                       .. } =>
      {
        if let Some(initializer) = initializer
        {
          visit_statements(registry, std::slice::from_ref(initializer), aggregates);
        }
        visit_block(registry, body, aggregates);
      }
      Statement::ForEach { binding,
                           iterable_type,
                           body,
                           .. } =>
      {
        let _ = registry.visit_type(&binding.ty, aggregates);
        let _ = registry.visit_type(iterable_type, aggregates);
        visit_block(registry, body, aggregates);
      }
      Statement::Match { arms, .. } =>
      {
        for arm in arms
        {
          visit_block(registry, &arm.body, aggregates);
        }
      }
      Statement::Expr(_) |
      Statement::AssignIndex { .. } |
      Statement::AssignTupleElement { .. } |
      Statement::Return { .. } |
      Statement::Break { .. } |
      Statement::Continue { .. } |
      Statement::Panic { .. } =>
      {}
    }
  }
}

fn visit_block(registry: &mut CollectionRegistry, block: &Block, aggregates: &AggregateRegistry)
{
  visit_statements(registry, &block.statements, aggregates);
}
