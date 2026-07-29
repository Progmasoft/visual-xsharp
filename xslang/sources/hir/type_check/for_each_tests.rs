/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 */

use super::*;

const fn span() -> Span
{
  Span::new(1, 0, 1)
}

fn local(name: &str, ty: Type) -> Local
{
  Local { name: name.to_string(),
          ty,
          mutable: false,
          span: span() }
}

fn function(iterable_type: Type, binding_type: Type) -> Function
{
  Function { name: "visit".to_string(),
             return_type: None,
             locals: vec![local("values", iterable_type.clone())],
             body: vec![Statement::ForEach { binding: local("value", binding_type),
                                             iterable: Expression::Local { name: "values".to_string(),
                                                                           span: span() },
                                             iterable_type,
                                             body: Block { statements: vec![Statement::Continue { span:
                                                                                                    span() }],
                                                           tail: None,
                                                           span: span() },
                                             span: span() }] }
}

#[test]
fn accepts_matching_fixed_array_for_each_binding()
{
  let element = Type::Primitive(PrimitiveType::Long);
  let array = Type::Array { element: Box::new(element.clone()),
                            length: Some(3) };
  assert!(TypeChecker::new().check_function(&function(array, element)).is_empty());
}

#[test]
fn rejects_non_array_and_mismatched_for_each_bindings()
{
  let non_array = TypeChecker::new().check_function(&function(Type::Primitive(PrimitiveType::Long),
                                                              Type::Primitive(PrimitiveType::Long)));
  assert!(non_array.iter()
                   .any(|diagnostic| diagnostic.code == DiagnosticCode::ForEachTypeMismatch));

  let array = Type::Array { element: Box::new(Type::Primitive(PrimitiveType::Long)),
                            length: Some(2) };
  let mismatch = TypeChecker::new().check_function(&function(array, Type::Primitive(PrimitiveType::Bool)));
  assert!(mismatch.iter()
                  .any(|diagnostic| diagnostic.code == DiagnosticCode::ForEachTypeMismatch));
}
