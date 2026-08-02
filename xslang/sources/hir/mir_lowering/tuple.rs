/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 */

use super::*;

impl HirToMirLowerer
{
    pub(super) fn lower_tuple_assignment(&mut self, statement: &Statement, lowered: &mut mir::Function)
    {
        let Statement::AssignTupleElement {
            target,
            index,
            value,
            tuple_type,
            element_type,
            span,
        } = statement
        else
        {
            return;
        };
        let Some(storage) = self.locals.get(target).copied()
        else
        {
            self.report(
                DiagnosticCode::UnknownLocal,
                format!("unknown tuple local '{target}'"),
                *span,
            );
            return;
        };
        let Some(value_type) = self.lower_value_type(tuple_type, *span)
        else
        {
            return;
        };
        if self.local_value_type(storage, lowered) != Some(value_type)
        {
            self.report(
                DiagnosticCode::UnsupportedType,
                "tuple assignment target has the wrong MIR type",
                *span,
            );
            return;
        }
        let Type::Tuple {
            fields: source_fields,
        } = tuple_type
        else
        {
            return;
        };
        if source_fields.get(*index as usize).map(|field| &field.ty) != Some(element_type)
        {
            self.report(
                DiagnosticCode::UnsupportedType,
                "tuple assignment field type does not match its layout",
                *span,
            );
            return;
        }
        let Some(loaded) = self.declare_temp(value_type, *span, lowered)
        else
        {
            return;
        };
        self.current_block_mut(lowered)
            .statements
            .push(mir::Statement::LoadLocal {
                result: loaded,
                local: storage,
                span: *span,
            });
        let mut fields = Vec::with_capacity(source_fields.len());
        let mut field_types = Vec::with_capacity(source_fields.len());
        for (position, field) in source_fields.iter().enumerate()
        {
            let Some(field_type) = self.lower_value_type(&field.ty, *span)
            else
            {
                return;
            };
            let field_value = if position == *index as usize
            {
                match self.lower_expression_to_local(value, field_type, lowered)
                {
                    Some(value) => value,
                    None => return,
                }
            }
            else
            {
                let Some(extracted) = self.declare_temp(field_type, *span, lowered)
                else
                {
                    return;
                };
                self.current_block_mut(lowered)
                    .statements
                    .push(mir::Statement::Extract {
                        result: extracted,
                        aggregate: loaded,
                        field: position as u32,
                        field_type,
                        span: *span,
                    });
                extracted
            };
            fields.push(field_value);
            field_types.push(field_type);
        }
        let Some(updated) = self.declare_temp(value_type, *span, lowered)
        else
        {
            return;
        };
        self.current_block_mut(lowered)
            .statements
            .push(mir::Statement::Aggregate {
                result: updated,
                value_type,
                fields,
                field_types,
                span: *span,
            });
        self.current_block_mut(lowered)
            .statements
            .push(mir::Statement::StoreLocal {
                local: storage,
                value: updated,
                span: *span,
            });
    }

    pub(super) fn lower_tuple_expression(
        &mut self,
        expression: &Expression,
        expected_type: XlilType,
        lowered: &mut mir::Function,
    ) -> Option<mir::LocalId>
    {
        let Expression::Tuple {
            fields,
            tuple_type,
            span,
        } = expression
        else
        {
            return None;
        };
        if self.lower_value_type(tuple_type, *span)? != expected_type
        {
            self.report(
                DiagnosticCode::UnsupportedType,
                "tuple literal type does not match its MIR target type",
                *span,
            );
            return None;
        }
        let Type::Tuple {
            fields: field_types,
        } = tuple_type.as_ref()
        else
        {
            return None;
        };
        if fields.len() != field_types.len()
        {
            self.report(
                DiagnosticCode::UnsupportedExpression,
                "tuple literal field count does not match its HIR type",
                *span,
            );
            return None;
        }
        let mut values = Vec::with_capacity(fields.len());
        let mut lowered_types = Vec::with_capacity(fields.len());
        for (field, field_type) in fields.iter().zip(field_types)
        {
            if field.name != field_type.name
            {
                self.report(
                    DiagnosticCode::UnsupportedExpression,
                    "tuple literal field name does not match its HIR type",
                    field.span,
                );
                return None;
            }
            let value_type = self.lower_value_type(&field_type.ty, field.span)?;
            values.push(self.lower_expression_to_local(&field.value, value_type, lowered)?);
            lowered_types.push(value_type);
        }
        let result = self.declare_temp(expected_type, *span, lowered)?;
        self.current_block_mut(lowered)
            .statements
            .push(mir::Statement::Aggregate {
                result,
                value_type: expected_type,
                fields: values,
                field_types: lowered_types,
                span: *span,
            });
        Some(result)
    }

    pub(super) fn lower_tuple_element(
        &mut self,
        expression: &Expression,
        expected_type: XlilType,
        lowered: &mut mir::Function,
    ) -> Option<mir::LocalId>
    {
        let Expression::TupleElement {
            tuple,
            index,
            element_type,
            span,
        } = expression
        else
        {
            return None;
        };
        if self.lower_value_type(element_type, *span)? != expected_type
        {
            self.report(
                DiagnosticCode::UnsupportedType,
                "tuple element type does not match its MIR target type",
                *span,
            );
            return None;
        }
        let tuple_type = self.expression_value_type(tuple, lowered)?;
        let tuple_value = self.lower_expression_to_local(tuple, tuple_type, lowered)?;
        let result = self.declare_temp(expected_type, *span, lowered)?;
        self.current_block_mut(lowered)
            .statements
            .push(mir::Statement::Extract {
                result,
                aggregate: tuple_value,
                field: *index,
                field_type: expected_type,
                span: *span,
            });
        Some(result)
    }
}
