/*
 * SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
 * SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.0
 */

use crate::hir::Span;
use crate::mir::{self, BasicBlock, BlockId, Local, LocalId};

use super::lowering::MirToXlilLowerer;
use super::parser::parse_module;
use super::verify::verify_module;
use super::writer::module_to_string;
use super::{Function, Instruction, Module, StrComparisonOperation, Terminator, Type};

fn span() -> Span
{
    Span::new(1, 0, 1)
}

#[test]
fn roundtrips_str_comparisons_in_xlil()
{
    let mut module = Module::new("str_comparisons");
    let mut function = Function::definition("same", Type::BOOL, vec![Type::STR, Type::STR]);
    let block = function.append_block("entry");
    let left = function.parameter_value(0).expect("left parameter");
    let right = function.parameter_value(1).expect("right parameter");
    let equal = function
        .compare_str(block, StrComparisonOperation::Equal, left, right)
        .expect("typed Str equality");
    let _ = function
        .compare_str(block, StrComparisonOperation::NotEqual, left, right)
        .expect("typed Str inequality");
    assert!(function.set_return(block, Some(equal)));
    module.add_function(function);
    assert!(verify_module(&module).is_empty());
    let text = module_to_string(&module);
    assert!(text.contains("eq.str %r0, %r1"));
    assert!(text.contains("ne.str %r0, %r1"));
    assert_eq!(parse_module(&text).expect("Str comparisons should parse"), module);
}

#[test]
fn roundtrips_xmir_and_lowers_str_comparison()
{
    let function = mir::Function {
        name: "same".to_string(),
        parameters: vec![],
        return_type: Type::BOOL,
        locals: vec![local(0, Type::STR), local(1, Type::STR), local(2, Type::BOOL)],
        blocks: vec![BasicBlock {
            id: BlockId(0),
            statements: vec![
                mir::Statement::ConstStr {
                    local: LocalId(0),
                    units: vec![0x0078, 0x0073],
                    span: span(),
                },
                mir::Statement::ConstStr {
                    local: LocalId(1),
                    units: vec![0x0078, 0x0073],
                    span: span(),
                },
                mir::Statement::CompareStr {
                    operation: StrComparisonOperation::Equal,
                    result: LocalId(2),
                    left: LocalId(0),
                    right: LocalId(1),
                    span: span(),
                },
            ],
            terminator: Some(mir::Terminator::Return(Some(LocalId(2)))),
            span: span(),
        }],
    };
    assert!(crate::mir::verify::verify_function(&function).is_empty());
    let text = crate::mir::text::function_to_xmir(&function);
    let parsed = crate::mir::text::parse_xmir_function(&text).expect("Str comparison XMIR should parse");
    assert_eq!(crate::mir::text::function_to_xmir(&parsed), text);
    let lowered = MirToXlilLowerer::new()
        .lower_function(&parsed)
        .expect("Str comparison MIR should lower");
    assert!(matches!(lowered.blocks[0].instructions[2], Instruction::CompareStr {
        operation: StrComparisonOperation::Equal,
        ..
    }));
    assert!(matches!(lowered.blocks[0].terminator, Some(Terminator::Return(_))));
}

fn local(id: u32, value_type: Type) -> Local
{
    Local {
        id: LocalId(id),
        name: format!("local_{id}"),
        value_type: Some(value_type),
        mutable: false,
        span: span(),
    }
}
