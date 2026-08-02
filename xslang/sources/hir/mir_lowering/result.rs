/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 */

use super::*;

impl HirToMirLowerer
{
    pub(super) fn lower_result_constructor(
        &mut self,
        target: mir::LocalId,
        success_variant: bool,
        payload: &Expression,
        result_source_type: &Type,
        span: Span,
        lowered: &mut mir::Function,
    )
    {
        let Some(result_type) = self.lower_value_type(result_source_type, span)
        else
        {
            return;
        };
        let Some((success_type, error_type)) = self.result_layout(result_type)
        else
        {
            self.report(
                DiagnosticCode::UnsupportedType,
                "Result constructor target has no registered MIR aggregate layout",
                span,
            );
            return;
        };
        if self.local_value_type(target, lowered) != Some(result_type)
        {
            self.report(
                DiagnosticCode::UnsupportedType,
                "Result constructor target local has the wrong MIR type",
                span,
            );
            return;
        }
        let expected_payload_type = if success_variant
        {
            success_type
        }
        else
        {
            error_type
        };
        let Some(payload) = self.lower_expression_to_local(payload, expected_payload_type, lowered)
        else
        {
            return;
        };
        let Some(tag) = self.declare_temp(XlilType::BOOL, span, lowered)
        else
        {
            return;
        };
        self.current_block_mut(lowered)
            .statements
            .push(mir::Statement::ConstBool {
                local: tag,
                value: success_variant,
                span,
            });
        let Some(inactive) = self.canonical_result_payload(
            if success_variant
            {
                error_type
            }
            else
            {
                success_type
            },
            span,
            lowered,
        )
        else
        {
            self.report(
                DiagnosticCode::UnsupportedType,
                "inactive Result variant has no canonical MIR value",
                span,
            );
            return;
        };
        let fields = if success_variant
        {
            vec![tag, payload, inactive]
        }
        else
        {
            vec![tag, inactive, payload]
        };
        self.current_block_mut(lowered)
            .statements
            .push(mir::Statement::Aggregate {
                result: target,
                value_type: result_type,
                fields,
                field_types: vec![XlilType::BOOL, success_type, error_type],
                span,
            });
    }

    pub(super) fn lower_result_propagation(
        &mut self,
        expression: &Expression,
        expected_success_type: XlilType,
        lowered: &mut mir::Function,
    ) -> Option<mir::LocalId>
    {
        let Expression::ResultPropagation {
            value,
            span,
        } = expression
        else
        {
            return None;
        };
        let input_type = self.expression_value_type(value, lowered)?;
        let Some((input_success_type, input_error_type)) = self.result_layout(input_type)
        else
        {
            self.report(
                DiagnosticCode::UnsupportedType,
                "Result propagation operand has no registered MIR Result layout",
                *span,
            );
            return None;
        };
        if input_success_type != expected_success_type
        {
            self.report(
                DiagnosticCode::UnsupportedType,
                "Result propagation success payload does not match its MIR expression context",
                *span,
            );
            return None;
        }
        let output_source_type = self.function_return_type.clone()?;
        let output_type = self.lower_value_type(&output_source_type, *span)?;
        let Some((output_success_type, output_error_type)) = self.result_layout(output_type)
        else
        {
            self.report(
                DiagnosticCode::UnsupportedType,
                "Result propagation requires an enclosing Result-returning function",
                *span,
            );
            return None;
        };
        if input_error_type != output_error_type
        {
            self.report(
                DiagnosticCode::UnsupportedType,
                "Result propagation error payload differs from the enclosing function",
                *span,
            );
            return None;
        }

        let input = self.lower_expression_to_local(value, input_type, lowered)?;
        let tag = self.extract_result_field(input, 0, XlilType::BOOL, *span, lowered)?;
        let branch = self.current_block;
        let success = self.append_block(*span, lowered);
        let failure = self.append_block(*span, lowered);
        self.switch_to(branch);
        self.set_terminator(
            mir::Terminator::BranchIf {
                condition: tag,
                then_block: success,
                else_block: failure,
            },
            *span,
            lowered,
        );

        self.switch_to(failure);
        let error = self.extract_result_field(input, 2, input_error_type, *span, lowered)?;
        let false_tag = self.declare_temp(XlilType::BOOL, *span, lowered)?;
        self.current_block_mut(lowered)
            .statements
            .push(mir::Statement::ConstBool {
                local: false_tag,
                value: false,
                span: *span,
            });
        let default_success = self.canonical_result_payload(output_success_type, *span, lowered)?;
        let propagated = self.declare_temp(output_type, *span, lowered)?;
        self.current_block_mut(lowered)
            .statements
            .push(mir::Statement::Aggregate {
                result: propagated,
                value_type: output_type,
                fields: vec![false_tag, default_success, error],
                field_types: vec![XlilType::BOOL, output_success_type, output_error_type],
                span: *span,
            });
        self.set_terminator(mir::Terminator::Return(Some(propagated)), *span, lowered);

        self.switch_to(success);
        self.extract_result_field(input, 1, input_success_type, *span, lowered)
    }

    pub(super) fn result_layout(&self, result_type: XlilType) -> Option<(XlilType, XlilType)>
    {
        self.result_layouts
            .iter()
            .find(|(value_type, _, _)| *value_type == result_type)
            .map(|(_, success, error)| (*success, *error))
    }

    fn canonical_result_payload(
        &mut self,
        value_type: XlilType,
        span: Span,
        lowered: &mut mir::Function,
    ) -> Option<mir::LocalId>
    {
        let value = self.declare_temp(value_type, span, lowered)?;
        self.lower_default_value(value, value_type, span, lowered, &mut HashSet::new())
            .then_some(value)
    }

    fn extract_result_field(
        &mut self,
        aggregate: mir::LocalId,
        field: u32,
        field_type: XlilType,
        span: Span,
        lowered: &mut mir::Function,
    ) -> Option<mir::LocalId>
    {
        let result = self.declare_temp(field_type, span, lowered)?;
        self.current_block_mut(lowered)
            .statements
            .push(mir::Statement::Extract {
                result,
                aggregate,
                field,
                field_type,
                span,
            });
        Some(result)
    }
}
