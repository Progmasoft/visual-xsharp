/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 */

use super::{DiagnosticCode, Parser};
use crate::xlil::{Function, Instruction, StrComparisonOperation, Type, Value};

impl Parser<'_>
{
  pub(super) fn str_comparison(&mut self,
                               function: &mut Function,
                               result: &str,
                               operands: &str,
                               operation: &str,
                               line: usize)
                               -> Option<Instruction>
  {
    let result = self.value_id(result.strip_suffix(":bool")?, line)?;
    let (left, right) = operands.split_once(", ")?;
    let left = self.value_operand(left, line)?;
    let right = self.value_operand(right, line)?;
    function.values.push(Value { id: result,
                                 value_type: Type::BOOL });
    Some(Instruction::CompareStr { operation: StrComparisonOperation::parse_text_stem(operation)?,
                                   result,
                                   left,
                                   right })
  }

  pub(super) fn const_str(&mut self, function: &mut Function, text: &str, line: usize) -> Option<Instruction>
  {
    let (result, value) = text.split_once(" = const.str ")?;
    let result = self.value_id(result.strip_suffix(":str")?, line)?;
    let Some((encoding, units)) = crate::text::parse_encoded(value)
    else
    {
      self.report(DiagnosticCode::InvalidInstruction,
                  line,
                  "XLIL const.str UTF-16 code units are invalid");
      return None;
    };
    function.values.push(Value { id: result,
                                 value_type: Type::STR });
    Some(Instruction::ConstStr { result,
                                 encoding,
                                 units })
  }

  pub(super) fn const_bool(&mut self,
                           function: &mut Function,
                           result: &str,
                           value: &str,
                           line: usize)
                           -> Option<Instruction>
  {
    let result = self.value_id(result.strip_suffix(":bool")?, line)?;
    let value = match value
    {
      "true" => true,
      "false" => false,
      _ =>
      {
        self.report(DiagnosticCode::InvalidInstruction,
                    line,
                    "XLIL const.bool immediate is invalid");
        return None;
      }
    };
    function.values.push(Value { id: result,
                                 value_type: Type::BOOL });
    Some(Instruction::ConstBool { result,
                                  value })
  }
}
