/*
 * SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
 * SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.0
 */

use super::*;

impl HirToMirLowerer
{
    pub(super) fn lower_call_statement(&mut self, expression: &Expression, lowered: &mut mir::Function)
    {
        let Expression::Call {
            return_type,
            span,
            ..
        } = expression
        else
        {
            return;
        };
        if matches!(return_type.as_ref(), Type::Unit)
        {
            self.lower_void_call(expression, lowered);
            return;
        }
        let Some(return_type) = self.lower_value_type(return_type, *span)
        else
        {
            return;
        };
        let Some(target) = self.declare_temp(return_type, *span, lowered)
        else
        {
            return;
        };
        self.lower_call_into(target, expression, lowered);
    }

    fn lower_void_call(&mut self, expression: &Expression, lowered: &mut mir::Function)
    {
        let Expression::Call {
            function,
            arguments,
            parameter_types,
            return_type,
            span,
        } = expression
        else
        {
            return;
        };
        if !matches!(return_type.as_ref(), Type::Unit)
        {
            self.report(
                DiagnosticCode::UnsupportedType,
                "void call statement must have unit HIR return type",
                *span,
            );
            return;
        }
        let Some(arguments) = self.lower_call_arguments(arguments, parameter_types, *span, lowered)
        else
        {
            return;
        };
        self.current_block_mut(lowered).statements.push(mir::Statement::Call {
            result: None,
            function: function.clone(),
            arguments,
            return_type: crate::xlil::Type::VOID,
            span: *span,
        });
    }

    pub(super) fn lower_call_into(&mut self, target: mir::LocalId, expression: &Expression, lowered: &mut mir::Function)
    {
        let Expression::Call {
            function,
            arguments,
            parameter_types,
            return_type,
            span,
        } = expression
        else
        {
            return;
        };
        if function == "Some" && arguments.len() == 1 && parameter_types.len() == 1
        {
            self.lower_optional_some(target, &arguments[0], &parameter_types[0], return_type, *span, lowered);
            return;
        }
        if matches!(function.as_str(), "Ok" | "Error") && arguments.len() == 1 && parameter_types.len() == 1
        {
            self.lower_result_constructor(target, function == "Ok", &arguments[0], return_type, *span, lowered);
            return;
        }
        let Some(return_type) = self.lower_value_type(return_type, *span)
        else
        {
            return;
        };
        if self.local_value_type(target, lowered) != Some(return_type)
        {
            self.report(
                DiagnosticCode::UnsupportedType,
                "typed HIR call result type does not match its target local",
                *span,
            );
            return;
        }
        let Some(lowered_arguments) = self.lower_call_arguments(arguments, parameter_types, *span, lowered)
        else
        {
            return;
        };
        self.current_block_mut(lowered).statements.push(mir::Statement::Call {
            result: Some(target),
            function: function.clone(),
            arguments: lowered_arguments,
            return_type,
            span: *span,
        });
    }

    fn lower_optional_some(
        &mut self,
        target: mir::LocalId,
        argument: &Expression,
        element: &Type,
        return_type: &Type,
        span: Span,
        lowered: &mut mir::Function,
    )
    {
        if !return_type.is_optional()
        {
            self.report(DiagnosticCode::UnsupportedType, "Some must return Optional<T>", span);
            return;
        }
        let Some(optional_type) = self.lower_value_type(return_type, span)
        else
        {
            return;
        };
        let Some(element_type) = self.lower_value_type(element, span)
        else
        {
            return;
        };
        if self.local_value_type(target, lowered) != Some(optional_type)
        {
            self.report(
                DiagnosticCode::UnsupportedType,
                "Optional<T> result does not match its target local",
                span,
            );
            return;
        }
        let Some(value) = self.lower_expression_to_local(argument, element_type, lowered)
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
                value: true,
                span,
            });
        self.current_block_mut(lowered)
            .statements
            .push(mir::Statement::Aggregate {
                result: target,
                value_type: optional_type,
                fields: vec![tag, value],
                field_types: vec![XlilType::BOOL, element_type],
                span,
            });
    }

    fn lower_call_arguments(
        &mut self,
        arguments: &[Expression],
        parameter_types: &[Type],
        span: Span,
        lowered: &mut mir::Function,
    ) -> Option<Vec<mir::LocalId>>
    {
        if arguments.len() != parameter_types.len()
        {
            self.report(
                DiagnosticCode::UnsupportedExpression,
                "typed HIR call argument count does not match parameter types",
                span,
            );
            return None;
        }
        let mut lowered_arguments = Vec::with_capacity(arguments.len());
        for (argument, parameter_type) in arguments.iter().zip(parameter_types)
        {
            if let Type::Named(type_name) = parameter_type
            {
                lowered_arguments.extend(self.lower_nominal_argument(argument, type_name, lowered)?);
                continue;
            }
            let parameter_type = self.lower_value_type(parameter_type, span)?;
            lowered_arguments.push(self.lower_expression_to_local(argument, parameter_type, lowered)?);
        }
        Some(lowered_arguments)
    }
}
