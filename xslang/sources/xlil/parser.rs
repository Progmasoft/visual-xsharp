/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 */

use crate::xlil::{
  Block, BlockId, Function, I32BinaryOperation, I64BinaryOperation, I64ComparisonOperation, Instruction, Module, Slot,
  SlotId, Terminator, Type, Value, ValueId, type_from_name, type_name,
};

mod aggregate;
mod aggregate_instruction;
mod array;
mod control;
mod diagnostic;
mod float;
mod header;
mod i64;
mod integer_operation;
mod scalar;
mod string;

pub use diagnostic::{Diagnostic, DiagnosticCode};

/// Parses a complete human-readable XLIL module.
pub fn parse_module(text: &str) -> Result<Module, Vec<Diagnostic>>
{
  let mut parser = Parser { lines: text.lines().collect(),
                            cursor: 0,
                            aggregate_type_count: 0,
                            array_type_count: 0,
                            diagnostics: vec![] };
  let module = parser.module();
  if parser.diagnostics.is_empty()
  {
    Ok(module.unwrap_or_else(|| Module::new("")))
  }
  else
  {
    Err(parser.diagnostics)
  }
}

struct Parser<'text>
{
  lines: Vec<&'text str>,
  cursor: usize,
  aggregate_type_count: u32,
  array_type_count: u32,
  diagnostics: Vec<Diagnostic>,
}

impl Parser<'_>
{
  fn module(&mut self) -> Option<Module>
  {
    if !self.version_header()
    {
      return None;
    }
    let Some(header) = self.next_line()
    else
    {
      self.report(DiagnosticCode::InvalidModuleHeader, 2, "XLIL module header is missing");
      return None;
    };
    let Some(name) = header.strip_prefix(".xlil module ")
    else
    {
      self.report(DiagnosticCode::InvalidModuleHeader, 2, "XLIL module header is invalid");
      return None;
    };
    let mut module = Module::new(name);
    while self.cursor < self.lines.len()
    {
      if self.current_line().is_some_and(str::is_empty)
      {
        self.cursor += 1;
        continue;
      }
      if self.current_line().is_some_and(|line| line.starts_with(".type "))
      {
        if !self.aggregate_type(&mut module)
        {
          break;
        }
        continue;
      }
      if self.current_line().is_some_and(|line| line.starts_with(".array "))
      {
        if !self.array_type(&mut module)
        {
          break;
        }
        continue;
      }
      let Some(function) = self.function()
      else
      {
        break;
      };
      module.add_function(function);
    }
    Some(module)
  }

  fn function(&mut self) -> Option<Function>
  {
    let line_number = self.line_number();
    let line = self.next_line()?;
    if let Some(signature) = line.strip_prefix(".extern ")
    {
      return self.signature(signature, false, line_number);
    }
    if let Some(signature) = line.strip_prefix(".func ")
    {
      let mut function = self.signature(signature, true, line_number)?;
      self.body(&mut function);
      return Some(function);
    }
    self.report(DiagnosticCode::InvalidFunctionRecord,
                line_number,
                "XLIL function record is invalid");
    None
  }

  fn signature(&mut self, signature: &str, is_definition: bool, line: usize) -> Option<Function>
  {
    let Some((name, rest)) = signature.split_once(" : ")
    else
    {
      self.report(DiagnosticCode::InvalidSignature, line, "XLIL signature is invalid");
      return None;
    };
    let Some((parameters, return_type)) = rest.split_once(" -> ")
    else
    {
      self.report(DiagnosticCode::InvalidSignature, line, "XLIL signature is invalid");
      return None;
    };
    let Some(parameters) = parameters.strip_prefix('(')
                                     .and_then(|parameters| parameters.strip_suffix(')'))
    else
    {
      self.report(DiagnosticCode::InvalidSignature, line, "XLIL parameter list is invalid");
      return None;
    };
    let return_type = self.type_name(return_type, line)?;
    let parameters = self.parameters(parameters, line)?;
    Some(if is_definition
         {
           Function::definition(name, return_type, parameters)
         }
         else
         {
           Function::declaration(name, return_type, parameters)
         })
  }

  fn parameters(&mut self, parameters: &str, line: usize) -> Option<Vec<Type>>
  {
    if parameters.is_empty()
    {
      return Some(vec![]);
    }
    let mut parsed = Vec::new();
    for parameter in parameters.split(", ")
    {
      parsed.push(self.type_name(parameter, line)?);
    }
    Some(parsed)
  }

  fn type_name(&mut self, name: &str, line: usize) -> Option<Type>
  {
    let parsed = name.strip_prefix("%t")
                     .and_then(|id| id.parse::<u32>().ok())
                     .filter(|id| *id < self.aggregate_type_count)
                     .map(Type::aggregate)
                     .or_else(|| {
                       name.strip_prefix("%a")
                           .and_then(|id| id.parse::<u32>().ok())
                           .filter(|id| *id < self.array_type_count)
                           .map(Type::array)
                     })
                     .or_else(|| type_from_name(name));
    if parsed.is_none()
    {
      self.report(DiagnosticCode::InvalidType, line, "XLIL type name is unknown");
    }
    parsed
  }

  fn body(&mut self, function: &mut Function)
  {
    let mut parameter = 0;
    while self.cursor < self.lines.len()
    {
      let line = self.current_line().unwrap_or_default();
      if line == ".end"
      {
        self.cursor += 1;
        if parameter != function.parameters.len()
        {
          self.report(DiagnosticCode::InvalidInstruction,
                      self.line_number() - 1,
                      "XLIL function parameter records are incomplete");
        }
        return;
      }
      if let Some(record) = line.strip_prefix(".param ")
      {
        let line = self.line_number();
        let record = record.to_string();
        self.cursor += 1;
        self.parameter_record(function, parameter, &record, line);
        parameter += 1;
        continue;
      }
      if let Some(record) = line.strip_prefix(".slot ")
      {
        if parameter != function.parameters.len()
        {
          self.report(DiagnosticCode::InvalidInstruction,
                      self.line_number(),
                      "XLIL stack slot appears before parameter records are complete");
          return;
        }
        let line = self.line_number();
        let record = record.to_string();
        self.cursor += 1;
        self.slot_record(function, &record, line);
        continue;
      }
      if parameter != function.parameters.len()
      {
        self.report(DiagnosticCode::InvalidInstruction,
                    self.line_number(),
                    "XLIL function parameter records are incomplete");
        return;
      }
      if !line.starts_with("bb")
      {
        self.report(DiagnosticCode::InvalidBlockRecord,
                    self.line_number(),
                    "XLIL block record is invalid");
        return;
      }
      let block = self.block(function);
      function.blocks.push(block);
    }
    self.report(DiagnosticCode::UnexpectedEndOfInput,
                self.line_number(),
                "XLIL function body is not closed");
  }

  fn slot_record(&mut self, function: &mut Function, record: &str, line: usize)
  {
    let Some((slot, type_name)) = record.split_once(':')
    else
    {
      self.report(DiagnosticCode::InvalidInstruction,
                  line,
                  "XLIL stack slot record is invalid");
      return;
    };
    let Some(id) = slot.strip_prefix("%s")
                       .and_then(|id| id.parse::<u32>().ok())
                       .map(SlotId)
    else
    {
      self.report(DiagnosticCode::InvalidInstruction,
                  line,
                  "XLIL stack slot id is invalid");
      return;
    };
    let Some(value_type) = self.type_name(type_name, line)
    else
    {
      return;
    };
    if id.0 as usize != function.slots.len() || value_type == Type::VOID
    {
      self.report(DiagnosticCode::InvalidInstruction,
                  line,
                  "XLIL stack slot record is invalid or out of order");
      return;
    }
    function.slots.push(Slot { id,
                               value_type });
  }

  fn parameter_record(&mut self, function: &Function, parameter: usize, record: &str, line: usize)
  {
    let Some((value, type_name)) = record.split_once(':')
    else
    {
      self.report(DiagnosticCode::InvalidInstruction,
                  line,
                  "XLIL parameter record is invalid");
      return;
    };
    let Some(value) = value.strip_prefix('%').and_then(|value| self.value_id(value, line))
    else
    {
      return;
    };
    let Some(value_type) = self.type_name(type_name, line)
    else
    {
      return;
    };
    if parameter >= function.parameters.len() ||
       value != ValueId(parameter as u32) ||
       value_type != function.parameters[parameter]
    {
      self.report(DiagnosticCode::InvalidInstruction,
                  line,
                  "XLIL parameter record does not match function signature");
    }
  }

  fn block(&mut self, function: &mut Function) -> Block
  {
    let line_number = self.line_number();
    let line = self.next_line().unwrap_or_default();
    let (id, label) = self.block_header(&line, line_number)
                          .unwrap_or((BlockId(0), String::new()));
    let mut block = Block { id,
                            label,
                            instructions: vec![],
                            terminator: None };
    while self.cursor < self.lines.len()
    {
      let line = self.current_line().unwrap_or_default().to_string();
      if line == ".end" || line.starts_with("bb")
      {
        return block;
      }
      let line_number = self.line_number();
      self.cursor += 1;
      if let Some(instruction) = line.strip_prefix("  %")
      {
        if let Some(instruction) = self.instruction(function, instruction, line_number)
        {
          block.instructions.push(instruction);
        }
      }
      else if let Some(call) = line.strip_prefix("  call ")
      {
        if let Some(instruction) = self.void_call(function, call, line_number)
        {
          block.instructions.push(instruction);
        }
      }
      else if let Some(store) = line.strip_prefix("  store ")
      {
        if let Some(instruction) = self.store(store, line_number)
        {
          block.instructions.push(instruction);
        }
      }
      else if let Some(terminator) = line.strip_prefix("  ret")
      {
        block.terminator = self.return_terminator(terminator, line_number);
      }
      else if let Some(target) = line.strip_prefix("  br ")
      {
        block.terminator = self.branch_terminator(target, line_number);
      }
      else if let Some(branch_if) = line.strip_prefix("  br_if ")
      {
        block.terminator = self.branch_if_terminator(branch_if, line_number);
      }
      else if line == "  panic"
      {
        block.terminator = Some(Terminator::Panic);
      }
      else if line == "  .missing_terminator"
      {
        block.terminator = None;
      }
      else
      {
        self.report(DiagnosticCode::InvalidInstruction,
                    line_number,
                    "XLIL instruction is invalid");
      }
    }
    block
  }

  fn block_header(&mut self, line: &str, line_number: usize) -> Option<(BlockId, String)>
  {
    let line = line.strip_suffix(':')?;
    let (id, label) = line.split_once('.')?;
    let id = id.strip_prefix("bb")?.parse::<u32>().ok()?;
    Some((BlockId(id), label.to_string())).or_else(|| {
                                            self.report(DiagnosticCode::InvalidBlockRecord,
                                                        line_number,
                                                        "XLIL block header is invalid");
                                            None
                                          })
  }

  fn instruction(&mut self, function: &mut Function, text: &str, line: usize) -> Option<Instruction>
  {
    if let Some((result, fields)) = text.split_once(" = aggregate ")
    {
      return self.aggregate_instruction(function, result, fields, line);
    }
    if let Some((result, elements)) = text.split_once(" = array ")
    {
      return self.array_instruction(function, result, elements, line);
    }
    if let Some((result, source)) = text.split_once(" = extract ")
    {
      return self.extract_instruction(function, result, source, line);
    }
    if let Some((result, source)) = text.split_once(" = extract.array ")
    {
      return self.extract_instruction(function, result, source, line);
    }
    if let Some((result, operands)) = text.split_once(" = array.get ")
    {
      return self.array_get_instruction(function, result, operands, line);
    }
    if let Some((result, operands)) = text.split_once(" = array.set ")
    {
      return self.array_set_instruction(function, result, operands, line);
    }
    if let Some((result, array)) = text.split_once(" = len.array ")
    {
      return self.array_length_instruction(function, result, array, line);
    }
    if let Some((result, call)) = text.split_once(" = call ")
    {
      return self.value_call(function, result, call, line);
    }
    if let Some((result, slot)) = text.split_once(" = load ")
    {
      return self.load(function, result, slot, line);
    }
    if let Some((result, operands)) = text.split_once(" = add.i64 ")
    {
      return self.binary_i64(function, result, operands, "add.i64", line);
    }
    if let Some((result, operands)) = text.split_once(" = sub.i64 ")
    {
      return self.binary_i64(function, result, operands, "sub.i64", line);
    }
    if let Some((result, operands)) = text.split_once(" = mul.i64 ")
    {
      return self.binary_i64(function, result, operands, "mul.i64", line);
    }
    if let Some((result, operands)) = text.split_once(" = eq.i64 ")
    {
      return self.eq_i64(function, result, operands, line);
    }
    for instruction in ["div.i64", "rem.i64", "and.i64", "or.i64", "xor.i64", "shl.i64", "shr.i64", "lt.i64", "le.i64",
                        "gt.i64", "ge.i64"]
    {
      let pattern = format!(" = {instruction} ");
      if let Some((result, operands)) = text.split_once(&pattern)
      {
        return self.extended_i64(function, result, operands, instruction, line);
      }
    }
    if let Some((result, operand)) = text.split_once(" = not.bool ")
    {
      return self.not_bool(function, result, operand, line);
    }
    for instruction in ["add.i32", "sub.i32", "mul.i32", "div.i32", "rem.i32", "and.i32", "or.i32", "xor.i32",
                        "shl.i32", "shr.i32", "eq.i32", "lt.i32", "le.i32", "gt.i32", "ge.i32"]
    {
      let pattern = format!(" = {instruction} ");
      if let Some((result, operands)) = text.split_once(&pattern)
      {
        return self.binary_i32(function, result, operands, instruction, line);
      }
    }
    if let Some(instruction) = self.integer_operation(function, text, line)
    {
      return Some(instruction);
    }
    for value_type in [Type::F32, Type::F64]
    {
      let suffix = type_name(value_type);
      for operation in ["add", "sub", "mul", "div", "rem", "eq", "ne", "lt", "le", "gt", "ge"]
      {
        let instruction = format!("{operation}.{suffix}");
        let pattern = format!(" = {instruction} ");
        if let Some((result, operands)) = text.split_once(&pattern)
        {
          return self.float_binary(function, result, operands, operation, value_type, line);
        }
      }
    }
    for operation in ["eq", "ne"]
    {
      let pattern = format!(" = {operation}.str ");
      if let Some((result, operands)) = text.split_once(&pattern)
      {
        return self.str_comparison(function, result, operands, operation, line);
      }
    }
    if let Some((result, rest)) = text.split_once(" = const.bool ")
    {
      return self.const_bool(function, result, rest, line);
    }
    if let Some(parsed) = self.const_str(function, text, line)
    {
      return Some(parsed);
    }
    if let Some(parsed) = self.const_u16(function, text, line)
    {
      return Some(parsed);
    }
    if let Some(parsed) = self.integer_constant(function, text, line)
    {
      return Some(parsed);
    }
    if let Some((result, rest)) = text.split_once(" = const.i32 ")
    {
      return self.const_i32(function, result, rest, line);
    }
    if let Some((result, rest)) = text.split_once(" = const.f32 ")
    {
      return self.const_float(function, result, rest, Type::F32, line);
    }
    if let Some((result, rest)) = text.split_once(" = const.f64 ")
    {
      return self.const_float(function, result, rest, Type::F64, line);
    }
    let Some((result, rest)) = text.split_once(" = const.i64 ")
    else
    {
      self.report(DiagnosticCode::InvalidInstruction, line, "XLIL instruction is invalid");
      return None;
    };
    let Some(result) = result.strip_suffix(":i64")
    else
    {
      self.report(DiagnosticCode::InvalidInstruction,
                  line,
                  "XLIL const.i64 result type is invalid");
      return None;
    };
    let result = self.value_id(result, line)?;
    let value = rest.parse::<i64>().map_err(|_| ()).ok();
    let Some(value) = value
    else
    {
      self.report(DiagnosticCode::InvalidInteger,
                  line,
                  "XLIL const.i64 immediate is invalid");
      return None;
    };
    function.values.push(Value { id: result,
                                 value_type: Type::I64 });
    Some(Instruction::ConstI64 { result,
                                 value })
  }

  fn load(&mut self, function: &mut Function, result: &str, slot: &str, line: usize) -> Option<Instruction>
  {
    let (result, result_type) = result.split_once(':')?;
    let result = self.value_id(result, line)?;
    let result_type = self.type_name(result_type, line)?;
    let slot = self.slot_operand(slot, line)?;
    if !function.slots
                .get(slot.0 as usize)
                .is_some_and(|entry| entry.id == slot && entry.value_type == result_type)
    {
      self.report(DiagnosticCode::InvalidInstruction,
                  line,
                  "XLIL load type does not match stack slot type");
      return None;
    }
    function.values.push(Value { id: result,
                                 value_type: result_type });
    Some(Instruction::Load { result,
                             slot })
  }

  fn store(&mut self, text: &str, line: usize) -> Option<Instruction>
  {
    let Some((value, slot)) = text.split_once(", ")
    else
    {
      self.report(DiagnosticCode::InvalidInstruction,
                  line,
                  "XLIL store operands are invalid");
      return None;
    };
    Some(Instruction::Store { value: self.value_operand(value, line)?,
                              slot: self.slot_operand(slot, line)? })
  }

  fn slot_operand(&mut self, text: &str, line: usize) -> Option<SlotId>
  {
    let parsed = text.strip_prefix("%s")
                     .and_then(|value| value.parse::<u32>().ok())
                     .map(SlotId);
    if parsed.is_none()
    {
      self.report(DiagnosticCode::InvalidInstruction,
                  line,
                  "XLIL stack slot operand is invalid");
    }
    parsed
  }

  fn const_i32(&mut self, function: &mut Function, result: &str, value: &str, line: usize) -> Option<Instruction>
  {
    let Some(result) = result.strip_suffix(":i32")
    else
    {
      self.report(DiagnosticCode::InvalidInstruction,
                  line,
                  "XLIL const.i32 result type is invalid");
      return None;
    };
    let result = self.value_id(result, line)?;
    let Some(value) = value.parse::<i32>().ok()
    else
    {
      self.report(DiagnosticCode::InvalidInteger,
                  line,
                  "XLIL const.i32 immediate is invalid");
      return None;
    };
    function.values.push(Value { id: result,
                                 value_type: Type::I32 });
    Some(Instruction::ConstI32 { result,
                                 value })
  }

  fn binary_i64(&mut self,
                function: &mut Function,
                result: &str,
                operands: &str,
                instruction: &str,
                line: usize)
                -> Option<Instruction>
  {
    let Some(result) = result.strip_suffix(":i64")
    else
    {
      self.report(DiagnosticCode::InvalidInstruction,
                  line,
                  &format!("XLIL {instruction} result type is invalid"));
      return None;
    };
    let result = self.value_id(result, line)?;
    let Some((left, right)) = operands.split_once(", ")
    else
    {
      self.report(DiagnosticCode::InvalidInstruction,
                  line,
                  &format!("XLIL {instruction} operands are invalid"));
      return None;
    };
    let left = self.value_operand(left, line)?;
    let right = self.value_operand(right, line)?;
    function.values.push(Value { id: result,
                                 value_type: Type::I64 });
    match instruction
    {
      "add.i64" => Some(Instruction::AddI64 { result,
                                              left,
                                              right }),
      "sub.i64" => Some(Instruction::SubI64 { result,
                                              left,
                                              right }),
      "mul.i64" => Some(Instruction::MulI64 { result,
                                              left,
                                              right }),
      _ => None,
    }
  }

  fn eq_i64(&mut self, function: &mut Function, result: &str, operands: &str, line: usize) -> Option<Instruction>
  {
    let Some(result) = result.strip_suffix(":bool")
    else
    {
      self.report(DiagnosticCode::InvalidInstruction,
                  line,
                  "XLIL eq.i64 result type is invalid");
      return None;
    };
    let result = self.value_id(result, line)?;
    let Some((left, right)) = operands.split_once(", ")
    else
    {
      self.report(DiagnosticCode::InvalidInstruction,
                  line,
                  "XLIL eq.i64 operands are invalid");
      return None;
    };
    let left = self.value_operand(left, line)?;
    let right = self.value_operand(right, line)?;
    function.values.push(Value { id: result,
                                 value_type: Type::BOOL });
    Some(Instruction::EqI64 { result,
                              left,
                              right })
  }

  fn binary_i32(&mut self,
                function: &mut Function,
                result: &str,
                operands: &str,
                instruction: &str,
                line: usize)
                -> Option<Instruction>
  {
    let result_type = match instruction
    {
      "add.i32" | "sub.i32" | "mul.i32" | "div.i32" | "rem.i32" | "and.i32" | "or.i32" | "xor.i32" | "shl.i32" |
      "shr.i32" => Type::I32,
      "eq.i32" | "lt.i32" | "le.i32" | "gt.i32" | "ge.i32" => Type::BOOL,
      _ => return None,
    };
    let expected_suffix = if result_type == Type::I32
    {
      ":i32"
    }
    else
    {
      ":bool"
    };
    let Some(result) = result.strip_suffix(expected_suffix)
    else
    {
      self.report(DiagnosticCode::InvalidInstruction,
                  line,
                  &format!("XLIL {instruction} result type is invalid"));
      return None;
    };
    let result = self.value_id(result, line)?;
    let Some((left, right)) = operands.split_once(", ")
    else
    {
      self.report(DiagnosticCode::InvalidInstruction,
                  line,
                  &format!("XLIL {instruction} operands are invalid"));
      return None;
    };
    let left = self.value_operand(left, line)?;
    let right = self.value_operand(right, line)?;
    function.values.push(Value { id: result,
                                 value_type: result_type });
    Some(match instruction
    {
      "add.i32" => Instruction::AddI32 { result,
                                         left,
                                         right },
      "sub.i32" => Instruction::SubI32 { result,
                                         left,
                                         right },
      "mul.i32" => Instruction::MulI32 { result,
                                         left,
                                         right },
      name if I32BinaryOperation::parse_text(name).is_some() =>
      {
        Instruction::BinaryI32 { operation: I32BinaryOperation::parse_text(name).expect("guarded i32 operation \
                                                                                         must parse"),
                                 result,
                                 left,
                                 right }
      }
      "eq.i32" => Instruction::EqI32 { result,
                                       left,
                                       right },
      "lt.i32" => Instruction::LtI32 { result,
                                       left,
                                       right },
      "le.i32" => Instruction::LeI32 { result,
                                       left,
                                       right },
      "gt.i32" => Instruction::GtI32 { result,
                                       left,
                                       right },
      "ge.i32" => Instruction::GeI32 { result,
                                       left,
                                       right },
      _ => return None,
    })
  }

  fn not_bool(&mut self, function: &mut Function, result: &str, operand: &str, line: usize) -> Option<Instruction>
  {
    let Some(result) = result.strip_suffix(":bool")
    else
    {
      self.report(DiagnosticCode::InvalidInstruction,
                  line,
                  "XLIL not.bool result type is invalid");
      return None;
    };
    let result = self.value_id(result, line)?;
    let operand = self.value_operand(operand, line)?;
    function.values.push(Value { id: result,
                                 value_type: Type::BOOL });
    Some(Instruction::NotBool { result,
                                operand })
  }

  fn value_call(&mut self, function: &mut Function, result: &str, call: &str, line: usize) -> Option<Instruction>
  {
    let Some((result, return_type)) = result.split_once(':')
    else
    {
      self.report(DiagnosticCode::InvalidInstruction, line, "XLIL call result is invalid");
      return None;
    };
    let result = self.value_id(result, line)?;
    let return_type = self.type_name(return_type, line)?;
    if return_type == Type::VOID
    {
      self.report(DiagnosticCode::InvalidInstruction,
                  line,
                  "XLIL value call result cannot have void type");
      return None;
    }
    let (function_name, arguments) = self.call_operands(call, line)?;
    function.values.push(Value { id: result,
                                 value_type: return_type });
    Some(Instruction::Call { result: Some(result),
                             function: function_name,
                             arguments,
                             return_type })
  }

  fn void_call(&mut self, _function: &mut Function, call: &str, line: usize) -> Option<Instruction>
  {
    let (function_name, arguments) = self.call_operands(call, line)?;
    Some(Instruction::Call { result: None,
                             function: function_name,
                             arguments,
                             return_type: Type::VOID })
  }

  fn current_line(&self) -> Option<&str>
  {
    self.lines.get(self.cursor).copied()
  }

  fn next_line(&mut self) -> Option<String>
  {
    let line = self.current_line()?.to_string();
    self.cursor += 1;
    Some(line)
  }

  fn line_number(&self) -> usize
  {
    self.cursor + 1
  }

  fn report(&mut self, code: DiagnosticCode, line: usize, message: &str)
  {
    self.diagnostics.push(Diagnostic { code,
                                       line,
                                       message: message.to_string() });
  }
}

#[cfg(test)]
mod tests;
