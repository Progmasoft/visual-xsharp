/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 */

use super::*;

impl HirToMirLowerer
{
    pub(super) fn lower_if_statement(&mut self, statement: &Statement, lowered: &mut mir::Function)
    {
        let Statement::If {
            condition,
            then_block,
            else_block,
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
        let Some(condition) = self.lower_expression_to_local(condition, XlilType::BOOL, lowered)
        else
        {
            return;
        };
        let branch_block = self.current_block;
        let then_id = self.append_block(then_block.span, lowered);
        let else_span = else_block.as_ref().map_or(*span, |block| block.span);
        let else_id = self.append_block(else_span, lowered);
        self.switch_to(branch_block);
        self.set_terminator(
            mir::Terminator::BranchIf {
                condition,
                then_block: then_id,
                else_block: else_id,
            },
            *span,
            lowered,
        );

        let outer_locals = self.locals.clone();
        self.switch_to(then_id);
        self.lower_block_statements(then_block, lowered);
        let then_end = self.current_block;
        let then_open = !self.current_is_terminated(lowered);

        self.locals.clone_from(&outer_locals);
        self.switch_to(else_id);
        if let Some(else_block) = else_block
        {
            self.lower_block_statements(else_block, lowered);
        }
        let else_end = self.current_block;
        let else_open = !self.current_is_terminated(lowered);
        self.locals = outer_locals;

        if then_open || else_open
        {
            let merge = self.append_block(*span, lowered);
            if then_open
            {
                self.switch_to(then_end);
                self.set_terminator(mir::Terminator::Goto(merge), *span, lowered);
            }
            if else_open
            {
                self.switch_to(else_end);
                self.set_terminator(mir::Terminator::Goto(merge), *span, lowered);
            }
            self.switch_to(merge);
        }
        else
        {
            self.switch_to(else_end);
        }
    }

    pub(super) fn lower_if_expression(
        &mut self,
        expression: &Expression,
        expected_type: XlilType,
        lowered: &mut mir::Function,
    ) -> Option<mir::LocalId>
    {
        let Expression::If {
            condition,
            then_block,
            else_block,
            result_type,
            span,
        } = expression
        else
        {
            return None;
        };
        let actual_type = match result_type.as_ref()
        {
            Type::Primitive(value) => primitive_to_xlil(*value),
            Type::Unit |
            Type::Named(_) |
            Type::Optional {
                ..
            } |
            Type::Result {
                ..
            } |
            Type::Reference {
                ..
            } |
            Type::Array {
                ..
            } |
            Type::Set {
                ..
            } |
            Type::Map {
                ..
            } |
            Type::Tuple {
                ..
            } => None,
        };
        if actual_type != Some(expected_type)
        {
            self.report(
                DiagnosticCode::UnsupportedType,
                "if expression result type does not match MIR context",
                *span,
            );
            return None;
        }
        let condition = self.lower_expression_to_local(condition, XlilType::BOOL, lowered)?;
        let result_storage = self.declare_storage_temp(expected_type, *span, lowered)?;
        let branch = self.current_block;
        let then_id = self.append_block(then_block.span, lowered);
        let else_id = self.append_block(else_block.span, lowered);
        let merge = self.append_block(*span, lowered);
        self.switch_to(branch);
        self.set_terminator(
            mir::Terminator::BranchIf {
                condition,
                then_block: then_id,
                else_block: else_id,
            },
            *span,
            lowered,
        );

        let outer_locals = self.locals.clone();
        self.switch_to(then_id);
        let then_open = self.lower_value_block_into_storage(
            then_block,
            result_storage,
            expected_type,
            merge,
            then_block.span,
            lowered,
        );
        self.locals.clone_from(&outer_locals);
        self.switch_to(else_id);
        let else_open = self.lower_value_block_into_storage(
            else_block,
            result_storage,
            expected_type,
            merge,
            else_block.span,
            lowered,
        );
        self.locals = outer_locals;
        if !then_open && !else_open
        {
            self.report(
                DiagnosticCode::MissingReturn,
                "if expression has no value-producing continuation",
                *span,
            );
            return None;
        }
        self.switch_to(merge);
        let result = self.declare_temp(expected_type, *span, lowered)?;
        self.current_block_mut(lowered)
            .statements
            .push(mir::Statement::LoadLocal {
                result,
                local: result_storage,
                span: *span,
            });
        Some(result)
    }

    pub(super) fn lower_if_return(&mut self, expression: &Expression, return_span: Span, lowered: &mut mir::Function)
    {
        let Expression::If {
            condition,
            then_block,
            else_block,
            span,
            ..
        } = expression
        else
        {
            return;
        };
        let Some(condition) = self.lower_expression_to_local(condition, XlilType::BOOL, lowered)
        else
        {
            return;
        };
        let branch_block = self.current_block;
        let then_id = self.append_block(then_block.span, lowered);
        let else_id = self.append_block(else_block.span, lowered);
        self.switch_to(branch_block);
        self.set_terminator(
            mir::Terminator::BranchIf {
                condition,
                then_block: then_id,
                else_block: else_id,
            },
            *span,
            lowered,
        );

        let outer_locals = self.locals.clone();
        self.switch_to(then_id);
        self.lower_value_block_return(then_block, return_span, lowered);
        let then_end = self.current_block;

        self.locals.clone_from(&outer_locals);
        self.switch_to(else_id);
        self.lower_value_block_return(else_block, return_span, lowered);
        let else_end = self.current_block;
        self.locals = outer_locals;

        if !self.block_is_terminated(then_end, lowered) || !self.block_is_terminated(else_end, lowered)
        {
            self.report(
                DiagnosticCode::MissingReturn,
                "if expression branches must terminate with their value",
                *span,
            );
        }
        self.switch_to(else_end);
    }

    fn lower_value_block_return(&mut self, block: &Block, return_span: Span, lowered: &mut mir::Function)
    {
        for statement in &block.statements
        {
            if self.current_is_terminated(lowered)
            {
                return;
            }
            self.lower_statement(statement, lowered);
        }
        if !self.current_is_terminated(lowered)
        {
            self.lower_return(block.tail.as_deref(), return_span, lowered);
        }
    }
}
