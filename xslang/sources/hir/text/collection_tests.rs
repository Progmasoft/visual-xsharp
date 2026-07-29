/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 */

use super::{function_to_xhir, parse_xhir_function};
use crate::hir::async_check::Span;
use crate::hir::type_check::{Expression, Function, Literal, Local, MapEntry, PrimitiveType, Statement, Type};

const fn span() -> Span
{
  Span::new(0, 0, 0)
}

fn integer(value: &str) -> Expression
{
  Expression::Literal { literal: Literal::Integer(value.to_string()),
                        span: span() }
}

#[test]
fn roundtrips_builtin_array_and_map_records()
{
  let array_type = Type::Array { element: Box::new(Type::Primitive(PrimitiveType::Int)),
                                 length: Some(2) };
  let map_type = Type::Map { key: Box::new(Type::Primitive(PrimitiveType::Str)),
                             value: Box::new(Type::Primitive(PrimitiveType::Int)) };
  let set_type = Type::Set { element: Box::new(Type::Primitive(PrimitiveType::Int)) };
  let function = Function { name: "collections".to_string(),
                            return_type: None,
                            locals: vec![Local { name: "values".to_string(),
                                                 ty: array_type.clone(),
                                                 mutable: false,
                                                 span: span() },
                                         Local { name: "lookup".to_string(),
                                                 ty: map_type.clone(),
                                                 mutable: false,
                                                 span: span() },
                                         Local { name: "unique".to_string(),
                                                 ty: set_type.clone(),
                                                 mutable: false,
                                                 span: span() }],
                            body: vec![
      Statement::Let { local: Local { name: "values".to_string(),
                                      ty: array_type,
                                      mutable: false,
                                      span: span() },
                       initializer: Some(Expression::Array { elements: vec![integer("1"), integer("2")],
                                                             span: span() }) },
      Statement::Let { local: Local { name: "lookup".to_string(),
                                      ty: map_type,
                                      mutable: false,
                                      span: span() },
                       initializer: Some(Expression::Map {
                         entries: vec![MapEntry { key: Expression::Literal {
                                                   literal: Literal::String("one".to_string()),
                                                   span: span(),
                                                 },
                                                  value: integer("1"),
                                                  span: span() }],
                         span: span(),
                       }) },
      Statement::Let { local: Local { name: "unique".to_string(),
                                      ty: set_type,
                                      mutable: false,
                                      span: span() },
                       initializer: Some(Expression::Set { elements: vec![integer("1"), integer("2")],
                                                           span: span() }) },
    ] };

  let text = function_to_xhir(&function);
  assert!(text.contains("local values: [Int; 2] immutable"));
  assert!(text.contains("local lookup: [Str: Int] immutable"));
  assert!(text.contains("local unique: set [Int] immutable"));
  assert_eq!(parse_xhir_function(&text).expect("collection XHIR should parse"),
             function);
}

#[test]
fn roundtrips_array_index_assignment()
{
  let function = Function { name: "mutate_array".to_string(),
                            return_type: None,
                            locals: Vec::new(),
                            body: vec![Statement::AssignIndex { target: "values".to_string(),
                                                                index: integer("1"),
                                                                value: integer("7"),
                                                                element_type:
                                                                  Type::Primitive(PrimitiveType::Long),
                                                                span: span() }] };

  let text = function_to_xhir(&function);
  assert!(text.contains("assign_index values"));
  assert!(text.contains("type Long"));
  assert_eq!(parse_xhir_function(&text).expect("array assignment XHIR should parse"),
             function);
}
