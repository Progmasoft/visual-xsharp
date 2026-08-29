// SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
// SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.0

impl HirToMirLowerer
{
    pub(super) fn lower_while_statement(&mut self, statement: &Statement, lowered: &mut mir::Function)
    {
        let Statement::While {
            condition,
            body,
            span,
        } = statement
        else
        {
            return;
        };
        if self.current_is_terminated(lowered)
        {
            return;
        }
        let preheader = self.current_block;
        let header = self.append_block(*span, lowered);
        let body_id = self.append_block(body.span, lowered);
        let exit = self.append_block(*span, lowered);
        self.switch_to(preheader);
        self.set_terminator(mir::Terminator::Goto(header), *span, lowered);

        self.switch_to(header);
        let Some(condition) = self.lower_expression_to_local(condition, XlilType::BOOL, lowered)
        else
        {
            return;
        };
        self.set_terminator(
            mir::Terminator::BranchIf {
                condition,
                then_block: body_id,
                else_block: exit,
            },
            *span,
            lowered,
        );

        let outer_locals = self.locals.clone();
        self.loop_targets.push((header, exit));
        self.switch_to(body_id);
        self.lower_block_statements(body, lowered);
        if !self.current_is_terminated(lowered)
        {
            self.set_terminator(mir::Terminator::Goto(header), body.span, lowered);
        }
        self.loop_targets.pop();
        self.locals = outer_locals;
        self.switch_to(exit);
    }

    pub(super) fn lower_for_statement(&mut self, statement: &Statement, lowered: &mut mir::Function)
    {
        let Statement::For {
            initializer,
            condition,
            update,
            body,
            span,
        } = statement
        else
        {
            return;
        };
        if self.current_is_terminated(lowered)
        {
            return;
        }

        let outer_locals = self.locals.clone();
        if let Some(initializer) = initializer
        {
            self.lower_statement(initializer, lowered);
        }
        let loop_locals = self.locals.clone();
        let preheader = self.current_block;
        let header = self.append_block(*span, lowered);
        let body_id = self.append_block(body.span, lowered);
        let update_id = self.append_block(*span, lowered);
        let exit = self.append_block(*span, lowered);
        self.switch_to(preheader);
        self.set_terminator(mir::Terminator::Goto(header), *span, lowered);

        self.switch_to(header);
        let condition = match condition
        {
            Some(condition) => self.lower_expression_to_local(condition, XlilType::BOOL, lowered),
            None =>
            {
                let result = self.declare_temp(XlilType::BOOL, *span, lowered);
                if let Some(result) = result
                {
                    self.lower_literal_into(result, &Literal::Bool(true), *span, lowered);
                }
                result
            }
        };
        let Some(condition) = condition
        else
        {
            return;
        };
        self.set_terminator(
            mir::Terminator::BranchIf {
                condition,
                then_block: body_id,
                else_block: exit,
            },
            *span,
            lowered,
        );

        self.loop_targets.push((update_id, exit));
        self.switch_to(body_id);
        self.lower_block_statements(body, lowered);
        if !self.current_is_terminated(lowered)
        {
            self.set_terminator(mir::Terminator::Goto(update_id), body.span, lowered);
        }
        self.loop_targets.pop();

        self.locals.clone_from(&loop_locals);
        self.switch_to(update_id);
        if let Some(update) = update
        {
            self.lower_statement(&Statement::Expr(update.clone()), lowered);
        }
        if !self.current_is_terminated(lowered)
        {
            self.set_terminator(mir::Terminator::Goto(header), *span, lowered);
        }

        self.locals = outer_locals;
        self.switch_to(exit);
    }

    pub(super) fn lower_for_each_statement(&mut self, statement: &Statement, lowered: &mut mir::Function)
    {
        let Statement::ForEach {
            binding,
            iterable,
            iterable_type,
            body,
            span,
        } = statement
        else
        {
            return;
        };
        if self.current_is_terminated(lowered)
        {
            return;
        }
        let Type::Array {
            length, ..
        } = iterable_type
        else
        {
            self.report(
                DiagnosticCode::UnsupportedType,
                "native for-each requires an array iterable",
                *span,
            );
            return;
        };
        if length.is_some_and(|value| i64::try_from(value).is_err())
        {
            self.report(
                DiagnosticCode::UnsupportedType,
                "for-each fixed-array length exceeds the MIR index range",
                *span,
            );
            return;
        }
        let Some(array_type) = self.lower_value_type(iterable_type, *span)
        else
        {
            return;
        };
        let Some((_, element_type, registered_length)) = self
            .array_layouts
            .iter()
            .find(|(value_type, _, _)| *value_type == array_type)
            .copied()
        else
        {
            self.report(
                DiagnosticCode::UnsupportedType,
                "for-each iterable has no array MIR layout",
                *span,
            );
            return;
        };
        if registered_length != *length || self.lower_value_type(&binding.ty, binding.span) != Some(element_type)
        {
            self.report(
                DiagnosticCode::UnsupportedType,
                "for-each binding does not match its array layout",
                binding.span,
            );
            return;
        }
        let Some(array) = self.lower_expression_to_local(iterable, array_type, lowered)
        else
        {
            return;
        };
        let Some(index_storage) = self.declare_storage_temp(XlilType::I64, *span, lowered)
        else
        {
            return;
        };
        let Some(zero) = self.declare_temp(XlilType::I64, *span, lowered)
        else
        {
            return;
        };
        self.lower_literal_into(zero, &Literal::Integer("0".to_string()), *span, lowered);
        self.current_block_mut(lowered)
            .statements
            .push(mir::Statement::StoreLocal {
                local: index_storage,
                value: zero,
                span: *span,
            });

        let preheader = self.current_block;
        let header = self.append_block(*span, lowered);
        let body_id = self.append_block(body.span, lowered);
        let update_id = self.append_block(*span, lowered);
        let exit = self.append_block(*span, lowered);
        self.switch_to(preheader);
        self.set_terminator(mir::Terminator::Goto(header), *span, lowered);

        self.switch_to(header);
        let Some(index) = self.declare_temp(XlilType::I64, *span, lowered)
        else
        {
            return;
        };
        self.current_block_mut(lowered)
            .statements
            .push(mir::Statement::LoadLocal {
                result: index,
                local: index_storage,
                span: *span,
            });
        let Some(limit) = self.declare_temp(XlilType::I64, *span, lowered)
        else
        {
            return;
        };
        if let Some(length) = length
        {
            self.lower_literal_into(limit, &Literal::Integer(length.to_string()), *span, lowered);
        }
        else
        {
            self.current_block_mut(lowered)
                .statements
                .push(mir::Statement::ArrayLength {
                    result: limit,
                    array,
                    array_type,
                    span: *span,
                });
        }
        let Some(condition) = self.declare_temp(XlilType::BOOL, *span, lowered)
        else
        {
            return;
        };
        self.current_block_mut(lowered)
            .statements
            .push(mir::Statement::BinaryInteger {
                operation: crate::xlil::IntegerBinaryOperation::Less,
                value_type: XlilType::I64,
                result: condition,
                left: index,
                right: limit,
                span: *span,
            });
        self.set_terminator(
            mir::Terminator::BranchIf {
                condition,
                then_block: body_id,
                else_block: exit,
            },
            *span,
            lowered,
        );

        let outer_locals = self.locals.clone();
        self.locals.remove(&binding.name);
        self.switch_to(body_id);
        let binding_local = self.declare_local(binding.name.clone(), &binding.ty, false, binding.span, lowered);
        let Some(element) = self.declare_temp(element_type, binding.span, lowered)
        else
        {
            return;
        };
        self.current_block_mut(lowered)
            .statements
            .push(mir::Statement::ArrayGet {
                result: element,
                array,
                index,
                array_type,
                element_type,
                span: binding.span,
            });
        self.current_block_mut(lowered)
            .statements
            .push(mir::Statement::StoreLocal {
                local: binding_local,
                value: element,
                span: binding.span,
            });
        self.loop_targets.push((update_id, exit));
        self.lower_block_statements(body, lowered);
        if !self.current_is_terminated(lowered)
        {
            self.set_terminator(mir::Terminator::Goto(update_id), body.span, lowered);
        }
        self.loop_targets.pop();

        self.switch_to(update_id);
        let Some(current) = self.declare_temp(XlilType::I64, *span, lowered)
        else
        {
            return;
        };
        self.current_block_mut(lowered)
            .statements
            .push(mir::Statement::LoadLocal {
                result: current,
                local: index_storage,
                span: *span,
            });
        let Some(one) = self.declare_temp(XlilType::I64, *span, lowered)
        else
        {
            return;
        };
        self.lower_literal_into(one, &Literal::Integer("1".to_string()), *span, lowered);
        let Some(next) = self.declare_temp(XlilType::I64, *span, lowered)
        else
        {
            return;
        };
        self.current_block_mut(lowered)
            .statements
            .push(mir::Statement::BinaryInteger {
                operation: crate::xlil::IntegerBinaryOperation::Add,
                value_type: XlilType::I64,
                result: next,
                left: current,
                right: one,
                span: *span,
            });
        self.current_block_mut(lowered)
            .statements
            .push(mir::Statement::StoreLocal {
                local: index_storage,
                value: next,
                span: *span,
            });
        self.set_terminator(mir::Terminator::Goto(header), *span, lowered);

        self.locals = outer_locals;
        self.switch_to(exit);
    }

    pub(super) fn lower_loop_jump(&mut self, is_continue: bool, span: Span, lowered: &mut mir::Function)
    {
        let Some((header, exit)) = self.loop_targets.last().copied()
        else
        {
            self.report(
                DiagnosticCode::UnsupportedExpression,
                "loop jump appears outside a loop",
                span,
            );
            return;
        };
        let target = if is_continue
        {
            header
        }
        else
        {
            exit
        };
        self.set_terminator(mir::Terminator::Goto(target), span, lowered);
    }

    pub(super) fn lower_block_statements(&mut self, block: &Block, lowered: &mut mir::Function)
    {
        let mut declared_here = std::collections::HashSet::new();
        for statement in &block.statements
        {
            if self.current_is_terminated(lowered)
            {
                break;
            }
            if let Statement::Let {
                local, ..
            } = statement &&
                declared_here.insert(local.name.clone())
            {
                self.locals.remove(&local.name);
            }
            self.lower_statement(statement, lowered);
        }
        if let Some(tail) = &block.tail &&
            !self.current_is_terminated(lowered)
        {
            self.unsupported_expression(tail);
        }
    }

    pub(super) fn block_is_terminated(&self, id: mir::BlockId, lowered: &mir::Function) -> bool
    {
        lowered
            .blocks
            .iter()
            .find(|block| block.id == id)
            .is_some_and(|block| block.terminator.is_some())
    }
}
