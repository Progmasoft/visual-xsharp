/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 */

use super::*;

impl HirToMirLowerer
{
  pub(super) fn lower_optional_coalesce(&mut self,
                                        optional: &Expression,
                                        fallback: &Expression,
                                        result_type: XlilType,
                                        span: Span,
                                        lowered: &mut mir::Function)
                                        -> Option<mir::LocalId>
  {
    let optional_type = self.expression_value_type(optional, lowered)
                            .filter(|value_type| self.optional_element_type(*value_type) == Some(result_type))
                            .or_else(|| {
                              self.optional_layouts
                                  .iter()
                                  .find(|(_, element_type)| *element_type == result_type)
                                  .map(|(value_type, _)| *value_type)
                            });
    let Some(optional_type) = optional_type
    else
    {
      self.report(DiagnosticCode::UnsupportedType,
                  "left operand of ?? is not Optional<T> for the result type",
                  span);
      return None;
    };
    let optional = self.lower_expression_to_local(optional, optional_type, lowered)?;
    let tag = self.declare_temp(XlilType::BOOL, span, lowered)?;
    self.current_block_mut(lowered)
        .statements
        .push(mir::Statement::Extract { result: tag,
                                        aggregate: optional,
                                        field: 0,
                                        field_type: XlilType::BOOL,
                                        span });

    let result_storage = self.declare_storage_temp(result_type, span, lowered)?;
    let branch = self.current_block;
    let some_block = self.append_block(span, lowered);
    let none_block = self.append_block(span, lowered);
    let merge = self.append_block(span, lowered);
    self.switch_to(branch);
    self.set_terminator(mir::Terminator::BranchIf { condition: tag,
                                                    then_block: some_block,
                                                    else_block: none_block },
                        span,
                        lowered);

    self.switch_to(some_block);
    let payload = self.declare_temp(result_type, span, lowered)?;
    self.current_block_mut(lowered)
        .statements
        .push(mir::Statement::Extract { result: payload,
                                        aggregate: optional,
                                        field: 1,
                                        field_type: result_type,
                                        span });
    self.store_coalesced_value(result_storage, payload, merge, span, lowered);

    self.switch_to(none_block);
    let fallback = self.lower_expression_to_local(fallback, result_type, lowered)?;
    self.store_coalesced_value(result_storage, fallback, merge, span, lowered);

    self.switch_to(merge);
    let result = self.declare_temp(result_type, span, lowered)?;
    self.current_block_mut(lowered)
        .statements
        .push(mir::Statement::LoadLocal { result,
                                          local: result_storage,
                                          span });
    Some(result)
  }

  fn optional_element_type(&self, optional_type: XlilType) -> Option<XlilType>
  {
    self.optional_layouts
        .iter()
        .find(|(value_type, _)| *value_type == optional_type)
        .map(|(_, element_type)| *element_type)
  }

  fn store_coalesced_value(&mut self,
                           storage: mir::LocalId,
                           value: mir::LocalId,
                           merge: mir::BlockId,
                           span: Span,
                           lowered: &mut mir::Function)
  {
    self.current_block_mut(lowered)
        .statements
        .push(mir::Statement::StoreLocal { local: storage,
                                           value,
                                           span });
    self.set_terminator(mir::Terminator::Goto(merge), span, lowered);
  }
}
