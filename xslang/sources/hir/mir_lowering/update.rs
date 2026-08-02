/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 */

use super::*;

struct UpdateOperands
{
    result: mir::LocalId,
    value: mir::LocalId,
    one: mir::LocalId,
    span: Span,
}

impl HirToMirLowerer
{
    pub(super) fn lower_update_expression(
        &mut self,
        expression: &Expression,
        expected_type: XlilType,
        lowered: &mut mir::Function,
    ) -> Option<mir::LocalId>
    {
        let Expression::Update {
            target,
            operator,
            position,
            span,
        } = expression
        else
        {
            return None;
        };
        let Some(target_local) = self.locals.get(target).copied()
        else
        {
            self.report(
                DiagnosticCode::UnknownLocal,
                format!("unknown HIR local '{target}'"),
                *span,
            );
            return None;
        };
        let Some(value_type) = self.local_value_type(target_local, lowered)
        else
        {
            self.report(
                DiagnosticCode::UnsupportedType,
                "update target has no MIR value type",
                *span,
            );
            return None;
        };
        if value_type != expected_type || !value_type.is_integer() || !self.storage_locals.contains(&target_local)
        {
            self.report(
                DiagnosticCode::UnsupportedType,
                "update requires an integer storage local matching the expression type",
                *span,
            );
            return None;
        }

        let old_value = self.declare_temp(value_type, *span, lowered)?;
        self.current_block_mut(lowered)
            .statements
            .push(mir::Statement::LoadLocal {
                result: old_value,
                local: target_local,
                span: *span,
            });
        let one = self.declare_temp(value_type, *span, lowered)?;
        self.lower_literal_into(one, &Literal::Integer("1".to_string()), *span, lowered);
        let updated = self.declare_temp(value_type, *span, lowered)?;
        self.emit_update_operation(
            *operator,
            value_type,
            UpdateOperands {
                result: updated,
                value: old_value,
                one,
                span: *span,
            },
            lowered,
        );
        self.current_block_mut(lowered)
            .statements
            .push(mir::Statement::StoreLocal {
                local: target_local,
                value: updated,
                span: *span,
            });
        Some(
            if *position == UpdatePosition::Prefix
            {
                updated
            }
            else
            {
                old_value
            },
        )
    }

    fn emit_update_operation(
        &self,
        operator: UpdateOperator,
        value_type: XlilType,
        operands: UpdateOperands,
        lowered: &mut mir::Function,
    )
    {
        let UpdateOperands {
            result,
            value,
            one,
            span,
        } = operands;
        let block = self.current_block_mut(lowered);
        match (operator, value_type)
        {
            (UpdateOperator::Increment, XlilType::I32) => block.statements.push(mir::Statement::AddI32 {
                result,
                left: value,
                right: one,
                span,
            }),
            (UpdateOperator::Decrement, XlilType::I32) => block.statements.push(mir::Statement::SubI32 {
                result,
                left: value,
                right: one,
                span,
            }),
            (UpdateOperator::Increment, XlilType::I64) => block.statements.push(mir::Statement::AddI64 {
                result,
                left: value,
                right: one,
                span,
            }),
            (UpdateOperator::Decrement, XlilType::I64) => block.statements.push(mir::Statement::SubI64 {
                result,
                left: value,
                right: one,
                span,
            }),
            (operator, _) => block.statements.push(mir::Statement::BinaryInteger {
                operation: if operator == UpdateOperator::Increment
                {
                    crate::xlil::IntegerBinaryOperation::Add
                }
                else
                {
                    crate::xlil::IntegerBinaryOperation::Sub
                },
                value_type,
                result,
                left: value,
                right: one,
                span,
            }),
        }
    }
}
