/*
 * SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
 * SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.0
 */

use super::result_match::result_match_is_exhaustive;
use super::*;

impl HirToMirLowerer
{
    pub(super) fn lower_panic(&mut self, span: Span, lowered: &mut mir::Function)
    {
        if self.current_block_mut(lowered).terminator.is_some()
        {
            return;
        }
        let block = self.current_block_mut(lowered);
        block.terminator = Some(mir::Terminator::Panic);
        block.span = span;
    }

    pub(super) fn lower_match_expression(
        &mut self,
        expression: &Expression,
        expected_type: XlilType,
        lowered: &mut mir::Function,
    ) -> Option<mir::LocalId>
    {
        let Expression::Match {
            selector,
            selector_type,
            arms,
            result_type,
            span,
        } = expression
        else
        {
            return None;
        };
        if primitive_to_xlil(match result_type.as_ref()
        {
            Type::Primitive(value) => *value,
            Type::Unit |
            Type::Named(_) |
            Type::Optional {
                ..
            } |
            Type::Result {
                ..
            } |
            Type::Reference {
                ..
            } |
            Type::Array {
                ..
            } |
            Type::Set {
                ..
            } |
            Type::Map {
                ..
            } |
            Type::Tuple {
                ..
            } =>
            {
                self.report(
                    DiagnosticCode::UnsupportedType,
                    "named match result cannot lower to MIR yet",
                    *span,
                );
                return None;
            }
        }) != Some(expected_type)
        {
            self.report(
                DiagnosticCode::UnsupportedType,
                "match result type does not match MIR context",
                *span,
            );
            return None;
        }
        let exhaustive_result = result_match_is_exhaustive(selector_type, arms);
        let exhaustive_enum_data = self.enum_data_match_is_exhaustive(selector_type, arms);
        let exhaustive = exhaustive_result || exhaustive_enum_data;
        if !exhaustive && !matches!(arms.last().map(|arm| &arm.pattern), Some(MatchPattern::Else))
        {
            self.report(
                DiagnosticCode::UnsupportedExpression,
                "MIR match expression lowering requires a final else arm",
                *span,
            );
            return None;
        }
        let selector_value_type = self.match_selector_value_type(selector_type, *span)?;
        let selector = self.lower_expression_to_local(selector, selector_value_type, lowered)?;
        let result_storage = self.declare_storage_temp(expected_type, *span, lowered)?;
        let merge = self.append_block(*span, lowered);
        let outer_locals = self.locals.clone();
        let mut test = self.current_block;
        let mut open_arm = false;
        for (index, arm) in arms.iter().enumerate()
        {
            self.locals.clone_from(&outer_locals);
            self.switch_to(test);
            match &arm.pattern
            {
                MatchPattern::Literal(literal) =>
                {
                    let body = self.append_block(arm.body.span, lowered);
                    let next = self.append_block(arm.span, lowered);
                    let condition = self.lower_match_test(selector, selector_value_type, literal, arm.span, lowered)?;
                    let (then_block, else_block) = if matches!(literal, Literal::Bool(false))
                    {
                        (next, body)
                    }
                    else
                    {
                        (body, next)
                    };
                    self.set_terminator(
                        mir::Terminator::BranchIf {
                            condition,
                            then_block,
                            else_block,
                        },
                        arm.span,
                        lowered,
                    );
                    self.switch_to(body);
                    open_arm |= self.lower_value_block_into_storage(
                        &arm.body,
                        result_storage,
                        expected_type,
                        merge,
                        arm.span,
                        lowered,
                    );
                    test = next;
                }
                MatchPattern::ResultVariant {
                    success, ..
                } =>
                {
                    if exhaustive_result && index + 1 == arms.len()
                    {
                        self.bind_result_pattern(selector, &arm.pattern, arm.span, lowered)?;
                        open_arm |= self.lower_value_block_into_storage(
                            &arm.body,
                            result_storage,
                            expected_type,
                            merge,
                            arm.span,
                            lowered,
                        );
                        continue;
                    }
                    let body = self.append_block(arm.body.span, lowered);
                    let next = self.append_block(arm.span, lowered);
                    let condition = self.result_tag(selector, selector_value_type, arm.span, lowered)?;
                    let (then_block, else_block) = if *success
                    {
                        (body, next)
                    }
                    else
                    {
                        (next, body)
                    };
                    self.set_terminator(
                        mir::Terminator::BranchIf {
                            condition,
                            then_block,
                            else_block,
                        },
                        arm.span,
                        lowered,
                    );
                    self.switch_to(body);
                    self.bind_result_pattern(selector, &arm.pattern, arm.span, lowered)?;
                    open_arm |= self.lower_value_block_into_storage(
                        &arm.body,
                        result_storage,
                        expected_type,
                        merge,
                        arm.span,
                        lowered,
                    );
                    test = next;
                }
                MatchPattern::EnumDataVariant {
                    ..
                } =>
                {
                    if exhaustive_enum_data && index + 1 == arms.len()
                    {
                        self.bind_enum_data_pattern(selector, selector_value_type, &arm.pattern, arm.span, lowered)?;
                        open_arm |= self.lower_value_block_into_storage(
                            &arm.body,
                            result_storage,
                            expected_type,
                            merge,
                            arm.span,
                            lowered,
                        );
                        continue;
                    }
                    let body = self.append_block(arm.body.span, lowered);
                    let next = self.append_block(arm.span, lowered);
                    let condition =
                        self.enum_data_pattern_test(selector, selector_value_type, &arm.pattern, arm.span, lowered)?;
                    self.set_terminator(
                        mir::Terminator::BranchIf {
                            condition,
                            then_block: body,
                            else_block: next,
                        },
                        arm.span,
                        lowered,
                    );
                    self.switch_to(body);
                    self.bind_enum_data_pattern(selector, selector_value_type, &arm.pattern, arm.span, lowered)?;
                    open_arm |= self.lower_value_block_into_storage(
                        &arm.body,
                        result_storage,
                        expected_type,
                        merge,
                        arm.span,
                        lowered,
                    );
                    test = next;
                }
                MatchPattern::Else =>
                {
                    open_arm |= self.lower_value_block_into_storage(
                        &arm.body,
                        result_storage,
                        expected_type,
                        merge,
                        arm.span,
                        lowered,
                    );
                }
            }
        }
        self.locals = outer_locals;
        if !open_arm
        {
            self.report(
                DiagnosticCode::MissingReturn,
                "match expression has no value-producing arm",
                *span,
            );
            return None;
        }
        self.switch_to(merge);
        let result = self.declare_temp(expected_type, *span, lowered)?;
        self.current_block_mut(lowered)
            .statements
            .push(mir::Statement::LoadLocal {
                result,
                local: result_storage,
                span: *span,
            });
        Some(result)
    }

    fn lower_match_test(
        &mut self,
        selector: mir::LocalId,
        selector_type: XlilType,
        literal: &Literal,
        span: Span,
        lowered: &mut mir::Function,
    ) -> Option<mir::LocalId>
    {
        if selector_type == XlilType::BOOL
        {
            return Some(selector);
        }
        let selector = if let Literal::EnumVariant {
            ..
        } = literal
        {
            let extracted = self.declare_temp(XlilType::I32, span, lowered)?;
            self.current_block_mut(lowered)
                .statements
                .push(mir::Statement::Extract {
                    result: extracted,
                    aggregate: selector,
                    field: 0,
                    field_type: XlilType::I32,
                    span,
                });
            extracted
        }
        else
        {
            selector
        };
        let pattern = self.declare_temp(XlilType::I32, span, lowered)?;
        if let Literal::EnumVariant {
            tag, ..
        } = literal
        {
            let Ok(value) = i32::try_from(*tag)
            else
            {
                self.report(
                    DiagnosticCode::UnsupportedExpression,
                    "enum variant tag exceeds i32",
                    span,
                );
                return None;
            };
            self.current_block_mut(lowered)
                .statements
                .push(mir::Statement::ConstI32 {
                    local: pattern,
                    value,
                    span,
                });
        }
        else
        {
            self.lower_literal_into(pattern, literal, span, lowered);
        }
        let condition = self.declare_temp(XlilType::BOOL, span, lowered)?;
        self.current_block_mut(lowered).statements.push(mir::Statement::EqI32 {
            result: condition,
            left: selector,
            right: pattern,
            span,
        });
        Some(condition)
    }

    pub(super) fn lower_value_block_into_storage(
        &mut self,
        block: &Block,
        result_storage: mir::LocalId,
        result_type: XlilType,
        merge: mir::BlockId,
        span: Span,
        lowered: &mut mir::Function,
    ) -> bool
    {
        for statement in &block.statements
        {
            if self.current_is_terminated(lowered)
            {
                return false;
            }
            self.lower_statement(statement, lowered);
        }
        if self.current_is_terminated(lowered)
        {
            return false;
        }
        let Some(tail) = block.tail.as_deref()
        else
        {
            self.report(
                DiagnosticCode::MissingReturn,
                "value-producing block requires a tail value",
                span,
            );
            return false;
        };
        let Some(value) = self.lower_expression_to_local(tail, result_type, lowered)
        else
        {
            return false;
        };
        self.current_block_mut(lowered)
            .statements
            .push(mir::Statement::StoreLocal {
                local: result_storage,
                value,
                span: expression_span(tail),
            });
        self.set_terminator(mir::Terminator::Goto(merge), span, lowered);
        true
    }

    pub(super) fn lower_match_statement(&mut self, statement: &Statement, lowered: &mut mir::Function)
    {
        let Statement::Match {
            selector,
            selector_type,
            arms,
            span,
        } = statement
        else
        {
            return;
        };
        let exhaustive_result = result_match_is_exhaustive(selector_type, arms);
        let exhaustive_enum_data = self.enum_data_match_is_exhaustive(selector_type, arms);
        let exhaustive = exhaustive_result || exhaustive_enum_data;
        if !exhaustive && !matches!(arms.last().map(|arm| &arm.pattern), Some(MatchPattern::Else))
        {
            self.report(
                DiagnosticCode::UnsupportedExpression,
                "MIR match lowering requires a final else arm",
                *span,
            );
            return;
        }
        let Some(selector_type) = self.match_selector_value_type(selector_type, *span)
        else
        {
            return;
        };
        let Some(selector) = self.lower_expression_to_local(selector, selector_type, lowered)
        else
        {
            return;
        };
        let merge = self.append_block(*span, lowered);
        let outer_locals = self.locals.clone();
        let mut test = self.current_block;
        let mut has_open_arm = false;
        let mut final_arm_end = self.current_block;
        for (index, arm) in arms.iter().enumerate()
        {
            self.locals.clone_from(&outer_locals);
            self.switch_to(test);
            match &arm.pattern
            {
                MatchPattern::Literal(literal) =>
                {
                    let body = self.append_block(arm.body.span, lowered);
                    let next = self.append_block(arm.span, lowered);
                    let Some(condition) = self.lower_match_test(selector, selector_type, literal, arm.span, lowered)
                    else
                    {
                        return;
                    };
                    let (then_block, else_block) = if matches!(literal, Literal::Bool(false))
                    {
                        (next, body)
                    }
                    else
                    {
                        (body, next)
                    };
                    self.set_terminator(
                        mir::Terminator::BranchIf {
                            condition,
                            then_block,
                            else_block,
                        },
                        arm.span,
                        lowered,
                    );
                    self.switch_to(body);
                    self.lower_block_statements(&arm.body, lowered);
                    final_arm_end = self.current_block;
                    if !self.current_is_terminated(lowered)
                    {
                        self.set_terminator(mir::Terminator::Goto(merge), arm.span, lowered);
                        has_open_arm = true;
                    }
                    test = next;
                }
                MatchPattern::ResultVariant {
                    success, ..
                } =>
                {
                    if exhaustive_result && index + 1 == arms.len()
                    {
                        if self
                            .bind_result_pattern(selector, &arm.pattern, arm.span, lowered)
                            .is_some()
                        {
                            self.lower_block_statements(&arm.body, lowered);
                            final_arm_end = self.current_block;
                            if !self.current_is_terminated(lowered)
                            {
                                self.set_terminator(mir::Terminator::Goto(merge), arm.span, lowered);
                                has_open_arm = true;
                            }
                        }
                        continue;
                    }
                    let body = self.append_block(arm.body.span, lowered);
                    let next = self.append_block(arm.span, lowered);
                    let Some(condition) = self.result_tag(selector, selector_type, arm.span, lowered)
                    else
                    {
                        return;
                    };
                    let (then_block, else_block) = if *success
                    {
                        (body, next)
                    }
                    else
                    {
                        (next, body)
                    };
                    self.set_terminator(
                        mir::Terminator::BranchIf {
                            condition,
                            then_block,
                            else_block,
                        },
                        arm.span,
                        lowered,
                    );
                    self.switch_to(body);
                    if self
                        .bind_result_pattern(selector, &arm.pattern, arm.span, lowered)
                        .is_some()
                    {
                        self.lower_block_statements(&arm.body, lowered);
                        final_arm_end = self.current_block;
                        if !self.current_is_terminated(lowered)
                        {
                            self.set_terminator(mir::Terminator::Goto(merge), arm.span, lowered);
                            has_open_arm = true;
                        }
                    }
                    test = next;
                }
                MatchPattern::EnumDataVariant {
                    ..
                } =>
                {
                    if exhaustive_enum_data && index + 1 == arms.len()
                    {
                        if self
                            .bind_enum_data_pattern(selector, selector_type, &arm.pattern, arm.span, lowered)
                            .is_some()
                        {
                            self.lower_block_statements(&arm.body, lowered);
                            final_arm_end = self.current_block;
                            if !self.current_is_terminated(lowered)
                            {
                                self.set_terminator(mir::Terminator::Goto(merge), arm.span, lowered);
                                has_open_arm = true;
                            }
                        }
                        continue;
                    }
                    let body = self.append_block(arm.body.span, lowered);
                    let next = self.append_block(arm.span, lowered);
                    let Some(condition) =
                        self.enum_data_pattern_test(selector, selector_type, &arm.pattern, arm.span, lowered)
                    else
                    {
                        return;
                    };
                    self.set_terminator(
                        mir::Terminator::BranchIf {
                            condition,
                            then_block: body,
                            else_block: next,
                        },
                        arm.span,
                        lowered,
                    );
                    self.switch_to(body);
                    if self
                        .bind_enum_data_pattern(selector, selector_type, &arm.pattern, arm.span, lowered)
                        .is_some()
                    {
                        self.lower_block_statements(&arm.body, lowered);
                        final_arm_end = self.current_block;
                        if !self.current_is_terminated(lowered)
                        {
                            self.set_terminator(mir::Terminator::Goto(merge), arm.span, lowered);
                            has_open_arm = true;
                        }
                    }
                    test = next;
                }
                MatchPattern::Else =>
                {
                    self.lower_block_statements(&arm.body, lowered);
                    final_arm_end = self.current_block;
                    if !self.current_is_terminated(lowered)
                    {
                        self.set_terminator(mir::Terminator::Goto(merge), arm.span, lowered);
                        has_open_arm = true;
                    }
                }
            }
        }
        self.locals = outer_locals;
        if has_open_arm
        {
            self.switch_to(merge);
        }
        else
        {
            lowered.blocks.retain(|block| block.id != merge);
            self.switch_to(final_arm_end);
        }
    }

}
include!("control_flow/loops.rs");
