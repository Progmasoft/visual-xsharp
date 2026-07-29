/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 */

use crate::hir::Span;
use crate::mir::{self, BasicBlock, BlockId, Local, LocalId};

use super::lowering::MirToXlilLowerer;
use super::parser::parse_module;
use super::verify::verify_module;
use super::writer::module_to_string;
use super::{FloatBinaryOperation, FloatComparisonOperation, Function, Instruction, Module, Terminator, Type};

fn span() -> Span
{
  Span::new(1, 0, 1)
}

#[test]
fn roundtrips_all_float_operations_for_both_widths()
{
  let mut module = Module::new("float_operators");
  for value_type in [Type::F32, Type::F64]
  {
    let mut function = Function::definition(format!("ops_{}", super::type_name(value_type)),
                                            value_type,
                                            vec![value_type, value_type]);
    let block = function.append_block("entry");
    let left = function.parameter_value(0).expect("left parameter");
    let right = function.parameter_value(1).expect("right parameter");
    let mut result = left;
    for operation in [FloatBinaryOperation::Add,
                      FloatBinaryOperation::Sub,
                      FloatBinaryOperation::Mul,
                      FloatBinaryOperation::Div,
                      FloatBinaryOperation::Rem]
    {
      result = function.binary_float(block, operation, value_type, result, right)
                       .expect("typed floating operation");
    }
    for operation in [FloatComparisonOperation::Equal,
                      FloatComparisonOperation::NotEqual,
                      FloatComparisonOperation::Less,
                      FloatComparisonOperation::LessEqual,
                      FloatComparisonOperation::Greater,
                      FloatComparisonOperation::GreaterEqual]
    {
      let _ = function.compare_float(block, operation, value_type, left, right)
                      .expect("typed floating comparison");
    }
    assert!(function.set_return(block, Some(result)));
    module.add_function(function);
  }
  assert!(verify_module(&module).is_empty());
  let text = module_to_string(&module);
  let parsed = parse_module(&text).expect("floating operator registry should parse");
  assert_eq!(parsed, module);
}

#[test]
fn roundtrips_xmir_and_lowers_float_operations()
{
  let function = mir::Function { name: "float_add".to_string(),
                                 parameters: vec![],
                                 return_type: Type::F64,
                                 locals: vec![local(0, Type::F64), local(1, Type::F64), local(2, Type::F64)],
                                 blocks: vec![BasicBlock { id: BlockId(0),
                                                           statements:
                                                             vec![mir::Statement::ConstF64 { local: LocalId(0),
                                                                                bits: 1.5_f64.to_bits(),
                                                                                span: span() },
                                                    mir::Statement::ConstF64 { local: LocalId(1),
                                                                                bits: 2.5_f64.to_bits(),
                                                                                span: span() },
                                                    mir::Statement::BinaryFloat {
                                                      operation: FloatBinaryOperation::Add,
                                                      value_type: Type::F64,
                                                      result: LocalId(2),
                                                      left: LocalId(0),
                                                      right: LocalId(1),
                                                      span: span(),
                                                    }],
                                                           terminator:
                                                             Some(mir::Terminator::Return(Some(LocalId(2)))),
                                                           span: span() }] };
  assert!(crate::mir::verify::verify_function(&function).is_empty());
  let text = crate::mir::text::function_to_xmir(&function);
  let parsed = crate::mir::text::parse_xmir_function(&text).expect("floating XMIR should parse");
  assert_eq!(crate::mir::text::function_to_xmir(&parsed), text);
  let lowered = MirToXlilLowerer::new().lower_function(&parsed)
                                       .expect("floating MIR should lower");
  assert!(matches!(lowered.blocks[0].instructions[2],
                   Instruction::BinaryFloat { operation: FloatBinaryOperation::Add,
                                              value_type: Type::F64,
                                              .. }));
  assert!(matches!(lowered.blocks[0].terminator, Some(Terminator::Return(_))));
}

fn local(id: u32, value_type: Type) -> Local
{
  Local { id: LocalId(id),
          name: format!("local_{id}"),
          value_type: Some(value_type),
          mutable: false,
          span: span() }
}

#[test]
fn rejects_mismatched_float_operation_types()
{
  let mut function = Function::definition("bad", Type::F32, vec![]);
  let block = function.append_block("entry");
  let left = function.add_const_f32_bits(block, 0).expect("f32 left");
  let right = function.add_const_f64_bits(block, 0).expect("f64 right");
  assert!(function.binary_float(block, FloatBinaryOperation::Add, Type::F32, left, right)
                  .is_none());
}
