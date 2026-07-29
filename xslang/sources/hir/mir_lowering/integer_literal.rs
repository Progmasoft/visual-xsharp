/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 */

use super::*;

impl HirToMirLowerer
{
  pub(super) fn lower_integer_literal_into(&mut self,
                                           target: mir::LocalId,
                                           text: &str,
                                           value_type: XlilType,
                                           span: Span,
                                           lowered: &mut mir::Function)
  {
    let text = text.replace('\'', "");
    let Some(value) = mir::IntegerConstant::parse(value_type, &text)
    else
    {
      self.report(DiagnosticCode::InvalidIntegerLiteral,
                  format!("HIR integer literal does not fit MIR const.{}",
                          crate::xlil::type_name(value_type)),
                  span);
      return;
    };
    self.push_integer_constant(target, value, span, lowered);
  }

  pub(super) fn lower_negative_integer_literal_into(&mut self,
                                                    target: mir::LocalId,
                                                    text: &str,
                                                    value_type: XlilType,
                                                    span: Span,
                                                    lowered: &mut mir::Function)
                                                    -> bool
  {
    let Ok(magnitude) = text.replace('\'', "").parse::<u128>()
    else
    {
      self.report(DiagnosticCode::InvalidIntegerLiteral,
                  "invalid negative HIR integer literal",
                  span);
      return true;
    };
    let value = match value_type
    {
      XlilType::I8 if magnitude <= 128 => mir::IntegerConstant::I8(if magnitude == 128
                                                                   {
                                                                     i8::MIN
                                                                   }
                                                                   else
                                                                   {
                                                                     -(magnitude as i8)
                                                                   }),
      XlilType::I16 if magnitude <= 32_768 => mir::IntegerConstant::I16(if magnitude == 32_768
                                                                        {
                                                                          i16::MIN
                                                                        }
                                                                        else
                                                                        {
                                                                          -(magnitude as i16)
                                                                        }),
      XlilType::I32 if magnitude <= 2_147_483_648 => mir::IntegerConstant::I32(if magnitude == 2_147_483_648
                                                                               {
                                                                                 i32::MIN
                                                                               }
                                                                               else
                                                                               {
                                                                                 -(magnitude as i32)
                                                                               }),
      XlilType::I64 if magnitude <= 9_223_372_036_854_775_808 => mir::IntegerConstant::I64(if magnitude ==
                                                                                              9_223_372_036_854_775_808
                                                                                           {
                                                                                             i64::MIN
                                                                                           }
                                                                                           else
                                                                                           {
                                                                                             -(magnitude as i64)
                                                                                           }),
      XlilType::I128 if magnitude <= (1_u128 << 127) => mir::IntegerConstant::I128(if magnitude == (1_u128 << 127)
                                                                                   {
                                                                                     i128::MIN
                                                                                   }
                                                                                   else
                                                                                   {
                                                                                     -(magnitude as i128)
                                                                                   }),
      XlilType::I8 | XlilType::I16 | XlilType::I32 | XlilType::I64 | XlilType::I128 =>
      {
        self.report(DiagnosticCode::InvalidIntegerLiteral,
                    format!("negative HIR integer literal does not fit {}",
                            crate::xlil::type_name(value_type)),
                    span);
        return true;
      }
      _ => return false,
    };
    self.push_integer_constant(target, value, span, lowered);
    true
  }

  fn push_integer_constant(&mut self,
                           target: mir::LocalId,
                           value: mir::IntegerConstant,
                           span: Span,
                           lowered: &mut mir::Function)
  {
    let statement = match value
    {
      mir::IntegerConstant::I32(value) => mir::Statement::ConstI32 { local: target,
                                                                     value,
                                                                     span },
      mir::IntegerConstant::I64(value) => mir::Statement::ConstI64 { local: target,
                                                                     value,
                                                                     span },
      mir::IntegerConstant::U16(value) => mir::Statement::ConstU16 { local: target,
                                                                     value,
                                                                     span },
      value => mir::Statement::ConstInteger { local: target,
                                              value,
                                              span },
    };
    self.current_block_mut(lowered).statements.push(statement);
  }
}
