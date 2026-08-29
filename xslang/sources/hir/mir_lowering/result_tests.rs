/*
 * SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
 * SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.0
 */

use super::*;
use crate::hir::aggregate_registry;
use crate::hir::result_desugar::ResultDesugar;
use crate::hir::type_check::{DiagnosticCode as TypeDiagnosticCode, Local};

const fn span() -> Span
{
    Span::new(1, 0, 1)
}

fn long() -> Type
{
    Type::Primitive(PrimitiveType::Long)
}

fn boolean() -> Type
{
    Type::Primitive(PrimitiveType::Bool)
}

fn result(success: Type, error: Type) -> Type
{
    Type::Result {
        success: Box::new(success),
        error: Box::new(error),
    }
}

fn result_long() -> Type
{
    result(long(), long())
}

fn integer(value: &str) -> Expression
{
    Expression::Literal {
        literal: Literal::Integer(value.to_string()),
        span: span(),
    }
}

fn local(name: &str) -> Expression
{
    Expression::Local {
        name: name.to_string(),
        span: span(),
    }
}

fn constructor(name: &str, value: Expression, payload_type: Type, return_type: Type) -> Expression
{
    Expression::Call {
        function: name.to_string(),
        arguments: vec![value],
        parameter_types: vec![payload_type],
        return_type: Box::new(return_type),
        span: span(),
    }
}

fn ok(value: &str) -> Expression
{
    constructor("Ok", integer(value), long(), result_long())
}

fn error(value: &str) -> Expression
{
    constructor("Error", integer(value), long(), result_long())
}

fn propagation(value: Expression) -> Expression
{
    Expression::ResultPropagation {
        value: Box::new(value),
        span: span(),
    }
}

fn binding(name: &str, ty: Type, initializer: Expression) -> Statement
{
    Statement::Let {
        local: Local {
            name: name.to_string(),
            ty,
            mutable: false,
            span: span(),
        },
        initializer: Some(initializer),
    }
}

fn success_function() -> Function
{
    Function {
        name: "Forward".to_string(),
        return_type: Some(result_long()),
        locals: vec![],
        body: vec![
            binding("input", result_long(), ok("7")),
            binding("value", long(), propagation(local("input"))),
            Statement::Return {
                value: Some(constructor("Ok", local("value"), long(), result_long())),
                span: span(),
            },
        ],
    }
}

fn failure_function() -> Function
{
    Function {
        name: "ForwardFailure".to_string(),
        return_type: Some(result_long()),
        locals: vec![],
        body: vec![
            binding("input", result_long(), error("13")),
            binding("value", long(), propagation(local("input"))),
            Statement::Return {
                value: Some(constructor("Ok", local("value"), long(), result_long())),
                span: span(),
            },
        ],
    }
}

fn lower(function: &Function) -> (mir::Function, crate::xlil::Module)
{
    let diagnostics = crate::hir::type_check::TypeChecker::new().check_function(function);
    assert!(diagnostics.is_empty(), "{diagnostics:#?}");
    let desugared = ResultDesugar::new()
        .desugar_function(function)
        .expect("typed Result function should desugar");
    let registry = aggregate_registry::build_functions_with_nominals(&[], std::slice::from_ref(function))
        .expect("Result registry should build");
    let mir = HirToMirLowerer::new()
        .with_aggregate_types(&registry)
        .lower_desugared_function(&desugared)
        .expect("Result function should lower to MIR");
    assert!(crate::mir::verify::verify_function(&mir).is_empty());
    assert!(crate::mir::BorrowChecker::new().check_function(&mir).is_empty());
    let lowered = crate::xlil::lowering::MirToXlilLowerer::new()
        .lower_function(&mir)
        .expect("Result MIR should lower to XLIL");
    let mut module = crate::xlil::Module::new("ResultFlow");
    for layout in &registry.layouts
    {
        assert_eq!(
            module.add_aggregate_type(&layout.name, layout.fields.clone()),
            Some(layout.value_type)
        );
    }
    module.add_function(lowered);
    assert!(crate::xlil::verify_module(&module).is_empty());
    (mir, module)
}

#[test]
fn xhir_round_trips_structured_result_and_propagation()
{
    let function = success_function();
    let text = crate::hir::text::function_to_xhir(&function);
    assert!(text.contains("returns Result<Long, Long>"));
    assert!(text.contains("propagate"));
    assert!(text.contains("call Ok"));
    let parsed = crate::hir::text::parse_xhir_function(&text).expect("Result XHIR should parse");
    assert_eq!(crate::hir::text::function_to_xhir(&parsed), text);
    assert_eq!(parsed.return_type, Some(result_long()));
    assert!(matches!(&parsed.body[1], Statement::Let {
        initializer: Some(Expression::ResultPropagation { .. }),
        ..
    }));
}

#[test]
fn lowers_ok_and_propagation_to_explicit_mir_control_flow()
{
    let (mir, module) = lower(&success_function());
    assert!(
        mir.blocks
            .iter()
            .any(|block| matches!(block.terminator, Some(mir::Terminator::BranchIf { .. })))
    );
    assert!(mir.blocks.iter().flat_map(|block| &block.statements).any(
        |statement| matches!(statement, mir::Statement::Aggregate { field_types, .. }
                                       if field_types == &[XlilType::BOOL, XlilType::I32, XlilType::I32])
    ));
    assert!(
        mir.blocks
            .iter()
            .flat_map(|block| &block.statements)
            .filter(|statement| matches!(statement, mir::Statement::Extract { .. }))
            .count() >=
            3
    );

    let xmir = crate::mir::text::function_to_xmir(&mir);
    assert!(xmir.contains("terminator branch_if"));
    assert!(xmir.contains("statement aggregate"));
    assert!(xmir.contains("statement extract"));
    let parsed = crate::mir::text::parse_xmir_function(&xmir).expect("Result XMIR should parse");
    assert_eq!(crate::mir::text::function_to_xmir(&parsed), xmir);

    let xlil = crate::xlil::module_to_string(&module);
    assert!(xlil.contains(".type %t0 result.0 : (bool, i32, i32)"));
    assert!(xlil.contains("br_if "));
    assert!(xlil.contains("extract "));
    let parsed = crate::xlil::parse_module(&xlil)
        .unwrap_or_else(|diagnostics| panic!("Result XLIL should parse:\n{xlil}\n{diagnostics:#?}"));
    assert!(crate::xlil::verify_module(&parsed).is_empty());
    assert_eq!(crate::xlil::module_to_string(&parsed), xlil);
}

#[test]
fn lowers_error_constructor_and_early_error_return()
{
    let (mir, module) = lower(&failure_function());
    let failure = mir
        .blocks
        .iter()
        .find(|block| {
            matches!(block.terminator, Some(mir::Terminator::Return(Some(_)))) &&
                block.statements.iter().any(|statement| {
                    matches!(statement, mir::Statement::Extract {
                        field: 2,
                        ..
                    })
                })
        })
        .expect("propagation should return the error payload from its failure edge");
    assert!(failure.statements.iter().any(|statement| {
        matches!(statement, mir::Statement::ConstBool {
            value: false,
            ..
        })
    }));
    let text = crate::xlil::module_to_string(&module);
    assert!(text.contains("const.bool false"));
    assert!(text.contains(" = aggregate "));
    assert!(text.contains("ret %r"));
}

#[test]
fn result_registry_deduplicates_identical_layouts()
{
    let functions = [success_function(), failure_function()];
    let registry =
        aggregate_registry::build_functions_with_nominals(&[], &functions).expect("Result layouts should build");
    assert_eq!(registry.results.len(), 1);
    assert_eq!(registry.layouts.len(), 1);
    assert_eq!(registry.layouts[0].fields, vec![
        XlilType::BOOL,
        XlilType::I32,
        XlilType::I32
    ]);
}

#[test]
fn result_registry_supports_nested_payloads()
{
    let nested = result(result_long(), long());
    let function = Function {
        name: "Nested".to_string(),
        return_type: Some(nested.clone()),
        locals: vec![],
        body: vec![],
    };
    let registry =
        aggregate_registry::build_functions_with_nominals(&[], &[function]).expect("nested Result should register");
    assert_eq!(registry.results.len(), 2);
    let outer_type = registry
        .results
        .iter()
        .find(|(source, _)| source == &nested)
        .map(|(_, ty)| *ty)
        .expect("outer Result layout");
    let fields = &registry.layouts[outer_type.registry_id as usize].fields;
    assert_eq!(fields[0], XlilType::BOOL);
    assert_eq!(fields[2], XlilType::I32);
    assert_eq!(fields[1].kind, crate::xlil::TypeKind::Aggregate);
}

#[test]
fn type_checker_rejects_propagation_outside_result_function()
{
    let invalid = Function {
        name: "Invalid".to_string(),
        return_type: Some(long()),
        locals: vec![],
        body: vec![binding("input", result_long(), ok("7")), Statement::Return {
            value: Some(propagation(local("input"))),
            span: span(),
        }],
    };
    let diagnostics = crate::hir::type_check::TypeChecker::new().check_function(&invalid);
    assert!(
        diagnostics
            .iter()
            .any(|diagnostic| { diagnostic.code == TypeDiagnosticCode::ResultPropagationReturnMismatch })
    );
}

#[test]
fn type_checker_rejects_mismatched_propagation_error_type()
{
    let input_type = result(long(), boolean());
    let input = constructor("Ok", integer("7"), long(), input_type.clone());
    let invalid = Function {
        name: "InvalidError".to_string(),
        return_type: Some(result_long()),
        locals: vec![],
        body: vec![
            binding("input", input_type, input),
            binding("value", long(), propagation(local("input"))),
            Statement::Return {
                value: Some(ok("1")),
                span: span(),
            },
        ],
    };
    let diagnostics = crate::hir::type_check::TypeChecker::new().check_function(&invalid);
    assert!(
        diagnostics
            .iter()
            .any(|diagnostic| { diagnostic.code == TypeDiagnosticCode::ResultPropagationReturnMismatch })
    );
}

#[test]
fn type_checker_rejects_wrong_constructor_payload()
{
    let invalid = Function {
        name: "InvalidConstructor".to_string(),
        return_type: Some(result_long()),
        locals: vec![],
        body: vec![Statement::Return {
            value: Some(constructor(
                "Ok",
                Expression::Literal {
                    literal: Literal::Bool(true),
                    span: span(),
                },
                long(),
                result_long(),
            )),
            span: span(),
        }],
    };
    let diagnostics = crate::hir::type_check::TypeChecker::new().check_function(&invalid);
    assert!(
        diagnostics
            .iter()
            .any(|diagnostic| { diagnostic.code == TypeDiagnosticCode::LiteralTypeMismatch })
    );
}

fn result_arm(success: bool, binding: &str) -> MatchArm
{
    MatchArm {
        pattern: MatchPattern::ResultVariant {
            success,
            binding: Some(binding.to_string()),
            payload_type: long(),
        },
        body: Block {
            statements: vec![],
            tail: Some(Box::new(local(binding))),
            span: span(),
        },
        span: span(),
    }
}

fn result_match_function(initializer: Expression) -> Function
{
    Function {
        name: "Read".to_string(),
        return_type: Some(long()),
        locals: vec![],
        body: vec![binding("input", result_long(), initializer), Statement::Return {
            value: Some(Expression::Match {
                selector: Box::new(local("input")),
                selector_type: Box::new(result_long()),
                arms: vec![result_arm(true, "value"), result_arm(false, "failure")],
                result_type: Box::new(long()),
                span: span(),
            }),
            span: span(),
        }],
    }
}

#[test]
fn result_match_binds_payloads_and_is_exhaustive_without_else()
{
    let function = result_match_function(ok("7"));
    let diagnostics = crate::hir::type_check::TypeChecker::new().check_function(&function);
    assert!(diagnostics.is_empty(), "{diagnostics:#?}");
    let text = crate::hir::text::function_to_xhir(&function);
    assert!(text.contains("arm result Ok binding value : Long"));
    assert!(text.contains("arm result Error binding failure : Long"));
    assert!(!text.contains("arm else"));
    let parsed = crate::hir::text::parse_xhir_function(&text).expect("Result match XHIR should parse");
    assert_eq!(crate::hir::text::function_to_xhir(&parsed), text);

    let (mir, module) = lower(&function);
    assert_eq!(
        mir.blocks
            .iter()
            .filter(|block| matches!(block.terminator, Some(mir::Terminator::BranchIf { .. })))
            .count(),
        1
    );
    assert!(mir.blocks.iter().flat_map(|block| &block.statements).any(|statement| {
        matches!(statement, mir::Statement::Extract {
            field: 1,
            ..
        })
    }));
    assert!(mir.blocks.iter().flat_map(|block| &block.statements).any(|statement| {
        matches!(statement, mir::Statement::Extract {
            field: 2,
            ..
        })
    }));
    let xlil = crate::xlil::module_to_string(&module);
    assert!(xlil.contains("br_if "));
    assert!(xlil.matches("extract ").count() >= 3);
}

#[test]
fn result_match_error_variant_uses_the_error_payload()
{
    let (mir, _) = lower(&result_match_function(error("9")));
    let error_extract = mir
        .blocks
        .iter()
        .flat_map(|block| &block.statements)
        .find_map(|statement| match statement
        {
            mir::Statement::Extract {
                result,
                field: 2,
                ..
            } => Some(*result),
            _ => None,
        })
        .expect("Error arm should extract field 2");
    assert!(mir.blocks.iter().any(|block| {
        block.statements.iter().any(|statement| {
            matches!(statement,
                                                            mir::Statement::StoreLocal { value, .. }
                                                              if *value == error_extract)
        })
    }));
}

#[test]
fn type_checker_rejects_duplicate_result_match_variants()
{
    let mut invalid = result_match_function(ok("7"));
    let Statement::Return {
        value: Some(Expression::Match {
            arms, ..
        }),
        ..
    } = &mut invalid.body[1]
    else
    {
        unreachable!("fixture contains a match return");
    };
    arms[1] = result_arm(true, "other");
    let diagnostics = crate::hir::type_check::TypeChecker::new().check_function(&invalid);
    assert!(
        diagnostics
            .iter()
            .any(|diagnostic| { diagnostic.code == TypeDiagnosticCode::DuplicateMatchPattern })
    );
    assert!(
        diagnostics
            .iter()
            .any(|diagnostic| { diagnostic.code == TypeDiagnosticCode::MatchRequiresFinalElse })
    );
}

#[test]
fn type_checker_rejects_result_pattern_on_plain_selector()
{
    let invalid = Function {
        name: "InvalidMatch".to_string(),
        return_type: Some(long()),
        locals: vec![],
        body: vec![Statement::Return {
            value: Some(Expression::Match {
                selector: Box::new(integer("7")),
                selector_type: Box::new(long()),
                arms: vec![result_arm(true, "value"), MatchArm {
                    pattern: MatchPattern::Else,
                    body: Block {
                        statements: vec![],
                        tail: Some(Box::new(integer("3"))),
                        span: span(),
                    },
                    span: span(),
                }],
                result_type: Box::new(long()),
                span: span(),
            }),
            span: span(),
        }],
    };
    let diagnostics = crate::hir::type_check::TypeChecker::new().check_function(&invalid);
    assert!(diagnostics.iter().any(|diagnostic| {
        diagnostic.code == TypeDiagnosticCode::LiteralTypeMismatch && diagnostic.message.contains("Result")
    }));
}

#[test]
fn type_checker_rejects_result_pattern_with_inconsistent_payload_type()
{
    let mut invalid = result_match_function(ok("7"));
    let Statement::Return {
        value: Some(Expression::Match {
            arms, ..
        }),
        ..
    } = &mut invalid.body[1]
    else
    {
        unreachable!("fixture contains a match return");
    };
    let MatchPattern::ResultVariant {
        payload_type, ..
    } = &mut arms[0].pattern
    else
    {
        unreachable!("first arm is Ok");
    };
    *payload_type = boolean();
    let diagnostics = crate::hir::type_check::TypeChecker::new().check_function(&invalid);
    assert!(diagnostics.iter().any(|diagnostic| {
        diagnostic.code == TypeDiagnosticCode::LiteralTypeMismatch && diagnostic.message.contains("payload")
    }));
}
