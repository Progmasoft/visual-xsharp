/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 */

use super::*;
use crate::hir::aggregate_registry::{AggregateLayout, AggregateRegistry};
use crate::hir::type_check::{Local, TupleFieldType, TupleFieldValue};

const fn span() -> Span
{
  Span::new(1, 0, 1)
}

fn integer(value: &str) -> Expression
{
  Expression::Literal { literal: Literal::Integer(value.to_string()),
                        span: span() }
}

#[test]
fn lowers_named_tuple_construction_and_projection_through_xlil()
{
  let tuple_type = Type::Tuple { fields: vec![TupleFieldType { name: Some("left".to_string()),
                                                               ty: Type::Primitive(PrimitiveType::Long) },
                                              TupleFieldType { name: Some("right".to_string()),
                                                               ty: Type::Primitive(PrimitiveType::Long) }] };
  let tuple_value_type = XlilType::aggregate(0);
  let registry = AggregateRegistry { layouts: vec![AggregateLayout { name: "tuple.0".to_string(),
                                                                     value_type: tuple_value_type,
                                                                     fields: vec![XlilType::I32, XlilType::I32] }],
                                     tuples: vec![(tuple_type.clone(), tuple_value_type)],
                                     ..AggregateRegistry::default() };
  let function = Function { name: "main".to_string(),
                            return_type: Some(Type::Primitive(PrimitiveType::Long)),
                            locals: vec![],
                            body: vec![Statement::Let {
                 local: Local { name: "pair".to_string(),
                                ty: tuple_type.clone(),
                                mutable: false,
                                span: span() },
                 initializer: Some(Expression::Tuple {
                   fields: vec![TupleFieldValue { name: Some("left".to_string()),
                                                  value: integer("2"),
                                                  span: span() },
                                TupleFieldValue { name: Some("right".to_string()),
                                                  value: integer("7"),
                                                  span: span() }],
                   tuple_type: Box::new(tuple_type.clone()),
                   span: span(),
                 }),
               },
               Statement::AssignTupleElement { target: "pair".to_string(),
                                                index: 1,
                                                value: integer("9"),
                                                tuple_type: tuple_type.clone(),
                                                element_type: Type::Primitive(PrimitiveType::Long),
                                                span: span() },
               Statement::Return {
                 value: Some(Expression::TupleElement {
                   tuple: Box::new(Expression::Local { name: "pair".to_string(), span: span() }),
                   index: 1,
                   element_type: Box::new(Type::Primitive(PrimitiveType::Long)),
                   span: span(),
                 }),
                 span: span(),
               }] };

  assert!(crate::hir::type_check::TypeChecker::new().check_function(&function)
                                                    .is_empty());
  let mir = HirToMirLowerer::new().with_aggregate_types(&registry)
                                  .lower_function(&function)
                                  .expect("tuple HIR should lower");
  assert!(crate::mir::verify::verify_function(&mir).is_empty());
  let xlil = crate::xlil::lowering::MirToXlilLowerer::new().lower_function(&mir)
                                                           .expect("tuple MIR should lower");
  let mut module = crate::xlil::Module::new("Tuples");
  assert_eq!(module.add_aggregate_type("tuple.0", vec![XlilType::I32, XlilType::I32]),
             Some(tuple_value_type));
  module.add_function(xlil);
  assert!(crate::xlil::verify_module(&module).is_empty());
  let text = crate::xlil::module_to_string(&module);
  assert!(text.contains(":%t0 = aggregate "));
  assert!(text.contains(":i32 = extract "));
}
