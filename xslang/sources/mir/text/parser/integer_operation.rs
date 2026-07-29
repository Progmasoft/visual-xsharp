/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 */

use crate::mir::Statement;
use crate::xlil::{IntegerBinaryOperation, type_from_name};

use super::{Parser, span};

pub(super) fn is_integer_operation(name: &str) -> bool
{
  let Some((operation, value_type)) = name.split_once('.')
  else
  {
    return false;
  };
  IntegerBinaryOperation::parse_text_stem(operation).is_some() &&
  type_from_name(value_type).is_some_and(|value_type| {
                              value_type.is_integer() &&
                              !matches!(value_type, crate::xlil::Type::I32 | crate::xlil::Type::I64)
                            })
}

impl Parser<'_>
{
  pub(super) fn integer_operation_statement(&mut self, instruction: &str) -> Statement
  {
    let (operation, value_type) = instruction.split_once('.')
                                             .expect("guarded integer instruction has a type");
    let operation = IntegerBinaryOperation::parse_text_stem(operation).expect("guarded integer operation must parse");
    let value_type = type_from_name(value_type).expect("guarded integer type must parse");
    let result = self.binary_local(instruction, "result");
    let left = self.binary_local(instruction, "left");
    let right = self.binary_local(instruction, "right");
    Statement::BinaryInteger { operation,
                               value_type,
                               result,
                               left,
                               right,
                               span: span() }
  }
}
