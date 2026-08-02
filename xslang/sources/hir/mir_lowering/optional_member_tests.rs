/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 */

use super::*;
use crate::compiler_core::SourceSpan;
use crate::hir::declarations::{Field, NominalKind, NominalType, TypeRef};
use crate::hir::type_check::{Local, ObjectField};

const fn span() -> Span
{
    Span::new(1, 0, 1)
}

const fn source_span() -> SourceSpan
{
    SourceSpan {
        file_id: 1,
        start_offset: 0,
        end_offset: 1,
        start_line: 1,
        start_column: 1,
        end_line: 1,
        end_column: 2,
    }
}

fn long() -> Type
{
    Type::Primitive(PrimitiveType::Long)
}

fn point_type() -> Type
{
    Type::Named("Point".to_string())
}

fn optional(ty: Type) -> Type
{
    Type::Optional {
        element: Box::new(ty),
    }
}

fn point_declaration() -> NominalType
{
    NominalType {
        name: "Point".to_string(),
        kind: NominalKind::Data,
        bases: Vec::new(),
        fields: vec![
            Field {
                name: "x".to_string(),
                ty: TypeRef::Primitive(PrimitiveType::Long),
                mutable: false,
                span: source_span(),
            },
            Field {
                name: "y".to_string(),
                ty: TypeRef::Primitive(PrimitiveType::Long),
                mutable: false,
                span: source_span(),
            },
        ],
        variants: Vec::new(),
        span: source_span(),
    }
}

fn integer(value: &str) -> Expression
{
    Expression::Literal {
        literal: Literal::Integer(value.to_string()),
        span: span(),
    }
}

fn point(x: &str, y: &str) -> Expression
{
    Expression::Object {
        nominal_type: "Point".to_string(),
        fields: vec![
            ObjectField {
                name: "x".to_string(),
                value: integer(x),
                span: span(),
            },
            ObjectField {
                name: "y".to_string(),
                value: integer(y),
                span: span(),
            },
        ],
        span: span(),
    }
}

fn some_point(x: &str, y: &str) -> Expression
{
    Expression::Call {
        function: "Some".to_string(),
        arguments: vec![point(x, y)],
        parameter_types: vec![point_type()],
        return_type: Box::new(optional(point_type())),
        span: span(),
    }
}

fn optional_member(receiver: Expression, name: &str) -> Expression
{
    Expression::OptionalMember {
        receiver: Box::new(receiver),
        owner: "Point".to_string(),
        name: name.to_string(),
        field_type: Box::new(long()),
        result_type: Box::new(optional(long())),
        span: span(),
    }
}

fn function(initializer: Expression, member: &str, fallback: &str) -> Function
{
    Function {
        name: "main".to_string(),
        return_type: Some(long()),
        locals: vec![],
        body: vec![
            Statement::Let {
                local: Local {
                    name: "point".to_string(),
                    ty: optional(point_type()),
                    mutable: false,
                    span: span(),
                },
                initializer: Some(initializer),
            },
            Statement::Return {
                value: Some(Expression::Binary {
                    operator: BinaryOperator::Coalesce,
                    left: Box::new(optional_member(
                        Expression::Local {
                            name: "point".to_string(),
                            span: span(),
                        },
                        member,
                    )),
                    right: Box::new(integer(fallback)),
                    span: span(),
                }),
                span: span(),
            },
        ],
    }
}

fn lower(function: &Function) -> (mir::Function, crate::xlil::Module)
{
    let declaration = point_declaration();
    let checker = crate::hir::type_check::TypeChecker::new().with_nominal_types(std::slice::from_ref(&declaration));
    assert!(checker.check_function(function).is_empty());
    let registry = crate::hir::aggregate_registry::build_functions_with_nominals(
        std::slice::from_ref(&declaration),
        std::slice::from_ref(function),
    )
    .expect("optional member aggregate registry");
    let mir = HirToMirLowerer::new()
        .with_nominal_types(std::slice::from_ref(&declaration))
        .with_aggregate_types(&registry)
        .lower_function(function)
        .expect("optional member should lower");
    assert!(crate::mir::verify::verify_function(&mir).is_empty());
    let function = crate::xlil::lowering::MirToXlilLowerer::new()
        .lower_function(&mir)
        .expect("optional member MIR should lower");
    let mut module = crate::xlil::Module::new("OptionalMember");
    for layout in &registry.layouts
    {
        assert_eq!(
            module.add_aggregate_type(&layout.name, layout.fields.clone()),
            Some(layout.value_type)
        );
    }
    module.add_function(function);
    assert!(crate::xlil::verify_module(&module).is_empty());
    (mir, module)
}

#[test]
fn xhir_round_trips_optional_member_access()
{
    let function = function(some_point("7", "9"), "x", "3");
    let xhir = crate::hir::text::function_to_xhir(&function);
    assert!(xhir.contains("optional_member Point::x : Long -> Optional<Long>"));
    assert!(xhir.contains("receiver"));
    let parsed = crate::hir::text::parse_xhir_function(&xhir).expect("optional member XHIR should parse");
    assert_eq!(crate::hir::text::function_to_xhir(&parsed), xhir);
    let Statement::Return {
        value: Some(Expression::Binary {
            left, ..
        }),
        ..
    } = &parsed.body[1]
    else
    {
        panic!("fixture should retain the coalescing return");
    };
    assert!(matches!(left.as_ref(),
                   Expression::OptionalMember {
                     owner,
                     name,
                     field_type,
                     result_type,
                     ..
                   } if owner == "Point" && name == "x" && field_type.as_ref() == &long() &&
                        result_type.as_ref() == &optional(long())));
}

#[test]
fn lowers_present_optional_member_through_mir_and_xlil()
{
    let function = function(some_point("7", "9"), "x", "3");
    let (mir, module) = lower(&function);
    assert!(
        mir.blocks
            .iter()
            .filter(|block| matches!(block.terminator, Some(mir::Terminator::BranchIf { .. })))
            .count() >=
            2
    );
    let extracted_fields = mir
        .blocks
        .iter()
        .flat_map(|block| &block.statements)
        .filter_map(|statement| match statement
        {
            mir::Statement::Extract {
                field, ..
            } => Some(*field),
            _ => None,
        })
        .collect::<Vec<_>>();
    assert!(extracted_fields.contains(&0));
    assert!(extracted_fields.contains(&1));

    let xmir = crate::mir::text::function_to_xmir(&mir);
    assert!(xmir.matches("statement extract").count() >= 4);
    assert!(xmir.matches("terminator branch_if").count() >= 2);
    let parsed_mir = crate::mir::text::parse_xmir_function(&xmir).expect("optional member XMIR should parse");
    assert_eq!(crate::mir::text::function_to_xmir(&parsed_mir), xmir);

    let xlil = crate::xlil::module_to_string(&module);
    assert!(xlil.matches("extract ").count() >= 4);
    assert!(xlil.matches("br_if ").count() >= 2);
    assert!(xlil.contains("store %r"));
    let parsed = crate::xlil::parse_module(&xlil).expect("optional member XLIL should parse");
    assert_eq!(crate::xlil::module_to_string(&parsed), xlil);
}

#[test]
fn lowers_absent_optional_member_to_none_before_fallback()
{
    let function = function(
        Expression::Literal {
            literal: Literal::None,
            span: span(),
        },
        "y",
        "13",
    );
    let (mir, module) = lower(&function);
    let false_tags = mir
        .blocks
        .iter()
        .flat_map(|block| &block.statements)
        .filter(|statement| {
            matches!(statement, mir::Statement::ConstBool {
                value: false,
                ..
            })
        })
        .count();
    assert!(false_tags >= 2);
    let text = crate::xlil::module_to_string(&module);
    assert!(text.contains("const.i32 13"));
    assert!(text.contains("const.bool false"));
}

#[test]
fn type_checker_rejects_a_non_optional_member_receiver()
{
    let declaration = point_declaration();
    let invalid = Function {
        name: "bad".to_string(),
        return_type: Some(optional(long())),
        locals: vec![],
        body: vec![Statement::Return {
            value: Some(optional_member(point("1", "2"), "x")),
            span: span(),
        }],
    };
    let diagnostics = crate::hir::type_check::TypeChecker::new()
        .with_nominal_types(std::slice::from_ref(&declaration))
        .check_function(&invalid);
    assert!(diagnostics.iter().any(|diagnostic| {
        diagnostic.code == crate::hir::type_check::DiagnosticCode::UnknownField &&
            diagnostic.message.contains("Optional<Point>")
    }));
}

#[test]
fn type_checker_rejects_an_unknown_optional_field()
{
    let point = point_declaration();
    let invalid = function(some_point("1", "2"), "z", "0");
    let diagnostics = crate::hir::type_check::TypeChecker::new()
        .with_nominal_types(std::slice::from_ref(&point))
        .check_function(&invalid);
    assert!(diagnostics.iter().any(|diagnostic| {
        diagnostic.code == crate::hir::type_check::DiagnosticCode::UnknownField &&
            diagnostic.message.contains("Point") &&
            diagnostic.message.contains('z')
    }));
}

#[test]
fn type_checker_rejects_a_mismatched_optional_member_result()
{
    let point = point_declaration();
    let invalid_member = Expression::OptionalMember {
        receiver: Box::new(Expression::Local {
            name: "point".to_string(),
            span: span(),
        }),
        owner: "Point".to_string(),
        name: "x".to_string(),
        field_type: Box::new(long()),
        result_type: Box::new(optional(Type::Primitive(PrimitiveType::Bool))),
        span: span(),
    };
    let invalid = Function {
        name: "bad".to_string(),
        return_type: Some(optional(Type::Primitive(PrimitiveType::Bool))),
        locals: vec![],
        body: vec![
            Statement::Let {
                local: Local {
                    name: "point".to_string(),
                    ty: optional(point_type()),
                    mutable: false,
                    span: span(),
                },
                initializer: Some(Expression::Literal {
                    literal: Literal::None,
                    span: span(),
                }),
            },
            Statement::Return {
                value: Some(invalid_member),
                span: span(),
            },
        ],
    };
    let diagnostics = crate::hir::type_check::TypeChecker::new()
        .with_nominal_types(std::slice::from_ref(&point))
        .check_function(&invalid);
    assert!(
        diagnostics
            .iter()
            .any(|diagnostic| { diagnostic.code == crate::hir::type_check::DiagnosticCode::LiteralTypeMismatch })
    );
}
