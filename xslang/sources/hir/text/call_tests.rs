/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 */

use super::{function_to_xhir, parse_xhir_function};
use crate::hir::{Expression, Function, Literal, Local, PrimitiveType, Span, Statement, Type};

fn span() -> Span
{
  Span::new(1, 0, 1)
}

#[test]
fn roundtrips_typed_call_with_nested_generic_parameter()
{
  let result_type = Type::Result { success: Box::new(Type::Primitive(PrimitiveType::Long)),
                                   error: Box::new(Type::Named("Error".to_string())) };
  let function = Function { name: "consume".to_string(),
                            return_type: Some(Type::Primitive(PrimitiveType::Long)),
                            locals: vec![Local { name: "result".to_string(),
                                                 ty: result_type.clone(),
                                                 mutable: false,
                                                 span: span() }],
                            body: vec![Statement::Return {
      value: Some(Expression::Call {
        function: "select".to_string(),
        arguments: vec![Expression::Local { name: "result".to_string(),
                                            span: span() },
                        Expression::Literal { literal: Literal::Integer("7".to_string()),
                                              span: span() }],
        parameter_types: vec![result_type.clone(), Type::Primitive(PrimitiveType::Long)],
        return_type: Box::new(Type::Primitive(PrimitiveType::Long)),
        span: span(),
      }),
      span: span(),
    }] };

  let text = function_to_xhir(&function);
  let parsed = parse_xhir_function(&text).expect("typed call XHIR should parse");

  assert!(text.contains("call select : (Result<Long, Error>, Long) -> Long"));
  assert!(matches!(&parsed.body[0],
                   Statement::Return {
                     value: Some(Expression::Call { parameter_types, arguments, .. }), ..
                   } if parameter_types == &vec![result_type, Type::Primitive(PrimitiveType::Long)]
                     && arguments.len() == 2));
}

#[test]
fn roundtrips_unit_call_statement()
{
  let function = Function { name: "main".to_string(),
                            return_type: None,
                            locals: vec![],
                            body: vec![Statement::Expr(Expression::Call { function: "touch".to_string(),
                                                                          arguments: vec![],
                                                                          parameter_types: vec![],
                                                                          return_type: Box::new(Type::Unit),
                                                                          span: span() })] };

  let text = function_to_xhir(&function);
  let parsed = parse_xhir_function(&text).expect("unit call XHIR should parse");

  assert!(text.contains("call touch : () -> ()"));
  assert!(matches!(&parsed.body[0],
                   Statement::Expr(Expression::Call { return_type, .. }) if return_type.as_ref() == &Type::Unit));
}
