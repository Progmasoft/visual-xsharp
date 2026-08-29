/*
 * SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
 * SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.0
 */

use super::*;

impl HirToMirLowerer
{
    pub(super) fn lower_unary_into(
        &mut self,
        target: mir::LocalId,
        operator: UnaryOperator,
        operand: &Expression,
        span: Span,
        lowered: &mut mir::Function,
    )
    {
        let Some(target_type) = self.local_value_type(target, lowered)
        else
        {
            self.report(
                DiagnosticCode::UnsupportedType,
                "unary expression target has no MIR type",
                span,
            );
            return;
        };
        if operator == UnaryOperator::Negative &&
            let Expression::Literal {
                literal: Literal::Integer(text),
                ..
            } = operand &&
            self.lower_negative_integer_literal_into(target, text, target_type, span, lowered)
        {
            return;
        }
        let operand_type = match operator
        {
            UnaryOperator::LogicalNot => XlilType::BOOL,
            UnaryOperator::Positive | UnaryOperator::Negative => target_type,
        };
        let Some(operand) = self.lower_expression_to_local(operand, operand_type, lowered)
        else
        {
            return;
        };
        match (operator, target_type)
        {
            (UnaryOperator::LogicalNot, XlilType::BOOL) =>
            {
                self.current_block_mut(lowered)
                    .statements
                    .push(mir::Statement::NotBool {
                        result: target,
                        operand,
                        span,
                    })
            }
            (UnaryOperator::Negative, XlilType::I32 | XlilType::I64) =>
            {
                let Some(zero) = self.declare_temp(target_type, span, lowered)
                else
                {
                    return;
                };
                if target_type == XlilType::I32
                {
                    self.current_block_mut(lowered)
                        .statements
                        .push(mir::Statement::ConstI32 {
                            local: zero,
                            value: 0,
                            span,
                        });
                    self.current_block_mut(lowered).statements.push(mir::Statement::SubI32 {
                        result: target,
                        left: zero,
                        right: operand,
                        span,
                    });
                }
                else
                {
                    self.current_block_mut(lowered)
                        .statements
                        .push(mir::Statement::ConstI64 {
                            local: zero,
                            value: 0,
                            span,
                        });
                    self.current_block_mut(lowered).statements.push(mir::Statement::SubI64 {
                        result: target,
                        left: zero,
                        right: operand,
                        span,
                    });
                }
            }
            _ => self.report(
                DiagnosticCode::UnsupportedExpression,
                "HIR unary expression has no MIR instruction for this type",
                span,
            ),
        }
    }
}
