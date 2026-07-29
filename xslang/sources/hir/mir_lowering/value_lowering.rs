/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 */

use super::*;

impl HirToMirLowerer
{
  pub(super) fn lower_literal_into(&mut self,
                                   target: mir::LocalId,
                                   literal: &Literal,
                                   span: Span,
                                   lowered: &mut mir::Function)
  {
    let value_type = lowered.locals
                            .iter()
                            .find(|local| local.id == target)
                            .and_then(|local| local.value_type);
    match (literal, value_type)
    {
      (Literal::Integer(value), Some(XlilType { kind: TypeKind::Bool, .. })) =>
      {
        let Ok(value) = value.replace('\'', "").parse::<u128>()
        else
        {
          self.report(DiagnosticCode::InvalidIntegerLiteral,
                      "HIR integer literal is not valid in Bool context",
                      span);
          return;
        };
        self.current_block_mut(lowered)
            .statements
            .push(mir::Statement::ConstBool { local: target,
                                              value: value != 0,
                                              span });
      }
      (Literal::Integer(value), Some(value_type)) if value_type.integer_width().is_some() =>
      {
        self.lower_integer_literal_into(target, value, value_type, span, lowered);
      }
      (Literal::Bool(value), Some(XlilType { kind: TypeKind::Bool, .. })) =>
      {
        self.current_block_mut(lowered)
            .statements
            .push(mir::Statement::ConstBool { local: target,
                                              value: *value,
                                              span });
      }
      (Literal::Float(value), Some(XlilType { kind: TypeKind::F32, .. })) =>
      {
        let Ok(value) = value.replace('\'', "").parse::<f32>()
        else
        {
          self.report(DiagnosticCode::UnsupportedExpression,
                      "HIR float literal is not a finite f32 value",
                      span);
          return;
        };
        if !value.is_finite()
        {
          self.report(DiagnosticCode::UnsupportedExpression,
                      "HIR float literal is not a finite f32 value",
                      span);
          return;
        }
        self.current_block_mut(lowered)
            .statements
            .push(mir::Statement::ConstF32 { local: target,
                                             bits: value.to_bits(),
                                             span });
      }
      (Literal::Float(value), Some(XlilType { kind: TypeKind::F64, .. })) =>
      {
        let Ok(value) = value.replace('\'', "").parse::<f64>()
        else
        {
          self.report(DiagnosticCode::UnsupportedExpression,
                      "HIR float literal is not a finite f64 value",
                      span);
          return;
        };
        if !value.is_finite()
        {
          self.report(DiagnosticCode::UnsupportedExpression,
                      "HIR float literal is not a finite f64 value",
                      span);
          return;
        }
        self.current_block_mut(lowered)
            .statements
            .push(mir::Statement::ConstF64 { local: target,
                                             bits: value.to_bits(),
                                             span });
      }
      (Literal::String(value), Some(XlilType { kind: TypeKind::Str, .. })) =>
      {
        self.current_block_mut(lowered)
            .statements
            .push(mir::Statement::ConstStr { local: target,
                                             units: value.chars().map(u32::from).collect(),
                                             span });
      }
      (Literal::Char(value), Some(XlilType { kind: TypeKind::U32, .. })) =>
      {
        self.current_block_mut(lowered)
            .statements
            .push(mir::Statement::ConstInteger { local: target,
                                                 value: mir::IntegerConstant::U32(*value),
                                                 span });
      }
      (Literal::EnumVariant { enum_type,
                              variant,
                              tag, },
       Some(value_type))
        if self.aggregate_types.get(enum_type).copied() == Some(value_type) =>
      {
        let valid = self.nominal_types
                        .get(enum_type)
                        .filter(|definition| definition.kind == crate::hir::declarations::NominalKind::Enum)
                        .and_then(|definition| definition.variants.iter().find(|candidate| candidate.name == *variant))
                        .is_some_and(|candidate| candidate.tag == *tag);
        let Ok(tag_value) = i32::try_from(*tag)
        else
        {
          self.report(DiagnosticCode::UnsupportedExpression,
                      "enum variant tag exceeds the XLIL i32 representation",
                      span);
          return;
        };
        if !valid
        {
          self.report(DiagnosticCode::UnsupportedExpression,
                      format!("unknown or inconsistent enum variant '{enum_type}::{variant}'"),
                      span);
          return;
        }
        let Some(tag_local) = self.declare_temp(XlilType::I32, span, lowered)
        else
        {
          return;
        };
        self.current_block_mut(lowered)
            .statements
            .push(mir::Statement::ConstI32 { local: tag_local,
                                             value: tag_value,
                                             span });
        self.current_block_mut(lowered)
            .statements
            .push(mir::Statement::Aggregate { result: target,
                                              value_type,
                                              fields: vec![tag_local],
                                              field_types: vec![XlilType::I32],
                                              span });
      }
      (Literal::None, Some(value_type)) => self.lower_optional_none(target, value_type, span, lowered),
      _ => self.report(DiagnosticCode::UnsupportedExpression,
                       "HIR literal kind cannot lower to MIR yet",
                       span),
    }
  }

  pub(super) fn lower_optional_none(&mut self,
                                    target: mir::LocalId,
                                    optional_type: XlilType,
                                    span: Span,
                                    lowered: &mut mir::Function)
  {
    let Some((_, element_type)) = self.optional_layouts
                                      .iter()
                                      .find(|(value_type, _)| *value_type == optional_type)
                                      .copied()
    else
    {
      self.report(DiagnosticCode::UnsupportedType,
                  "None target is not an Optional<T> MIR value",
                  span);
      return;
    };
    let Some(tag) = self.declare_temp(XlilType::BOOL, span, lowered)
    else
    {
      return;
    };
    let Some(payload) = self.declare_temp(element_type, span, lowered)
    else
    {
      return;
    };
    self.current_block_mut(lowered)
        .statements
        .push(mir::Statement::ConstBool { local: tag,
                                          value: false,
                                          span });
    if !self.lower_default_value(payload, element_type, span, lowered, &mut HashSet::new())
    {
      self.report(DiagnosticCode::UnsupportedType,
                  "Optional<T> None payload has no canonical zero value yet",
                  span);
      return;
    }
    self.current_block_mut(lowered)
        .statements
        .push(mir::Statement::Aggregate { result: target,
                                          value_type: optional_type,
                                          fields: vec![tag, payload],
                                          field_types: vec![XlilType::BOOL, element_type],
                                          span });
  }

  pub(super) fn lower_default_value(&mut self,
                                    target: mir::LocalId,
                                    value_type: XlilType,
                                    span: Span,
                                    lowered: &mut mir::Function,
                                    visiting: &mut HashSet<XlilType>)
                                    -> bool
  {
    if let Some(value) = mir::IntegerConstant::from_bits(value_type, 0)
    {
      self.current_block_mut(lowered)
          .statements
          .push(mir::Statement::ConstInteger { local: target,
                                               value,
                                               span });
      return true;
    }
    let scalar = match value_type
    {
      XlilType::BOOL => Some(mir::Statement::ConstBool { local: target,
                                                         value: false,
                                                         span }),
      XlilType::F32 => Some(mir::Statement::ConstF32 { local: target,
                                                       bits: 0,
                                                       span }),
      XlilType::F64 => Some(mir::Statement::ConstF64 { local: target,
                                                       bits: 0,
                                                       span }),
      XlilType::STR => Some(mir::Statement::ConstStr { local: target,
                                                       units: Vec::new(),
                                                       span }),
      _ => None,
    };
    if let Some(scalar) = scalar
    {
      self.current_block_mut(lowered).statements.push(scalar);
      return true;
    }
    let Some(field_types) = self.aggregate_layouts.get(&value_type).cloned()
    else
    {
      return false;
    };
    if !visiting.insert(value_type)
    {
      return false;
    }
    let mut fields = Vec::with_capacity(field_types.len());
    for field_type in &field_types
    {
      let Some(field) = self.declare_temp(*field_type, span, lowered)
      else
      {
        visiting.remove(&value_type);
        return false;
      };
      if !self.lower_default_value(field, *field_type, span, lowered, visiting)
      {
        visiting.remove(&value_type);
        return false;
      }
      fields.push(field);
    }
    visiting.remove(&value_type);
    self.current_block_mut(lowered)
        .statements
        .push(mir::Statement::Aggregate { result: target,
                                          value_type,
                                          fields,
                                          field_types,
                                          span });
    true
  }

  pub(super) fn declare_local(&mut self,
                              name: String,
                              ty: &Type,
                              mutable: bool,
                              span: Span,
                              lowered: &mut mir::Function)
                              -> mir::LocalId
  {
    if let Some(id) = self.locals.get(&name).copied()
    {
      return id;
    }
    let id = mir::LocalId(self.next_local);
    self.next_local += 1;
    let value_type = self.lower_value_type(ty, span);
    lowered.locals.push(mir::Local { id,
                                     name: name.clone(),
                                     value_type,
                                     mutable,
                                     span });
    self.locals.insert(name, id);
    self.storage_locals.insert(id);
    id
  }

  pub(super) fn declare_temp(&mut self,
                             value_type: XlilType,
                             span: Span,
                             lowered: &mut mir::Function)
                             -> Option<mir::LocalId>
  {
    if value_type == XlilType::VOID
    {
      self.report(DiagnosticCode::UnsupportedType,
                  "void return value cannot allocate a MIR local",
                  span);
      return None;
    }
    let id = mir::LocalId(self.next_local);
    self.next_local += 1;
    lowered.locals.push(mir::Local { id,
                                     name: format!("$tmp{}", id.0),
                                     value_type: Some(value_type),
                                     mutable: false,
                                     span });
    Some(id)
  }

  pub(super) fn declare_storage_temp(&mut self,
                                     value_type: XlilType,
                                     span: Span,
                                     lowered: &mut mir::Function)
                                     -> Option<mir::LocalId>
  {
    if value_type == XlilType::VOID
    {
      self.report(DiagnosticCode::UnsupportedType,
                  "void expression cannot allocate MIR result storage",
                  span);
      return None;
    }
    let id = mir::LocalId(self.next_local);
    self.next_local += 1;
    lowered.locals.push(mir::Local { id,
                                     name: format!("$match_result{}", id.0),
                                     value_type: Some(value_type),
                                     mutable: true,
                                     span });
    self.storage_locals.insert(id);
    Some(id)
  }

  pub(super) fn lower_return_type(&mut self, ty: Option<&Type>, span: Span) -> XlilType
  {
    if matches!(ty, Some(Type::Unit))
    {
      return XlilType::VOID;
    }
    ty.and_then(|ty| self.lower_value_type(ty, span))
      .unwrap_or(XlilType::VOID)
  }

  pub(super) fn lower_value_type(&mut self, ty: &Type, span: Span) -> Option<XlilType>
  {
    match ty
    {
      Type::Named(name) => self.aggregate_types.get(name).copied().or_else(|| {
                                                                    self.report(DiagnosticCode::UnsupportedType,
                                                                                format!("nominal HIR type '{name}' \
                                                                                         has no aggregate MIR layout"),
                                                                                span);
                                                                    None
                                                                  }),
      Type::Primitive(PrimitiveType::Str) =>
      {
        self.report(DiagnosticCode::UnsupportedType,
                    "Str is unsized; use an explicit &Str reference",
                    span);
        None
      }
      Type::Primitive(primitive) => primitive_to_xlil(*primitive).or_else(|| {
                                                                   self.report(DiagnosticCode::UnsupportedType,
                                                                               "HIR primitive has no XLIL-backed MIR \
                                                                                value type yet",
                                                                               span);
                                                                   None
                                                                 }),
      Type::Reference { referent,
                        mutable: false, }
        if **referent == Type::Primitive(PrimitiveType::Str) =>
      {
        Some(XlilType::STR)
      }
      Type::Reference { .. } =>
      {
        self.report(DiagnosticCode::UnsupportedType,
                    "this explicit HIR reference has no MIR value model yet",
                    span);
        None
      }
      Type::Optional { .. } => self.optional_types
                                   .iter()
                                   .find(|(source_type, _)| source_type == ty)
                                   .map(|(_, value_type)| *value_type)
                                   .or_else(|| {
                                     self.report(DiagnosticCode::UnsupportedType,
                                                 "Optional<T> has no MIR aggregate registry entry",
                                                 span);
                                     None
                                   }),
      Type::Result { .. } => self.result_types
                                 .iter()
                                 .find(|(source_type, _)| source_type == ty)
                                 .map(|(_, value_type)| *value_type)
                                 .or_else(|| {
                                   self.report(DiagnosticCode::UnsupportedType,
                                               "Result<T, E> has no MIR aggregate registry entry",
                                               span);
                                   None
                                 }),
      Type::Array { .. } => self.array_types
                                .iter()
                                .find(|(source_type, _)| source_type == ty)
                                .map(|(_, value_type)| *value_type)
                                .or_else(|| {
                                  self.report(DiagnosticCode::UnsupportedType,
                                              "fixed array HIR type has no MIR collection registry entry",
                                              span);
                                  None
                                }),
      Type::Tuple { .. } => self.tuple_types
                                .iter()
                                .find(|(source_type, _)| source_type == ty)
                                .map(|(_, value_type)| *value_type)
                                .or_else(|| {
                                  self.report(DiagnosticCode::UnsupportedType,
                                              "tuple HIR type has no MIR aggregate registry entry",
                                              span);
                                  None
                                }),
      _ =>
      {
        self.report(DiagnosticCode::UnsupportedType,
                    "HIR type has no MIR value model yet",
                    span);
        None
      }
    }
  }
}
