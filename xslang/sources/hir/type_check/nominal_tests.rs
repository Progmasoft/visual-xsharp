/*
 * SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
 * SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.0
 */

use super::*;
use crate::compiler_core::SourceSpan;
use crate::hir::declarations::{Field, NominalKind, NominalType, TypeRef};

fn span() -> Span
{
  Span::new(1, 0, 1)
}

fn source_span() -> SourceSpan
{
  SourceSpan { file_id: 1,
               start_offset: 0,
               end_offset: 1,
               start_line: 1,
               start_column: 1,
               end_line: 1,
               end_column: 2 }
}

fn point_type() -> NominalType
{
  NominalType { name: "Point".to_string(),
                kind: NominalKind::Data,
                bases: Vec::new(),
                fields: vec![Field { name: "x".to_string(),
                                     ty: TypeRef::Primitive(PrimitiveType::Long),
                                     mutable: true,
                                     span: source_span() },
                             Field { name: "y".to_string(),
                                     ty: TypeRef::Primitive(PrimitiveType::Long),
                                     mutable: true,
                                     span: source_span() }],
                variants: Vec::new(),
                span: source_span() }
}

fn field(name: &str, value: i32) -> ObjectField
{
  ObjectField { name: name.to_string(),
                value: Expression::Literal { literal: Literal::Integer(value.to_string()),
                                             span: span() },
                span: span() }
}

fn checked_object(fields: Vec<ObjectField>) -> Function
{
  Function { name: "main".to_string(),
             return_type: None,
             locals: Vec::new(),
             body: vec![Statement::Let { local: Local { name: "point".to_string(),
                                                        ty: Type::Named("Point".to_string()),
                                                        mutable: false,
                                                        span: span() },
                                         initializer: Some(Expression::Object { nominal_type:
                                                                                  "Point".to_string(),
                                                                                fields,
                                                                                span: span() }) }] }
}

#[test]
fn accepts_complete_typed_nominal_object()
{
  let point = point_type();
  let diagnostics = TypeChecker::new().with_nominal_types(std::slice::from_ref(&point))
                                      .check_function(&checked_object(vec![field("x", 2), field("y", 3)]));
  assert!(diagnostics.is_empty());
}

#[test]
fn rejects_missing_unknown_and_duplicate_object_fields()
{
  let point = point_type();
  let diagnostics =
    TypeChecker::new().with_nominal_types(std::slice::from_ref(&point))
                      .check_function(&checked_object(vec![field("x", 2), field("x", 3), field("z", 4)]));
  assert!(diagnostics.iter()
                     .any(|diagnostic| diagnostic.code == DiagnosticCode::DuplicateField));
  assert!(diagnostics.iter()
                     .any(|diagnostic| diagnostic.code == DiagnosticCode::UnknownField));
  assert!(diagnostics.iter()
                     .any(|diagnostic| diagnostic.code == DiagnosticCode::MissingField));
}
