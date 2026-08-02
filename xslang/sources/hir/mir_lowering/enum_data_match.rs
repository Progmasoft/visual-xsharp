/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 */

use super::*;

impl HirToMirLowerer
{
    pub(super) fn enum_data_match_is_exhaustive(&self, selector_type: &Type, arms: &[MatchArm]) -> bool
    {
        let Type::Named(enum_type) = selector_type
        else
        {
            return false;
        };
        let Some(layout) = self.enum_data_layouts.iter().find(|layout| layout.name == *enum_type)
        else
        {
            return false;
        };
        let tags = arms
            .iter()
            .filter_map(|arm| match &arm.pattern
            {
                MatchPattern::EnumDataVariant {
                    enum_type: pattern_type,
                    tag,
                    ..
                } if pattern_type == enum_type => Some(*tag),
                _ => None,
            })
            .collect::<HashSet<_>>();
        layout.variants.iter().all(|variant| tags.contains(&variant.tag))
    }

    pub(super) fn enum_data_pattern_test(
        &mut self,
        selector: mir::LocalId,
        selector_type: XlilType,
        pattern: &MatchPattern,
        span: Span,
        lowered: &mut mir::Function,
    ) -> Option<mir::LocalId>
    {
        let variant = self.enum_data_pattern_layout(selector_type, pattern, span)?;
        let tag = self.enum_data_tag(selector, selector_type, span, lowered)?;
        let expected = self.declare_temp(XlilType::I32, span, lowered)?;
        let value = i32::try_from(variant.tag).ok().or_else(|| {
            self.report(
                DiagnosticCode::UnsupportedExpression,
                "enum data tag exceeds the XLIL i32 representation",
                span,
            );
            None
        })?;
        self.current_block_mut(lowered)
            .statements
            .push(mir::Statement::ConstI32 {
                local: expected,
                value,
                span,
            });
        let condition = self.declare_temp(XlilType::BOOL, span, lowered)?;
        self.current_block_mut(lowered).statements.push(mir::Statement::EqI32 {
            result: condition,
            left: tag,
            right: expected,
            span,
        });
        Some(condition)
    }

    pub(super) fn bind_enum_data_pattern(
        &mut self,
        selector: mir::LocalId,
        selector_type: XlilType,
        pattern: &MatchPattern,
        span: Span,
        lowered: &mut mir::Function,
    ) -> Option<()>
    {
        let MatchPattern::EnumDataVariant {
            binding,
            payload_type,
            ..
        } = pattern
        else
        {
            return None;
        };
        let variant = self.enum_data_pattern_layout(selector_type, pattern, span)?;
        match (binding, payload_type, variant.field, variant.payload_type)
        {
            (None, _, _, _) => Some(()),
            (Some(_), None, _, _) | (Some(_), Some(_), None, _) | (Some(_), Some(_), Some(_), None) =>
            {
                self.report(
                    DiagnosticCode::UnsupportedType,
                    "enum data binding does not have a concrete payload layout",
                    span,
                );
                None
            }
            (Some(binding), Some(payload_type), Some(field), Some(field_type)) =>
            {
                if self.known_value_type(payload_type) != Some(field_type)
                {
                    self.report(
                        DiagnosticCode::UnsupportedType,
                        "enum data match binding type differs from its payload slot",
                        span,
                    );
                    return None;
                }
                let value = self.declare_temp(field_type, span, lowered)?;
                self.current_block_mut(lowered)
                    .statements
                    .push(mir::Statement::Extract {
                        result: value,
                        aggregate: selector,
                        field,
                        field_type,
                        span,
                    });
                self.locals.insert(binding.clone(), value);
                Some(())
            }
        }
    }

    fn enum_data_tag(
        &mut self,
        selector: mir::LocalId,
        selector_type: XlilType,
        span: Span,
        lowered: &mut mir::Function,
    ) -> Option<mir::LocalId>
    {
        let fields = self.aggregate_layouts.get(&selector_type)?;
        if fields.first() != Some(&XlilType::I32)
        {
            self.report(
                DiagnosticCode::UnsupportedType,
                "enum data aggregate does not start with an i32 tag",
                span,
            );
            return None;
        }
        let tag = self.declare_temp(XlilType::I32, span, lowered)?;
        self.current_block_mut(lowered)
            .statements
            .push(mir::Statement::Extract {
                result: tag,
                aggregate: selector,
                field: 0,
                field_type: XlilType::I32,
                span,
            });
        Some(tag)
    }

    fn enum_data_pattern_layout(
        &mut self,
        selector_type: XlilType,
        pattern: &MatchPattern,
        span: Span,
    ) -> Option<crate::hir::aggregate_registry::EnumDataVariantLayout>
    {
        let MatchPattern::EnumDataVariant {
            enum_type,
            owner,
            variant,
            tag,
            payload_type,
            ..
        } = pattern
        else
        {
            return None;
        };
        let Some(layout) = self
            .enum_data_layouts
            .iter()
            .find(|layout| layout.name == *enum_type && layout.value_type == selector_type)
        else
        {
            self.report(
                DiagnosticCode::UnsupportedType,
                format!("enum data match selector '{enum_type}' has no aggregate layout"),
                span,
            );
            return None;
        };
        let Some(selected) = layout.variant(owner, variant, *tag).cloned()
        else
        {
            self.report(
                DiagnosticCode::UnsupportedExpression,
                format!("enum data pattern '{owner}::{variant}' has inconsistent tag metadata"),
                span,
            );
            return None;
        };
        let checked_payload = payload_type.as_ref().and_then(|payload| self.known_value_type(payload));
        if selected.payload_type != checked_payload
        {
            self.report(
                DiagnosticCode::UnsupportedType,
                format!("enum data pattern '{owner}::{variant}' payload does not match its aggregate slot"),
                span,
            );
            return None;
        }
        Some(selected)
    }
}
