/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 */

use super::*;
use crate::hir::aggregate_registry;
use crate::hir::type_check::Local;

const fn span() -> Span
{
  Span::new(1, 0, 1)
}

fn long() -> Type
{
  Type::Primitive(PrimitiveType::Long)
}

fn optional_long() -> Type
{
  Type::Optional { element: Box::new(long()) }
}

fn integer(value: &str) -> Expression
{
  Expression::Literal { literal: Literal::Integer(value.to_string()),
                        span: span() }
}

fn function(initializer: Expression) -> Function
{
  Function { name: "main".to_string(),
             return_type: Some(long()),
             locals: vec![],
             body: vec![
               Statement::Let { local: Local { name: "value".to_string(),
                                               ty: optional_long(),
                                               mutable: false,
                                               span: span() },
                                initializer: Some(initializer) },
               Statement::Return {
                 value: Some(Expression::Binary { operator: BinaryOperator::Coalesce,
                                                  left: Box::new(Expression::Local {
                                                    name: "value".to_string(),
                                                    span: span(),
                                                  }),
                                                  right: Box::new(integer("3")),
                                                  span: span() }),
                 span: span(),
               },
             ] }
}

fn some(value: &str) -> Expression
{
  Expression::Call { function: "Some".to_string(),
                     arguments: vec![integer(value)],
                     parameter_types: vec![long()],
                     return_type: Box::new(optional_long()),
                     span: span() }
}

fn lower(function: &Function) -> (mir::Function, crate::xlil::Module)
{
  assert!(crate::hir::type_check::TypeChecker::new().check_function(function)
                                                    .is_empty());
  let registry =
    aggregate_registry::build_functions_with_nominals(&[], std::slice::from_ref(function)).expect("Optional<Long> \
                                                                                                   registry should \
                                                                                                   build");
  let mir = HirToMirLowerer::new().with_aggregate_types(&registry)
                                  .lower_function(function)
                                  .expect("Optional coalescing should lower to MIR");
  assert!(crate::mir::verify::verify_function(&mir).is_empty());

  let lowered = crate::xlil::lowering::MirToXlilLowerer::new().lower_function(&mir)
                                                              .expect("Optional MIR should lower to XLIL");
  let mut module = crate::xlil::Module::new("OptionalCoalesce");
  for layout in &registry.layouts
  {
    assert_eq!(module.add_aggregate_type(&layout.name, layout.fields.clone()),
               Some(layout.value_type));
  }
  module.add_function(lowered);
  assert!(crate::xlil::verify_module(&module).is_empty());
  (mir, module)
}

#[test]
fn lowers_some_coalesce_through_xhir_xmir_and_xlil()
{
  let function = function(some("7"));
  let xhir = crate::hir::text::function_to_xhir(&function);
  assert!(xhir.contains("binary coalesce"));
  let parsed_xhir = crate::hir::text::parse_xhir_function(&xhir).expect("XHIR should parse");
  assert!(matches!(&parsed_xhir.body[0],
                   Statement::Let { local, .. } if local.ty == optional_long()));
  assert!(matches!(&parsed_xhir.body[1], Statement::Return { value:
                                                               Some(Expression::Binary { operator:
                                                                                           BinaryOperator::Coalesce,
                                                                                         .. }),
                                                             .. }));

  let (mir, module) = lower(&function);
  assert!(mir.blocks
             .iter()
             .any(|block| { matches!(block.terminator, Some(mir::Terminator::BranchIf { .. })) }));
  let xmir = crate::mir::text::function_to_xmir(&mir);
  assert!(xmir.contains("statement extract"));
  assert!(xmir.contains("terminator branch_if"));
  assert!(xmir.contains("statement store.local"));
  assert!(xmir.contains("statement load.local"));
  assert_eq!(crate::mir::text::function_to_xmir(&crate::mir::text::parse_xmir_function(&xmir).expect("XMIR should \
                                                                                                      parse")),
             xmir);

  let xlil = crate::xlil::module_to_string(&module);
  assert!(xlil.contains("br_if "));
  assert!(xlil.contains("extract "));
  assert!(xlil.contains("store %r"));
  assert!(xlil.contains(" = load %s"));
  let parsed = crate::xlil::parse_module(&xlil).expect("XLIL should parse");
  assert!(crate::xlil::verify_module(&parsed).is_empty());
  assert_eq!(crate::xlil::module_to_string(&parsed), xlil);
}

#[test]
fn lowers_none_coalesce_with_a_canonical_payload()
{
  let function = function(Expression::Literal { literal: Literal::None,
                                                span: span() });
  let (mir, module) = lower(&function);
  assert!(mir.blocks[0].statements.iter().any(|statement| {
                                           matches!(statement, mir::Statement::ConstBool { value: false,
                                                                                           .. })
                                         }));
  assert!(module.functions[0].blocks.iter().any(|block| {
                                             matches!(block.terminator, Some(crate::xlil::Terminator::BranchIf { .. }))
                                           }));
}

#[test]
fn rejects_coalescing_a_non_optional_left_operand()
{
  let invalid =
    Function { name: "main".to_string(),
               return_type: Some(long()),
               locals: vec![],
               body: vec![Statement::Return { value: Some(Expression::Binary { operator:
                                                                                 BinaryOperator::Coalesce,
                                                                               left: Box::new(integer("7")),
                                                                               right: Box::new(integer("3")),
                                                                               span: span() }),
                                              span: span() }] };
  assert!(!crate::hir::type_check::TypeChecker::new().check_function(&invalid)
                                                     .is_empty());
}
