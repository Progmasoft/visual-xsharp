/*
 * SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
 * SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.0
 */

use super::*;

impl HirToMirLowerer
{
    pub(super) fn lower_optional_member(
        &mut self,
        expression: &Expression,
        result_type: XlilType,
        lowered: &mut mir::Function,
    ) -> Option<mir::LocalId>
    {
        let Expression::OptionalMember {
            receiver,
            owner,
            name,
            field_type,
            result_type: source_result_type,
            span,
        } = expression
        else
        {
            return None;
        };
        if self.lower_value_type(source_result_type, *span)? != result_type
        {
            self.report(
                DiagnosticCode::UnsupportedType,
                "optional member result does not match its MIR target type",
                *span,
            );
            return None;
        }
        let field_value_type = self.lower_value_type(field_type, *span)?;
        if self.optional_element_type(result_type) != Some(field_value_type)
        {
            self.report(
                DiagnosticCode::UnsupportedType,
                "optional member result layout does not wrap its field type",
                *span,
            );
            return None;
        }
        let owner_type = *self.aggregate_types.get(owner)?;
        let receiver_type = self
            .optional_layouts
            .iter()
            .find(|(_, element_type)| *element_type == owner_type)
            .map(|(optional_type, _)| *optional_type)?;
        let receiver = self.lower_expression_to_local(receiver, receiver_type, lowered)?;
        let receiver_tag = self.extract_optional_field(receiver, 0, XlilType::BOOL, *span, lowered)?;
        let definition = self.nominal_types.get(owner)?.clone();
        let fields = self.resolved_nominal_fields(&definition)?;
        let field_index = fields.iter().position(|field| field.name == *name)?;

        let storage = self.declare_storage_temp(result_type, *span, lowered)?;
        let branch = self.current_block;
        let present = self.append_block(*span, lowered);
        let absent = self.append_block(*span, lowered);
        let merge = self.append_block(*span, lowered);
        self.switch_to(branch);
        self.set_terminator(
            mir::Terminator::BranchIf {
                condition: receiver_tag,
                then_block: present,
                else_block: absent,
            },
            *span,
            lowered,
        );

        self.switch_to(present);
        let owner_value = self.extract_optional_field(receiver, 1, owner_type, *span, lowered)?;
        let field_value = self.extract_optional_field(
            owner_value,
            u32::try_from(field_index).ok()?,
            field_value_type,
            *span,
            lowered,
        )?;
        let true_tag = self.declare_temp(XlilType::BOOL, *span, lowered)?;
        self.current_block_mut(lowered)
            .statements
            .push(mir::Statement::ConstBool {
                local: true_tag,
                value: true,
                span: *span,
            });
        let some = self.declare_temp(result_type, *span, lowered)?;
        self.current_block_mut(lowered)
            .statements
            .push(mir::Statement::Aggregate {
                result: some,
                value_type: result_type,
                fields: vec![true_tag, field_value],
                field_types: vec![XlilType::BOOL, field_value_type],
                span: *span,
            });
        self.store_coalesced_value(storage, some, merge, *span, lowered);

        self.switch_to(absent);
        let none = self.declare_temp(result_type, *span, lowered)?;
        self.lower_optional_none(none, result_type, *span, lowered);
        self.store_coalesced_value(storage, none, merge, *span, lowered);

        self.switch_to(merge);
        let result = self.declare_temp(result_type, *span, lowered)?;
        self.current_block_mut(lowered)
            .statements
            .push(mir::Statement::LoadLocal {
                result,
                local: storage,
                span: *span,
            });
        Some(result)
    }

    pub(super) fn lower_optional_unwrap(
        &mut self,
        optional: &Expression,
        result_type: XlilType,
        span: Span,
        lowered: &mut mir::Function,
    ) -> Option<mir::LocalId>
    {
        let optional_type = self
            .optional_layouts
            .iter()
            .find(|(_, element_type)| *element_type == result_type)
            .map(|(value_type, _)| *value_type)?;
        let optional = self.lower_expression_to_local(optional, optional_type, lowered)?;
        let tag = self.extract_optional_field(optional, 0, XlilType::BOOL, span, lowered)?;
        let branch = self.current_block;
        let success = self.append_block(span, lowered);
        let failure = self.append_block(span, lowered);
        self.switch_to(branch);
        self.set_terminator(
            mir::Terminator::BranchIf {
                condition: tag,
                then_block: success,
                else_block: failure,
            },
            span,
            lowered,
        );
        self.switch_to(failure);
        self.set_terminator(mir::Terminator::Panic, span, lowered);
        self.switch_to(success);
        self.extract_optional_field(optional, 1, result_type, span, lowered)
    }

    pub(super) fn lower_optional_coalesce_assign(
        &mut self,
        target: &str,
        value: &Expression,
        optional_type: XlilType,
        span: Span,
        lowered: &mut mir::Function,
    ) -> Option<mir::LocalId>
    {
        let target = self
            .locals
            .get(target)
            .copied()
            .filter(|local| self.local_value_type(*local, lowered) == Some(optional_type));
        let Some(target) = target
        else
        {
            self.report(
                DiagnosticCode::UnsupportedType,
                "coalescing assignment target is not an Optional<T> MIR place",
                span,
            );
            return None;
        };
        let loaded = self.declare_temp(optional_type, span, lowered)?;
        self.current_block_mut(lowered)
            .statements
            .push(mir::Statement::LoadLocal {
                result: loaded,
                local: target,
                span,
            });
        let tag = self.extract_optional_field(loaded, 0, XlilType::BOOL, span, lowered)?;
        let branch = self.current_block;
        let keep = self.append_block(span, lowered);
        let assign = self.append_block(span, lowered);
        let merge = self.append_block(span, lowered);
        self.switch_to(branch);
        self.set_terminator(
            mir::Terminator::BranchIf {
                condition: tag,
                then_block: keep,
                else_block: assign,
            },
            span,
            lowered,
        );
        self.switch_to(keep);
        self.set_terminator(mir::Terminator::Goto(merge), span, lowered);
        self.switch_to(assign);
        let assigned = self.lower_expression_to_local(value, optional_type, lowered)?;
        self.current_block_mut(lowered)
            .statements
            .push(mir::Statement::StoreLocal {
                local: target,
                value: assigned,
                span,
            });
        self.set_terminator(mir::Terminator::Goto(merge), span, lowered);
        self.switch_to(merge);
        let result = self.declare_temp(optional_type, span, lowered)?;
        self.current_block_mut(lowered)
            .statements
            .push(mir::Statement::LoadLocal {
                result,
                local: target,
                span,
            });
        Some(result)
    }

    pub(super) fn lower_optional_coalesce(
        &mut self,
        optional: &Expression,
        fallback: &Expression,
        result_type: XlilType,
        span: Span,
        lowered: &mut mir::Function,
    ) -> Option<mir::LocalId>
    {
        let optional_type = self
            .expression_value_type(optional, lowered)
            .filter(|value_type| self.optional_element_type(*value_type) == Some(result_type))
            .or_else(|| {
                self.optional_layouts
                    .iter()
                    .find(|(_, element_type)| *element_type == result_type)
                    .map(|(value_type, _)| *value_type)
            });
        let Some(optional_type) = optional_type
        else
        {
            self.report(
                DiagnosticCode::UnsupportedType,
                "left operand of ?? is not Optional<T> for the result type",
                span,
            );
            return None;
        };
        let optional = self.lower_expression_to_local(optional, optional_type, lowered)?;
        let tag = self.declare_temp(XlilType::BOOL, span, lowered)?;
        self.current_block_mut(lowered)
            .statements
            .push(mir::Statement::Extract {
                result: tag,
                aggregate: optional,
                field: 0,
                field_type: XlilType::BOOL,
                span,
            });

        let result_storage = self.declare_storage_temp(result_type, span, lowered)?;
        let branch = self.current_block;
        let some_block = self.append_block(span, lowered);
        let none_block = self.append_block(span, lowered);
        let merge = self.append_block(span, lowered);
        self.switch_to(branch);
        self.set_terminator(
            mir::Terminator::BranchIf {
                condition: tag,
                then_block: some_block,
                else_block: none_block,
            },
            span,
            lowered,
        );

        self.switch_to(some_block);
        let payload = self.declare_temp(result_type, span, lowered)?;
        self.current_block_mut(lowered)
            .statements
            .push(mir::Statement::Extract {
                result: payload,
                aggregate: optional,
                field: 1,
                field_type: result_type,
                span,
            });
        self.store_coalesced_value(result_storage, payload, merge, span, lowered);

        self.switch_to(none_block);
        let fallback = self.lower_expression_to_local(fallback, result_type, lowered)?;
        self.store_coalesced_value(result_storage, fallback, merge, span, lowered);

        self.switch_to(merge);
        let result = self.declare_temp(result_type, span, lowered)?;
        self.current_block_mut(lowered)
            .statements
            .push(mir::Statement::LoadLocal {
                result,
                local: result_storage,
                span,
            });
        Some(result)
    }

    fn optional_element_type(&self, optional_type: XlilType) -> Option<XlilType>
    {
        self.optional_layouts
            .iter()
            .find(|(value_type, _)| *value_type == optional_type)
            .map(|(_, element_type)| *element_type)
    }

    fn extract_optional_field(
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

    fn store_coalesced_value(
        &mut self,
        storage: mir::LocalId,
        value: mir::LocalId,
        merge: mir::BlockId,
        span: Span,
        lowered: &mut mir::Function,
    )
    {
        self.current_block_mut(lowered)
            .statements
            .push(mir::Statement::StoreLocal {
                local: storage,
                value,
                span,
            });
        self.set_terminator(mir::Terminator::Goto(merge), span, lowered);
    }
}
