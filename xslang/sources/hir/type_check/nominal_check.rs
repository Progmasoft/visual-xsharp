/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 */

use super::*;

impl TypeChecker
{
  pub(super) fn check_optional_member(&mut self,
                                      receiver: &Expression,
                                      owner: &str,
                                      name: &str,
                                      field_type: &Type,
                                      result_type: &Type,
                                      span: Span)
  {
    self.check_expression(receiver);
    let expected_receiver = Type::Optional { element: Box::new(Type::Named(owner.to_string())) };
    if self.expression_type(receiver) != Some(expected_receiver)
    {
      self.diagnostics
          .push(Diagnostic { code: DiagnosticCode::UnknownField,
                             message: format!("optional member receiver is not an Optional<{owner}> value"),
                             span });
      return;
    }
    let expected_result = Type::Optional { element: Box::new(field_type.clone()) };
    if result_type != &expected_result
    {
      self.diagnostics
          .push(Diagnostic { code: DiagnosticCode::LiteralTypeMismatch,
                             message: "optional member result must wrap the declared field type".to_string(),
                             span });
    }
    let Some(definition) = self.nominal_types.get(owner)
    else
    {
      self.diagnostics
          .push(Diagnostic { code: DiagnosticCode::UnknownNominalType,
                             message: format!("unknown nominal type '{owner}'"),
                             span });
      return;
    };
    let Ok(fields) = super::super::declarations::resolved_fields(definition, &self.nominal_types)
    else
    {
      return;
    };
    let declared = fields.into_iter()
                         .find(|field| field.name == name)
                         .and_then(|field| super::super::declarations::type_ref_to_checked(&field.ty));
    if declared.as_ref() != Some(field_type)
    {
      self.diagnostics
          .push(Diagnostic { code: DiagnosticCode::UnknownField,
                             message: format!("type '{owner}' has no matching field '{name}' for optional access"),
                             span });
    }
  }

  pub(super) fn check_member(&mut self, receiver: &Expression, owner: &str, name: &str, field_type: &Type, span: Span)
  {
    self.check_expression(receiver);
    if self.expression_type(receiver) != Some(Type::Named(owner.to_string()))
    {
      self.diagnostics.push(Diagnostic { code: DiagnosticCode::UnknownField,
                                         message: format!("member receiver is not a '{owner}' value"),
                                         span });
      return;
    }
    let Some(definition) = self.nominal_types.get(owner)
    else
    {
      self.diagnostics
          .push(Diagnostic { code: DiagnosticCode::UnknownNominalType,
                             message: format!("unknown nominal type '{owner}'"),
                             span });
      return;
    };
    let Ok(fields) = super::super::declarations::resolved_fields(definition, &self.nominal_types)
    else
    {
      return;
    };
    let declared = fields.into_iter()
                         .find(|field| field.name == name)
                         .and_then(|field| super::super::declarations::type_ref_to_checked(&field.ty));
    if declared.as_ref() != Some(field_type)
    {
      self.diagnostics.push(Diagnostic { code: DiagnosticCode::UnknownField,
                                         message: format!("type '{owner}' has no matching field '{name}'"),
                                         span });
    }
  }

  pub(super) fn check_field_against_type(&mut self, expression: &Expression, path: &FieldPath, expected: &Type)
  {
    self.check_expression(expression);
    if path.ty != *expected
    {
      self.diagnostics
          .push(Diagnostic { code: DiagnosticCode::LiteralTypeMismatch,
                             message: "field expression is not assignable to the target type".to_string(),
                             span: path.span });
    }
  }

  pub(super) fn check_member_against_type(&mut self,
                                          expression: &Expression,
                                          field_type: &Type,
                                          span: Span,
                                          expected: &Type)
  {
    self.check_expression(expression);
    if field_type != expected
    {
      self.diagnostics
          .push(Diagnostic { code: DiagnosticCode::LiteralTypeMismatch,
                             message: "member expression is not assignable to the target type".to_string(),
                             span });
    }
  }

  pub(super) fn check_object(&mut self, nominal_type: &str, fields: &[ObjectField], span: Span)
  {
    let Some(definition) = self.nominal_types.get(nominal_type).cloned()
    else
    {
      self.diagnostics
          .push(Diagnostic { code: DiagnosticCode::UnknownNominalType,
                             message: format!("unknown nominal type '{nominal_type}'"),
                             span });
      return;
    };
    let Ok(definition_fields) = super::super::declarations::resolved_fields(&definition, &self.nominal_types)
    else
    {
      self.diagnostics
          .push(Diagnostic { code: DiagnosticCode::UnknownNominalType,
                             message: format!("type '{nominal_type}' has an invalid base layout"),
                             span });
      return;
    };
    let mut seen = std::collections::HashSet::new();
    for field in fields
    {
      if !seen.insert(field.name.as_str())
      {
        self.diagnostics.push(Diagnostic { code: DiagnosticCode::DuplicateField,
                                           message: format!("field '{}' is initialized more than once", field.name),
                                           span: field.span });
        continue;
      }
      let Some(expected) = definition_fields.iter().find(|candidate| candidate.name == field.name)
      else
      {
        self.diagnostics.push(Diagnostic { code: DiagnosticCode::UnknownField,
                                           message: format!("type '{nominal_type}' has no field '{}'", field.name),
                                           span: field.span });
        continue;
      };
      if let Some(expected) = super::super::declarations::type_ref_to_checked(&expected.ty)
      {
        self.check_expression_against_type(&field.value, &expected);
      }
    }
    for field in &definition_fields
    {
      if !seen.contains(field.name.as_str())
      {
        self.diagnostics.push(Diagnostic { code: DiagnosticCode::MissingField,
                                           message: format!("object literal is missing field '{}'", field.name),
                                           span });
      }
    }
  }

  pub(super) fn check_field_path(&mut self, path: &FieldPath)
  {
    let Some(mut current_type) = self.find_local(&path.root).map(|local| local.ty.clone())
    else
    {
      self.diagnostics.push(Diagnostic { code: DiagnosticCode::UnknownLocal,
                                         message: format!("unknown local '{}'", path.root),
                                         span: path.span });
      return;
    };
    for field_name in &path.fields
    {
      let Type::Named(type_name) = current_type
      else
      {
        self.diagnostics.push(Diagnostic { code: DiagnosticCode::UnknownField,
                                           message: format!("type has no field '{field_name}'"),
                                           span: path.span });
        return;
      };
      let Some(definition) = self.nominal_types.get(&type_name)
      else
      {
        self.diagnostics.push(Diagnostic { code: DiagnosticCode::UnknownField,
                                           message: format!("type '{type_name}' has no field '{field_name}'"),
                                           span: path.span });
        return;
      };
      let Ok(fields) = super::super::declarations::resolved_fields(definition, &self.nominal_types)
      else
      {
        return;
      };
      let Some(field) = fields.into_iter().find(|field| field.name == *field_name)
      else
      {
        self.diagnostics.push(Diagnostic { code: DiagnosticCode::UnknownField,
                                           message: format!("type '{type_name}' has no field '{field_name}'"),
                                           span: path.span });
        return;
      };
      let Some(field_type) = super::super::declarations::type_ref_to_checked(&field.ty)
      else
      {
        return;
      };
      current_type = field_type;
    }
    if current_type != path.ty
    {
      self.diagnostics
          .push(Diagnostic { code: DiagnosticCode::LiteralTypeMismatch,
                             message: "field path type does not match its nominal declaration".to_string(),
                             span: path.span });
    }
  }
}
