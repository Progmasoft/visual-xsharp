/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 */

use super::*;
use crate::compiler_core::SourceSpan;
use crate::hir::declarations::{EnumVariant, NominalKind, NominalType, TypeRef};

fn span() -> Span
{
    Span::new(1, 0, 8)
}

fn source_span() -> SourceSpan
{
    SourceSpan {
        file_id: 1,
        start_offset: 0,
        end_offset: 8,
        start_line: 1,
        start_column: 1,
        end_line: 1,
        end_column: 9,
    }
}

fn value_declaration() -> NominalType
{
    NominalType {
        name: "Value".to_string(),
        kind: NominalKind::EnumData,
        bases: Vec::new(),
        fields: Vec::new(),
        variants: vec![
            EnumVariant {
                name: "Number".to_string(),
                payload: Some(TypeRef::Primitive(PrimitiveType::Int)),
                tag: 0,
                span: source_span(),
            },
            EnumVariant {
                name: "Number".to_string(),
                payload: Some(TypeRef::Primitive(PrimitiveType::Long)),
                tag: 1,
                span: source_span(),
            },
            EnumVariant {
                name: "Empty".to_string(),
                payload: None,
                tag: 2,
                span: source_span(),
            },
        ],
        span: source_span(),
    }
}

fn enum_value(tag: u32, payload_type: PrimitiveType) -> Expression
{
    Expression::EnumData {
        enum_type: "Value".to_string(),
        owner: "Value".to_string(),
        variant: "Number".to_string(),
        tag,
        payload: Some(Box::new(Expression::Local {
            name: "payload".to_string(),
            span: span(),
        })),
        payload_type: Some(Box::new(Type::Primitive(payload_type))),
        span: span(),
    }
}

fn function(value: Expression, payload_type: PrimitiveType) -> Function
{
    Function {
        name: "wrap".to_string(),
        return_type: Some(Type::Named("Value".to_string())),
        locals: vec![Local {
            name: "payload".to_string(),
            ty: Type::Primitive(payload_type),
            mutable: false,
            span: span(),
        }],
        body: vec![Statement::Return {
            value: Some(value),
            span: span(),
        }],
    }
}

#[test]
fn accepts_exact_enum_data_payload_metadata()
{
    let declaration = value_declaration();
    let checked = function(enum_value(1, PrimitiveType::Long), PrimitiveType::Long);
    assert!(
        TypeChecker::new()
            .with_nominal_types(&[declaration])
            .check_function(&checked)
            .is_empty()
    );
}

#[test]
fn rejects_spoofed_enum_data_tag()
{
    let declaration = value_declaration();
    let checked = function(enum_value(0, PrimitiveType::Long), PrimitiveType::Long);
    let diagnostics = TypeChecker::new()
        .with_nominal_types(&[declaration])
        .check_function(&checked);
    assert!(
        diagnostics
            .iter()
            .any(|diagnostic| diagnostic.code == DiagnosticCode::UnknownEnumVariant)
    );
}

#[test]
fn rejects_payload_type_that_does_not_match_selected_overload()
{
    let declaration = value_declaration();
    let mut value = enum_value(0, PrimitiveType::Int);
    let Expression::EnumData {
        payload_type, ..
    } = &mut value
    else
    {
        unreachable!();
    };
    *payload_type = Some(Box::new(Type::Primitive(PrimitiveType::Long)));
    let checked = function(value, PrimitiveType::Int);
    let diagnostics = TypeChecker::new()
        .with_nominal_types(&[declaration])
        .check_function(&checked);
    assert!(
        diagnostics
            .iter()
            .any(|diagnostic| diagnostic.code == DiagnosticCode::UnknownEnumVariant)
    );
    assert!(
        diagnostics
            .iter()
            .any(|diagnostic| diagnostic.code == DiagnosticCode::LiteralTypeMismatch)
    );
}

#[test]
fn accepts_payload_free_enum_data_variant()
{
    let declaration = value_declaration();
    let value = Expression::EnumData {
        enum_type: "Value".to_string(),
        owner: "Value".to_string(),
        variant: "Empty".to_string(),
        tag: 2,
        payload: None,
        payload_type: None,
        span: span(),
    };
    let checked = Function {
        name: "empty".to_string(),
        return_type: Some(Type::Named("Value".to_string())),
        locals: Vec::new(),
        body: vec![Statement::Return {
            value: Some(value),
            span: span(),
        }],
    };
    assert!(
        TypeChecker::new()
            .with_nominal_types(&[declaration])
            .check_function(&checked)
            .is_empty()
    );
}
