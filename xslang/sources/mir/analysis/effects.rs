/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 */

use crate::mir::{LocalId, Statement, Terminator};

/// Local reads/writes and observable behavior of one MIR statement.
#[derive(Clone, Debug, Eq, PartialEq)]
pub struct StatementEffects
{
    uses: Vec<LocalId>,
    definitions: Vec<LocalId>,
    side_effects: bool,
    may_trap: bool,
}

impl StatementEffects
{
    /// Locals read by the statement.
    #[must_use]
    pub fn uses(&self) -> &[LocalId]
    {
        &self.uses
    }

    /// Locals whose current values are replaced by the statement.
    #[must_use]
    pub fn definitions(&self) -> &[LocalId]
    {
        &self.definitions
    }

    /// Whether removing the statement could remove an observable effect.
    #[must_use]
    pub const fn has_side_effects(&self) -> bool
    {
        self.side_effects
    }

    /// Whether execution can fail even when the result is unused.
    #[must_use]
    pub const fn may_trap(&self) -> bool
    {
        self.may_trap
    }

    /// Whether the statement may be erased when every definition is dead.
    #[must_use]
    pub const fn removable_when_dead(&self) -> bool
    {
        !self.side_effects && !self.may_trap
    }
}

/// Classifies a statement for liveness and dead-code transformations.
#[must_use]
pub fn statement_effects(statement: &Statement) -> StatementEffects
{
    match statement
    {
        Statement::Use {
            local, ..
        } |
        Statement::Move {
            local, ..
        } |
        Statement::BorrowShared {
            local, ..
        } |
        Statement::BorrowMutable {
            local, ..
        } |
        Statement::EndBorrow {
            local, ..
        } |
        Statement::Drop {
            local, ..
        } => effects(&[*local], &[], true, false),
        Statement::ConstI64 {
            local, ..
        } |
        Statement::ConstI32 {
            local, ..
        } |
        Statement::ConstU16 {
            local, ..
        } |
        Statement::ConstInteger {
            local, ..
        } |
        Statement::ConstF32 {
            local, ..
        } |
        Statement::ConstF64 {
            local, ..
        } |
        Statement::ConstStr {
            local, ..
        } |
        Statement::ConstBool {
            local, ..
        } => effects(&[], &[*local], false, false),
        Statement::StoreLocal {
            local,
            value,
            ..
        } => effects(&[*value], &[*local], false, false),
        Statement::LoadLocal {
            result,
            local,
            ..
        } => effects(&[*local], &[*result], false, false),
        Statement::BinaryInteger {
            operation,
            result,
            left,
            right,
            ..
        } => effects(&[*left, *right], &[*result], false, integer_may_trap(*operation)),
        Statement::BinaryI64 {
            operation,
            result,
            left,
            right,
            ..
        } => effects(&[*left, *right], &[*result], false, i64_may_trap(*operation)),
        Statement::BinaryI32 {
            operation,
            result,
            left,
            right,
            ..
        } => effects(&[*left, *right], &[*result], false, i32_may_trap(*operation)),
        Statement::BinaryFloat {
            result,
            left,
            right,
            ..
        } |
        Statement::CompareFloat {
            result,
            left,
            right,
            ..
        } |
        Statement::CompareStr {
            result,
            left,
            right,
            ..
        } |
        Statement::AddI64 {
            result,
            left,
            right,
            ..
        } |
        Statement::SubI64 {
            result,
            left,
            right,
            ..
        } |
        Statement::MulI64 {
            result,
            left,
            right,
            ..
        } |
        Statement::EqI64 {
            result,
            left,
            right,
            ..
        } |
        Statement::CompareI64 {
            result,
            left,
            right,
            ..
        } |
        Statement::AddI32 {
            result,
            left,
            right,
            ..
        } |
        Statement::SubI32 {
            result,
            left,
            right,
            ..
        } |
        Statement::MulI32 {
            result,
            left,
            right,
            ..
        } |
        Statement::EqI32 {
            result,
            left,
            right,
            ..
        } |
        Statement::LtI32 {
            result,
            left,
            right,
            ..
        } |
        Statement::LeI32 {
            result,
            left,
            right,
            ..
        } |
        Statement::GtI32 {
            result,
            left,
            right,
            ..
        } |
        Statement::GeI32 {
            result,
            left,
            right,
            ..
        } => effects(&[*left, *right], &[*result], false, false),
        Statement::NotBool {
            result,
            operand,
            ..
        } => effects(&[*operand], &[*result], false, false),
        Statement::Call {
            result,
            arguments,
            ..
        } => effects(arguments, &result.iter().copied().collect::<Vec<_>>(), true, true),
        Statement::Aggregate {
            result,
            fields,
            ..
        } => effects(fields, &[*result], false, false),
        Statement::Extract {
            result,
            aggregate,
            ..
        } => effects(&[*aggregate], &[*result], false, true),
        Statement::ArrayGet {
            result,
            array,
            index,
            ..
        } => effects(&[*array, *index], &[*result], false, true),
        Statement::ArraySet {
            result,
            array,
            index,
            value,
            ..
        } => effects(&[*array, *index, *value], &[*result], true, true),
        Statement::ArrayLength {
            result,
            array,
            ..
        } => effects(&[*array], &[*result], false, true),
    }
}

/// Locals read by a block terminator.
#[must_use]
pub fn terminator_uses(terminator: Option<&Terminator>) -> Vec<LocalId>
{
    match terminator
    {
        Some(Terminator::Return(Some(value))) => vec![*value],
        Some(Terminator::BranchIf {
            condition, ..
        }) => vec![*condition],
        _ => Vec::new(),
    }
}

fn effects(uses: &[LocalId], definitions: &[LocalId], side_effects: bool, may_trap: bool) -> StatementEffects
{
    StatementEffects {
        uses: uses.to_vec(),
        definitions: definitions.to_vec(),
        side_effects,
        may_trap,
    }
}

fn integer_may_trap(operation: crate::xlil::IntegerBinaryOperation) -> bool
{
    use crate::xlil::IntegerBinaryOperation::{Div, Rem};
    matches!(operation, Div | Rem)
}

fn i64_may_trap(operation: crate::xlil::I64BinaryOperation) -> bool
{
    use crate::xlil::I64BinaryOperation::{Div, Rem};
    matches!(operation, Div | Rem)
}

fn i32_may_trap(operation: crate::xlil::I32BinaryOperation) -> bool
{
    use crate::xlil::I32BinaryOperation::{Div, Rem};
    matches!(operation, Div | Rem)
}
