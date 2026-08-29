// SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
// SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.0

impl<'a> Verifier<'a>
{
    fn verify_local_storage(&mut self, local: LocalId, value: LocalId, instruction: &str, span: Span)
    {
        self.verify_local(local, span);
        self.verify_local(value, span);
        let local_type = self.local_type(local);
        let value_type = self.local_type(value);
        match (local_type, value_type)
        {
            (Some(local_type), Some(value_type)) if local_type == value_type =>
            {}
            (Some(_), Some(_)) => self.report(
                DiagnosticCode::LocalTypeMismatch,
                format!("{instruction} local and value types must match"),
                span,
            ),
            _ => self.report(
                DiagnosticCode::MissingLocalType,
                format!("{instruction} requires typed locals"),
                span,
            ),
        }
    }

    fn local_type(&self, id: LocalId) -> Option<crate::xlil::Type>
    {
        self.function
            .parameters
            .iter()
            .find(|parameter| parameter.local == id)
            .map(|parameter| parameter.value_type)
            .or_else(|| {
                self.function
                    .locals
                    .iter()
                    .find(|local| local.id == id)
                    .and_then(|local| local.value_type)
            })
    }

    fn verify_const_i64(&mut self, local: LocalId, span: Span)
    {
        self.verify_local(local, span);
        let Some(local) = self.function.locals.iter().find(|candidate| candidate.id == local)
        else
        {
            return;
        };
        match local.value_type
        {
            Some(crate::xlil::Type::I64) =>
            {}
            Some(_) => self.report(
                DiagnosticCode::LocalTypeMismatch,
                "const.i64 target local must have XLIL i64 type".to_string(),
                span,
            ),
            None => self.report(
                DiagnosticCode::MissingLocalType,
                "const.i64 target local has no XLIL value type".to_string(),
                span,
            ),
        }
    }

    fn verify_const_bool(&mut self, local: LocalId, span: Span)
    {
        self.verify_local(local, span);
        let Some(local) = self.function.locals.iter().find(|candidate| candidate.id == local)
        else
        {
            return;
        };
        match local.value_type
        {
            Some(crate::xlil::Type::BOOL) =>
            {}
            Some(_) => self.report(
                DiagnosticCode::LocalTypeMismatch,
                "const.bool target local must have XLIL bool type".to_string(),
                span,
            ),
            None => self.report(
                DiagnosticCode::MissingLocalType,
                "const.bool target local has no XLIL value type".to_string(),
                span,
            ),
        }
    }

    fn verify_const_i32(&mut self, local: LocalId, span: Span)
    {
        self.verify_local(local, span);
        let Some(local) = self.function.locals.iter().find(|candidate| candidate.id == local)
        else
        {
            return;
        };
        match local.value_type
        {
            Some(crate::xlil::Type::I32) =>
            {}
            Some(_) => self.report(
                DiagnosticCode::LocalTypeMismatch,
                "const.i32 target local must have XLIL i32 type".to_string(),
                span,
            ),
            None => self.report(
                DiagnosticCode::MissingLocalType,
                "const.i32 target local has no XLIL value type".to_string(),
                span,
            ),
        }
    }

    fn verify_typed_const(&mut self, local: LocalId, expected: crate::xlil::Type, instruction: &str, span: Span)
    {
        self.verify_local(local, span);
        let Some(local) = self.function.locals.iter().find(|candidate| candidate.id == local)
        else
        {
            return;
        };
        match local.value_type
        {
            Some(actual) if actual == expected =>
            {}
            Some(_) => self.report(
                DiagnosticCode::LocalTypeMismatch,
                format!("{instruction} target local has the wrong XLIL type"),
                span,
            ),
            None => self.report(
                DiagnosticCode::MissingLocalType,
                format!("{instruction} target local has no XLIL value type"),
                span,
            ),
        }
    }

    fn verify_exact_local(&mut self, local: LocalId, expected: crate::xlil::Type, label: &str, span: Span)
    {
        self.verify_local(local, span);
        match self.local_type(local)
        {
            Some(actual) if actual == expected =>
            {}
            Some(_) => self.report(
                DiagnosticCode::LocalTypeMismatch,
                format!("{label} has the wrong XLIL type"),
                span,
            ),
            None => self.report(
                DiagnosticCode::MissingLocalType,
                format!("{label} has no XLIL value type"),
                span,
            ),
        }
    }

    fn verify_i64_binary(&mut self, result: LocalId, left: LocalId, right: LocalId, instruction: &str, span: Span)
    {
        self.verify_i64_local(result, &format!("{instruction} result local"), span);
        self.verify_i64_local(left, &format!("{instruction} left operand"), span);
        self.verify_i64_local(right, &format!("{instruction} right operand"), span);
    }

    fn verify_integer_binary(
        &mut self,
        operation: crate::xlil::IntegerBinaryOperation,
        value_type: crate::xlil::Type,
        result: LocalId,
        left: LocalId,
        right: LocalId,
        span: Span,
    )
    {
        let name = format!("{}.{}", operation.text_stem(), crate::xlil::type_name(value_type));
        if !value_type.is_integer()
        {
            self.report(
                DiagnosticCode::LocalTypeMismatch,
                format!("{name} declares a non-integer operation type"),
                span,
            );
            return;
        }
        let expected_result = if operation.is_comparison()
        {
            crate::xlil::Type::BOOL
        }
        else
        {
            value_type
        };
        for (local, expected, role) in [
            (result, expected_result, "result"),
            (left, value_type, "left operand"),
            (right, value_type, "right operand"),
        ]
        {
            self.verify_local(local, span);
            match self.local_type(local)
            {
                Some(actual) if actual == expected =>
                {}
                Some(_) => self.report(
                    DiagnosticCode::LocalTypeMismatch,
                    format!("{name} {role} has the wrong XLIL type"),
                    span,
                ),
                None => self.report(
                    DiagnosticCode::MissingLocalType,
                    format!("{name} {role} has no XLIL value type"),
                    span,
                ),
            }
        }
    }

    fn verify_float_binary(
        &mut self,
        result: LocalId,
        left: LocalId,
        right: LocalId,
        value_type: crate::xlil::Type,
        instruction: &str,
        span: Span,
    )
    {
        self.verify_float_local(result, value_type, &format!("{instruction} result local"), span);
        self.verify_float_local(left, value_type, &format!("{instruction} left operand"), span);
        self.verify_float_local(right, value_type, &format!("{instruction} right operand"), span);
    }

    fn verify_float_local(&mut self, local: LocalId, expected: crate::xlil::Type, label: &str, span: Span)
    {
        self.verify_local(local, span);
        if !matches!(expected, crate::xlil::Type::F32 | crate::xlil::Type::F64)
        {
            self.report(
                DiagnosticCode::LocalTypeMismatch,
                format!("{label} declares a non-floating operation type"),
                span,
            );
            return;
        }
        let Some(local) = self.function.locals.iter().find(|candidate| candidate.id == local)
        else
        {
            return;
        };
        match local.value_type
        {
            Some(actual) if actual == expected =>
            {}
            Some(_) => self.report(
                DiagnosticCode::LocalTypeMismatch,
                format!("{label} has the wrong XLIL floating type"),
                span,
            ),
            None => self.report(
                DiagnosticCode::MissingLocalType,
                format!("{label} has no XLIL value type"),
                span,
            ),
        }
    }

    fn verify_eq_i64(&mut self, result: LocalId, left: LocalId, right: LocalId, span: Span)
    {
        self.verify_bool_local(result, "eq.i64 result local", span);
        self.verify_i64_local(left, "eq.i64 left operand", span);
        self.verify_i64_local(right, "eq.i64 right operand", span);
    }

    fn verify_i64_comparison(&mut self, result: LocalId, left: LocalId, right: LocalId, instruction: &str, span: Span)
    {
        self.verify_bool_local(result, &format!("{instruction} result local"), span);
        self.verify_i64_local(left, &format!("{instruction} left operand"), span);
        self.verify_i64_local(right, &format!("{instruction} right operand"), span);
    }

    fn verify_i32_binary(&mut self, result: LocalId, left: LocalId, right: LocalId, instruction: &str, span: Span)
    {
        self.verify_i32_local(result, &format!("{instruction} result local"), span);
        self.verify_i32_local(left, &format!("{instruction} left operand"), span);
        self.verify_i32_local(right, &format!("{instruction} right operand"), span);
    }

    fn verify_i32_compare(&mut self, result: LocalId, left: LocalId, right: LocalId, instruction: &str, span: Span)
    {
        self.verify_bool_local(result, &format!("{instruction} result local"), span);
        self.verify_i32_local(left, &format!("{instruction} left operand"), span);
        self.verify_i32_local(right, &format!("{instruction} right operand"), span);
    }

    fn verify_bool_local(&mut self, local: LocalId, label: &str, span: Span)
    {
        self.verify_local(local, span);
        let Some(local) = self.function.locals.iter().find(|candidate| candidate.id == local)
        else
        {
            return;
        };
        match local.value_type
        {
            Some(crate::xlil::Type::BOOL) =>
            {}
            Some(_) => self.report(
                DiagnosticCode::LocalTypeMismatch,
                format!("{label} must have XLIL bool type"),
                span,
            ),
            None => self.report(
                DiagnosticCode::MissingLocalType,
                format!("{label} has no XLIL value type"),
                span,
            ),
        }
    }

    fn verify_i64_local(&mut self, local: LocalId, label: &str, span: Span)
    {
        self.verify_local(local, span);
        let Some(local) = self.function.locals.iter().find(|candidate| candidate.id == local)
        else
        {
            return;
        };
        match local.value_type
        {
            Some(crate::xlil::Type::I64) =>
            {}
            Some(_) => self.report(
                DiagnosticCode::LocalTypeMismatch,
                format!("{label} must have XLIL i64 type"),
                span,
            ),
            None => self.report(
                DiagnosticCode::MissingLocalType,
                format!("{label} has no XLIL value type"),
                span,
            ),
        }
    }

    fn verify_i32_local(&mut self, local: LocalId, label: &str, span: Span)
    {
        self.verify_local(local, span);
        let Some(local) = self.function.locals.iter().find(|candidate| candidate.id == local)
        else
        {
            return;
        };
        match local.value_type
        {
            Some(crate::xlil::Type::I32) =>
            {}
            Some(_) => self.report(
                DiagnosticCode::LocalTypeMismatch,
                format!("{label} must have XLIL i32 type"),
                span,
            ),
            None => self.report(
                DiagnosticCode::MissingLocalType,
                format!("{label} has no XLIL value type"),
                span,
            ),
        }
    }

    fn verify_return_value(&mut self, local: LocalId, span: Span)
    {
        self.verify_local(local, span);
        let Some(local) = self.function.locals.iter().find(|candidate| candidate.id == local)
        else
        {
            return;
        };
        match local.value_type
        {
            Some(value_type) if value_type == self.function.return_type =>
            {}
            Some(_) => self.report(
                DiagnosticCode::ReturnTypeMismatch,
                "MIR return local type does not match function return type".to_string(),
                span,
            ),
            None => self.report(
                DiagnosticCode::MissingLocalType,
                "MIR return local has no XLIL value type".to_string(),
                span,
            ),
        }
    }

    fn verify_call(
        &mut self,
        result: Option<LocalId>,
        arguments: &[LocalId],
        return_type: crate::xlil::Type,
        span: Span,
    )
    {
        for argument in arguments
        {
            self.verify_local(*argument, span);
        }
        match (result, return_type)
        {
            (Some(result), crate::xlil::Type::VOID) =>
            {
                self.verify_local(result, span);
                self.report(
                    DiagnosticCode::ReturnTypeMismatch,
                    "void MIR call cannot write a result local".to_string(),
                    span,
                );
            }
            (Some(result), return_type) =>
            {
                self.verify_local(result, span);
                let Some(local) = self.function.locals.iter().find(|candidate| candidate.id == result)
                else
                {
                    return;
                };
                match local.value_type
                {
                    Some(value_type) if value_type == return_type =>
                    {}
                    Some(_) => self.report(
                        DiagnosticCode::LocalTypeMismatch,
                        "MIR call result local type must match call return type".to_string(),
                        span,
                    ),
                    None => self.report(
                        DiagnosticCode::MissingLocalType,
                        "MIR call result local has no XLIL value type".to_string(),
                        span,
                    ),
                }
            }
            (None, return_type) if return_type != crate::xlil::Type::VOID =>
            {
                self.report(
                    DiagnosticCode::ReturnTypeMismatch,
                    "non-void MIR call must write a result local".to_string(),
                    span,
                );
            }
            (None, _) =>
            {}
        }
    }

    fn report(&mut self, code: DiagnosticCode, message: String, span: Span)
    {
        self.diagnostics.push(Diagnostic {
            code,
            message,
            span,
        });
    }
}

#[cfg(test)]
#[path = "tests.rs"]
mod tests;
