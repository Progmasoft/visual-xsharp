/*
 * SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
 * SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.0
 */

use super::*;

impl HirToMirLowerer
{
    pub(super) fn lower_short_circuit_expression(
        &mut self,
        operator: BinaryOperator,
        left: &Expression,
        right: &Expression,
        span: Span,
        lowered: &mut mir::Function,
    ) -> Option<mir::LocalId>
    {
        if !matches!(operator, BinaryOperator::LogicalAnd | BinaryOperator::LogicalOr)
        {
            return None;
        }
        let result_storage = self.declare_storage_temp(XlilType::BOOL, span, lowered)?;
        let left = self.lower_expression_to_local(left, XlilType::BOOL, lowered)?;
        let right_block = self.append_block(span, lowered);
        let short_block = self.append_block(span, lowered);
        let merge_block = self.append_block(span, lowered);
        let (then_block, else_block, short_value) = if operator == BinaryOperator::LogicalAnd
        {
            (right_block, short_block, false)
        }
        else
        {
            (short_block, right_block, true)
        };
        self.set_terminator(
            mir::Terminator::BranchIf {
                condition: left,
                then_block,
                else_block,
            },
            span,
            lowered,
        );

        self.switch_to(short_block);
        let short = self.declare_temp(XlilType::BOOL, span, lowered)?;
        self.lower_literal_into(short, &Literal::Bool(short_value), span, lowered);
        self.store_short_circuit_result(result_storage, short, span, lowered);
        self.set_terminator(mir::Terminator::Goto(merge_block), span, lowered);

        self.switch_to(right_block);
        let right = self.lower_expression_to_local(right, XlilType::BOOL, lowered)?;
        self.store_short_circuit_result(result_storage, right, span, lowered);
        self.set_terminator(mir::Terminator::Goto(merge_block), span, lowered);

        self.switch_to(merge_block);
        let result = self.declare_temp(XlilType::BOOL, span, lowered)?;
        self.current_block_mut(lowered)
            .statements
            .push(mir::Statement::LoadLocal {
                result,
                local: result_storage,
                span,
            });
        Some(result)
    }

    fn store_short_circuit_result(
        &mut self,
        storage: mir::LocalId,
        value: mir::LocalId,
        span: Span,
        lowered: &mut mir::Function,
    )
    {
        self.current_block_mut(lowered)
            .statements
            .push(mir::Statement::StoreLocal {
                local: storage,
                value,
                span,
            });
    }
}
