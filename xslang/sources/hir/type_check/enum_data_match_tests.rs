/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 */

use super::*;
use crate::compiler_core::SourceSpan;
use crate::hir::declarations::{Base, EnumVariant, NominalKind, NominalType, TypeRef, Visibility};

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
               start_column: 0,
               end_line: 1,
               end_column: 1 }
}

fn variant(name: &str, payload: Option<PrimitiveType>, tag: u32) -> EnumVariant
{
  EnumVariant { name: name.to_string(),
                payload: payload.map(TypeRef::Primitive),
                tag,
                span: source_span() }
}

fn declaration(name: &str, bases: &[&str], variants: Vec<EnumVariant>) -> NominalType
{
  NominalType { name: name.to_string(),
                kind: NominalKind::EnumData,
                bases: bases.iter()
                            .map(|base| Base { ty: TypeRef::Named((*base).to_string()),
                                               visibility: Visibility::Internal,
                                               is_virtual: false,
                                               span: source_span() })
                            .collect(),
                fields: Vec::new(),
                variants,
                span: source_span() }
}

fn enum_pattern(enum_type: &str,
                owner: &str,
                variant: &str,
                tag: u32,
                binding: Option<&str>,
                payload_type: Option<PrimitiveType>)
                -> MatchPattern
{
  MatchPattern::EnumDataVariant { enum_type: enum_type.to_string(),
                                  owner: owner.to_string(),
                                  variant: variant.to_string(),
                                  tag,
                                  binding: binding.map(str::to_string),
                                  payload_type: payload_type.map(Type::Primitive) }
}

fn arm(pattern: MatchPattern, tail: Option<Expression>) -> MatchArm
{
  MatchArm { pattern,
             body: Block { statements: Vec::new(),
                           tail: tail.map(Box::new),
                           span: span() },
             span: span() }
}

fn local(name: &str) -> Expression
{
  Expression::Local { name: name.to_string(),
                      span: span() }
}

fn selector_local(enum_type: &str) -> Local
{
  Local { name: "input".to_string(),
          ty: Type::Named(enum_type.to_string()),
          mutable: false,
          span: span() }
}

fn statement(enum_type: &str, arms: Vec<MatchArm>) -> Function
{
  Function { name: "inspect".to_string(),
             return_type: None,
             locals: vec![selector_local(enum_type)],
             body: vec![Statement::Match { selector: local("input"),
                                           selector_type: Type::Named(enum_type.to_string()),
                                           arms,
                                           span: span() }] }
}

fn expression(enum_type: &str, arms: Vec<MatchArm>) -> Function
{
  Function { name: "inspect".to_string(),
             return_type: Some(Type::Primitive(PrimitiveType::Long)),
             locals: vec![selector_local(enum_type)],
             body: vec![Statement::Return { value:
                                              Some(Expression::Match { selector: Box::new(local("input")),
                                                                       selector_type:
                                                                         Box::new(Type::Named(enum_type.to_string())),
                                                                       arms,
                                                                       result_type:
                                                                         Box::new(Type::Primitive(PrimitiveType::Long)),
                                                                       span: span() }),
                                            span: span() }] }
}

fn token_declaration() -> NominalType
{
  declaration("Token", &[], vec![variant("Text", Some(PrimitiveType::String), 0),
                                 variant("Integer", Some(PrimitiveType::Int), 1),
                                 variant("End", None, 2)])
}

#[test]
fn accepts_exhaustive_enum_data_statement_without_else()
{
  let token = token_declaration();
  let function = statement("Token", vec![arm(enum_pattern("Token",
                                                          "Token",
                                                          "Text",
                                                          0,
                                                          Some("text"),
                                                          Some(PrimitiveType::String)),
                                             None),
                                         arm(enum_pattern("Token",
                                                          "Token",
                                                          "Integer",
                                                          1,
                                                          Some("number"),
                                                          Some(PrimitiveType::Int)),
                                             None),
                                         arm(enum_pattern("Token", "Token", "End", 2, None, None),
                                             None)]);
  let diagnostics = TypeChecker::new().with_nominal_types(&[token])
                                      .check_function(&function);
  assert!(diagnostics.is_empty(), "{diagnostics:#?}");
}

#[test]
fn requires_else_when_one_enum_data_variant_is_missing()
{
  let token = token_declaration();
  let function = statement("Token", vec![arm(enum_pattern("Token",
                                                          "Token",
                                                          "Text",
                                                          0,
                                                          None,
                                                          Some(PrimitiveType::String)),
                                             None),
                                         arm(enum_pattern("Token", "Token", "End", 2, None, None),
                                             None)]);
  let diagnostics = TypeChecker::new().with_nominal_types(&[token])
                                      .check_function(&function);
  assert!(diagnostics.iter()
                     .any(|diagnostic| diagnostic.code == DiagnosticCode::MatchRequiresFinalElse));
}

#[test]
fn else_keeps_a_partial_enum_data_match_valid()
{
  let token = token_declaration();
  let function = statement("Token", vec![arm(enum_pattern("Token", "Token", "End", 2, None, None),
                                             None),
                                         arm(MatchPattern::Else, None)]);
  let diagnostics = TypeChecker::new().with_nominal_types(&[token])
                                      .check_function(&function);
  assert!(diagnostics.is_empty(), "{diagnostics:#?}");
}

#[test]
fn rejects_duplicate_enum_data_tag_even_when_metadata_names_differ()
{
  let token = token_declaration();
  let function = statement("Token", vec![arm(enum_pattern("Token",
                                                          "Token",
                                                          "Text",
                                                          0,
                                                          None,
                                                          Some(PrimitiveType::String)),
                                             None),
                                         arm(enum_pattern("Token",
                                                          "Token",
                                                          "Text",
                                                          0,
                                                          None,
                                                          Some(PrimitiveType::String)),
                                             None),
                                         arm(MatchPattern::Else, None)]);
  let diagnostics = TypeChecker::new().with_nominal_types(&[token])
                                      .check_function(&function);
  assert!(diagnostics.iter()
                     .any(|diagnostic| diagnostic.code == DiagnosticCode::DuplicateMatchPattern));
}

#[test]
fn rejects_payload_type_that_selects_no_overload()
{
  let value = declaration("Value", &[], vec![variant("Number", Some(PrimitiveType::Int), 0),
                                             variant("Number", Some(PrimitiveType::Long), 1)]);
  let function = statement("Value", vec![arm(enum_pattern("Value",
                                                          "Value",
                                                          "Number",
                                                          0,
                                                          Some("number"),
                                                          Some(PrimitiveType::Bool)),
                                             None),
                                         arm(MatchPattern::Else, None)]);
  let diagnostics = TypeChecker::new().with_nominal_types(&[value])
                                      .check_function(&function);
  assert!(diagnostics.iter()
                     .any(|diagnostic| diagnostic.code == DiagnosticCode::UnknownEnumVariant));
}

#[test]
fn rejects_forged_owner_or_flattened_tag()
{
  let token = token_declaration();
  let function = statement("Token", vec![arm(enum_pattern("Token",
                                                          "Base",
                                                          "Text",
                                                          99,
                                                          None,
                                                          Some(PrimitiveType::String)),
                                             None),
                                         arm(MatchPattern::Else, None)]);
  let diagnostics = TypeChecker::new().with_nominal_types(&[token])
                                      .check_function(&function);
  assert!(diagnostics.iter()
                     .any(|diagnostic| diagnostic.code == DiagnosticCode::UnknownEnumVariant &&
                                       diagnostic.message.contains("metadata")));
}

#[test]
fn payload_binding_is_visible_inside_its_arm()
{
  let value = declaration("Value", &[], vec![variant("Number", Some(PrimitiveType::Long), 0),
                                             variant("Empty", None, 1)]);
  let function = expression("Value", vec![arm(enum_pattern("Value",
                                                           "Value",
                                                           "Number",
                                                           0,
                                                           Some("number"),
                                                           Some(PrimitiveType::Long)),
                                              Some(local("number"))),
                                          arm(enum_pattern("Value", "Value", "Empty", 1, None,
                                                           None),
                                              Some(Expression::Literal { literal:
                                                                           Literal::Integer("0".to_string()),
                                                                         span: span() }))]);
  let diagnostics = TypeChecker::new().with_nominal_types(&[value])
                                      .check_function(&function);
  assert!(diagnostics.is_empty(), "{diagnostics:#?}");
}

#[test]
fn inherited_variants_participate_in_exhaustiveness()
{
  let root = declaration("Root", &[], vec![variant("Number", Some(PrimitiveType::Long), 0)]);
  let child = declaration("Value", &["Root"], vec![variant("Text",
                                                           Some(PrimitiveType::String),
                                                           0)]);
  let function = statement("Value", vec![arm(enum_pattern("Value",
                                                          "Root",
                                                          "Number",
                                                          0,
                                                          None,
                                                          Some(PrimitiveType::Long)),
                                             None),
                                         arm(enum_pattern("Value",
                                                          "Value",
                                                          "Text",
                                                          1,
                                                          None,
                                                          Some(PrimitiveType::String)),
                                             None)]);
  let diagnostics = TypeChecker::new().with_nominal_types(&[root, child])
                                      .check_function(&function);
  assert!(diagnostics.is_empty(), "{diagnostics:#?}");
}

#[test]
fn inherited_match_rejects_declaration_tag_in_place_of_flattened_tag()
{
  let root = declaration("Root", &[], vec![variant("Number", Some(PrimitiveType::Long), 7)]);
  let child = declaration("Value", &["Root"], vec![variant("Text",
                                                           Some(PrimitiveType::String),
                                                           3)]);
  let function = statement("Value", vec![arm(enum_pattern("Value",
                                                          "Root",
                                                          "Number",
                                                          7,
                                                          None,
                                                          Some(PrimitiveType::Long)),
                                             None),
                                         arm(MatchPattern::Else, None)]);
  let diagnostics = TypeChecker::new().with_nominal_types(&[root, child])
                                      .check_function(&function);
  assert!(diagnostics.iter()
                     .any(|diagnostic| diagnostic.code == DiagnosticCode::UnknownEnumVariant));
}

#[test]
fn payload_binding_does_not_escape_the_arm()
{
  let token = token_declaration();
  let function = Function { name: "scope".to_string(),
                            return_type: None,
                            locals: vec![selector_local("Token")],
                            body: vec![Statement::Match { selector: local("input"),
                                                          selector_type: Type::Named("Token".to_string()),
                                                          arms: vec![arm(enum_pattern("Token",
                                                                                  "Token",
                                                                                  "Text",
                                                                                  0,
                                                                                  Some("text"),
                                                                                  Some(PrimitiveType::String)),
                                                                     None),
                                                                 arm(MatchPattern::Else, None)],
                                                          span: span() },
                                       Statement::Expr(local("text"))] };
  let diagnostics = TypeChecker::new().with_nominal_types(&[token])
                                      .check_function(&function);
  assert!(diagnostics.iter()
                     .any(|diagnostic| diagnostic.code == DiagnosticCode::UnknownLocal));
}
