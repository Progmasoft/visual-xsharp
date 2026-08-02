/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 */

use std::collections::HashMap;

use super::verify::{Diagnostic as VerifyDiagnostic, verify_function};
use super::{BasicBlock, Function, LocalId, Statement, Terminator, reachable_blocks};

mod control_flow;
mod i32_operations;
mod i64_operations;
mod integer_operations;
mod pipeline;

use control_flow::{collapse_single_predecessor_gotos, simplify_bool_branches};
use i32_operations::fold_const_i32_binary;
use i64_operations::fold_const_i64_operations;
use integer_operations::fold_const_integer_operations;
pub use pipeline::{OptimizationLevel, OptimizationPipeline};

#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub enum OptimizationPass
{
    RemoveUnreachableBlocks,
    RemoveRedundantEndBorrow,
    FoldConstI64Add,
    FoldConstI64Sub,
    FoldConstI64Mul,
    FoldConstI64Eq,
    FoldConstI64Binary,
    FoldConstI64Comparison,
    FoldConstI32Binary,
    FoldConstIntegerBinary,
    FoldConstBoolNot,
    FoldConstBoolBranch,
    SimplifyBoolBranch,
    CollapseSinglePredecessorGoto,
    DeadPureStatement,
    RemoveUnusedLocal,
    SimplifySameTargetBranch,
    CanonicalizeBlockOrder,
}

#[derive(Clone, Debug, Eq, PartialEq)]
pub struct OptimizationReport
{
    pub pass: OptimizationPass,
    pub removed_items: usize,
}

#[derive(Clone, Debug, Eq, PartialEq)]
pub struct OptimizedFunction
{
    pub function: Function,
    pub reports: Vec<OptimizationReport>,
}

#[derive(Clone, Debug, Eq, PartialEq)]
pub enum OptimizationError
{
    InputInvalid(Vec<VerifyDiagnostic>),
    OutputInvalid(Vec<VerifyDiagnostic>),
}

#[must_use]
pub fn optimize_function(mut function: Function) -> OptimizedFunction
{
    let mut reports = Vec::new();
    let removed_blocks = remove_unreachable_blocks(&mut function);
    if removed_blocks != 0
    {
        reports.push(OptimizationReport {
            pass: OptimizationPass::RemoveUnreachableBlocks,
            removed_items: removed_blocks,
        });
    }
    let removed_end_borrows = remove_redundant_end_borrows(&mut function);
    if removed_end_borrows != 0
    {
        reports.push(OptimizationReport {
            pass: OptimizationPass::RemoveRedundantEndBorrow,
            removed_items: removed_end_borrows,
        });
    }
    let folded_adds = fold_const_i64_adds(&mut function);
    if folded_adds != 0
    {
        reports.push(OptimizationReport {
            pass: OptimizationPass::FoldConstI64Add,
            removed_items: folded_adds,
        });
    }
    let folded_subs = fold_const_i64_subs(&mut function);
    if folded_subs != 0
    {
        reports.push(OptimizationReport {
            pass: OptimizationPass::FoldConstI64Sub,
            removed_items: folded_subs,
        });
    }
    let folded_muls = fold_const_i64_muls(&mut function);
    if folded_muls != 0
    {
        reports.push(OptimizationReport {
            pass: OptimizationPass::FoldConstI64Mul,
            removed_items: folded_muls,
        });
    }
    let folded_eqs = fold_const_i64_eqs(&mut function);
    if folded_eqs != 0
    {
        reports.push(OptimizationReport {
            pass: OptimizationPass::FoldConstI64Eq,
            removed_items: folded_eqs,
        });
    }
    let (folded_i64, folded_i64_comparisons) = fold_const_i64_operations(&mut function);
    if folded_i64 != 0
    {
        reports.push(OptimizationReport {
            pass: OptimizationPass::FoldConstI64Binary,
            removed_items: folded_i64,
        });
    }
    if folded_i64_comparisons != 0
    {
        reports.push(OptimizationReport {
            pass: OptimizationPass::FoldConstI64Comparison,
            removed_items: folded_i64_comparisons,
        });
    }
    let folded_i32 = fold_const_i32_binary(&mut function);
    if folded_i32 != 0
    {
        reports.push(OptimizationReport {
            pass: OptimizationPass::FoldConstI32Binary,
            removed_items: folded_i32,
        });
    }
    let folded_integer = fold_const_integer_operations(&mut function);
    if folded_integer != 0
    {
        reports.push(OptimizationReport {
            pass: OptimizationPass::FoldConstIntegerBinary,
            removed_items: folded_integer,
        });
    }
    let folded_nots = fold_const_bool_nots(&mut function);
    if folded_nots != 0
    {
        reports.push(OptimizationReport {
            pass: OptimizationPass::FoldConstBoolNot,
            removed_items: folded_nots,
        });
    }
    let folded_branches = fold_const_bool_branches(&mut function);
    if folded_branches != 0
    {
        reports.push(OptimizationReport {
            pass: OptimizationPass::FoldConstBoolBranch,
            removed_items: folded_branches,
        });
    }
    let simplified_branches = simplify_bool_branches(&mut function);
    if simplified_branches != 0
    {
        reports.push(OptimizationReport {
            pass: OptimizationPass::SimplifyBoolBranch,
            removed_items: simplified_branches,
        });
    }
    let collapsed_gotos = collapse_single_predecessor_gotos(&mut function);
    if collapsed_gotos != 0
    {
        reports.push(OptimizationReport {
            pass: OptimizationPass::CollapseSinglePredecessorGoto,
            removed_items: collapsed_gotos,
        });
    }
    let removed_blocks = remove_unreachable_blocks(&mut function);
    if removed_blocks != 0
    {
        reports.push(OptimizationReport {
            pass: OptimizationPass::RemoveUnreachableBlocks,
            removed_items: removed_blocks,
        });
    }
    OptimizedFunction {
        function,
        reports,
    }
}

pub fn optimize_verified_function(function: Function) -> Result<OptimizedFunction, OptimizationError>
{
    let input_diagnostics = verify_function(&function);
    if !input_diagnostics.is_empty()
    {
        return Err(OptimizationError::InputInvalid(input_diagnostics));
    }
    let optimized = optimize_function(function);
    let output_diagnostics = verify_function(&optimized.function);
    if !output_diagnostics.is_empty()
    {
        return Err(OptimizationError::OutputInvalid(output_diagnostics));
    }
    Ok(optimized)
}

fn remove_unreachable_blocks(function: &mut Function) -> usize
{
    let reachable = reachable_blocks(function);
    let before = function.blocks.len();
    function.blocks.retain(|block| reachable.contains(&block.id));
    before - function.blocks.len()
}

fn remove_redundant_end_borrows(function: &mut Function) -> usize
{
    let mut removed = 0;
    for block in &mut function.blocks
    {
        removed += remove_redundant_end_borrows_in_block(block);
    }
    removed
}

fn remove_redundant_end_borrows_in_block(block: &mut BasicBlock) -> usize
{
    let mut borrow_depths: HashMap<LocalId, u32> = HashMap::new();
    let mut optimized = Vec::with_capacity(block.statements.len());
    let mut removed = 0;
    for statement in block.statements.drain(..)
    {
        match statement
        {
            Statement::BorrowShared {
                local, ..
            } |
            Statement::BorrowMutable {
                local, ..
            } =>
            {
                *borrow_depths.entry(local).or_default() += 1;
                optimized.push(statement);
            }
            Statement::EndBorrow {
                local, ..
            } =>
            {
                let depth = borrow_depths.entry(local).or_default();
                if *depth == 0
                {
                    removed += 1;
                }
                else
                {
                    *depth -= 1;
                    optimized.push(statement);
                }
            }
            _ => optimized.push(statement),
        }
    }
    block.statements = optimized;
    removed
}

fn fold_const_i64_adds(function: &mut Function) -> usize
{
    let mut folded = 0;
    for block in &mut function.blocks
    {
        folded += fold_const_i64_adds_in_block(block);
    }
    folded
}

fn fold_const_i64_adds_in_block(block: &mut BasicBlock) -> usize
{
    let mut constants = HashMap::new();
    let mut folded = 0;
    for statement in &mut block.statements
    {
        match statement
        {
            Statement::ConstI64 {
                local,
                value,
                ..
            } =>
            {
                constants.insert(*local, *value);
            }
            Statement::AddI64 {
                result,
                left,
                right,
                span,
            } =>
            {
                let result = *result;
                let span = *span;
                let (Some(left), Some(right)) = (constants.get(left).copied(), constants.get(right).copied())
                else
                {
                    constants.remove(&result);
                    continue;
                };
                let Some(value) = left.checked_add(right)
                else
                {
                    constants.remove(&result);
                    continue;
                };
                *statement = Statement::ConstI64 {
                    local: result,
                    value,
                    span,
                };
                constants.insert(result, value);
                folded += 1;
            }
            Statement::Call {
                result: Some(result), ..
            } =>
            {
                constants.remove(result);
            }
            _ =>
            {}
        }
    }
    folded
}

fn fold_const_i64_subs(function: &mut Function) -> usize
{
    let mut folded = 0;
    for block in &mut function.blocks
    {
        folded += fold_const_i64_subs_in_block(block);
    }
    folded
}

fn fold_const_i64_subs_in_block(block: &mut BasicBlock) -> usize
{
    let mut constants = HashMap::new();
    let mut folded = 0;
    for statement in &mut block.statements
    {
        match statement
        {
            Statement::ConstI64 {
                local,
                value,
                ..
            } =>
            {
                constants.insert(*local, *value);
            }
            Statement::SubI64 {
                result,
                left,
                right,
                span,
            } =>
            {
                let result = *result;
                let span = *span;
                let (Some(left), Some(right)) = (constants.get(left).copied(), constants.get(right).copied())
                else
                {
                    constants.remove(&result);
                    continue;
                };
                let Some(value) = left.checked_sub(right)
                else
                {
                    constants.remove(&result);
                    continue;
                };
                *statement = Statement::ConstI64 {
                    local: result,
                    value,
                    span,
                };
                constants.insert(result, value);
                folded += 1;
            }
            Statement::Call {
                result: Some(result), ..
            } =>
            {
                constants.remove(result);
            }
            _ =>
            {}
        }
    }
    folded
}

fn fold_const_i64_muls(function: &mut Function) -> usize
{
    let mut folded = 0;
    for block in &mut function.blocks
    {
        folded += fold_const_i64_muls_in_block(block);
    }
    folded
}

fn fold_const_i64_muls_in_block(block: &mut BasicBlock) -> usize
{
    let mut constants = HashMap::new();
    let mut folded = 0;
    for statement in &mut block.statements
    {
        match statement
        {
            Statement::ConstI64 {
                local,
                value,
                ..
            } =>
            {
                constants.insert(*local, *value);
            }
            Statement::MulI64 {
                result,
                left,
                right,
                span,
            } =>
            {
                let result = *result;
                let span = *span;
                let (Some(left), Some(right)) = (constants.get(left).copied(), constants.get(right).copied())
                else
                {
                    constants.remove(&result);
                    continue;
                };
                let Some(value) = left.checked_mul(right)
                else
                {
                    constants.remove(&result);
                    continue;
                };
                *statement = Statement::ConstI64 {
                    local: result,
                    value,
                    span,
                };
                constants.insert(result, value);
                folded += 1;
            }
            Statement::Call {
                result: Some(result), ..
            } =>
            {
                constants.remove(result);
            }
            _ =>
            {}
        }
    }
    folded
}

fn fold_const_i64_eqs(function: &mut Function) -> usize
{
    let mut folded = 0;
    for block in &mut function.blocks
    {
        folded += fold_const_i64_eqs_in_block(block);
    }
    folded
}

fn fold_const_i64_eqs_in_block(block: &mut BasicBlock) -> usize
{
    let mut constants = HashMap::new();
    let mut folded = 0;
    for statement in &mut block.statements
    {
        match statement
        {
            Statement::ConstI64 {
                local,
                value,
                ..
            } =>
            {
                constants.insert(*local, *value);
            }
            Statement::EqI64 {
                result,
                left,
                right,
                span,
            } =>
            {
                let result = *result;
                let span = *span;
                let (Some(left), Some(right)) = (constants.get(left).copied(), constants.get(right).copied())
                else
                {
                    continue;
                };
                *statement = Statement::ConstBool {
                    local: result,
                    value: left == right,
                    span,
                };
                folded += 1;
            }
            Statement::Call {
                result: Some(result), ..
            } =>
            {
                constants.remove(result);
            }
            _ =>
            {}
        }
    }
    folded
}

fn fold_const_bool_nots(function: &mut Function) -> usize
{
    function.blocks.iter_mut().map(fold_const_bool_nots_in_block).sum()
}

fn fold_const_bool_nots_in_block(block: &mut BasicBlock) -> usize
{
    let mut constants = HashMap::new();
    let mut folded = 0;
    for statement in &mut block.statements
    {
        match statement
        {
            Statement::ConstBool {
                local,
                value,
                ..
            } =>
            {
                constants.insert(*local, *value);
            }
            Statement::NotBool {
                result,
                operand,
                span,
            } =>
            {
                let result = *result;
                let span = *span;
                let Some(value) = constants.get(operand).map(|value| !value)
                else
                {
                    constants.remove(&result);
                    continue;
                };
                *statement = Statement::ConstBool {
                    local: result,
                    value,
                    span,
                };
                constants.insert(result, value);
                folded += 1;
            }
            Statement::EqI64 {
                result, ..
            } |
            Statement::CompareI64 {
                result, ..
            } |
            Statement::EqI32 {
                result, ..
            } |
            Statement::LtI32 {
                result, ..
            } |
            Statement::LeI32 {
                result, ..
            } |
            Statement::GtI32 {
                result, ..
            } |
            Statement::GeI32 {
                result, ..
            } |
            Statement::Call {
                result: Some(result), ..
            } =>
            {
                constants.remove(result);
            }
            _ =>
            {}
        }
    }
    folded
}

fn fold_const_bool_branches(function: &mut Function) -> usize
{
    let mut folded = 0;
    for block in &mut function.blocks
    {
        folded += fold_const_bool_branch_in_block(block);
    }
    folded
}

fn fold_const_bool_branch_in_block(block: &mut BasicBlock) -> usize
{
    let mut constants = HashMap::new();
    for statement in &block.statements
    {
        match statement
        {
            Statement::ConstI64 {
                local, ..
            } =>
            {
                constants.remove(local);
            }
            Statement::ConstBool {
                local,
                value,
                ..
            } =>
            {
                constants.insert(*local, *value);
            }
            Statement::AddI64 {
                result, ..
            } |
            Statement::SubI64 {
                result, ..
            } |
            Statement::MulI64 {
                result, ..
            } |
            Statement::BinaryI64 {
                result, ..
            } |
            Statement::EqI64 {
                result, ..
            } |
            Statement::CompareI64 {
                result, ..
            } |
            Statement::AddI32 {
                result, ..
            } |
            Statement::SubI32 {
                result, ..
            } |
            Statement::MulI32 {
                result, ..
            } |
            Statement::BinaryI32 {
                result, ..
            } |
            Statement::EqI32 {
                result, ..
            } |
            Statement::LtI32 {
                result, ..
            } |
            Statement::LeI32 {
                result, ..
            } |
            Statement::GtI32 {
                result, ..
            } |
            Statement::GeI32 {
                result, ..
            } =>
            {
                constants.remove(result);
            }
            Statement::NotBool {
                result, ..
            } =>
            {
                constants.remove(result);
            }
            Statement::Call {
                result: Some(result), ..
            } =>
            {
                constants.remove(result);
            }
            _ =>
            {}
        }
    }
    let Some(Terminator::BranchIf {
        condition,
        then_block,
        else_block,
    }) = block.terminator
    else
    {
        return 0;
    };
    let Some(condition) = constants.get(&condition).copied()
    else
    {
        return 0;
    };
    let target = if condition
    {
        then_block
    }
    else
    {
        else_block
    };
    block.terminator = Some(Terminator::Goto(target));
    1
}

include!("optimizer/tests.rs");
