/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 */

use super::*;

fn span(start: u32, end: u32) -> Span
{
  Span::new(1, start, end)
}

fn primitive(primitive: PrimitiveType) -> Type
{
  Type::Primitive(primitive)
}

fn local(name: &str, ty: Type, mutable: bool) -> super::super::type_check::Local
{
  super::super::type_check::Local { name: name.to_string(),
                                    ty,
                                    mutable,
                                    span: span(0, 1) }
}

#[test]
fn rejects_surface_result_propagation_without_desugar()
{
  let function = Function { name: "TryWork".to_string(),
                            return_type: None,
                            locals: vec![local("work", primitive(PrimitiveType::Int), false)],
                            body: vec![Statement::Expr(Expression::ResultPropagation {
                              value: Box::new(Expression::Local { name: "work".to_string(),
                                                                  span: span(4, 8) }),
                              span: span(4, 9),
                            })] };

  let diagnostics =
    HirToMirLowerer::new().lower_function(&function)
                          .expect_err("surface Result propagation must be desugared before MIR lowering");

  assert!(diagnostics.iter()
                     .any(|diagnostic| diagnostic.code == DiagnosticCode::UnsupportedExpression));
}

#[test]
fn lowers_desugared_function_without_result_match()
{
  let function = DesugaredFunction { name: "Answer".to_string(),
                                     return_type: Some(primitive(PrimitiveType::Long)),
                                     locals: vec![],
                                     body: vec![DesugaredStatement::Return {
      value: Some(DesugaredExpression::Literal { literal: Literal::Integer("7".to_string()),
                                                 span: span(10, 11) }),
      span: span(3, 11),
    }] };

  let lowered = HirToMirLowerer::new().lower_desugared_function(&function)
                                      .expect("desugared function without ResultMatch should lower");

  assert_eq!(lowered.return_type, XlilType::I32);
  assert!(matches!(lowered.blocks[0].terminator, Some(mir::Terminator::Return(Some(_)))));
}

#[test]
fn rejects_desugared_result_match_without_registered_layout()
{
  let function = DesugaredFunction { name: "TryWork".to_string(),
                                     return_type: Some(Type::Named("Result<()>".to_string())),
                                     locals: vec![],
                                     body: vec![DesugaredStatement::Expr(DesugaredExpression::ResultMatch {
      value: Box::new(DesugaredExpression::Local { name: "work".to_string(),
                                                   span: span(4, 8) }),
      success_binding: "__xs_try_ok_0".to_string(),
      error_binding: "__xs_try_error_0".to_string(),
      success_type: primitive(PrimitiveType::Long),
      error_type: Type::Named("Error".to_string()),
      span: span(4, 9),
    })] };

  let diagnostics = HirToMirLowerer::new().lower_desugared_function(&function)
                                          .expect_err("a desugared Result requires a registered aggregate layout");

  assert!(diagnostics.iter()
                     .any(|diagnostic| diagnostic.code == DiagnosticCode::UnsupportedType));
}
