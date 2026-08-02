impl Parser<'_>
{
    fn load(&mut self, function: &mut Function, result: &str, slot: &str, line: usize) -> Option<Instruction>
    {
        let (result, result_type) = result.split_once(':')?;
        let result = self.value_id(result, line)?;
        let result_type = self.type_name(result_type, line)?;
        let slot = self.slot_operand(slot, line)?;
        if !function
            .slots
            .get(slot.0 as usize)
            .is_some_and(|entry| entry.id == slot && entry.value_type == result_type)
        {
            self.report(
                DiagnosticCode::InvalidInstruction,
                line,
                "XLIL load type does not match stack slot type",
            );
            return None;
        }
        function.values.push(Value {
            id: result,
            value_type: result_type,
        });
        Some(Instruction::Load {
            result,
            slot,
        })
    }

    fn store(&mut self, text: &str, line: usize) -> Option<Instruction>
    {
        let Some((value, slot)) = text.split_once(", ")
        else
        {
            self.report(
                DiagnosticCode::InvalidInstruction,
                line,
                "XLIL store operands are invalid",
            );
            return None;
        };
        Some(Instruction::Store {
            value: self.value_operand(value, line)?,
            slot: self.slot_operand(slot, line)?,
        })
    }

    fn slot_operand(&mut self, text: &str, line: usize) -> Option<SlotId>
    {
        let parsed = text
            .strip_prefix("%s")
            .and_then(|value| value.parse::<u32>().ok())
            .map(SlotId);
        if parsed.is_none()
        {
            self.report(
                DiagnosticCode::InvalidInstruction,
                line,
                "XLIL stack slot operand is invalid",
            );
        }
        parsed
    }

    fn const_i32(&mut self, function: &mut Function, result: &str, value: &str, line: usize) -> Option<Instruction>
    {
        let Some(result) = result.strip_suffix(":i32")
        else
        {
            self.report(
                DiagnosticCode::InvalidInstruction,
                line,
                "XLIL const.i32 result type is invalid",
            );
            return None;
        };
        let result = self.value_id(result, line)?;
        let Some(parsed) = parse_i32_immediate(value)
        else
        {
            self.report(
                DiagnosticCode::InvalidInteger,
                line,
                "XLIL const.i32 immediate is invalid",
            );
            return None;
        };
        function.values.push(Value {
            id: result,
            value_type: Type::I32,
        });
        Some(
            if value.starts_with("0x")
            {
                Instruction::ConstInteger {
                    result,
                    value: IntegerConstant::new(Type::I32, parsed as u32 as u128)?,
                }
            }
            else
            {
                Instruction::ConstI32 {
                    result,
                    value: parsed,
                }
            },
        )
    }

    fn binary_i64(
        &mut self,
        function: &mut Function,
        result: &str,
        operands: &str,
        instruction: &str,
        line: usize,
    ) -> Option<Instruction>
    {
        let Some(result) = result.strip_suffix(":i64")
        else
        {
            self.report(
                DiagnosticCode::InvalidInstruction,
                line,
                &format!("XLIL {instruction} result type is invalid"),
            );
            return None;
        };
        let result = self.value_id(result, line)?;
        let Some((left, right)) = operands.split_once(", ")
        else
        {
            self.report(
                DiagnosticCode::InvalidInstruction,
                line,
                &format!("XLIL {instruction} operands are invalid"),
            );
            return None;
        };
        let left = self.value_operand(left, line)?;
        let right = self.value_operand(right, line)?;
        function.values.push(Value {
            id: result,
            value_type: Type::I64,
        });
        match instruction
        {
            "add.i64" => Some(Instruction::AddI64 {
                result,
                left,
                right,
            }),
            "sub.i64" => Some(Instruction::SubI64 {
                result,
                left,
                right,
            }),
            "mul.i64" => Some(Instruction::MulI64 {
                result,
                left,
                right,
            }),
            _ => None,
        }
    }

    fn eq_i64(&mut self, function: &mut Function, result: &str, operands: &str, line: usize) -> Option<Instruction>
    {
        let Some(result) = result.strip_suffix(":bool")
        else
        {
            self.report(
                DiagnosticCode::InvalidInstruction,
                line,
                "XLIL eq.i64 result type is invalid",
            );
            return None;
        };
        let result = self.value_id(result, line)?;
        let Some((left, right)) = operands.split_once(", ")
        else
        {
            self.report(
                DiagnosticCode::InvalidInstruction,
                line,
                "XLIL eq.i64 operands are invalid",
            );
            return None;
        };
        let left = self.value_operand(left, line)?;
        let right = self.value_operand(right, line)?;
        function.values.push(Value {
            id: result,
            value_type: Type::BOOL,
        });
        Some(Instruction::EqI64 {
            result,
            left,
            right,
        })
    }

    fn binary_i32(
        &mut self,
        function: &mut Function,
        result: &str,
        operands: &str,
        instruction: &str,
        line: usize,
    ) -> Option<Instruction>
    {
        let result_type = match instruction
        {
            "add.i32" | "sub.i32" | "mul.i32" | "div.i32" | "rem.i32" | "and.i32" | "or.i32" | "xor.i32" |
            "shl.i32" | "shr.i32" => Type::I32,
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
            self.report(
                DiagnosticCode::InvalidInstruction,
                line,
                &format!("XLIL {instruction} result type is invalid"),
            );
            return None;
        };
        let result = self.value_id(result, line)?;
        let Some((left, right)) = operands.split_once(", ")
        else
        {
            self.report(
                DiagnosticCode::InvalidInstruction,
                line,
                &format!("XLIL {instruction} operands are invalid"),
            );
            return None;
        };
        let left = self.value_operand(left, line)?;
        let right = self.value_operand(right, line)?;
        function.values.push(Value {
            id: result,
            value_type: result_type,
        });
        Some(match instruction
        {
            "add.i32" => Instruction::AddI32 {
                result,
                left,
                right,
            },
            "sub.i32" => Instruction::SubI32 {
                result,
                left,
                right,
            },
            "mul.i32" => Instruction::MulI32 {
                result,
                left,
                right,
            },
            name if I32BinaryOperation::parse_text(name).is_some() => Instruction::BinaryI32 {
                operation: I32BinaryOperation::parse_text(name).expect("guarded i32 operation must parse"),
                result,
                left,
                right,
            },
            "eq.i32" => Instruction::EqI32 {
                result,
                left,
                right,
            },
            "lt.i32" => Instruction::LtI32 {
                result,
                left,
                right,
            },
            "le.i32" => Instruction::LeI32 {
                result,
                left,
                right,
            },
            "gt.i32" => Instruction::GtI32 {
                result,
                left,
                right,
            },
            "ge.i32" => Instruction::GeI32 {
                result,
                left,
                right,
            },
            _ => return None,
        })
    }

    fn not_bool(&mut self, function: &mut Function, result: &str, operand: &str, line: usize) -> Option<Instruction>
    {
        let Some(result) = result.strip_suffix(":bool")
        else
        {
            self.report(
                DiagnosticCode::InvalidInstruction,
                line,
                "XLIL not.bool result type is invalid",
            );
            return None;
        };
        let result = self.value_id(result, line)?;
        let operand = self.value_operand(operand, line)?;
        function.values.push(Value {
            id: result,
            value_type: Type::BOOL,
        });
        Some(Instruction::NotBool {
            result,
            operand,
        })
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
            self.report(
                DiagnosticCode::InvalidInstruction,
                line,
                "XLIL value call result cannot have void type",
            );
            return None;
        }
        let (function_name, arguments) = self.call_operands(call, line)?;
        function.values.push(Value {
            id: result,
            value_type: return_type,
        });
        Some(Instruction::Call {
            result: Some(result),
            function: function_name,
            arguments,
            return_type,
        })
    }

    fn void_call(&mut self, _function: &mut Function, call: &str, line: usize) -> Option<Instruction>
    {
        let (function_name, arguments) = self.call_operands(call, line)?;
        Some(Instruction::Call {
            result: None,
            function: function_name,
            arguments,
            return_type: Type::VOID,
        })
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
        self.diagnostics.push(Diagnostic {
            code,
            line,
            message: message.to_string(),
        });
    }
}

fn parse_i32_immediate(value: &str) -> Option<i32>
{
    if let Some(hexadecimal) = value.strip_prefix("0x")
    {
        if hexadecimal.len() != 8
        {
            return None;
        }
        return u32::from_str_radix(hexadecimal, 16).ok().map(|bits| bits as i32);
    }
    value.parse::<i32>().ok()
}

#[cfg(test)]
#[path = "tests.rs"]
mod tests;
