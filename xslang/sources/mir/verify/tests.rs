/*
 * SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
 * SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.0
 */

use super::*;
use crate::mir::{Local, LocalId, Parameter};
use crate::xlil::Type;

fn span(start: u32, end: u32) -> Span
{
    Span::new(1, start, end)
}

fn local(id: u32) -> Local
{
    Local {
        id: LocalId(id),
        name: format!("local{id}"),
        value_type: None,
        mutable: true,
        span: span(0, 1),
    }
}

fn typed_local(id: u32, value_type: Type) -> Local
{
    Local {
        id: LocalId(id),
        name: format!("local{id}"),
        value_type: Some(value_type),
        mutable: true,
        span: span(0, 1),
    }
}

fn block(id: u32, terminator: Option<Terminator>) -> BasicBlock
{
    BasicBlock {
        id: BlockId(id),
        statements: Vec::new(),
        terminator,
        span: span(id, id + 1),
    }
}

#[test]
fn accepts_valid_function()
{
    let function = Function {
        name: "main".to_string(),
        parameters: vec![Parameter {
            local: LocalId(0),
            name: "input".to_string(),
            value_type: Type::I64,
            span: span(0, 1),
        }],
        return_type: Type::I64,
        locals: vec![],
        blocks: vec![block(0, Some(Terminator::Return(Some(LocalId(0)))))],
    };

    assert!(verify_function(&function).is_empty());
}

#[test]
fn reports_duplicate_ids_and_missing_terminator()
{
    let function = Function {
        name: "bad".to_string(),
        parameters: vec![],
        return_type: Type::VOID,
        locals: vec![local(0), local(0)],
        blocks: vec![block(0, None), block(0, Some(Terminator::Return(None)))],
    };

    let diagnostics = verify_function(&function);

    assert_eq!(diagnostics[0].code, DiagnosticCode::DuplicateLocal);
    assert_eq!(diagnostics[1].code, DiagnosticCode::DuplicateBlock);
    assert_eq!(diagnostics[2].code, DiagnosticCode::MissingTerminator);
}

#[test]
fn reports_unknown_references()
{
    let function = Function {
        name: "bad".to_string(),
        parameters: vec![],
        return_type: Type::VOID,
        locals: vec![local(0)],
        blocks: vec![BasicBlock {
            id: BlockId(0),
            statements: vec![Statement::Use {
                local: LocalId(9),
                span: span(2, 3),
            }],
            terminator: Some(Terminator::Goto(BlockId(9))),
            span: span(0, 1),
        }],
    };

    let diagnostics = verify_function(&function);

    assert_eq!(diagnostics[0].code, DiagnosticCode::UnknownLocal);
    assert_eq!(diagnostics[1].code, DiagnosticCode::UnknownBlock);
}

#[test]
fn reports_return_type_mismatch()
{
    let function = Function {
        name: "bad_return".to_string(),
        parameters: vec![],
        return_type: Type::I64,
        locals: vec![],
        blocks: vec![block(0, Some(Terminator::Return(None)))],
    };

    let diagnostics = verify_function(&function);

    assert_eq!(diagnostics[0].code, DiagnosticCode::ReturnTypeMismatch);
}

#[test]
fn reports_const_i64_type_mismatch()
{
    let function = Function {
        name: "bad_const".to_string(),
        parameters: vec![],
        return_type: Type::VOID,
        locals: vec![local(0)],
        blocks: vec![BasicBlock {
            id: BlockId(0),
            statements: vec![Statement::ConstI64 {
                local: LocalId(0),
                value: 1,
                span: span(1, 2),
            }],
            terminator: Some(Terminator::Return(None)),
            span: span(0, 3),
        }],
    };

    let diagnostics = verify_function(&function);

    assert_eq!(diagnostics[0].code, DiagnosticCode::MissingLocalType);
}

#[test]
fn accepts_i64_add_statement()
{
    let function = Function {
        name: "add".to_string(),
        parameters: vec![],
        return_type: Type::I64,
        locals: vec![
            typed_local(0, Type::I64),
            typed_local(1, Type::I64),
            typed_local(2, Type::I64),
        ],
        blocks: vec![BasicBlock {
            id: BlockId(0),
            statements: vec![Statement::AddI64 {
                result: LocalId(2),
                left: LocalId(0),
                right: LocalId(1),
                span: span(1, 2),
            }],
            terminator: Some(Terminator::Return(Some(LocalId(2)))),
            span: span(0, 3),
        }],
    };

    assert!(verify_function(&function).is_empty());
}

#[test]
fn accepts_i32_const_statement()
{
    let function = Function {
        name: "main".to_string(),
        parameters: vec![],
        return_type: Type::I32,
        locals: vec![typed_local(0, Type::I32)],
        blocks: vec![BasicBlock {
            id: BlockId(0),
            statements: vec![Statement::ConstI32 {
                local: LocalId(0),
                value: 0,
                span: span(1, 2),
            }],
            terminator: Some(Terminator::Return(Some(LocalId(0)))),
            span: span(0, 3),
        }],
    };

    assert!(verify_function(&function).is_empty());
}

#[test]
fn accepts_i32_instruction_family()
{
    let function = Function {
        name: "i32_ops".to_string(),
        parameters: vec![],
        return_type: Type::VOID,
        locals: vec![
            typed_local(0, Type::I32),
            typed_local(1, Type::I32),
            typed_local(2, Type::I32),
            typed_local(3, Type::BOOL),
        ],
        blocks: vec![BasicBlock {
            id: BlockId(0),
            statements: vec![
                Statement::AddI32 {
                    result: LocalId(2),
                    left: LocalId(0),
                    right: LocalId(1),
                    span: span(1, 2),
                },
                Statement::SubI32 {
                    result: LocalId(2),
                    left: LocalId(0),
                    right: LocalId(1),
                    span: span(2, 3),
                },
                Statement::MulI32 {
                    result: LocalId(2),
                    left: LocalId(0),
                    right: LocalId(1),
                    span: span(3, 4),
                },
                Statement::EqI32 {
                    result: LocalId(3),
                    left: LocalId(0),
                    right: LocalId(1),
                    span: span(4, 5),
                },
                Statement::LtI32 {
                    result: LocalId(3),
                    left: LocalId(0),
                    right: LocalId(1),
                    span: span(5, 6),
                },
                Statement::LeI32 {
                    result: LocalId(3),
                    left: LocalId(0),
                    right: LocalId(1),
                    span: span(6, 7),
                },
                Statement::GtI32 {
                    result: LocalId(3),
                    left: LocalId(0),
                    right: LocalId(1),
                    span: span(7, 8),
                },
                Statement::GeI32 {
                    result: LocalId(3),
                    left: LocalId(0),
                    right: LocalId(1),
                    span: span(8, 9),
                },
            ],
            terminator: Some(Terminator::Return(None)),
            span: span(0, 10),
        }],
    };

    assert!(verify_function(&function).is_empty());
}

#[test]
fn rejects_i32_instruction_type_mismatch()
{
    let function = Function {
        name: "bad_i32_ops".to_string(),
        parameters: vec![],
        return_type: Type::VOID,
        locals: vec![
            typed_local(0, Type::I64),
            typed_local(1, Type::I32),
            typed_local(2, Type::I32),
        ],
        blocks: vec![BasicBlock {
            id: BlockId(0),
            statements: vec![Statement::AddI32 {
                result: LocalId(2),
                left: LocalId(0),
                right: LocalId(1),
                span: span(1, 2),
            }],
            terminator: Some(Terminator::Return(None)),
            span: span(0, 3),
        }],
    };

    let diagnostics = verify_function(&function);

    assert_eq!(diagnostics[0].code, DiagnosticCode::LocalTypeMismatch);
}

#[test]
fn accepts_branch_if_with_bool_condition()
{
    let function = Function {
        name: "branch".to_string(),
        parameters: vec![],
        return_type: Type::VOID,
        locals: vec![typed_local(0, Type::BOOL)],
        blocks: vec![
            block(
                0,
                Some(Terminator::BranchIf {
                    condition: LocalId(0),
                    then_block: BlockId(1),
                    else_block: BlockId(2),
                }),
            ),
            block(1, Some(Terminator::Return(None))),
            block(2, Some(Terminator::Return(None))),
        ],
    };

    assert!(verify_function(&function).is_empty());
}

#[test]
fn reports_branch_if_type_and_target_errors()
{
    let function = Function {
        name: "bad_branch".to_string(),
        parameters: vec![],
        return_type: Type::VOID,
        locals: vec![typed_local(0, Type::I64)],
        blocks: vec![block(
            0,
            Some(Terminator::BranchIf {
                condition: LocalId(0),
                then_block: BlockId(1),
                else_block: BlockId(2),
            }),
        )],
    };

    let diagnostics = verify_function(&function);

    assert_eq!(diagnostics[0].code, DiagnosticCode::LocalTypeMismatch);
    assert_eq!(diagnostics[1].code, DiagnosticCode::UnknownBlock);
    assert_eq!(diagnostics[2].code, DiagnosticCode::UnknownBlock);
}

#[test]
fn reports_duplicate_parameters()
{
    let parameter = Parameter {
        local: LocalId(0),
        name: "value".to_string(),
        value_type: Type::I64,
        span: span(0, 1),
    };
    let function = Function {
        name: "bad_parameters".to_string(),
        parameters: vec![parameter.clone(), parameter],
        return_type: Type::VOID,
        locals: vec![],
        blocks: vec![block(0, Some(Terminator::Return(None)))],
    };

    let diagnostics = verify_function(&function);

    assert_eq!(diagnostics[0].code, DiagnosticCode::DuplicateParameter);
}

#[test]
fn verifies_local_storage_types()
{
    let mut function = Function {
        name: "storage".to_string(),
        parameters: vec![],
        return_type: Type::I32,
        locals: vec![
            typed_local(0, Type::I32),
            typed_local(1, Type::I32),
            typed_local(2, Type::I32),
        ],
        blocks: vec![BasicBlock {
            id: BlockId(0),
            statements: vec![
                Statement::ConstI32 {
                    local: LocalId(1),
                    value: 7,
                    span: span(0, 1),
                },
                Statement::StoreLocal {
                    local: LocalId(0),
                    value: LocalId(1),
                    span: span(1, 2),
                },
                Statement::LoadLocal {
                    result: LocalId(2),
                    local: LocalId(0),
                    span: span(2, 3),
                },
            ],
            terminator: Some(Terminator::Return(Some(LocalId(2)))),
            span: span(0, 3),
        }],
    };
    assert!(verify_function(&function).is_empty());

    function.locals[2].value_type = Some(Type::BOOL);
    let diagnostics = verify_function(&function);
    assert!(
        diagnostics
            .iter()
            .any(|value| value.code == DiagnosticCode::LocalTypeMismatch)
    );
}
