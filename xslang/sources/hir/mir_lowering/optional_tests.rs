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

fn local(name: &str) -> Expression
{
  Expression::Local { name: name.to_string(),
                      span: span() }
}

fn unwrap(value: Expression) -> Expression
{
  Expression::OptionalUnwrap { value: Box::new(value),
                               element_type: Box::new(long()),
                               span: span() }
}

fn coalesce_assign(target: &str, value: Expression) -> Expression
{
  Expression::OptionalCoalesceAssign { target: target.to_string(),
                                       value: Box::new(value),
                                       optional_type: Box::new(optional_long()),
                                       span: span() }
}

fn updating_function(initializer: Expression, replacement: Expression) -> Function
{
  Function { name: "main".to_string(),
             return_type: Some(long()),
             locals: vec![],
             body: vec![Statement::Let { local: Local { name: "value".to_string(),
                                                        ty: optional_long(),
                                                        mutable: true,
                                                        span: span() },
                                         initializer: Some(initializer) },
                        Statement::Expr(coalesce_assign("value", replacement)),
                        Statement::Return { value: Some(unwrap(local("value"))),
                                            span: span() },] }
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

#[test]
fn xhir_round_trips_coalescing_assignment_and_forced_unwrap()
{
  let function = updating_function(Expression::Literal { literal: Literal::None,
                                                         span: span() },
                                   some("7"));
  let xhir = crate::hir::text::function_to_xhir(&function);
  assert!(xhir.contains("optional_coalesce_assign value : Optional<Long>"));
  assert!(xhir.contains("optional_unwrap Long"));
  assert!(xhir.contains("local value"));

  let parsed = crate::hir::text::parse_xhir_function(&xhir).expect("Optional XHIR should parse");
  assert_eq!(crate::hir::text::function_to_xhir(&parsed), xhir);
  assert!(matches!(&parsed.body[1],
                   Statement::Expr(Expression::OptionalCoalesceAssign {
                     target,
                     optional_type,
                     ..
                   }) if target == "value" && optional_type.as_ref() == &optional_long()));
  assert!(matches!(&parsed.body[2],
                   Statement::Return {
                     value: Some(Expression::OptionalUnwrap { element_type, .. }),
                     ..
                   } if element_type.as_ref() == &long()));
}

#[test]
fn lowers_none_coalescing_assignment_and_unwrap_through_xlil()
{
  let function = updating_function(Expression::Literal { literal: Literal::None,
                                                         span: span() },
                                   some("7"));
  let (mir, module) = lower(&function);
  assert!(mir.blocks
             .iter()
             .filter(|block| matches!(block.terminator, Some(mir::Terminator::BranchIf { .. })))
             .count() >=
          2);
  assert!(mir.blocks
             .iter()
             .any(|block| matches!(block.terminator, Some(mir::Terminator::Panic))));
  assert!(mir.blocks
             .iter()
             .flat_map(|block| &block.statements)
             .any(|statement| matches!(statement, mir::Statement::StoreLocal { .. })));

  let xmir = crate::mir::text::function_to_xmir(&mir);
  assert!(xmir.contains("terminator branch_if"));
  assert!(xmir.contains("terminator panic"));
  assert!(xmir.contains("statement store.local"));
  assert_eq!(crate::mir::text::function_to_xmir(&crate::mir::text::parse_xmir_function(&xmir).expect("Optional XMIR \
                                                                                                      should parse")),
             xmir);

  let xlil = crate::xlil::module_to_string(&module);
  assert!(xlil.matches("br_if ").count() >= 2);
  assert!(xlil.contains("panic"));
  assert!(xlil.contains("store %r"));
  let parsed = crate::xlil::parse_module(&xlil).unwrap_or_else(|diagnostics| {
                                                 panic!("Optional XLIL should parse:\n{xlil}\n{diagnostics:#?}")
                                               });
  assert!(crate::xlil::verify_module(&parsed).is_empty());
  assert_eq!(crate::xlil::module_to_string(&parsed), xlil);
}

#[test]
fn coalescing_assignment_keeps_an_existing_some_value()
{
  let function = updating_function(some("5"), some("9"));
  let (mir, module) = lower(&function);
  let branch_count = mir.blocks
                        .iter()
                        .filter(|block| matches!(block.terminator, Some(mir::Terminator::BranchIf { .. })))
                        .count();
  assert_eq!(branch_count, 2);
  assert!(module.functions[0].blocks
                             .iter()
                             .any(|block| matches!(block.terminator, Some(crate::xlil::Terminator::Panic))));
  assert!(module.functions[0].blocks
                             .iter()
                             .flat_map(|block| &block.instructions)
                             .any(|instruction| matches!(instruction, crate::xlil::Instruction::Store { .. })));
}

#[test]
fn forced_unwrap_always_has_an_explicit_failure_edge()
{
  let function = Function { name: "unwrap".to_string(),
                            return_type: Some(long()),
                            locals: vec![],
                            body: vec![Statement::Let { local: Local { name: "value".to_string(),
                                                                       ty: optional_long(),
                                                                       mutable: false,
                                                                       span: span() },
                                                        initializer: Some(some("11")) },
                                       Statement::Return { value: Some(unwrap(local("value"))),
                                                           span: span() },] };
  let (mir, module) = lower(&function);
  let failure = mir.blocks
                   .iter()
                   .find(|block| matches!(block.terminator, Some(mir::Terminator::Panic)))
                   .expect("forced unwrap should have a panic block");
  assert!(failure.statements.is_empty());
  let xlil = crate::xlil::module_to_string(&module);
  assert!(xlil.contains("panic"));
  assert!(xlil.contains("extract "));
}

#[test]
fn rejects_forced_unwrap_of_a_non_optional_value()
{
  let invalid = Function { name: "main".to_string(),
                           return_type: Some(long()),
                           locals: vec![],
                           body: vec![Statement::Return { value:
                                                            Some(Expression::OptionalUnwrap { value:
                                                                                                Box::new(integer("7")),
                                                                                              element_type:
                                                                                                Box::new(long()),
                                                                                              span: span() }),
                                                          span: span() }] };
  let diagnostics = crate::hir::type_check::TypeChecker::new().check_function(&invalid);
  assert_eq!(diagnostics.len(), 1);
  assert_eq!(diagnostics[0].code,
             crate::hir::type_check::DiagnosticCode::UnaryTypeMismatch);
  assert!(diagnostics[0].message.contains("requires Optional"));
}

#[test]
fn rejects_coalescing_assignment_to_an_immutable_binding()
{
  let mut invalid = updating_function(Expression::Literal { literal: Literal::None,
                                                            span: span() },
                                      some("7"));
  let Statement::Let { local, .. } = &mut invalid.body[0]
  else
  {
    unreachable!("fixture starts with a binding");
  };
  local.mutable = false;
  let diagnostics = crate::hir::type_check::TypeChecker::new().check_function(&invalid);
  assert!(diagnostics.iter().any(|diagnostic| {
                              diagnostic.code == crate::hir::type_check::DiagnosticCode::ImmutableAssignment
                            }));
}

#[test]
fn rejects_coalescing_assignment_to_a_non_optional_binding()
{
  let invalid = Function { name: "main".to_string(),
                           return_type: Some(long()),
                           locals: vec![],
                           body: vec![Statement::Let { local: Local { name: "value".to_string(),
                                                                      ty: long(),
                                                                      mutable: true,
                                                                      span: span() },
                                                       initializer: Some(integer("3")) },
                                      Statement::Expr(coalesce_assign("value", some("7"))),
                                      Statement::Return { value: Some(local("value")),
                                                          span: span() },] };
  let diagnostics = crate::hir::type_check::TypeChecker::new().check_function(&invalid);
  assert!(diagnostics.iter().any(|diagnostic| {
                              diagnostic.code == crate::hir::type_check::DiagnosticCode::BinaryTypeMismatch &&
                              diagnostic.message.contains("Optional")
                            }));
}

#[test]
fn rejects_a_coalescing_assignment_with_the_wrong_payload_type()
{
  let invalid =
    updating_function(Expression::Literal { literal: Literal::None,
                                            span: span() },
                      Expression::Call { function: "Some".to_string(),
                                         arguments: vec![Expression::Literal { literal: Literal::Bool(true),
                                                                               span: span() }],
                                         parameter_types: vec![Type::Primitive(PrimitiveType::Bool)],
                                         return_type:
                                           Box::new(Type::Optional { element:
                                                                       Box::new(Type::Primitive(PrimitiveType::Bool)) }),
                                         span: span() });
  let diagnostics = crate::hir::type_check::TypeChecker::new().check_function(&invalid);
  assert!(diagnostics.iter().any(|diagnostic| {
                              diagnostic.code == crate::hir::type_check::DiagnosticCode::LiteralTypeMismatch
                            }));
}
