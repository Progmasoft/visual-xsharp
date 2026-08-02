/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 */

use super::*;

impl HirToMirLowerer
{
    pub(super) fn lower_array_length(
        &mut self,
        collection: &Expression,
        expected_type: XlilType,
        span: Span,
        lowered: &mut mir::Function,
    ) -> Option<mir::LocalId>
    {
        if expected_type != XlilType::I64
        {
            self.report(
                DiagnosticCode::UnsupportedType,
                "array length result must have type Int",
                span,
            );
            return None;
        }
        let array_type = self.expression_value_type(collection, lowered)?;
        if !self
            .array_layouts
            .iter()
            .any(|(value_type, _, _)| *value_type == array_type)
        {
            self.report(
                DiagnosticCode::UnsupportedType,
                "array length source has no MIR layout",
                span,
            );
            return None;
        }
        let array = self.lower_expression_to_local(collection, array_type, lowered)?;
        let result = self.declare_temp(XlilType::I64, span, lowered)?;
        self.current_block_mut(lowered)
            .statements
            .push(mir::Statement::ArrayLength {
                result,
                array,
                array_type,
                span,
            });
        Some(result)
    }

    pub(super) fn lower_index_assignment(
        &mut self,
        target: &str,
        index: &Expression,
        value: &Expression,
        element_type: &Type,
        span: Span,
        lowered: &mut mir::Function,
    )
    {
        let Some(local) = self.locals.get(target).copied()
        else
        {
            self.report(
                DiagnosticCode::UnknownLocal,
                format!("unknown array local '{target}'"),
                span,
            );
            return;
        };
        let Some(array_type) = self.local_value_type(local, lowered)
        else
        {
            self.report(
                DiagnosticCode::UnsupportedType,
                "array assignment target has no MIR value type",
                span,
            );
            return;
        };
        let Some((_, lowered_element_type, length)) = self
            .array_layouts
            .iter()
            .find(|(value_type, _, _)| *value_type == array_type)
            .copied()
        else
        {
            self.report(
                DiagnosticCode::UnsupportedType,
                "array assignment target has no fixed layout",
                span,
            );
            return;
        };
        if self.lower_value_type(element_type, span) != Some(lowered_element_type)
        {
            self.report(
                DiagnosticCode::UnsupportedType,
                "array assignment element type does not match its layout",
                span,
            );
            return;
        }
        let loaded = match self.declare_temp(array_type, span, lowered)
        {
            Some(value) => value,
            None => return,
        };
        self.current_block_mut(lowered)
            .statements
            .push(mir::Statement::LoadLocal {
                result: loaded,
                local,
                span,
            });
        if length.is_none() ||
            !matches!(index, Expression::Literal {
                literal: Literal::Integer(_),
                ..
            })
        {
            let Some(index) = self.lower_expression_to_local(index, XlilType::I64, lowered)
            else
            {
                return;
            };
            let Some(value) = self.lower_expression_to_local(value, lowered_element_type, lowered)
            else
            {
                return;
            };
            let Some(updated) = self.declare_temp(array_type, span, lowered)
            else
            {
                return;
            };
            self.current_block_mut(lowered)
                .statements
                .push(mir::Statement::ArraySet {
                    result: updated,
                    array: loaded,
                    index,
                    value,
                    array_type,
                    element_type: lowered_element_type,
                    span,
                });
            self.current_block_mut(lowered)
                .statements
                .push(mir::Statement::StoreLocal {
                    local,
                    value: updated,
                    span,
                });
            return;
        }
        let Some(length) = length
        else
        {
            return;
        };
        let Some(index) = literal_index(index, length, span, self)
        else
        {
            return;
        };
        let mut fields = Vec::with_capacity(usize::try_from(length).unwrap_or(0));
        for field in 0..length
        {
            if field == u64::from(index)
            {
                let Some(replacement) = self.lower_expression_to_local(value, lowered_element_type, lowered)
                else
                {
                    return;
                };
                fields.push(replacement);
                continue;
            }
            let Some(result) = self.declare_temp(lowered_element_type, span, lowered)
            else
            {
                return;
            };
            self.current_block_mut(lowered)
                .statements
                .push(mir::Statement::Extract {
                    result,
                    aggregate: loaded,
                    field: u32::try_from(field).unwrap_or(u32::MAX),
                    field_type: lowered_element_type,
                    span,
                });
            fields.push(result);
        }
        let Some(updated) = self.declare_temp(array_type, span, lowered)
        else
        {
            return;
        };
        self.current_block_mut(lowered)
            .statements
            .push(mir::Statement::Aggregate {
                result: updated,
                value_type: array_type,
                field_types: vec![lowered_element_type; fields.len()],
                fields,
                span,
            });
        self.current_block_mut(lowered)
            .statements
            .push(mir::Statement::StoreLocal {
                local,
                value: updated,
                span,
            });
    }

    pub(super) fn lower_array_expression(
        &mut self,
        expression: &Expression,
        expected_type: XlilType,
        lowered: &mut mir::Function,
    ) -> Option<mir::LocalId>
    {
        let Expression::Array {
            elements,
            span,
        } = expression
        else
        {
            return None;
        };
        let Some((_, element_type, length)) = self
            .array_layouts
            .iter()
            .find(|(value_type, _, _)| *value_type == expected_type)
            .copied()
        else
        {
            self.report(
                DiagnosticCode::UnsupportedType,
                "array literal target has no fixed-array MIR layout",
                *span,
            );
            return None;
        };
        if length.is_some_and(|length| u64::try_from(elements.len()).ok() != Some(length))
        {
            self.report(
                DiagnosticCode::UnsupportedExpression,
                "array literal length does not match its fixed-array type",
                *span,
            );
            return None;
        }
        let fields = elements
            .iter()
            .map(|element| self.lower_expression_to_local(element, element_type, lowered))
            .collect::<Option<Vec<_>>>()?;
        let result = self.declare_temp(expected_type, *span, lowered)?;
        self.current_block_mut(lowered)
            .statements
            .push(mir::Statement::Aggregate {
                result,
                value_type: expected_type,
                field_types: vec![element_type; fields.len()],
                fields,
                span: *span,
            });
        Some(result)
    }

    pub(super) fn lower_index_expression(
        &mut self,
        expression: &Expression,
        expected_type: XlilType,
        lowered: &mut mir::Function,
    ) -> Option<mir::LocalId>
    {
        let Expression::Index {
            collection,
            index,
            span,
            ..
        } = expression
        else
        {
            return None;
        };
        let collection_type = self.expression_value_type(collection, lowered)?;
        let Some((_, element_type, length)) = self
            .array_layouts
            .iter()
            .find(|(value_type, _, _)| *value_type == collection_type)
            .copied()
        else
        {
            self.report(
                DiagnosticCode::UnsupportedType,
                "index source has no fixed-array MIR layout",
                *span,
            );
            return None;
        };
        if element_type != expected_type
        {
            self.report(
                DiagnosticCode::UnsupportedType,
                "index result type does not match its fixed-array element type",
                *span,
            );
            return None;
        }
        let array = self.lower_expression_to_local(collection, collection_type, lowered)?;
        let result = self.declare_temp(element_type, *span, lowered)?;
        if let Some(length) = length &&
            matches!(index.as_ref(), Expression::Literal {
                literal: Literal::Integer(_),
                ..
            })
        {
            let index = literal_index(index, length, *span, self)?;
            self.current_block_mut(lowered)
                .statements
                .push(mir::Statement::Extract {
                    result,
                    aggregate: array,
                    field: index,
                    field_type: element_type,
                    span: *span,
                });
        }
        else
        {
            let index = self.lower_expression_to_local(index, XlilType::I64, lowered)?;
            self.current_block_mut(lowered)
                .statements
                .push(mir::Statement::ArrayGet {
                    result,
                    array,
                    index,
                    array_type: collection_type,
                    element_type,
                    span: *span,
                });
        }
        Some(result)
    }
}

fn literal_index(expression: &Expression, length: u64, span: Span, lowerer: &mut HirToMirLowerer) -> Option<u32>
{
    let Expression::Literal {
        literal: Literal::Integer(index),
        ..
    } = expression
    else
    {
        lowerer.report(
            DiagnosticCode::UnsupportedExpression,
            "fixed-array MIR indexing currently requires an integer literal",
            span,
        );
        return None;
    };
    let Ok(index) = index.replace('\'', "").parse::<u32>()
    else
    {
        lowerer.report(
            DiagnosticCode::InvalidIntegerLiteral,
            "array index is not a valid u32 literal",
            span,
        );
        return None;
    };
    if u64::from(index) >= length
    {
        lowerer.report(
            DiagnosticCode::UnsupportedExpression,
            "fixed-array index is out of bounds",
            span,
        );
        return None;
    }
    Some(index)
}
