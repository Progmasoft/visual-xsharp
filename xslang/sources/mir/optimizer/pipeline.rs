/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 */

use std::collections::{HashMap, HashSet};

use crate::mir::analysis::{ControlFlowGraph, Liveness, statement_effects, terminator_uses};
use crate::mir::verify::verify_function;
use crate::mir::{BlockId, Function, Terminator};

use super::{OptimizationError, OptimizationPass, OptimizationReport, OptimizedFunction, optimize_function};

/// Preset controlling the cost and scope of the MIR optimization pipeline.
#[derive(Clone, Copy, Debug, Default, Eq, PartialEq)]
pub enum OptimizationLevel
{
    /// Verify MIR without transforming it.
    None,
    /// Run the established local folding and CFG simplification passes once.
    Basic,
    /// Iterate local folding, dataflow DCE, and CFG canonicalization to a fixpoint.
    #[default]
    Aggressive,
}

/// Configurable verified MIR pass pipeline.
#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub struct OptimizationPipeline
{
    level: OptimizationLevel,
    iteration_limit: usize,
}

impl Default for OptimizationPipeline
{
    fn default() -> Self
    {
        Self {
            level: OptimizationLevel::Aggressive,
            iteration_limit: 8,
        }
    }
}

impl OptimizationPipeline
{
    /// Creates a pipeline for one preset level.
    #[must_use]
    pub const fn new(level: OptimizationLevel) -> Self
    {
        Self {
            level,
            iteration_limit: 8,
        }
    }

    /// Overrides the aggressive fixpoint safety bound.
    #[must_use]
    pub const fn with_iteration_limit(mut self, iteration_limit: usize) -> Self
    {
        self.iteration_limit = iteration_limit;
        self
    }

    /// Configured optimization level.
    #[must_use]
    pub const fn level(self) -> OptimizationLevel
    {
        self.level
    }

    /// Maximum number of aggressive iterations.
    #[must_use]
    pub const fn iteration_limit(self) -> usize
    {
        self.iteration_limit
    }

    /// Verifies, transforms, and verifies a MIR function.
    pub fn run(self, function: Function) -> Result<OptimizedFunction, OptimizationError>
    {
        let diagnostics = verify_function(&function);
        if !diagnostics.is_empty()
        {
            return Err(OptimizationError::InputInvalid(diagnostics));
        }
        let optimized = match self.level
        {
            OptimizationLevel::None => OptimizedFunction {
                function,
                reports: Vec::new(),
            },
            OptimizationLevel::Basic => optimize_function(function),
            OptimizationLevel::Aggressive => self.run_aggressive(function),
        };
        let diagnostics = verify_function(&optimized.function);
        if !diagnostics.is_empty()
        {
            return Err(OptimizationError::OutputInvalid(diagnostics));
        }
        Ok(optimized)
    }

    fn run_aggressive(self, mut function: Function) -> OptimizedFunction
    {
        let mut reports = Vec::new();
        record(
            &mut reports,
            OptimizationPass::SimplifySameTargetBranch,
            simplify_same_target_branches(&mut function),
        );
        let mut optimized = optimize_function(function);
        reports.append(&mut optimized.reports);
        for _ in 0..self.iteration_limit.max(1)
        {
            let mut changed = 0;
            changed += record(
                &mut reports,
                OptimizationPass::SimplifySameTargetBranch,
                simplify_same_target_branches(&mut optimized.function),
            );
            changed += record(
                &mut reports,
                OptimizationPass::DeadPureStatement,
                eliminate_dead_definitions(&mut optimized.function),
            );
            changed += record(
                &mut reports,
                OptimizationPass::RemoveUnusedLocal,
                remove_unused_locals(&mut optimized.function),
            );
            changed += record(
                &mut reports,
                OptimizationPass::CanonicalizeBlockOrder,
                canonicalize_block_order(&mut optimized.function),
            );

            let next = optimize_function(optimized.function);
            let folded = next.reports.iter().map(|report| report.removed_items).sum::<usize>();
            reports.extend(next.reports);
            optimized.function = next.function;
            if changed + folded == 0
            {
                break;
            }
        }
        OptimizedFunction {
            function: optimized.function,
            reports,
        }
    }
}

fn record(reports: &mut Vec<OptimizationReport>, pass: OptimizationPass, changed: usize) -> usize
{
    if changed != 0
    {
        reports.push(OptimizationReport {
            pass,
            removed_items: changed,
        });
    }
    changed
}

fn simplify_same_target_branches(function: &mut Function) -> usize
{
    let mut changed = 0;
    for block in &mut function.blocks
    {
        let Some(Terminator::BranchIf {
            then_block,
            else_block,
            ..
        }) = block.terminator
        else
        {
            continue;
        };
        if then_block == else_block
        {
            block.terminator = Some(Terminator::Goto(then_block));
            changed += 1;
        }
    }
    changed
}

fn eliminate_dead_definitions(function: &mut Function) -> usize
{
    let mut removed = 0;
    loop
    {
        let cfg = ControlFlowGraph::new(function);
        let liveness = Liveness::new(function, &cfg);
        let mut round_removed = 0;
        for block in &mut function.blocks
        {
            let Some(block_liveness) = liveness.block(block.id)
            else
            {
                continue;
            };
            let mut retained = Vec::with_capacity(block.statements.len());
            for (index, statement) in block.statements.drain(..).enumerate()
            {
                let effects = statement_effects(&statement);
                let live_after = if index + 1 < block_liveness.statement_count()
                {
                    block_liveness.before_statement(index + 1).unwrap_or_default()
                }
                else
                {
                    block_liveness.before_terminator()
                };
                let all_definitions_dead = !effects.definitions().is_empty() &&
                    effects.definitions().iter().all(|local| !live_after.contains(local));
                if effects.removable_when_dead() && all_definitions_dead
                {
                    round_removed += 1;
                }
                else
                {
                    retained.push(statement);
                }
            }
            block.statements = retained;
        }
        removed += round_removed;
        if round_removed == 0
        {
            break;
        }
    }
    removed
}

fn remove_unused_locals(function: &mut Function) -> usize
{
    let parameter_locals = function
        .parameters
        .iter()
        .map(|parameter| parameter.local)
        .collect::<HashSet<_>>();
    let mut referenced = parameter_locals.clone();
    for block in &function.blocks
    {
        for statement in &block.statements
        {
            let effects = statement_effects(statement);
            referenced.extend(effects.uses());
            referenced.extend(effects.definitions());
        }
        referenced.extend(terminator_uses(block.terminator.as_ref()));
    }
    let before = function.locals.len();
    function
        .locals
        .retain(|local| referenced.contains(&local.id) || parameter_locals.contains(&local.id));
    before - function.locals.len()
}

fn canonicalize_block_order(function: &mut Function) -> usize
{
    let cfg = ControlFlowGraph::new(function);
    let order = cfg.reverse_postorder();
    if order.len() != function.blocks.len()
    {
        return 0;
    }
    let positions = order
        .iter()
        .enumerate()
        .map(|(index, block)| (*block, index))
        .collect::<HashMap<BlockId, usize>>();
    let already_canonical = function
        .blocks
        .iter()
        .enumerate()
        .all(|(index, block)| positions.get(&block.id) == Some(&index));
    if already_canonical
    {
        return 0;
    }
    function.blocks.sort_by_key(|block| positions[&block.id]);
    1
}

#[cfg(test)]
mod tests
{
    use crate::hir::async_check::Span;
    use crate::mir::{BasicBlock, BlockId, Function, Local, LocalId, Statement, Terminator};
    use crate::xlil::Type;

    use super::{OptimizationLevel, OptimizationPass, OptimizationPipeline};

    fn span() -> Span
    {
        Span::new(1, 0, 1)
    }

    fn local(id: u32, value_type: Type) -> Local
    {
        Local {
            id: LocalId(id),
            name: format!("local{id}"),
            value_type: Some(value_type),
            mutable: false,
            span: span(),
        }
    }

    fn block(id: u32, statements: Vec<Statement>, terminator: Terminator) -> BasicBlock
    {
        BasicBlock {
            id: BlockId(id),
            statements,
            terminator: Some(terminator),
            span: span(),
        }
    }

    #[test]
    fn none_level_only_verifies()
    {
        let function = Function {
            name: "none".to_string(),
            parameters: Vec::new(),
            return_type: Type::VOID,
            locals: vec![local(0, Type::I64)],
            blocks: vec![block(
                0,
                vec![Statement::ConstI64 {
                    local: LocalId(0),
                    value: 7,
                    span: span(),
                }],
                Terminator::Return(None),
            )],
        };
        let result = OptimizationPipeline::new(OptimizationLevel::None)
            .run(function)
            .expect("valid MIR");

        assert!(result.reports.is_empty());
        assert_eq!(result.function.blocks[0].statements.len(), 1);
    }

    #[test]
    fn aggressive_pipeline_removes_dead_constant_chain()
    {
        let function = Function {
            name: "dead".to_string(),
            parameters: Vec::new(),
            return_type: Type::VOID,
            locals: vec![local(0, Type::I64), local(1, Type::I64), local(2, Type::I64)],
            blocks: vec![block(
                0,
                vec![
                    Statement::ConstI64 {
                        local: LocalId(0),
                        value: 20,
                        span: span(),
                    },
                    Statement::ConstI64 {
                        local: LocalId(1),
                        value: 22,
                        span: span(),
                    },
                    Statement::AddI64 {
                        result: LocalId(2),
                        left: LocalId(0),
                        right: LocalId(1),
                        span: span(),
                    },
                ],
                Terminator::Return(None),
            )],
        };
        let result = OptimizationPipeline::default().run(function).expect("valid MIR");

        assert!(result.function.blocks[0].statements.is_empty());
        assert!(result.function.locals.is_empty());
        assert!(
            result
                .reports
                .iter()
                .any(|report| report.pass == OptimizationPass::DeadPureStatement)
        );
    }

    #[test]
    fn aggressive_pipeline_preserves_calls_and_trapping_division()
    {
        let function = Function {
            name: "effects".to_string(),
            parameters: Vec::new(),
            return_type: Type::VOID,
            locals: vec![
                local(0, Type::I64),
                local(1, Type::I64),
                local(2, Type::I64),
                local(3, Type::I64),
            ],
            blocks: vec![block(
                0,
                vec![
                    Statement::ConstI64 {
                        local: LocalId(0),
                        value: 10,
                        span: span(),
                    },
                    Statement::ConstI64 {
                        local: LocalId(1),
                        value: 0,
                        span: span(),
                    },
                    Statement::BinaryI64 {
                        operation: crate::xlil::I64BinaryOperation::Div,
                        result: LocalId(2),
                        left: LocalId(0),
                        right: LocalId(1),
                        span: span(),
                    },
                    Statement::Call {
                        result: Some(LocalId(3)),
                        function: "effect".to_string(),
                        arguments: Vec::new(),
                        return_type: Type::I64,
                        span: span(),
                    },
                ],
                Terminator::Return(None),
            )],
        };
        let result = OptimizationPipeline::default().run(function).expect("valid MIR");

        assert!(
            result.function.blocks[0]
                .statements
                .iter()
                .any(|statement| matches!(statement, Statement::BinaryI64 { .. }))
        );
        assert!(
            result.function.blocks[0]
                .statements
                .iter()
                .any(|statement| matches!(statement, Statement::Call { .. }))
        );
    }

    #[test]
    fn simplifies_branch_with_identical_targets()
    {
        let function = Function {
            name: "same_target".to_string(),
            parameters: Vec::new(),
            return_type: Type::VOID,
            locals: vec![local(0, Type::BOOL)],
            blocks: vec![
                block(0, Vec::new(), Terminator::BranchIf {
                    condition: LocalId(0),
                    then_block: BlockId(1),
                    else_block: BlockId(1),
                }),
                block(1, Vec::new(), Terminator::Return(None)),
            ],
        };
        let result = OptimizationPipeline::default().run(function).expect("valid MIR");

        assert_eq!(result.function.blocks.len(), 1);
        assert_eq!(result.function.blocks[0].terminator, Some(Terminator::Return(None)));
        assert!(
            result
                .reports
                .iter()
                .any(|report| report.pass == OptimizationPass::SimplifySameTargetBranch)
        );
    }

    #[test]
    fn canonicalizes_reachable_blocks_to_reverse_postorder()
    {
        let function = Function {
            name: "order".to_string(),
            parameters: Vec::new(),
            return_type: Type::VOID,
            locals: vec![local(0, Type::BOOL)],
            blocks: vec![
                block(0, Vec::new(), Terminator::BranchIf {
                    condition: LocalId(0),
                    then_block: BlockId(2),
                    else_block: BlockId(1),
                }),
                block(1, Vec::new(), Terminator::Goto(BlockId(3))),
                block(2, Vec::new(), Terminator::Goto(BlockId(3))),
                block(3, Vec::new(), Terminator::Return(None)),
            ],
        };
        let result = OptimizationPipeline::default().run(function).expect("valid MIR");
        let order = result.function.blocks.iter().map(|block| block.id).collect::<Vec<_>>();

        assert_eq!(order, vec![BlockId(0), BlockId(1), BlockId(2), BlockId(3)]);
    }
}
