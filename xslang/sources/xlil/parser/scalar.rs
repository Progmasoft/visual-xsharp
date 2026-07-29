/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 */

use super::{DiagnosticCode, Parser};
use crate::xlil::{Function, Instruction, IntegerConstant, Type, Value, type_from_name};

impl Parser<'_>
{
  pub(super) fn const_u16(&mut self, function: &mut Function, text: &str, line: usize) -> Option<Instruction>
  {
    let (result, value) = text.split_once(" = const.u16 ")?;
    let result = self.value_id(result.strip_suffix(":u16")?, line)?;
    let Some(hexadecimal) = value.strip_prefix("0x").filter(|value| value.len() == 4)
    else
    {
      self.report(DiagnosticCode::InvalidInteger,
                  line,
                  "XLIL const.u16 immediate must contain four hexadecimal digits");
      return None;
    };
    let Some(value) = u16::from_str_radix(hexadecimal, 16).ok()
    else
    {
      self.report(DiagnosticCode::InvalidInteger,
                  line,
                  "XLIL const.u16 immediate is invalid");
      return None;
    };
    function.values.push(Value { id: result,
                                 value_type: Type::U16 });
    Some(Instruction::ConstU16 { result,
                                 value })
  }

  pub(super) fn integer_constant(&mut self, function: &mut Function, text: &str, line: usize) -> Option<Instruction>
  {
    let (result, operation) = text.split_once(" = const.")?;
    let (result, result_type_name) = result.split_once(':')?;
    let result_type = type_from_name(result_type_name)?;
    if matches!(result_type, Type::U16 | Type::I32 | Type::I64) || result_type.integer_width().is_none()
    {
      return None;
    }
    let (operation_type, hexadecimal) = operation.split_once(" 0x")?;
    if operation_type != result_type_name
    {
      self.report(DiagnosticCode::InvalidInstruction,
                  line,
                  "XLIL integer constant operation and result types differ");
      return None;
    }
    let digits = result_type.integer_width()? as usize / 4;
    if hexadecimal.len() != digits
    {
      self.report(DiagnosticCode::InvalidInteger,
                  line,
                  "XLIL integer constant has the wrong hexadecimal width");
      return None;
    }
    let Some(bits) = u128::from_str_radix(hexadecimal, 16).ok()
    else
    {
      self.report(DiagnosticCode::InvalidInteger,
                  line,
                  "XLIL integer constant bit pattern is invalid");
      return None;
    };
    let value = IntegerConstant::new(result_type, bits)?;
    let result = self.value_id(result, line)?;
    function.values.push(Value { id: result,
                                 value_type: result_type });
    Some(Instruction::ConstInteger { result,
                                     value })
  }
}
