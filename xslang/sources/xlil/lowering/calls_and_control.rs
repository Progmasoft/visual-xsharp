// SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
// SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.0

impl MirToXlilLowerer
{
    #[allow(clippy::too_many_arguments)]
    fn lower_call(
        &mut self,
        result: Option<mir::LocalId>,
        function: &str,
        arguments: &[mir::LocalId],
        return_type: Type,
        span: Span,
        xlil_block: BlockId,
        values: &mut HashMap<mir::LocalId, ValueId>,
        lowered: &mut Function,
    )
    {
        let mut lowered_arguments = Vec::new();
        for argument in arguments
        {
            let Some(value) = values.get(argument).copied()
            else
            {
                self.report(
                    DiagnosticCode::MissingLocalValue,
                    "MIR call argument does not have a lowered XLIL value",
                    span,
                );
                return;
            };
            lowered_arguments.push(value);
        }
        let Some(call_result) = lowered.add_call(xlil_block, function, lowered_arguments, return_type)
        else
        {
            self.report(
                DiagnosticCode::UnsupportedReturnValue,
                "MIR call could not be lowered to XLIL",
                span,
            );
            return;
        };
        if let Some(result) = result
        {
            let Some(call_result) = call_result
            else
            {
                self.report(
                    DiagnosticCode::UnsupportedReturnValue,
                    "void MIR call cannot produce a result local",
                    span,
                );
                return;
            };
            values.insert(result, call_result);
        }
    }

    #[allow(clippy::too_many_arguments)]
    fn lower_float_operation(
        &mut self,
        result: mir::LocalId,
        left: mir::LocalId,
        right: mir::LocalId,
        span: Span,
        instruction: &str,
        xlil_block: BlockId,
        values: &mut HashMap<mir::LocalId, ValueId>,
        lowered: &mut Function,
        operation: impl FnOnce(&mut Function, BlockId, ValueId, ValueId) -> Option<ValueId>,
    )
    {
        let Some(left) = values.get(&left).copied()
        else
        {
            self.report(
                DiagnosticCode::MissingLocalValue,
                &format!("MIR {instruction} left operand has no lowered XLIL value"),
                span,
            );
            return;
        };
        let Some(right) = values.get(&right).copied()
        else
        {
            self.report(
                DiagnosticCode::MissingLocalValue,
                &format!("MIR {instruction} right operand has no lowered XLIL value"),
                span,
            );
            return;
        };
        let Some(value) = operation(lowered, xlil_block, left, right)
        else
        {
            self.report(
                DiagnosticCode::UnsupportedLocalType,
                &format!("MIR {instruction} operands have incompatible XLIL types"),
                span,
            );
            return;
        };
        values.insert(result, value);
    }

    #[allow(clippy::too_many_arguments)]
    fn lower_binary_i64(
        &mut self,
        result: mir::LocalId,
        left: mir::LocalId,
        right: mir::LocalId,
        span: Span,
        instruction: &str,
        xlil_block: BlockId,
        values: &mut HashMap<mir::LocalId, ValueId>,
        lowered: &mut Function,
    )
    {
        let Some(left) = values.get(&left).copied()
        else
        {
            self.report(
                DiagnosticCode::MissingLocalValue,
                &format!("MIR {instruction} left operand does not have a lowered XLIL value"),
                span,
            );
            return;
        };
        let Some(right) = values.get(&right).copied()
        else
        {
            self.report(
                DiagnosticCode::MissingLocalValue,
                &format!("MIR {instruction} right operand does not have a lowered XLIL value"),
                span,
            );
            return;
        };
        let value = match instruction
        {
            "add.i64" => lowered.add_i64(xlil_block, left, right),
            "sub.i64" => lowered.sub_i64(xlil_block, left, right),
            "mul.i64" => lowered.mul_i64(xlil_block, left, right),
            name => crate::xlil::I64BinaryOperation::parse_text(name)
                .and_then(|operation| lowered.binary_i64(xlil_block, operation, left, right)),
        };
        let Some(value) = value
        else
        {
            self.report(
                DiagnosticCode::UnsupportedLocalType,
                &format!("MIR {instruction} operands must lower to XLIL i64 values"),
                span,
            );
            return;
        };
        values.insert(result, value);
    }

    #[allow(clippy::too_many_arguments)]
    fn lower_compare_i64(
        &mut self,
        result: mir::LocalId,
        left: mir::LocalId,
        right: mir::LocalId,
        span: Span,
        operation: crate::xlil::I64ComparisonOperation,
        xlil_block: BlockId,
        values: &mut HashMap<mir::LocalId, ValueId>,
        lowered: &mut Function,
    )
    {
        let Some(left) = values.get(&left).copied()
        else
        {
            self.report(
                DiagnosticCode::MissingLocalValue,
                &format!(
                    "MIR {} left operand does not have a lowered XLIL value",
                    operation.text_name()
                ),
                span,
            );
            return;
        };
        let Some(right) = values.get(&right).copied()
        else
        {
            self.report(
                DiagnosticCode::MissingLocalValue,
                &format!(
                    "MIR {} right operand does not have a lowered XLIL value",
                    operation.text_name()
                ),
                span,
            );
            return;
        };
        let Some(value) = lowered.compare_i64(xlil_block, operation, left, right)
        else
        {
            self.report(
                DiagnosticCode::UnsupportedLocalType,
                &format!("MIR {} operands must lower to XLIL i64 values", operation.text_name()),
                span,
            );
            return;
        };
        values.insert(result, value);
    }

    #[allow(clippy::too_many_arguments)]
    fn lower_eq_i64(
        &mut self,
        result: mir::LocalId,
        left: mir::LocalId,
        right: mir::LocalId,
        span: Span,
        xlil_block: BlockId,
        values: &mut HashMap<mir::LocalId, ValueId>,
        lowered: &mut Function,
    )
    {
        let Some(left) = values.get(&left).copied()
        else
        {
            self.report(
                DiagnosticCode::MissingLocalValue,
                "MIR eq.i64 left operand does not have a lowered XLIL value",
                span,
            );
            return;
        };
        let Some(right) = values.get(&right).copied()
        else
        {
            self.report(
                DiagnosticCode::MissingLocalValue,
                "MIR eq.i64 right operand does not have a lowered XLIL value",
                span,
            );
            return;
        };
        let Some(value) = lowered.eq_i64(xlil_block, left, right)
        else
        {
            self.report(
                DiagnosticCode::UnsupportedLocalType,
                "MIR eq.i64 operands must lower to XLIL i64 values",
                span,
            );
            return;
        };
        values.insert(result, value);
    }

    #[allow(clippy::too_many_arguments)]
    fn lower_binary_i32(
        &mut self,
        result: mir::LocalId,
        left: mir::LocalId,
        right: mir::LocalId,
        span: Span,
        instruction: &str,
        xlil_block: BlockId,
        values: &mut HashMap<mir::LocalId, ValueId>,
        lowered: &mut Function,
    )
    {
        let Some(left) = values.get(&left).copied()
        else
        {
            self.report(
                DiagnosticCode::MissingLocalValue,
                &format!("MIR {instruction} left operand does not have a lowered XLIL value"),
                span,
            );
            return;
        };
        let Some(right) = values.get(&right).copied()
        else
        {
            self.report(
                DiagnosticCode::MissingLocalValue,
                &format!("MIR {instruction} right operand does not have a lowered XLIL value"),
                span,
            );
            return;
        };
        let value = match instruction
        {
            "add.i32" => lowered.add_i32(xlil_block, left, right),
            "sub.i32" => lowered.sub_i32(xlil_block, left, right),
            "mul.i32" => lowered.mul_i32(xlil_block, left, right),
            name if I32BinaryOperation::parse_text(name).is_some() => lowered.binary_i32(
                xlil_block,
                left,
                right,
                I32BinaryOperation::parse_text(name).expect("guarded i32 operation must parse"),
            ),
            "eq.i32" => lowered.eq_i32(xlil_block, left, right),
            "lt.i32" => lowered.lt_i32(xlil_block, left, right),
            "le.i32" => lowered.le_i32(xlil_block, left, right),
            "gt.i32" => lowered.gt_i32(xlil_block, left, right),
            "ge.i32" => lowered.ge_i32(xlil_block, left, right),
            _ => None,
        };
        let Some(value) = value
        else
        {
            self.report(
                DiagnosticCode::UnsupportedLocalType,
                &format!("MIR {instruction} operands must lower to XLIL i32 values"),
                span,
            );
            return;
        };
        values.insert(result, value);
    }

    fn lower_terminator(
        &mut self,
        block: &mir::BasicBlock,
        xlil_block: BlockId,
        blocks: &HashMap<mir::BlockId, BlockId>,
        local_types: &HashMap<mir::LocalId, Option<Type>>,
        values: &HashMap<mir::LocalId, ValueId>,
        lowered: &mut Function,
    )
    {
        match block.terminator
        {
            Some(mir::Terminator::Return(None)) =>
            {
                if !lowered.set_return(xlil_block, None)
                {
                    self.report(
                        DiagnosticCode::MissingMirTerminator,
                        "XLIL block could not receive a return terminator",
                        block.span,
                    );
                }
            }
            Some(mir::Terminator::Return(Some(local))) =>
            {
                if local_types.get(&local).copied().flatten().is_none()
                {
                    self.report(
                        DiagnosticCode::MissingLocalType,
                        "MIR return local has no XLIL value type",
                        block.span,
                    );
                }
                let Some(value) = values.get(&local).copied()
                else
                {
                    self.report(
                        DiagnosticCode::MissingLocalValue,
                        "MIR return local does not have a lowered XLIL value",
                        block.span,
                    );
                    return;
                };
                if !lowered.set_return(xlil_block, Some(value))
                {
                    self.report(
                        DiagnosticCode::UnsupportedReturnValue,
                        "MIR return value does not match the lowered XLIL function signature",
                        block.span,
                    );
                }
            }
            Some(mir::Terminator::Goto(target)) =>
            {
                let Some(target) = blocks.get(&target).copied()
                else
                {
                    self.report(
                        DiagnosticCode::MissingBranchTarget,
                        "MIR goto target block is missing",
                        block.span,
                    );
                    return;
                };
                if !lowered.set_branch(xlil_block, target)
                {
                    self.report(
                        DiagnosticCode::MissingBranchTarget,
                        "XLIL branch target block is missing",
                        block.span,
                    );
                }
            }
            Some(mir::Terminator::BranchIf {
                condition,
                then_block,
                else_block,
            }) =>
            {
                match local_types.get(&condition).copied().flatten()
                {
                    Some(Type::BOOL) =>
                    {}
                    Some(_) =>
                    {
                        self.report(
                            DiagnosticCode::UnsupportedLocalType,
                            "MIR branch_if condition local must have XLIL bool type",
                            block.span,
                        );
                        return;
                    }
                    None => self.report(
                        DiagnosticCode::MissingLocalType,
                        "MIR branch_if condition local has no XLIL value type",
                        block.span,
                    ),
                }
                let Some(condition) = values.get(&condition).copied()
                else
                {
                    self.report(
                        DiagnosticCode::MissingLocalValue,
                        "MIR branch_if condition does not have a lowered XLIL value",
                        block.span,
                    );
                    return;
                };
                let Some(then_block) = blocks.get(&then_block).copied()
                else
                {
                    self.report(
                        DiagnosticCode::MissingBranchTarget,
                        "MIR branch_if then target block is missing",
                        block.span,
                    );
                    return;
                };
                let Some(else_block) = blocks.get(&else_block).copied()
                else
                {
                    self.report(
                        DiagnosticCode::MissingBranchTarget,
                        "MIR branch_if else target block is missing",
                        block.span,
                    );
                    return;
                };
                if !lowered.set_branch_if(xlil_block, condition, then_block, else_block)
                {
                    self.report(
                        DiagnosticCode::MissingBranchTarget,
                        "XLIL conditional branch could not be created",
                        block.span,
                    );
                }
            }
            Some(mir::Terminator::Panic) =>
            {
                if !lowered.set_panic(xlil_block)
                {
                    self.report(
                        DiagnosticCode::MissingMirTerminator,
                        "XLIL block could not receive a panic terminator",
                        block.span,
                    );
                }
            }
            Some(mir::Terminator::Unreachable) | None => self.report(
                DiagnosticCode::MissingMirTerminator,
                "MIR terminator cannot yet be lowered to XLIL",
                block.span,
            ),
        }
    }

    fn report(&mut self, code: DiagnosticCode, message: &str, span: Span)
    {
        self.diagnostics.push(Diagnostic {
            code,
            message: message.to_string(),
            span,
        });
    }
}

fn local_types(function: &mir::Function) -> HashMap<mir::LocalId, Option<Type>>
{
    function
        .locals
        .iter()
        .map(|local| (local.id, local.value_type))
        .collect()
}

#[cfg(test)]
#[path = "aggregate_tests.rs"]
mod aggregate_tests;
#[cfg(test)]
#[path = "tests.rs"]
mod tests;
