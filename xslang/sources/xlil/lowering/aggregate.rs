/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 */

use super::{BlockId, DiagnosticCode, Function, HashMap, MirToXlilLowerer, ValueId, mir};

impl MirToXlilLowerer
{
    pub(super) fn lower_aggregate_statement(
        &mut self,
        statement: &mir::Statement,
        block: BlockId,
        values: &mut HashMap<mir::LocalId, ValueId>,
        function: &mut Function,
    )
    {
        match statement
        {
            mir::Statement::Aggregate {
                result,
                value_type,
                fields,
                span,
                ..
            } =>
            {
                let Some(fields) = fields
                    .iter()
                    .map(|field| values.get(field).copied())
                    .collect::<Option<Vec<_>>>()
                else
                {
                    self.report(
                        DiagnosticCode::MissingLocalValue,
                        "MIR aggregate field has not been lowered",
                        *span,
                    );
                    return;
                };
                let value = if value_type.kind == crate::xlil::TypeKind::Array
                {
                    function.add_array(block, *value_type, fields)
                }
                else
                {
                    function.add_aggregate(block, *value_type, fields)
                };
                let Some(value) = value
                else
                {
                    self.report(
                        DiagnosticCode::UnsupportedLocalType,
                        "MIR aggregate could not lower to XLIL",
                        *span,
                    );
                    return;
                };
                values.insert(*result, value);
            }
            mir::Statement::Extract {
                result,
                aggregate,
                field,
                field_type,
                span,
            } =>
            {
                let Some(aggregate) = values.get(aggregate).copied()
                else
                {
                    self.report(
                        DiagnosticCode::MissingLocalValue,
                        "MIR extract source has not been lowered",
                        *span,
                    );
                    return;
                };
                let Some(value) = function.add_extract(block, aggregate, *field, *field_type)
                else
                {
                    self.report(
                        DiagnosticCode::UnsupportedLocalType,
                        "MIR extract could not lower to XLIL",
                        *span,
                    );
                    return;
                };
                values.insert(*result, value);
            }
            mir::Statement::ArrayGet {
                result,
                array,
                index,
                element_type,
                span,
                ..
            } =>
            {
                let Some((array, index)) = values.get(array).copied().zip(values.get(index).copied())
                else
                {
                    self.report(
                        DiagnosticCode::MissingLocalValue,
                        "MIR array.get operand has not been lowered",
                        *span,
                    );
                    return;
                };
                let Some(value) = function.add_array_get(block, array, index, *element_type)
                else
                {
                    self.report(
                        DiagnosticCode::UnsupportedLocalType,
                        "MIR array.get could not lower to XLIL",
                        *span,
                    );
                    return;
                };
                values.insert(*result, value);
            }
            mir::Statement::ArraySet {
                result,
                array,
                index,
                value,
                span,
                ..
            } =>
            {
                let Some((array, index, value)) = values
                    .get(array)
                    .copied()
                    .zip(values.get(index).copied())
                    .zip(values.get(value).copied())
                    .map(|((array, index), value)| (array, index, value))
                else
                {
                    self.report(
                        DiagnosticCode::MissingLocalValue,
                        "MIR array.set operand has not been lowered",
                        *span,
                    );
                    return;
                };
                let Some(value) = function.add_array_set(block, array, index, value)
                else
                {
                    self.report(
                        DiagnosticCode::UnsupportedLocalType,
                        "MIR array.set could not lower to XLIL",
                        *span,
                    );
                    return;
                };
                values.insert(*result, value);
            }
            mir::Statement::ArrayLength {
                result,
                array,
                span,
                ..
            } =>
            {
                let Some(array) = values.get(array).copied()
                else
                {
                    self.report(
                        DiagnosticCode::MissingLocalValue,
                        "MIR array.length source has not been lowered",
                        *span,
                    );
                    return;
                };
                let Some(value) = function.add_array_length(block, array)
                else
                {
                    self.report(
                        DiagnosticCode::UnsupportedLocalType,
                        "MIR array.length could not lower to XLIL",
                        *span,
                    );
                    return;
                };
                values.insert(*result, value);
            }
            _ =>
            {}
        }
    }
}
