/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 */

use super::*;
use crate::mir::verify::verify_function;

fn span(start: u32, end: u32) -> Span
{
  Span::new(1, start, end)
}

fn primitive(value: PrimitiveType) -> Type
{
  Type::Primitive(value)
}

#[test]
fn maps_hir_primitives_to_xlil_value_types()
{
  assert_eq!(primitive_to_xlil(PrimitiveType::Bool), Some(XlilType::BOOL));
  assert_eq!(primitive_to_xlil(PrimitiveType::Long), Some(XlilType::I32));
  assert_eq!(primitive_to_xlil(PrimitiveType::Int), Some(XlilType::I64));
  assert_eq!(primitive_to_xlil(PrimitiveType::Integer), Some(XlilType::I128));
  assert_eq!(primitive_to_xlil(PrimitiveType::SFloat), Some(XlilType::F16));
  assert_eq!(primitive_to_xlil(PrimitiveType::LFloat), Some(XlilType::F32));
  assert_eq!(primitive_to_xlil(PrimitiveType::Float), Some(XlilType::F64));
  assert_eq!(primitive_to_xlil(PrimitiveType::Double), Some(XlilType::F128));
  assert_eq!(primitive_to_xlil(PrimitiveType::Str), Some(XlilType::STR));
  assert_eq!(primitive_to_xlil(PrimitiveType::String), Some(XlilType::STRING));
}

#[test]
fn normalizes_contextual_integer_literals_to_bool()
{
  for (source, expected) in [("0", false), ("2", true)]
  {
    let function = Function { name: format!("bool_{source}"),
                              return_type: Some(primitive(PrimitiveType::Bool)),
                              locals: vec![],
                              body: vec![Statement::Return { value: Some(Expression::Literal { literal:
                                                                                   Literal::Integer(source.to_string()),
                                                                                 span: span(10, 11) }),
                                                span: span(3, 12) }] };
    let mir = HirToMirLowerer::new().lower_function(&function)
                                    .expect("contextual integer Bool literal should lower");
    assert!(matches!(mir.blocks[0].statements[0],
                     mir::Statement::ConstBool { value, .. } if value == expected));
  }
}

#[test]
fn lowers_str_literal_through_target_independent_mir()
{
  let function = Function { name: "Name".to_string(),
                            return_type: Some(Type::Reference { referent:
                                                                  Box::new(primitive(PrimitiveType::Str)),
                                                                mutable: false }),
                            locals: vec![],
                            body: vec![Statement::Return { value: Some(Expression::Literal { literal:
                                                                                  Literal::String("xs".to_string()),
                                                                                span: span(10, 14) }),
                                              span: span(3, 14) }] };

  let xhir = crate::hir::text::function_to_xhir(&function);
  assert!(xhir.contains("literal string \"xs\""));
  assert!(!xhir.contains("utf32"));

  let mir = HirToMirLowerer::new().lower_function(&function)
                                  .expect("Str literal should lower");
  assert!(matches!(&mir.blocks[0].statements[0],
                   mir::Statement::ConstStr { units, .. } if units == &[0x00000078, 0x00000073]));
  assert!(verify_function(&mir).is_empty());

  let xmir = crate::mir::text::function_to_xmir(&mir);
  assert!(xmir.contains("value utf32 [0x00000078, 0x00000073]"));
  assert!(!xmir.contains("utf32le"));
  assert!(!xmir.contains("utf32be"));

  let xlil = crate::xlil::lowering::MirToXlilLowerer::new().with_utf32_encoding(crate::xlil::Utf32Encoding::BigEndian)
                                                           .lower_function(&mir)
                                                           .expect("Str MIR should lower to XLIL");
  assert!(matches!(&xlil.blocks[0].instructions[0],
                   crate::xlil::Instruction::ConstStr { encoding: crate::xlil::Utf32Encoding::BigEndian,
                                                        units,
                                                        .. } if units == &[0x00000078, 0x00000073]));
}

#[test]
fn lowers_char_literal_as_one_u32_scalar()
{
  let function = Function { name: "omega".to_string(),
                            return_type: Some(primitive(PrimitiveType::Char)),
                            locals: vec![],
                            body: vec![Statement::Return { value: Some(Expression::Literal { literal:
                                                                                               Literal::Char(0x03a9),
                                                                                             span: span(10,
                                                                                                        18) }),
                                                           span: span(3, 19) }] };
  let xhir = crate::hir::text::function_to_xhir(&function);
  assert!(xhir.contains("literal char '\\u03a9'"));

  let mir = HirToMirLowerer::new().lower_function(&function)
                                  .expect("Char literal should lower");
  assert!(matches!(mir.blocks[0].statements[0], mir::Statement::ConstInteger { value:
                                                                                 mir::IntegerConstant::U32(0x000003a9),
                                                                               .. }));
  assert!(verify_function(&mir).is_empty());
  let xmir = crate::mir::text::function_to_xmir(&mir);
  assert!(xmir.contains("statement const.u32"));
  assert!(xmir.contains("value 937"));
  let parsed = crate::mir::text::parse_xmir_function(&xmir).expect("Char XMIR should parse");
  assert_eq!(crate::mir::text::function_to_xmir(&parsed), xmir);

  let xlil = crate::xlil::lowering::MirToXlilLowerer::new().lower_function(&mir)
                                                           .expect("Char MIR should lower");
  assert!(matches!(xlil.blocks[0].instructions[0],
                   crate::xlil::Instruction::ConstInteger { value, .. }
                     if value.value_type == XlilType::U32 && value.bits == 0x000003a9));
}

#[test]
fn lowers_leading_hir_locals_as_real_mir_parameters()
{
  let parameter = super::super::type_check::Local { name: "value".to_string(),
                                                    ty: primitive(PrimitiveType::Long),
                                                    mutable: false,
                                                    span: span(0, 1) };
  let function =
    Function { name: "identity".to_string(),
               return_type: Some(primitive(PrimitiveType::Long)),
               locals: vec![parameter],
               body: vec![Statement::Return { value: Some(Expression::Local { name: "value".to_string(),
                                                                              span: span(10, 15) }),
                                              span: span(3, 16) }] };

  let mir = HirToMirLowerer::new().lower_function_with_parameters(&function, 1)
                                  .expect("parameter function should lower");
  assert_eq!(mir.parameters.len(), 1);
  assert!(mir.locals.is_empty());
  assert!(matches!(mir.blocks[0].terminator,
                   Some(mir::Terminator::Return(Some(mir::LocalId(0))))));
  assert!(verify_function(&mir).is_empty());
}

#[test]
fn lowers_typed_hir_call_through_mir_and_xlil()
{
  let long = primitive(PrimitiveType::Long);
  let function = Function { name: "main".to_string(),
                            return_type: Some(long.clone()),
                            locals: vec![],
                            body: vec![Statement::Return {
      value: Some(Expression::Call {
        function: "add".to_string(),
        arguments: vec![Expression::Literal { literal: Literal::Integer("3".to_string()),
                                              span: span(10, 11) },
                        Expression::Literal { literal: Literal::Integer("4".to_string()),
                                              span: span(13, 14) }],
        parameter_types: vec![long.clone(), long],
        return_type: Box::new(primitive(PrimitiveType::Long)),
        span: span(6, 15),
      }),
      span: span(0, 16),
    }] };

  let mir = HirToMirLowerer::new().lower_function(&function)
                                  .expect("typed call should lower");
  assert!(matches!(&mir.blocks[0].statements[2],
                   mir::Statement::Call { function, arguments, .. }
                     if function == "add" && arguments.len() == 2));
  assert!(verify_function(&mir).is_empty());

  let xlil = crate::xlil::lowering::MirToXlilLowerer::new().lower_function(&mir)
                                                           .expect("MIR call should lower to XLIL");
  assert!(matches!(&xlil.blocks[0].instructions[2],
                   crate::xlil::Instruction::Call { function, arguments, .. }
                     if function == "add" && arguments.len() == 2));
}

#[test]
fn lowers_unit_and_discarded_value_call_statements()
{
  let long = primitive(PrimitiveType::Long);
  let function = Function { name: "main".to_string(),
                            return_type: None,
                            locals: vec![],
                            body: vec![Statement::Expr(Expression::Call { function: "touch".to_string(),
                                                                          arguments: vec![],
                                                                          parameter_types: vec![],
                                                                          return_type: Box::new(Type::Unit),
                                                                          span: span(1, 2) }),
                                       Statement::Expr(Expression::Call { function: "identity".to_string(),
                                                                          arguments: vec![Expression::Literal {
                                           literal: Literal::Integer("7".to_string()),
                                           span: span(3, 4),
                                         }],
                                                                          parameter_types: vec![long.clone()],
                                                                          return_type: Box::new(long),
                                                                          span: span(3, 5) }),] };

  let mir = HirToMirLowerer::new().lower_function(&function)
                                  .expect("call statements should lower");
  assert!(matches!(&mir.blocks[0].statements[0],
                   mir::Statement::Call { result: None, function, return_type, .. }
                     if function == "touch" && *return_type == crate::xlil::Type::VOID));
  assert!(matches!(&mir.blocks[0].statements[2],
                   mir::Statement::Call { result: Some(_), function, .. } if function == "identity"));
  assert!(verify_function(&mir).is_empty());

  let xlil = crate::xlil::lowering::MirToXlilLowerer::new().lower_function(&mir)
                                                           .expect("MIR calls should lower to XLIL");
  assert!(matches!(&xlil.blocks[0].instructions[0],
                   crate::xlil::Instruction::Call { result: None, function, .. } if function == "touch"));
  assert!(matches!(&xlil.blocks[0].instructions[2],
                   crate::xlil::Instruction::Call { result: Some(_), function, .. } if function == "identity"));
}
