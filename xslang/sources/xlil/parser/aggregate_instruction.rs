/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 */

use super::{DiagnosticCode, Parser};
use crate::xlil::{Function, Instruction, TypeKind, Value};

impl Parser<'_>
{
  pub(super) fn aggregate_instruction(&mut self,
                                      function: &mut Function,
                                      result: &str,
                                      fields: &str,
                                      line: usize)
                                      -> Option<Instruction>
  {
    self.composite_instruction(function, result, fields, line, TypeKind::Aggregate)
  }

  pub(super) fn array_instruction(&mut self,
                                  function: &mut Function,
                                  result: &str,
                                  elements: &str,
                                  line: usize)
                                  -> Option<Instruction>
  {
    self.composite_instruction(function, result, elements, line, TypeKind::Array)
  }

  fn composite_instruction(&mut self,
                           function: &mut Function,
                           result: &str,
                           fields: &str,
                           line: usize,
                           expected_kind: TypeKind)
                           -> Option<Instruction>
  {
    let (result, result_type) = result.split_once(':')?;
    let result = self.value_id(result, line)?;
    let result_type = self.type_name(result_type, line)?;
    if result_type.kind != expected_kind
    {
      self.report(DiagnosticCode::InvalidInstruction,
                  line,
                  "XLIL composite result must match its registry type");
      return None;
    }
    let mut parsed_fields = vec![];
    if !fields.is_empty()
    {
      for field in fields.split(", ")
      {
        parsed_fields.push(self.value_operand(field, line)?);
      }
    }
    function.values.push(Value { id: result,
                                 value_type: result_type });
    Some(Instruction::Aggregate { result,
                                  value_type: result_type,
                                  fields: parsed_fields })
  }

  pub(super) fn extract_instruction(&mut self,
                                    function: &mut Function,
                                    result: &str,
                                    source: &str,
                                    line: usize)
                                    -> Option<Instruction>
  {
    let (result, result_type) = result.split_once(':')?;
    let result = self.value_id(result, line)?;
    let result_type = self.type_name(result_type, line)?;
    let (aggregate, field) = source.split_once(", ")?;
    let aggregate = self.value_operand(aggregate, line)?;
    let field = field.parse::<u32>().ok()?;
    function.values.push(Value { id: result,
                                 value_type: result_type });
    Some(Instruction::Extract { result,
                                aggregate,
                                field })
  }

  pub(super) fn array_get_instruction(&mut self,
                                      function: &mut Function,
                                      result: &str,
                                      operands: &str,
                                      line: usize)
                                      -> Option<Instruction>
  {
    let (result, result_type) = self.result_value(function, result, line)?;
    if result_type == crate::xlil::Type::VOID
    {
      return None;
    }
    let (array, index) = operands.split_once(", ")?;
    Some(Instruction::ArrayGet { result,
                                 array: self.value_operand(array, line)?,
                                 index: self.value_operand(index, line)? })
  }

  pub(super) fn array_set_instruction(&mut self,
                                      function: &mut Function,
                                      result: &str,
                                      operands: &str,
                                      line: usize)
                                      -> Option<Instruction>
  {
    let (result, result_type) = self.result_value(function, result, line)?;
    let mut operands = operands.split(", ");
    let array = self.value_operand(operands.next()?, line)?;
    let index = self.value_operand(operands.next()?, line)?;
    let value = self.value_operand(operands.next()?, line)?;
    if operands.next().is_some() || result_type.kind != TypeKind::Array
    {
      return None;
    }
    Some(Instruction::ArraySet { result,
                                 array,
                                 index,
                                 value })
  }

  pub(super) fn array_length_instruction(&mut self,
                                         function: &mut Function,
                                         result: &str,
                                         array: &str,
                                         line: usize)
                                         -> Option<Instruction>
  {
    let (result, result_type) = self.result_value(function, result, line)?;
    if result_type != crate::xlil::Type::I64
    {
      return None;
    }
    Some(Instruction::ArrayLength { result,
                                    array: self.value_operand(array, line)? })
  }

  fn result_value(&mut self,
                  function: &mut Function,
                  result: &str,
                  line: usize)
                  -> Option<(crate::xlil::ValueId, crate::xlil::Type)>
  {
    let (result, result_type) = result.split_once(':')?;
    let result = self.value_id(result, line)?;
    let result_type = self.type_name(result_type, line)?;
    function.values.push(Value { id: result,
                                 value_type: result_type });
    Some((result, result_type))
  }
}
