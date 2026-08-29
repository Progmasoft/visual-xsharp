/*
 * SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
 * SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.0
 */

use std::collections::HashMap;

use crate::mir::{BlockId, Function, Statement, Terminator};

pub(super) fn simplify_bool_branches(function: &mut Function) -> usize
{
    let mut simplified = 0;
    for block in &mut function.blocks
    {
        let Some(Terminator::BranchIf {
            condition,
            then_block,
            else_block,
        }) = block.terminator.as_mut()
        else
        {
            continue;
        };
        if then_block == else_block
        {
            block.terminator = Some(Terminator::Goto(*then_block));
            simplified += 1;
            continue;
        }
        let Some(Statement::NotBool {
            result,
            operand,
            ..
        }) = block.statements.last()
        else
        {
            continue;
        };
        if result != condition
        {
            continue;
        }
        *condition = *operand;
        std::mem::swap(then_block, else_block);
        block.statements.pop();
        simplified += 1;
    }
    simplified
}

pub(super) fn collapse_single_predecessor_gotos(function: &mut Function) -> usize
{
    let Some(entry) = function.blocks.first().map(|block| block.id)
    else
    {
        return 0;
    };
    let mut collapsed = 0;
    loop
    {
        let predecessors = predecessor_counts(function);
        let candidate = function.blocks.iter().enumerate().find_map(|(source_index, source)| {
            let Some(Terminator::Goto(target)) = source.terminator.as_ref()
            else
            {
                return None;
            };
            (*target != source.id && *target != entry && predecessors.get(target) == Some(&1))
                .then_some((source_index, *target))
        });
        let Some((source_index, target)) = candidate
        else
        {
            break;
        };
        let Some(target_index) = function.blocks.iter().position(|block| block.id == target)
        else
        {
            break;
        };
        let target_block = function.blocks[target_index].clone();
        function.blocks[source_index].statements.extend(target_block.statements);
        function.blocks[source_index].terminator = target_block.terminator;
        function.blocks.remove(target_index);
        collapsed += 1;
    }
    collapsed
}

fn predecessor_counts(function: &Function) -> HashMap<BlockId, usize>
{
    let mut counts = HashMap::new();
    for block in &function.blocks
    {
        match block.terminator
        {
            Some(Terminator::Goto(target)) => *counts.entry(target).or_default() += 1,
            Some(Terminator::BranchIf {
                then_block,
                else_block,
                ..
            }) =>
            {
                *counts.entry(then_block).or_default() += 1;
                *counts.entry(else_block).or_default() += 1;
            }
            _ =>
            {}
        }
    }
    counts
}

#[cfg(test)]
mod tests
{
    use super::*;
    use crate::hir::Span;
    use crate::mir::{BasicBlock, Local, LocalId, Parameter};
    use crate::xlil::Type;

    fn span() -> Span
    {
        Span::new(1, 0, 1)
    }

    #[test]
    fn removes_terminal_bool_not_and_reverses_targets()
    {
        let mut function = Function {
            name: "branch".to_string(),
            parameters: vec![Parameter {
                local: LocalId(0),
                name: "condition".to_string(),
                value_type: Type::BOOL,
                span: span(),
            }],
            return_type: Type::VOID,
            locals: vec![Local {
                id: LocalId(1),
                name: "negated".to_string(),
                value_type: Some(Type::BOOL),
                mutable: false,
                span: span(),
            }],
            blocks: vec![BasicBlock {
                id: BlockId(0),
                statements: vec![Statement::NotBool {
                    result: LocalId(1),
                    operand: LocalId(0),
                    span: span(),
                }],
                terminator: Some(Terminator::BranchIf {
                    condition: LocalId(1),
                    then_block: BlockId(1),
                    else_block: BlockId(2),
                }),
                span: span(),
            }],
        };

        assert_eq!(simplify_bool_branches(&mut function), 1);
        assert!(function.blocks[0].statements.is_empty());
        assert_eq!(
            function.blocks[0].terminator,
            Some(Terminator::BranchIf {
                condition: LocalId(0),
                then_block: BlockId(2),
                else_block: BlockId(1)
            })
        );
    }

    #[test]
    fn replaces_same_target_branch_with_goto()
    {
        let mut function = Function {
            name: "branch".to_string(),
            parameters: vec![],
            return_type: Type::VOID,
            locals: vec![],
            blocks: vec![BasicBlock {
                id: BlockId(0),
                statements: vec![],
                terminator: Some(Terminator::BranchIf {
                    condition: LocalId(0),
                    then_block: BlockId(1),
                    else_block: BlockId(1),
                }),
                span: span(),
            }],
        };

        assert_eq!(simplify_bool_branches(&mut function), 1);
        assert_eq!(function.blocks[0].terminator, Some(Terminator::Goto(BlockId(1))));
    }
}
