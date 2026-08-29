/*
 * SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
 * SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.0
 */

use super::{function_to_xmir, parse_xmir_function};
use crate::hir::Span;
use crate::mir::{BasicBlock, BlockId, Function, Local, LocalId, Statement, Terminator};
use crate::xlil::Type;

fn span() -> Span
{
    Span::new(0, 0, 0)
}

#[test]
fn roundtrips_aggregate_and_extract_statements()
{
    let typed_local = |id, value_type| Local {
        id: LocalId(id),
        name: format!("$tmp{id}"),
        value_type: Some(value_type),
        mutable: false,
        span: span(),
    };
    let aggregate_type = Type::aggregate(0);
    let function = Function {
        name: "aggregate_value".to_string(),
        parameters: Vec::new(),
        return_type: Type::I32,
        locals: vec![
            typed_local(0, Type::I32),
            typed_local(1, Type::I32),
            typed_local(2, aggregate_type),
            typed_local(3, Type::I32),
        ],
        blocks: vec![BasicBlock {
            id: BlockId(0),
            statements: vec![
                Statement::ConstI32 {
                    local: LocalId(0),
                    value: 7,
                    span: span(),
                },
                Statement::ConstI32 {
                    local: LocalId(1),
                    value: 9,
                    span: span(),
                },
                Statement::Aggregate {
                    result: LocalId(2),
                    value_type: aggregate_type,
                    fields: vec![LocalId(0), LocalId(1)],
                    field_types: vec![Type::I32, Type::I32],
                    span: span(),
                },
                Statement::Extract {
                    result: LocalId(3),
                    aggregate: LocalId(2),
                    field: 0,
                    field_type: Type::I32,
                    span: span(),
                },
            ],
            terminator: Some(Terminator::Return(Some(LocalId(3)))),
            span: span(),
        }],
    };

    let text = function_to_xmir(&function);
    let parsed = parse_xmir_function(&text).expect("aggregate XMIR should parse");

    assert!(text.contains("type %t0"));
    assert!(text.contains("statement aggregate"));
    assert!(text.contains("statement extract"));
    assert_eq!(parsed, function);
}

#[test]
fn roundtrips_dynamic_array_access_statements()
{
    let array_type = Type::array(0);
    let typed_local = |id, value_type| Local {
        id: LocalId(id),
        name: format!("$tmp{id}"),
        value_type: Some(value_type),
        mutable: false,
        span: span(),
    };
    let function = Function {
        name: "array_access".to_string(),
        parameters: Vec::new(),
        return_type: Type::I32,
        locals: vec![
            typed_local(0, array_type),
            typed_local(1, Type::I64),
            typed_local(2, Type::I32),
            typed_local(3, array_type),
            typed_local(4, Type::I32),
            typed_local(5, Type::I64),
        ],
        blocks: vec![BasicBlock {
            id: BlockId(0),
            statements: vec![
                Statement::ArraySet {
                    result: LocalId(3),
                    array: LocalId(0),
                    index: LocalId(1),
                    value: LocalId(2),
                    array_type,
                    element_type: Type::I32,
                    span: span(),
                },
                Statement::ArrayGet {
                    result: LocalId(4),
                    array: LocalId(3),
                    index: LocalId(1),
                    array_type,
                    element_type: Type::I32,
                    span: span(),
                },
                Statement::ArrayLength {
                    result: LocalId(5),
                    array: LocalId(3),
                    array_type,
                    span: span(),
                },
            ],
            terminator: Some(Terminator::Return(Some(LocalId(4)))),
            span: span(),
        }],
    };

    let text = function_to_xmir(&function);
    assert!(text.contains("statement array.set"));
    assert!(text.contains("statement array.get"));
    assert!(text.contains("statement array.length"));
    assert_eq!(
        parse_xmir_function(&text).expect("dynamic array XMIR should parse"),
        function
    );
}
