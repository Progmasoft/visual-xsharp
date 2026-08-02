/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 */

use super::*;

impl HirToMirLowerer
{
  pub(super) fn lower_enum_data_value(&mut self,
                                      expression: &Expression,
                                      expected_type: XlilType,
                                      lowered: &mut mir::Function)
                                      -> Option<mir::LocalId>
  {
    let Expression::EnumData { enum_type,
                               owner,
                               variant,
                               tag,
                               payload,
                               payload_type,
                               span, } = expression
    else
    {
      return None;
    };
    let Some(layout) = self.enum_data_layouts
                           .iter()
                           .find(|layout| layout.name == *enum_type && layout.value_type == expected_type)
                           .cloned()
    else
    {
      self.report(DiagnosticCode::UnsupportedType,
                  format!("enum data '{enum_type}' has no MIR aggregate layout"),
                  *span);
      return None;
    };
    let Some(selected) = layout.variant(owner, variant, *tag).cloned()
    else
    {
      self.report(DiagnosticCode::UnsupportedExpression,
                  format!("enum data constructor '{owner}::{variant}' has inconsistent tag metadata"),
                  *span);
      return None;
    };
    let checked_payload = payload_type.as_ref().and_then(|ty| self.lower_value_type(ty, *span));
    if selected.payload_type != checked_payload || selected.payload_type.is_some() != payload.is_some()
    {
      self.report(DiagnosticCode::UnsupportedType,
                  format!("enum data constructor '{owner}::{variant}' payload does not match its MIR layout"),
                  *span);
      return None;
    }
    let Some(tag_value) = i32::try_from(*tag).ok()
    else
    {
      self.report(DiagnosticCode::UnsupportedExpression,
                  "enum data tag exceeds the XLIL i32 representation",
                  *span);
      return None;
    };
    let tag = self.declare_temp(XlilType::I32, *span, lowered)?;
    self.current_block_mut(lowered)
        .statements
        .push(mir::Statement::ConstI32 { local: tag,
                                         value: tag_value,
                                         span: *span });
    let field_types = self.aggregate_layouts.get(&expected_type).cloned()?;
    let mut fields = Vec::with_capacity(field_types.len());
    fields.push(tag);
    for (index, field_type) in field_types.iter().copied().enumerate().skip(1)
    {
      let field_index = u32::try_from(index).ok();
      if selected.field == field_index
      {
        fields.push(self.lower_expression_to_local(payload.as_deref()?, field_type, lowered)?);
        continue;
      }
      let local = self.declare_temp(field_type, *span, lowered)?;
      if !self.lower_default_value(local, field_type, *span, lowered, &mut HashSet::new())
      {
        self.report(DiagnosticCode::UnsupportedType,
                    format!("inactive enum data payload field {} has no canonical zero value",
                            index - 1),
                    *span);
        return None;
      }
      fields.push(local);
    }
    let result = self.declare_temp(expected_type, *span, lowered)?;
    self.current_block_mut(lowered)
        .statements
        .push(mir::Statement::Aggregate { result,
                                          value_type: expected_type,
                                          fields,
                                          field_types,
                                          span: *span });
    Some(result)
  }
}
