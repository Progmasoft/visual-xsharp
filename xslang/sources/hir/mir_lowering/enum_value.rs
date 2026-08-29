/*
 * SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
 * SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.0
 */

use super::*;

impl HirToMirLowerer
{
    pub(super) fn lower_enum_comparison(
        &mut self,
        target: mir::LocalId,
        operator: BinaryOperator,
        operand_type: XlilType,
        operands: (mir::LocalId, mir::LocalId),
        span: Span,
        lowered: &mut mir::Function,
    ) -> bool
    {
        if !matches!(operator, BinaryOperator::Equal | BinaryOperator::NotEqual) ||
            !self.nominal_types.values().any(|definition| {
                definition.kind == crate::hir::declarations::NominalKind::Enum &&
                    self.aggregate_types.get(&definition.name).copied() == Some(operand_type)
            })
        {
            return false;
        }
        let (left, right) = operands;
        let Some(left_tag) = self.extract_enum_tag(left, span, lowered)
        else
        {
            return true;
        };
        let Some(right_tag) = self.extract_enum_tag(right, span, lowered)
        else
        {
            return true;
        };
        let result = if operator == BinaryOperator::Equal
        {
            target
        }
        else
        {
            let Some(result) = self.declare_temp(XlilType::BOOL, span, lowered)
            else
            {
                return true;
            };
            result
        };
        self.current_block_mut(lowered).statements.push(mir::Statement::EqI32 {
            result,
            left: left_tag,
            right: right_tag,
            span,
        });
        if operator == BinaryOperator::NotEqual
        {
            self.current_block_mut(lowered)
                .statements
                .push(mir::Statement::NotBool {
                    result: target,
                    operand: result,
                    span,
                });
        }
        true
    }

    fn extract_enum_tag(
        &mut self,
        aggregate: mir::LocalId,
        span: Span,
        lowered: &mut mir::Function,
    ) -> Option<mir::LocalId>
    {
        let result = self.declare_temp(XlilType::I32, span, lowered)?;
        self.current_block_mut(lowered)
            .statements
            .push(mir::Statement::Extract {
                result,
                aggregate,
                field: 0,
                field_type: XlilType::I32,
                span,
            });
        Some(result)
    }
}
