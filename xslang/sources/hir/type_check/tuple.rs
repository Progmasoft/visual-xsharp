/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 */

use super::*;

impl TypeChecker
{
  pub(super) fn check_tuple_assignment(&mut self,
                                       target: &str,
                                       index: u32,
                                       value: &Expression,
                                       tuple_type: &Type,
                                       element_type: &Type,
                                       span: Span)
  {
    let Some(local_type) = self.find_local(target).map(|local| local.ty.clone())
    else
    {
      self.diagnostics.push(Diagnostic { code: DiagnosticCode::UnknownLocal,
                                         message: format!("unknown tuple local '{target}'"),
                                         span });
      return;
    };
    let Type::Tuple { fields } = tuple_type
    else
    {
      self.report_collection_mismatch(span, "tuple element assignment requires a tuple target type");
      return;
    };
    if local_type != *tuple_type || fields.get(index as usize).map(|field| &field.ty) != Some(element_type)
    {
      self.report_collection_mismatch(span, "tuple element assignment does not match its target layout");
    }
    self.check_expression_against_type(value, element_type);
  }

  pub(super) fn check_tuple(&mut self, fields: &[TupleFieldValue], tuple_type: &Type, span: Span)
  {
    let Type::Tuple { fields: expected } = tuple_type
    else
    {
      self.report_collection_mismatch(span, "tuple expression requires a tuple type");
      return;
    };
    if fields.len() != expected.len()
    {
      self.report_collection_mismatch(span, "tuple field count does not match its tuple type");
    }
    self.check_tuple_fields(fields, expected);
  }

  pub(super) fn check_tuple_element(&mut self, tuple: &Expression, index: u32, element_type: &Type, span: Span)
  {
    self.check_expression(tuple);
    let Some(Type::Tuple { fields }) = self.expression_type(tuple)
    else
    {
      self.report_collection_mismatch(span, "tuple element source must have tuple type");
      return;
    };
    if fields.get(index as usize).map(|field| &field.ty) != Some(element_type)
    {
      self.report_collection_mismatch(span, "tuple element type does not match the selected field");
    }
  }

  pub(super) fn check_tuple_against_type(&mut self,
                                         fields: &[TupleFieldValue],
                                         tuple_type: &Type,
                                         span: Span,
                                         target: &Type)
  {
    let Type::Tuple { fields: expected } = target
    else
    {
      self.report_collection_mismatch(span, "tuple literal requires a tuple target type");
      return;
    };
    if tuple_type != target || fields.len() != expected.len()
    {
      self.report_collection_mismatch(span, "tuple literal does not match its target type");
    }
    self.check_tuple_fields(fields, expected);
  }

  pub(super) fn check_tuple_element_against_type(&mut self, expression: &Expression, span: Span, target: &Type)
  {
    self.check_expression(expression);
    if self.expression_type(expression).as_ref() != Some(target)
    {
      self.report_collection_mismatch(span, "tuple element is not assignable to the target type");
    }
  }

  fn check_tuple_fields(&mut self, fields: &[TupleFieldValue], expected: &[TupleFieldType])
  {
    for (field, expected) in fields.iter().zip(expected)
    {
      if field.name != expected.name
      {
        self.report_collection_mismatch(field.span, "tuple field name does not match its tuple type");
      }
      self.check_expression_against_type(&field.value, &expected.ty);
    }
  }
}
