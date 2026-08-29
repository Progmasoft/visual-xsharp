/*
 * SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
 * SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.0
 */

use crate::hir::async_check::Span;
use crate::mir::{BasicBlock, BlockId, Function, Local, LocalId, Statement, Terminator};
use crate::xlil::Type;

use super::{ControlFlowGraph, DominatorTree, Liveness, LoopForest};

fn span() -> Span
{
    Span::new(1, 0, 1)
}

fn block(id: u32, terminator: Terminator) -> BasicBlock
{
    BasicBlock {
        id: BlockId(id),
        statements: Vec::new(),
        terminator: Some(terminator),
        span: span(),
    }
}

fn function(blocks: Vec<BasicBlock>) -> Function
{
    Function {
        name: "analysis".to_string(),
        parameters: Vec::new(),
        return_type: Type::VOID,
        locals: Vec::new(),
        blocks,
    }
}

fn diamond() -> Function
{
    function(vec![
        block(0, Terminator::BranchIf {
            condition: LocalId(0),
            then_block: BlockId(1),
            else_block: BlockId(2),
        }),
        block(1, Terminator::Goto(BlockId(3))),
        block(2, Terminator::Goto(BlockId(3))),
        block(3, Terminator::Return(None)),
    ])
}

#[test]
fn cfg_preserves_branch_order_and_predecessors()
{
    let cfg = ControlFlowGraph::new(&diamond());

    assert_eq!(cfg.entry(), Some(BlockId(0)));
    assert_eq!(cfg.successors(BlockId(0)), &[BlockId(1), BlockId(2)]);
    assert_eq!(cfg.predecessors(BlockId(3)), &[BlockId(1), BlockId(2)]);
    assert!(cfg.has_edge(BlockId(0), BlockId(1)));
    assert!(!cfg.has_edge(BlockId(1), BlockId(2)));
}

#[test]
fn cfg_reports_dangling_edges_without_traversing_them()
{
    let function = function(vec![block(0, Terminator::Goto(BlockId(99)))]);
    let cfg = ControlFlowGraph::new(&function);

    assert_eq!(cfg.dangling_targets(), &[(BlockId(0), BlockId(99))]);
    assert!(cfg.successors(BlockId(0)).is_empty());
    assert_eq!(cfg.exit_blocks(), vec![BlockId(0)]);
}

#[test]
fn cfg_finds_unreachable_blocks_and_stable_orders()
{
    let function = function(vec![
        block(0, Terminator::Goto(BlockId(2))),
        block(1, Terminator::Return(None)),
        block(2, Terminator::Return(None)),
    ]);
    let cfg = ControlFlowGraph::new(&function);

    assert_eq!(cfg.preorder(), vec![BlockId(0), BlockId(2)]);
    assert_eq!(cfg.reverse_postorder(), vec![BlockId(0), BlockId(2)]);
    assert_eq!(cfg.unreachable_blocks(), vec![BlockId(1)]);
}

#[test]
fn cfg_classifies_cycles_with_petgraph()
{
    let function = function(vec![
        block(0, Terminator::Goto(BlockId(1))),
        block(1, Terminator::Goto(BlockId(2))),
        block(2, Terminator::Goto(BlockId(1))),
        block(3, Terminator::Goto(BlockId(3))),
    ]);
    let cfg = ControlFlowGraph::new(&function);
    let components = cfg.strongly_connected_components();

    assert_eq!(components.len(), 3);
    assert_eq!(components[0].blocks(), &[BlockId(0)]);
    assert!(!components[0].is_cyclic());
    assert_eq!(components[1].blocks(), &[BlockId(1), BlockId(2)]);
    assert!(components[1].is_cyclic());
    assert_eq!(components[2].blocks(), &[BlockId(3)]);
    assert!(components[2].is_cyclic());
}

#[test]
fn dominators_solve_a_diamond()
{
    let cfg = ControlFlowGraph::new(&diamond());
    let tree = DominatorTree::new(&cfg);

    assert!(tree.dominates(BlockId(0), BlockId(3)));
    assert!(!tree.dominates(BlockId(1), BlockId(3)));
    assert_eq!(tree.immediate_dominator(BlockId(1)), Some(BlockId(0)));
    assert_eq!(tree.immediate_dominator(BlockId(2)), Some(BlockId(0)));
    assert_eq!(tree.immediate_dominator(BlockId(3)), Some(BlockId(0)));
    assert_eq!(tree.children(BlockId(0)), &[BlockId(1), BlockId(2), BlockId(3)]);
    assert_eq!(tree.frontier(BlockId(1)), &[BlockId(3)]);
    assert_eq!(tree.frontier(BlockId(2)), &[BlockId(3)]);
    assert_eq!(tree.nearest_common_dominator(BlockId(1), BlockId(2)), Some(BlockId(0)));
}

#[test]
fn dominators_handle_linear_depth()
{
    let function = function(vec![
        block(0, Terminator::Goto(BlockId(1))),
        block(1, Terminator::Goto(BlockId(2))),
        block(2, Terminator::Return(None)),
    ]);
    let cfg = ControlFlowGraph::new(&function);
    let tree = DominatorTree::new(&cfg);

    assert_eq!(tree.strict_dominators(BlockId(2)), vec![BlockId(0), BlockId(1)]);
    assert_eq!(tree.immediate_dominator(BlockId(2)), Some(BlockId(1)));
    assert_eq!(tree.depth(BlockId(0)), Some(0));
    assert_eq!(tree.depth(BlockId(2)), Some(2));
    assert_eq!(tree.nearest_common_dominator(BlockId(1), BlockId(2)), Some(BlockId(1)));
}

#[test]
fn natural_loop_collects_header_latch_body_and_exit()
{
    let function = function(vec![
        block(0, Terminator::Goto(BlockId(1))),
        block(1, Terminator::BranchIf {
            condition: LocalId(0),
            then_block: BlockId(2),
            else_block: BlockId(3),
        }),
        block(2, Terminator::Goto(BlockId(1))),
        block(3, Terminator::Return(None)),
    ]);
    let cfg = ControlFlowGraph::new(&function);
    let dominators = DominatorTree::new(&cfg);
    let loops = LoopForest::new(&cfg, &dominators);

    assert_eq!(loops.loops().len(), 1);
    assert_eq!(loops.loops()[0].header(), BlockId(1));
    assert_eq!(loops.loops()[0].latches(), &[BlockId(2)]);
    assert_eq!(loops.loops()[0].blocks(), &[BlockId(1), BlockId(2)]);
    assert_eq!(loops.loops()[0].exits(), &[(BlockId(1), BlockId(3))]);
    assert_eq!(loops.depth(BlockId(0)), 0);
    assert_eq!(loops.depth(BlockId(2)), 1);
    assert_eq!(loops.innermost_loop(BlockId(2)), Some(0));
}

#[test]
fn natural_loop_merges_multiple_latches()
{
    let function = function(vec![
        block(0, Terminator::Goto(BlockId(1))),
        block(1, Terminator::BranchIf {
            condition: LocalId(0),
            then_block: BlockId(2),
            else_block: BlockId(3),
        }),
        block(2, Terminator::Goto(BlockId(1))),
        block(3, Terminator::BranchIf {
            condition: LocalId(1),
            then_block: BlockId(1),
            else_block: BlockId(4),
        }),
        block(4, Terminator::Return(None)),
    ]);
    let cfg = ControlFlowGraph::new(&function);
    let loops = LoopForest::new(&cfg, &DominatorTree::new(&cfg));

    assert_eq!(loops.loops().len(), 1);
    assert_eq!(loops.loops()[0].latches(), &[BlockId(2), BlockId(3)]);
    assert_eq!(loops.loops()[0].blocks(), &[BlockId(1), BlockId(2), BlockId(3)]);
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

#[test]
fn liveness_flows_values_across_a_diamond()
{
    let mut function = diamond();
    function.return_type = Type::I64;
    function.locals = vec![local(0, Type::BOOL), local(1, Type::I64), local(2, Type::I64)];
    function.blocks[0].statements = vec![
        Statement::ConstBool {
            local: LocalId(0),
            value: true,
            span: span(),
        },
        Statement::ConstI64 {
            local: LocalId(1),
            value: 7,
            span: span(),
        },
    ];
    function.blocks[3].statements = vec![Statement::Use {
        local: LocalId(1),
        span: span(),
    }];
    function.blocks[3].terminator = Some(Terminator::Return(Some(LocalId(1))));
    let cfg = ControlFlowGraph::new(&function);
    let liveness = Liveness::new(&function, &cfg);

    assert!(liveness.is_live_out(BlockId(0), LocalId(1)));
    assert!(liveness.is_live_in(BlockId(1), LocalId(1)));
    assert!(liveness.is_live_in(BlockId(2), LocalId(1)));
    assert_eq!(liveness.block(BlockId(3)).expect("join").live_in(), &[LocalId(1)]);
    assert!(!liveness.is_live_in(BlockId(0), LocalId(0)));
}

#[test]
fn liveness_records_statement_boundaries()
{
    let mut function = function(vec![block(0, Terminator::Return(Some(LocalId(2))))]);
    function.return_type = Type::I64;
    function.locals = vec![local(0, Type::I64), local(1, Type::I64), local(2, Type::I64)];
    function.blocks[0].statements = vec![
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
    ];
    let cfg = ControlFlowGraph::new(&function);
    let liveness = Liveness::new(&function, &cfg);
    let facts = liveness.block(BlockId(0)).expect("entry facts");

    assert_eq!(facts.before_statement(0), Some([].as_slice()));
    assert_eq!(facts.before_statement(1), Some([LocalId(0)].as_slice()));
    assert_eq!(facts.before_statement(2), Some([LocalId(0), LocalId(1)].as_slice()));
    assert_eq!(facts.before_terminator(), &[LocalId(2)]);
    assert_eq!(facts.definitions(), &[LocalId(0), LocalId(1), LocalId(2)]);
}

#[test]
fn liveness_treats_call_arguments_as_uses_and_result_as_definition()
{
    let mut function = function(vec![block(0, Terminator::Return(Some(LocalId(1))))]);
    function.return_type = Type::I64;
    function.locals = vec![local(0, Type::I64), local(1, Type::I64)];
    function.blocks[0].statements = vec![Statement::Call {
        result: Some(LocalId(1)),
        function: "identity".to_string(),
        arguments: vec![LocalId(0)],
        return_type: Type::I64,
        span: span(),
    }];
    let cfg = ControlFlowGraph::new(&function);
    let liveness = Liveness::new(&function, &cfg);
    let facts = liveness.block(BlockId(0)).expect("entry facts");

    assert_eq!(facts.upward_exposed_uses(), &[LocalId(0)]);
    assert_eq!(facts.definitions(), &[LocalId(1)]);
    assert_eq!(facts.before_statement(0), Some([LocalId(0)].as_slice()));
}
