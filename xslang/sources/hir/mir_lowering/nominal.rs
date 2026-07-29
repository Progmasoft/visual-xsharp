/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 */

use super::*;
use crate::hir::type_check::{FieldPath, Local};

struct NominalArguments<'a>
{
  values: &'a mut Vec<mir::LocalId>,
  visiting: &'a mut Vec<String>,
}

impl HirToMirLowerer
{
  pub(super) fn lower_member_value(&mut self,
                                   expression: &Expression,
                                   expected_type: XlilType,
                                   lowered: &mut mir::Function)
                                   -> Option<mir::LocalId>
  {
    let Expression::Member { receiver,
                             owner,
                             name,
                             field_type,
                             span, } = expression
    else
    {
      return None;
    };
    if self.lower_value_type(field_type, *span)? != expected_type
    {
      self.report(DiagnosticCode::UnsupportedType,
                  "member result type does not match its MIR target type",
                  *span);
      return None;
    }
    let definition = self.nominal_types.get(owner)?.clone();
    let fields = self.resolved_nominal_fields(&definition)?;
    let field = fields.iter().position(|field| field.name == *name)?;
    let aggregate_type = *self.aggregate_types.get(owner)?;
    let aggregate = self.lower_expression_to_local(receiver, aggregate_type, lowered)?;
    let result = self.declare_temp(expected_type, *span, lowered)?;
    self.current_block_mut(lowered)
        .statements
        .push(mir::Statement::Extract { result,
                                        aggregate,
                                        field: u32::try_from(field).ok()?,
                                        field_type: expected_type,
                                        span: *span });
    Some(result)
  }

  pub(super) fn lower_object_value(&mut self,
                                   type_name: &str,
                                   initializers: &[crate::hir::type_check::ObjectField],
                                   span: Span,
                                   lowered: &mut mir::Function)
                                   -> Option<mir::LocalId>
  {
    let definition = self.nominal_types.get(type_name)?.clone();
    let value_type = self.aggregate_types.get(type_name).copied()?;
    let definition_fields = self.resolved_nominal_fields(&definition)?;
    let mut fields = Vec::with_capacity(definition_fields.len());
    let mut field_types = Vec::with_capacity(definition_fields.len());
    for field in &definition_fields
    {
      let initializer = initializers.iter().find(|candidate| candidate.name == field.name)?;
      let checked_type = crate::hir::declarations::type_ref_to_checked(&field.ty)?;
      let field_type = self.lower_value_type(&checked_type, initializer.span)?;
      let value = match &checked_type
      {
        Type::Named(nested) =>
        {
          let Expression::Object { nominal_type,
                                   fields,
                                   span, } = &initializer.value
          else
          {
            self.report(DiagnosticCode::UnsupportedExpression,
                        "aggregate field requires a matching object value",
                        initializer.span);
            return None;
          };
          if nominal_type != nested
          {
            self.report(DiagnosticCode::UnsupportedType,
                        "aggregate field object has the wrong nominal type",
                        initializer.span);
            return None;
          }
          self.lower_object_value(nested, fields, *span, lowered)?
        }
        _ => self.lower_expression_to_local(&initializer.value, field_type, lowered)?,
      };
      fields.push(value);
      field_types.push(field_type);
    }
    let result = self.declare_temp(value_type, span, lowered)?;
    self.current_block_mut(lowered)
        .statements
        .push(mir::Statement::Aggregate { result,
                                          value_type,
                                          fields,
                                          field_types,
                                          span });
    Some(result)
  }

  pub(super) fn lower_nominal_parameter(&mut self, local: &Local, lowered: &mut mir::Function)
  {
    let Type::Named(type_name) = &local.ty
    else
    {
      return;
    };
    let Some(definition) = self.nominal_types.get(type_name).cloned()
    else
    {
      self.report(DiagnosticCode::UnsupportedType,
                  format!("nominal parameter type '{type_name}' has no HIR declaration"),
                  local.span);
      return;
    };
    if definition.kind == crate::hir::declarations::NominalKind::Enum
    {
      let Some(value_type) = self.aggregate_types.get(type_name).copied()
      else
      {
        self.report(DiagnosticCode::UnsupportedType,
                    "enum parameter has no aggregate MIR layout",
                    local.span);
        return;
      };
      let id = mir::LocalId(self.next_local);
      self.next_local += 1;
      self.locals.insert(local.name.clone(), id);
      lowered.parameters.push(mir::Parameter { local: id,
                                               name: local.name.clone(),
                                               value_type,
                                               span: local.span });
      return;
    }
    if definition.kind != crate::hir::declarations::NominalKind::Data
    {
      self.report(DiagnosticCode::UnsupportedType,
                  "only data parameters have a scalar native ABI in this compiler stage",
                  local.span);
      return;
    }
    self.append_nominal_parameters(&local.name,
                                   &[],
                                   &definition,
                                   local.span,
                                   lowered,
                                   &mut vec![definition.name.clone()]);
  }

  pub(super) fn lower_nominal_binding(&mut self,
                                      local: &Local,
                                      initializer: Option<&Expression>,
                                      lowered: &mut mir::Function)
  {
    let Type::Named(type_name) = &local.ty
    else
    {
      return;
    };
    let Some(definition) = self.nominal_types.get(type_name).cloned()
    else
    {
      self.report(DiagnosticCode::UnsupportedType,
                  format!("nominal type '{type_name}' has no HIR declaration"),
                  local.span);
      return;
    };
    if definition.kind == crate::hir::declarations::NominalKind::Enum
    {
      let id = self.declare_local(local.name.clone(), &local.ty, local.mutable, local.span, lowered);
      if let Some(initializer) = initializer
      {
        self.lower_assignment(id, initializer, lowered);
      }
      return;
    }
    let Some(initializer) = initializer
    else
    {
      self.nominal_locals.insert(local.name.clone(), type_name.clone());
      self.declare_nominal_place_fields(&local.name, &[], &definition, local.span, lowered);
      return;
    };
    self.nominal_locals.insert(local.name.clone(), type_name.clone());
    if let Expression::Object { nominal_type,
                                fields,
                                .. } = initializer
    {
      if nominal_type != type_name
      {
        self.report(DiagnosticCode::UnsupportedType,
                    "object initializer type does not match nominal local type",
                    local.span);
        return;
      }
      self.lower_object_fields(&local.name, &[], &definition, fields, local.span, lowered);
      return;
    }
    let Some(value_type) = self.aggregate_types.get(type_name).copied()
    else
    {
      self.report(DiagnosticCode::UnsupportedType,
                  "nominal local type has no aggregate MIR layout",
                  local.span);
      return;
    };
    let Some(value) = self.lower_expression_to_local(initializer, value_type, lowered)
    else
    {
      return;
    };
    self.declare_nominal_place_fields(&local.name, &[], &definition, local.span, lowered);
    self.extract_into_nominal_place(&local.name, &[], &definition, value, local.span, lowered);
  }

  pub(super) fn lower_nominal_place_value(&mut self,
                                          root: &str,
                                          type_name: &str,
                                          span: Span,
                                          lowered: &mut mir::Function)
                                          -> Option<mir::LocalId>
  {
    let definition = self.nominal_types.get(type_name)?.clone();
    self.build_nominal_place_value(root, &[], &definition, span, lowered, &mut vec![type_name.to_string()])
  }

  fn build_nominal_place_value(&mut self,
                               root: &str,
                               prefix: &[String],
                               definition: &crate::hir::declarations::NominalType,
                               span: Span,
                               lowered: &mut mir::Function,
                               visiting: &mut Vec<String>)
                               -> Option<mir::LocalId>
  {
    let fields = self.resolved_nominal_fields(definition)?;
    let value_type = *self.aggregate_types.get(&definition.name)?;
    let mut values = Vec::with_capacity(fields.len());
    let mut field_types = Vec::with_capacity(fields.len());
    for field in fields
    {
      let field_type = crate::hir::declarations::type_ref_to_checked(&field.ty)?;
      let mut path = prefix.to_vec();
      path.push(field.name.clone());
      let lowered_type = self.lower_value_type(&field_type, span)?;
      let value = match field_type
      {
        Type::Primitive(_) => self.lower_field_load(&FieldPath { root: root.to_string(),
                                                                 fields: path,
                                                                 ty: field_type,
                                                                 mutable: false,
                                                                 span },
                                                    lowered_type,
                                                    lowered)?,
        Type::Named(ref nested_name) =>
        {
          if visiting.contains(nested_name)
          {
            self.report(DiagnosticCode::UnsupportedType,
                        "recursive data value requires an indirect ABI",
                        span);
            return None;
          }
          let nested = self.nominal_types.get(nested_name)?.clone();
          visiting.push(nested_name.clone());
          let value = self.build_nominal_place_value(root, &path, &nested, span, lowered, visiting)?;
          visiting.pop();
          value
        }
        _ => return None,
      };
      values.push(value);
      field_types.push(lowered_type);
    }
    let result = self.declare_temp(value_type, span, lowered)?;
    self.current_block_mut(lowered)
        .statements
        .push(mir::Statement::Aggregate { result,
                                          value_type,
                                          fields: values,
                                          field_types,
                                          span });
    Some(result)
  }

  fn declare_nominal_place_fields(&mut self,
                                  root: &str,
                                  prefix: &[String],
                                  definition: &crate::hir::declarations::NominalType,
                                  span: Span,
                                  lowered: &mut mir::Function)
  {
    let Some(definition_fields) = self.resolved_nominal_fields(definition)
    else
    {
      return;
    };
    for field in &definition_fields
    {
      let Some(field_type) = crate::hir::declarations::type_ref_to_checked(&field.ty)
      else
      {
        continue;
      };
      let mut path = prefix.to_vec();
      path.push(field.name.clone());
      match field_type
      {
        Type::Primitive(_) =>
        {
          let key = field_key(root, &path);
          let local = self.declare_local(key.clone(), &field_type, field.mutable, span, lowered);
          self.field_locals.insert(key, local);
        }
        Type::Named(ref nested_name) =>
        {
          if let Some(nested) = self.nominal_types.get(nested_name).cloned()
          {
            self.declare_nominal_place_fields(root, &path, &nested, span, lowered);
          }
        }
        _ =>
        {}
      }
    }
  }

  fn extract_into_nominal_place(&mut self,
                                root: &str,
                                prefix: &[String],
                                definition: &crate::hir::declarations::NominalType,
                                aggregate: mir::LocalId,
                                span: Span,
                                lowered: &mut mir::Function)
  {
    let Some(definition_fields) = self.resolved_nominal_fields(definition)
    else
    {
      return;
    };
    for (index, field) in definition_fields.iter().enumerate()
    {
      let Some(field_type) = crate::hir::declarations::type_ref_to_checked(&field.ty)
      else
      {
        continue;
      };
      let Some(value_type) = self.lower_value_type(&field_type, span)
      else
      {
        continue;
      };
      let Some(value) = self.declare_temp(value_type, span, lowered)
      else
      {
        continue;
      };
      self.current_block_mut(lowered)
          .statements
          .push(mir::Statement::Extract { result: value,
                                          aggregate,
                                          field: index as u32,
                                          field_type: value_type,
                                          span });
      let mut path = prefix.to_vec();
      path.push(field.name.clone());
      match field_type
      {
        Type::Primitive(_) =>
        {
          if let Some(local) = self.field_locals.get(&field_key(root, &path)).copied()
          {
            self.current_block_mut(lowered)
                .statements
                .push(mir::Statement::StoreLocal { local,
                                                   value,
                                                   span });
          }
        }
        Type::Named(ref nested_name) =>
        {
          if let Some(nested) = self.nominal_types.get(nested_name).cloned()
          {
            self.extract_into_nominal_place(root, &path, &nested, value, span, lowered);
          }
        }
        _ =>
        {}
      }
    }
  }

  pub(super) fn lower_field_load(&mut self,
                                 path: &FieldPath,
                                 expected_type: XlilType,
                                 lowered: &mut mir::Function)
                                 -> Option<mir::LocalId>
  {
    let key = field_key(&path.root, &path.fields);
    let Some(local) = self.field_locals.get(&key).copied()
    else
    {
      self.report(DiagnosticCode::UnknownLocal,
                  format!("field place '{key}' has not been initialized"),
                  path.span);
      return None;
    };
    if self.local_value_type(local, lowered) != Some(expected_type)
    {
      self.report(DiagnosticCode::UnsupportedType,
                  "field place type does not match the expected MIR type",
                  path.span);
      return None;
    }
    if lowered.parameters.iter().any(|parameter| parameter.local == local)
    {
      return Some(local);
    }
    let result = self.declare_temp(expected_type, path.span, lowered)?;
    self.current_block_mut(lowered)
        .statements
        .push(mir::Statement::LoadLocal { result,
                                          local,
                                          span: path.span });
    Some(result)
  }

  pub(super) fn lower_field_assignment(&mut self, target: &FieldPath, value: &Expression, lowered: &mut mir::Function)
  {
    if let Type::Named(type_name) = &target.ty
    {
      let assignment_span = expression_span(value);
      let Some(definition) = self.nominal_types.get(type_name).cloned()
      else
      {
        self.report(DiagnosticCode::UnsupportedType,
                    format!("assigned nominal field type '{type_name}' has no HIR declaration"),
                    target.span);
        return;
      };
      let Some(values) = self.lower_nominal_argument(value, type_name, lowered)
      else
      {
        return;
      };
      let Some(locals) = self.nominal_place_locals(&target.root, &target.fields, &definition)
      else
      {
        self.report(DiagnosticCode::UnknownLocal,
                    format!("nominal field place '{}' has not been initialized",
                            field_key(&target.root, &target.fields)),
                    target.span);
        return;
      };
      if locals.len() != values.len()
      {
        self.report(DiagnosticCode::UnsupportedType,
                    "nominal assignment source and destination layouts differ",
                    target.span);
        return;
      }
      for (local, value) in locals.into_iter().zip(values)
      {
        self.current_block_mut(lowered)
            .statements
            .push(mir::Statement::StoreLocal { local,
                                               value,
                                               span: assignment_span });
      }
      return;
    }
    let key = field_key(&target.root, &target.fields);
    let Some(local) = self.field_locals.get(&key).copied()
    else
    {
      self.report(DiagnosticCode::UnknownLocal,
                  format!("field place '{key}' has not been initialized"),
                  target.span);
      return;
    };
    self.lower_assignment(local, value, lowered);
  }

  pub(super) fn lower_nominal_root_assignment(&mut self,
                                              root: &str,
                                              type_name: &str,
                                              value: &Expression,
                                              span: Span,
                                              lowered: &mut mir::Function)
  {
    self.lower_field_assignment(&FieldPath { root: root.to_string(),
                                             fields: Vec::new(),
                                             ty: Type::Named(type_name.to_string()),
                                             mutable: true,
                                             span },
                                value,
                                lowered);
  }

  pub(super) fn lower_nominal_argument(&mut self,
                                       expression: &Expression,
                                       type_name: &str,
                                       lowered: &mut mir::Function)
                                       -> Option<Vec<mir::LocalId>>
  {
    let definition = self.nominal_types.get(type_name)?.clone();
    if definition.kind == crate::hir::declarations::NominalKind::Enum
    {
      let value_type = self.aggregate_types.get(type_name).copied()?;
      return self.lower_expression_to_local(expression, value_type, lowered)
                 .map(|value| vec![value]);
    }
    if definition.kind != crate::hir::declarations::NominalKind::Data
    {
      self.report(DiagnosticCode::UnsupportedType,
                  "only data arguments have a scalar native ABI in this compiler stage",
                  expression_span(expression));
      return None;
    }
    let mut arguments = Vec::new();
    match expression
    {
      Expression::Local { name,
                          span, } =>
      {
        let mut visiting = vec![definition.name.clone()];
        self.append_nominal_place_arguments(name,
                                            &[],
                                            &definition,
                                            *span,
                                            lowered,
                                            &mut NominalArguments { values: &mut arguments,
                                                                    visiting: &mut visiting })?;
      }
      Expression::Field { path } if path.ty == Type::Named(type_name.to_string()) =>
      {
        let mut visiting = vec![definition.name.clone()];
        self.append_nominal_place_arguments(&path.root,
                                            &path.fields,
                                            &definition,
                                            path.span,
                                            lowered,
                                            &mut NominalArguments { values: &mut arguments,
                                                                    visiting: &mut visiting })?;
      }
      Expression::Object { nominal_type,
                           fields,
                           span, }
        if nominal_type == type_name =>
      {
        self.append_nominal_object_arguments(&definition,
                                             fields,
                                             *span,
                                             lowered,
                                             &mut arguments,
                                             &mut vec![definition.name.clone()])?;
      }
      _ =>
      {
        let span = expression_span(expression);
        let value_type = *self.aggregate_types.get(type_name)?;
        let aggregate = self.lower_expression_to_local(expression, value_type, lowered)?;
        self.append_nominal_aggregate_arguments(&definition,
                                                aggregate,
                                                span,
                                                lowered,
                                                &mut arguments,
                                                &mut vec![definition.name.clone()])?;
      }
    }
    Some(arguments)
  }

  fn append_nominal_aggregate_arguments(&mut self,
                                        definition: &crate::hir::declarations::NominalType,
                                        aggregate: mir::LocalId,
                                        span: Span,
                                        lowered: &mut mir::Function,
                                        arguments: &mut Vec<mir::LocalId>,
                                        visiting: &mut Vec<String>)
                                        -> Option<()>
  {
    for (index, field) in self.resolved_nominal_fields(definition)?.iter().enumerate()
    {
      let field_type = crate::hir::declarations::type_ref_to_checked(&field.ty)?;
      let value_type = self.lower_value_type(&field_type, span)?;
      let value = self.declare_temp(value_type, span, lowered)?;
      self.current_block_mut(lowered)
          .statements
          .push(mir::Statement::Extract { result: value,
                                          aggregate,
                                          field: u32::try_from(index).ok()?,
                                          field_type: value_type,
                                          span });
      match field_type
      {
        Type::Primitive(_) => arguments.push(value),
        Type::Named(ref nested_type) =>
        {
          if visiting.contains(nested_type)
          {
            self.report(DiagnosticCode::UnsupportedType,
                        "recursive data aggregate argument requires an indirect ABI",
                        span);
            return None;
          }
          let nested = self.nominal_types.get(nested_type)?.clone();
          visiting.push(nested_type.clone());
          self.append_nominal_aggregate_arguments(&nested, value, span, lowered, arguments, visiting)?;
          visiting.pop();
        }
        _ =>
        {
          self.report(DiagnosticCode::UnsupportedType,
                      "data aggregate field has no scalar native call ABI",
                      span);
          return None;
        }
      }
    }
    Some(())
  }

  fn append_nominal_parameters(&mut self,
                               root: &str,
                               prefix: &[String],
                               definition: &crate::hir::declarations::NominalType,
                               span: Span,
                               lowered: &mut mir::Function,
                               visiting: &mut Vec<String>)
  {
    let Some(definition_fields) = self.resolved_nominal_fields(definition)
    else
    {
      return;
    };
    for field in &definition_fields
    {
      let Some(field_type) = crate::hir::declarations::type_ref_to_checked(&field.ty)
      else
      {
        continue;
      };
      let mut path = prefix.to_vec();
      path.push(field.name.clone());
      match field_type
      {
        Type::Primitive(_) =>
        {
          let Some(value_type) = self.lower_value_type(&field_type, span)
          else
          {
            continue;
          };
          let id = mir::LocalId(self.next_local);
          self.next_local += 1;
          let name = field_key(root, &path);
          self.field_locals.insert(name.clone(), id);
          lowered.parameters.push(mir::Parameter { local: id,
                                                   name,
                                                   value_type,
                                                   span });
        }
        Type::Named(ref nested_type) =>
        {
          if visiting.contains(nested_type)
          {
            self.report(DiagnosticCode::UnsupportedType,
                        format!("recursive data parameter field '{}' requires an indirect ABI",
                                field_key(root, &path)),
                        span);
            continue;
          }
          let Some(nested) = self.nominal_types.get(nested_type).cloned()
          else
          {
            self.report(DiagnosticCode::UnsupportedType,
                        format!("nested parameter field '{}' has no HIR declaration",
                                field_key(root, &path)),
                        span);
            continue;
          };
          visiting.push(nested_type.clone());
          self.append_nominal_parameters(root, &path, &nested, span, lowered, visiting);
          visiting.pop();
        }
        _ => self.report(DiagnosticCode::UnsupportedType,
                         format!("parameter field '{}' has no scalar native ABI", field_key(root, &path)),
                         span),
      }
    }
  }

  fn append_nominal_place_arguments(&mut self,
                                    root: &str,
                                    prefix: &[String],
                                    definition: &crate::hir::declarations::NominalType,
                                    span: Span,
                                    lowered: &mut mir::Function,
                                    arguments: &mut NominalArguments<'_>)
                                    -> Option<()>
  {
    for field in &self.resolved_nominal_fields(definition)?
    {
      let field_type = crate::hir::declarations::type_ref_to_checked(&field.ty)?;
      let mut path = prefix.to_vec();
      path.push(field.name.clone());
      match field_type
      {
        Type::Primitive(_) =>
        {
          let value_type = self.lower_value_type(&field_type, span)?;
          arguments.values
                   .push(self.lower_field_load(&FieldPath { root: root.to_string(),
                                                            fields: path,
                                                            ty: field_type,
                                                            mutable: false,
                                                            span },
                                               value_type,
                                               lowered)?);
        }
        Type::Named(ref nested_type) =>
        {
          if arguments.visiting.contains(nested_type)
          {
            self.report(DiagnosticCode::UnsupportedType,
                        format!("recursive data argument field '{}' requires an indirect ABI",
                                field_key(root, &path)),
                        span);
            return None;
          }
          let nested = self.nominal_types.get(nested_type)?.clone();
          arguments.visiting.push(nested_type.clone());
          self.append_nominal_place_arguments(root, &path, &nested, span, lowered, arguments)?;
          arguments.visiting.pop();
        }
        _ => return None,
      }
    }
    Some(())
  }

  fn append_nominal_object_arguments(&mut self,
                                     definition: &crate::hir::declarations::NominalType,
                                     initializers: &[crate::hir::type_check::ObjectField],
                                     span: Span,
                                     lowered: &mut mir::Function,
                                     arguments: &mut Vec<mir::LocalId>,
                                     visiting: &mut Vec<String>)
                                     -> Option<()>
  {
    for field in &self.resolved_nominal_fields(definition)?
    {
      let initializer = initializers.iter().find(|candidate| candidate.name == field.name)?;
      let field_type = crate::hir::declarations::type_ref_to_checked(&field.ty)?;
      match field_type
      {
        Type::Primitive(_) =>
        {
          let value_type = self.lower_value_type(&field_type, span)?;
          arguments.push(self.lower_expression_to_local(&initializer.value, value_type, lowered)?);
        }
        Type::Named(ref nested_type) =>
        {
          if visiting.contains(nested_type)
          {
            self.report(DiagnosticCode::UnsupportedType,
                        "recursive data object arguments require an indirect ABI",
                        initializer.span);
            return None;
          }
          let nested = self.nominal_types.get(nested_type)?.clone();
          let Expression::Object { nominal_type,
                                   fields,
                                   .. } = &initializer.value
          else
          {
            return None;
          };
          if nominal_type != nested_type
          {
            return None;
          }
          visiting.push(nested_type.clone());
          self.append_nominal_object_arguments(&nested, fields, initializer.span, lowered, arguments, visiting)?;
          visiting.pop();
        }
        _ => return None,
      }
    }
    Some(())
  }

  fn nominal_place_locals(&self,
                          root: &str,
                          prefix: &[String],
                          definition: &crate::hir::declarations::NominalType)
                          -> Option<Vec<mir::LocalId>>
  {
    let mut locals = Vec::new();
    self.append_nominal_place_locals(root,
                                     prefix,
                                     definition,
                                     &mut vec![definition.name.clone()],
                                     &mut locals)?;
    Some(locals)
  }

  fn append_nominal_place_locals(&self,
                                 root: &str,
                                 prefix: &[String],
                                 definition: &crate::hir::declarations::NominalType,
                                 visiting: &mut Vec<String>,
                                 locals: &mut Vec<mir::LocalId>)
                                 -> Option<()>
  {
    for field in &self.resolved_nominal_fields(definition)?
    {
      let field_type = crate::hir::declarations::type_ref_to_checked(&field.ty)?;
      let mut path = prefix.to_vec();
      path.push(field.name.clone());
      match field_type
      {
        Type::Primitive(_) => locals.push(*self.field_locals.get(&field_key(root, &path))?),
        Type::Named(ref nested_type) =>
        {
          if visiting.contains(nested_type)
          {
            return None;
          }
          let nested = self.nominal_types.get(nested_type)?;
          visiting.push(nested_type.clone());
          self.append_nominal_place_locals(root, &path, nested, visiting, locals)?;
          visiting.pop();
        }
        _ => return None,
      }
    }
    Some(())
  }

  fn lower_object_fields(&mut self,
                         root: &str,
                         prefix: &[String],
                         definition: &crate::hir::declarations::NominalType,
                         initializers: &[crate::hir::type_check::ObjectField],
                         fallback_span: Span,
                         lowered: &mut mir::Function)
  {
    let Some(definition_fields) = self.resolved_nominal_fields(definition)
    else
    {
      return;
    };
    for field in &definition_fields
    {
      let Some(initializer) = initializers.iter().find(|candidate| candidate.name == field.name)
      else
      {
        continue;
      };
      let Some(value_type) = crate::hir::declarations::type_ref_to_checked(&field.ty)
      else
      {
        continue;
      };
      let mut path = prefix.to_vec();
      path.push(field.name.clone());
      match value_type
      {
        Type::Primitive(_) =>
        {
          let key = field_key(root, &path);
          let field_local = self.declare_local(key.clone(), &value_type, field.mutable, initializer.span, lowered);
          self.field_locals.insert(key, field_local);
          self.lower_assignment(field_local, &initializer.value, lowered);
        }
        Type::Named(ref nested_type) =>
        {
          let Some(nested_definition) = self.nominal_types.get(nested_type).cloned()
          else
          {
            self.report(DiagnosticCode::UnsupportedType,
                        format!("nested nominal field '{}' has no HIR declaration",
                                field_key(root, &path)),
                        initializer.span);
            continue;
          };
          let Expression::Object { nominal_type,
                                   fields,
                                   .. } = &initializer.value
          else
          {
            self.report(DiagnosticCode::UnsupportedExpression,
                        format!("nested nominal field '{}' requires an object initializer",
                                field_key(root, &path)),
                        initializer.span);
            continue;
          };
          if nominal_type != nested_type
          {
            self.report(DiagnosticCode::UnsupportedType,
                        format!("nested object initializer for '{}' has the wrong nominal type",
                                field_key(root, &path)),
                        initializer.span);
            continue;
          }
          self.lower_object_fields(root, &path, &nested_definition, fields, fallback_span, lowered);
        }
        _ => self.report(DiagnosticCode::UnsupportedType,
                         format!("field '{}' is not scalar-lowerable yet", field_key(root, &path)),
                         fallback_span),
      }
    }
  }

  fn resolved_nominal_fields(&self,
                             definition: &crate::hir::declarations::NominalType)
                             -> Option<Vec<crate::hir::declarations::Field>>
  {
    crate::hir::declarations::resolved_fields(definition, &self.nominal_types).ok()
  }
}

fn field_key(root: &str, fields: &[String]) -> String
{
  std::iter::once(root).chain(fields.iter().map(String::as_str))
                       .collect::<Vec<_>>()
                       .join(".")
}
