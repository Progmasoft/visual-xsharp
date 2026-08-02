/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 */

use super::*;

impl HirToMirLowerer
{
    pub(super) fn match_selector_value_type(&mut self, selector_type: &Type, span: Span) -> Option<XlilType>
    {
        match selector_type
        {
            Type::Primitive(PrimitiveType::Bool) => Some(XlilType::BOOL),
            Type::Primitive(PrimitiveType::Long) => Some(XlilType::I32),
            Type::Named(name) => self.aggregate_types.get(name).copied(),
            Type::Result {
                ..
            } => self
                .result_types
                .iter()
                .find(|(source, _)| source == selector_type)
                .map(|(_, value_type)| *value_type),
            _ =>
            {
                self.report(
                    DiagnosticCode::UnsupportedType,
                    format!("MIR match lowering does not support selector type {selector_type:?}"),
                    span,
                );
                None
            }
        }
    }

    pub(super) fn result_tag(
        &mut self,
        selector: mir::LocalId,
        selector_type: XlilType,
        span: Span,
        lowered: &mut mir::Function,
    ) -> Option<mir::LocalId>
    {
        self.result_layout(selector_type)?;
        let tag = self.declare_temp(XlilType::BOOL, span, lowered)?;
        self.current_block_mut(lowered)
            .statements
            .push(mir::Statement::Extract {
                result: tag,
                aggregate: selector,
                field: 0,
                field_type: XlilType::BOOL,
                span,
            });
        Some(tag)
    }

    pub(super) fn bind_result_pattern(
        &mut self,
        selector: mir::LocalId,
        pattern: &MatchPattern,
        span: Span,
        lowered: &mut mir::Function,
    ) -> Option<()>
    {
        let MatchPattern::ResultVariant {
            success,
            binding: Some(binding),
            payload_type,
        } = pattern
        else
        {
            return Some(());
        };
        let selector_type = self.local_value_type(selector, lowered)?;
        let (success_type, error_type) = self.result_layout(selector_type)?;
        let payload_value_type = if *success
        {
            success_type
        }
        else
        {
            error_type
        };
        if self.known_value_type(payload_type) != Some(payload_value_type)
        {
            self.report(
                DiagnosticCode::UnsupportedType,
                "Result match binding type differs from the selected payload layout",
                span,
            );
            return None;
        }
        let value = self.declare_temp(payload_value_type, span, lowered)?;
        self.current_block_mut(lowered)
            .statements
            .push(mir::Statement::Extract {
                result: value,
                aggregate: selector,
                field: if *success
                {
                    1
                }
                else
                {
                    2
                },
                field_type: payload_value_type,
                span,
            });
        self.locals.insert(binding.clone(), value);
        Some(())
    }
}

pub(super) fn result_match_is_exhaustive(selector_type: &Type, arms: &[MatchArm]) -> bool
{
    matches!(selector_type, Type::Result { .. }) &&
        arms.iter()
            .filter_map(|arm| match arm.pattern
            {
                MatchPattern::ResultVariant {
                    success, ..
                } => Some(success),
                _ => None,
            })
            .collect::<HashSet<_>>()
            .len() ==
            2
}
