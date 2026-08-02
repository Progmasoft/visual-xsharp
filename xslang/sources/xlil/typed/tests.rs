/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 */

use crate::xlil::{
  FloatBinaryOperation, FloatComparisonOperation, IntegerBinaryOperation, StrComparisonOperation, Type, Utf32Encoding,
  module_to_string, parse_verified,
  types::{AnyValue, F16, Utf32Builder},
};

use super::*;

fn begin_i32(name: &str) -> TypedBuilder
{
  let mut builder = TypedBuilder::new("TypedTests");
  builder.begin(Signature::returning::<i32>(name)).unwrap();
  builder.append_block("entry").unwrap();
  builder
}

#[test]
fn typed_integer_function_roundtrips_through_canonical_text()
{
  let mut builder = TypedBuilder::new("TypedInteger");
  builder.begin(Signature::returning::<i64>("sum").parameter::<i64>().parameter::<i64>())
         .unwrap();
  builder.append_block("entry").unwrap();
  let left = builder.parameter::<i64>(0).unwrap();
  let right = builder.parameter::<i64>(1).unwrap();
  let result = builder.integer(IntegerBinaryOperation::Add, left, right).unwrap();
  builder.return_value(result).unwrap();

  let module = builder.finish().unwrap();
  let text = module_to_string(&module);
  assert!(text.contains(".func sum : (i64, i64) -> i64"));
  assert!(text.contains("%r2:i64 = add.i64 %r0, %r1"));
  assert!(parse_verified(&text).is_ok());
}

#[test]
fn exact_width_integer_constants_keep_their_selected_type()
{
  let mut builder = TypedBuilder::new("Widths");
  builder.begin(Signature::returning::<i16>("small")).unwrap();
  builder.append_block("entry").unwrap();
  let value = builder.const_integer::<i16>(0x7fff).unwrap();
  builder.return_value(value).unwrap();
  let text = module_to_string(&builder.finish().unwrap());
  assert!(text.contains("%r0:i16 = const.i16 0x7fff"));
}

#[test]
fn exact_width_integer_constants_reject_excess_bits_early()
{
  let mut builder = begin_i32("too_wide");
  let error = builder.const_integer::<i8>(0x1ff).unwrap_err();
  assert_eq!(error, TypedBuildError::IntegerConstantOutOfRange { value_type:
                                                                   Type::I8,
                                                                 bits: 0x1ff });
}

#[test]
fn arithmetic_method_rejects_comparison_operations()
{
  let mut builder = begin_i32("bad_arithmetic");
  let left = builder.const_i32(1).unwrap();
  let right = builder.const_i32(2).unwrap();
  let error = builder.integer(IntegerBinaryOperation::Less, left, right).unwrap_err();
  assert_eq!(error, TypedBuildError::TypeMismatch { operation:
                                                      "integer arithmetic result",
                                                    expected: Type::I32,
                                                    actual: Type::BOOL });
}

#[test]
fn comparison_method_rejects_arithmetic_operations()
{
  let mut builder = begin_i32("bad_comparison");
  let left = builder.const_i32(1).unwrap();
  let right = builder.const_i32(2).unwrap();
  let error = builder.compare_integer(IntegerBinaryOperation::Add, left, right)
                     .unwrap_err();
  assert_eq!(error, TypedBuildError::TypeMismatch { operation:
                                                      "integer comparison result",
                                                    expected: Type::BOOL,
                                                    actual: Type::I32 });
}

#[test]
fn typed_branch_requires_boolean_by_construction()
{
  let mut builder = TypedBuilder::new("TypedBranch");
  builder.begin(Signature::returning::<i32>("choose").parameter::<i32>()
                                                     .parameter::<i32>())
         .unwrap();
  let entry = builder.append_block("entry").unwrap();
  let then_block = builder.append_block("then").unwrap();
  let else_block = builder.append_block("else").unwrap();
  builder.position_at_end(entry).unwrap();
  let left = builder.parameter::<i32>(0).unwrap();
  let right = builder.parameter::<i32>(1).unwrap();
  let condition = builder.compare_integer(IntegerBinaryOperation::Greater, left, right)
                         .unwrap();
  builder.branch_if(condition, then_block, else_block).unwrap();
  builder.position_at_end(then_block).unwrap();
  builder.return_value(left).unwrap();
  builder.position_at_end(else_block).unwrap();
  builder.return_value(right).unwrap();

  let text = module_to_string(&builder.finish().unwrap());
  assert!(text.contains("%r2:bool = gt.i32 %r0, %r1"));
  assert!(text.contains("br_if %r2, bb1, bb2"));
}

#[test]
fn boolean_negation_produces_another_boolean()
{
  let mut builder = TypedBuilder::new("TypedBool");
  builder.begin(Signature::returning::<bool>("invert").parameter::<bool>())
         .unwrap();
  builder.append_block("entry").unwrap();
  let input = builder.parameter::<bool>(0).unwrap();
  let output = builder.not(input).unwrap();
  builder.return_value(output).unwrap();
  let text = module_to_string(&builder.finish().unwrap());
  assert!(text.contains("%r1:bool = not.bool %r0"));
}

#[test]
fn typed_slots_preserve_value_identity_across_load_and_store()
{
  let mut builder = begin_i32("slot_value");
  let slot = builder.add_slot::<i32>().unwrap();
  let initial = builder.const_i32(41).unwrap();
  builder.store(slot, initial).unwrap();
  let loaded = builder.load(slot).unwrap();
  let one = builder.const_i32(1).unwrap();
  let answer = builder.integer(IntegerBinaryOperation::Add, loaded, one).unwrap();
  builder.return_value(answer).unwrap();

  let text = module_to_string(&builder.finish().unwrap());
  assert!(text.contains(".slot %s0:i32"));
  assert!(text.contains("store %r0, %s0"));
  assert!(text.contains("load %s0"));
}

#[test]
fn typed_float_arithmetic_uses_ieee_bit_constants()
{
  let mut builder = TypedBuilder::new("TypedFloat");
  builder.begin(Signature::returning::<f32>("average")).unwrap();
  builder.append_block("entry").unwrap();
  let left = builder.const_f32(2.0).unwrap();
  let right = builder.const_f32(4.0).unwrap();
  let sum = builder.float(FloatBinaryOperation::Add, left, right).unwrap();
  let divisor = builder.const_f32(2.0).unwrap();
  let result = builder.float(FloatBinaryOperation::Div, sum, divisor).unwrap();
  builder.return_value(result).unwrap();
  let text = module_to_string(&builder.finish().unwrap());
  assert!(text.contains("const.f32 0x40000000"));
  assert!(text.contains("add.f32"));
  assert!(text.contains("div.f32"));
}

#[test]
fn typed_float_comparison_returns_bool()
{
  let mut builder = TypedBuilder::new("TypedFloatCompare");
  builder.begin(Signature::returning::<bool>("ordered")).unwrap();
  builder.append_block("entry").unwrap();
  let left = builder.const_f64(1.5).unwrap();
  let right = builder.const_f64(2.5).unwrap();
  let result = builder.compare_float(FloatComparisonOperation::Less, left, right)
                      .unwrap();
  builder.return_value(result).unwrap();
  let text = module_to_string(&builder.finish().unwrap());
  assert!(text.contains("lt.f64"));
}

#[test]
fn non_native_float_arithmetic_stays_explicitly_deferred()
{
  let raw = crate::xlil::Builder::new("DeferredHalf");
  let mut builder = TypedBuilder::from_raw(raw);
  builder.begin(Signature::returning::<F16>("half").parameter::<F16>())
         .unwrap();
  builder.append_block("entry").unwrap();
  let value = builder.parameter::<F16>(0).unwrap();
  let error = builder.float(FloatBinaryOperation::Add, value, value).unwrap_err();
  assert_eq!(error, TypedBuildError::TypeMismatch { operation:
                                                      "floating arithmetic",
                                                    expected: Type::F32,
                                                    actual: Type::F16 });
}

#[test]
fn typed_utf32_comparison_hides_rust_source_text()
{
  let mut builder = TypedBuilder::new("TypedText");
  builder.begin(Signature::returning::<bool>("same")).unwrap();
  builder.append_block("entry").unwrap();
  let text = Utf32Builder::with_encoding("Leitwolf", Utf32Encoding::LittleEndian);
  let left = builder.const_utf32(&text).unwrap();
  let right = builder.const_utf32(&text).unwrap();
  let result = builder.compare_utf32(StrComparisonOperation::Equal, left, right)
                      .unwrap();
  builder.return_value(result).unwrap();
  let output = module_to_string(&builder.finish().unwrap());
  assert!(!output.contains("Leitwolf"));
  assert!(output.contains("eq.str"));
}

#[test]
fn utf32_comparison_rejects_erased_non_string_values()
{
  let mut builder = TypedBuilder::new("BadText");
  builder.begin(Signature::returning::<bool>("bad")).unwrap();
  builder.append_block("entry").unwrap();
  let integer = builder.const_i32(1).unwrap().erase();
  let text = builder.const_utf32(&Utf32Builder::new("text")).unwrap();
  let error = builder.compare_utf32(StrComparisonOperation::Equal, integer, text)
                     .unwrap_err();
  assert_eq!(error, TypedBuildError::TypeMismatch { operation: "UTF-32 comparison",
                                                    expected: Type::STR,
                                                    actual: Type::I32 });
}

#[test]
fn typed_checked_calls_validate_arguments_and_result()
{
  let mut builder = TypedBuilder::new("TypedCalls");
  builder.declare(Signature::returning::<i32>("identity").parameter::<i32>())
         .unwrap();
  builder.declare(Signature::void("sink").parameter::<i32>()).unwrap();
  builder.begin(Signature::returning::<i32>("main")).unwrap();
  builder.append_block("entry").unwrap();
  let argument = builder.const_i32(9).unwrap();
  builder.call_void("sink", [argument.erase()]).unwrap();
  let result = builder.call::<i32>("identity", [argument.erase()]).unwrap();
  builder.return_value(result).unwrap();

  let text = module_to_string(&builder.finish().unwrap());
  assert!(text.contains("call sink(%r0)"));
  assert!(text.contains("call identity(%r0)"));
}

#[test]
fn typed_call_rejects_wrong_result_marker_before_emission()
{
  let mut builder = TypedBuilder::new("BadResult");
  builder.declare(Signature::returning::<i64>("wide")).unwrap();
  builder.begin(Signature::returning::<i32>("main")).unwrap();
  builder.append_block("entry").unwrap();
  let error = builder.call::<i32>("wide", []).unwrap_err();
  assert_eq!(error, TypedBuildError::TypeMismatch { operation: "call result",
                                                    expected: Type::I32,
                                                    actual: Type::I64 });
}

#[test]
fn typed_call_reports_unknown_symbols_through_raw_error()
{
  let mut builder = begin_i32("main");
  let error = builder.call::<i32>("missing", []).unwrap_err();
  assert!(matches!(error,
                   TypedBuildError::Raw(crate::xlil::BuildError::UnknownFunction(name)) if name == "missing"));
}

#[test]
fn typed_void_call_rejects_value_producing_callee()
{
  let mut builder = TypedBuilder::new("WrongVoid");
  builder.declare(Signature::returning::<i32>("value")).unwrap();
  builder.begin(Signature::void("main")).unwrap();
  builder.append_block("entry").unwrap();
  let error = builder.call_void("value", []).unwrap_err();
  assert_eq!(error, TypedBuildError::UnexpectedCallResult { function:
                                                              "value".to_owned(),
                                                            actual: Type::I32 });
}

#[test]
fn parameter_lookup_rejects_wrong_marker_and_index()
{
  let mut builder = TypedBuilder::new("Parameters");
  builder.begin(Signature::returning::<i32>("main").parameter::<i32>())
         .unwrap();
  builder.append_block("entry").unwrap();
  assert_eq!(builder.parameter::<i64>(0).unwrap_err(),
             TypedBuildError::TypeMismatch { operation: "parameter",
                                             expected: Type::I64,
                                             actual: Type::I32 });
  assert_eq!(builder.parameter::<i32>(1).unwrap_err(),
             TypedBuildError::ParameterOutOfRange { index: 1,
                                                    count: 1 });
}

#[test]
fn return_contract_is_checked_before_raw_verification()
{
  let mut builder = TypedBuilder::new("Returns");
  builder.begin(Signature::returning::<i64>("wide")).unwrap();
  builder.append_block("entry").unwrap();
  let narrow = builder.const_i32(1).unwrap();
  assert_eq!(builder.return_value(narrow).unwrap_err(),
             TypedBuildError::TypeMismatch { operation: "return",
                                             expected: Type::I32,
                                             actual: Type::I64 });
}

#[test]
fn void_return_contract_rejects_value_function()
{
  let mut builder = begin_i32("not_void");
  assert_eq!(builder.return_void().unwrap_err(),
             TypedBuildError::TypeMismatch { operation: "void return",
                                             expected: Type::VOID,
                                             actual: Type::I32 });
}

#[test]
fn typed_builder_can_end_one_function_and_begin_another()
{
  let mut builder = TypedBuilder::new("Multiple");
  builder.begin(Signature::void("first")).unwrap();
  builder.append_block("entry").unwrap();
  builder.return_void().unwrap();
  builder.end().unwrap();
  builder.begin(Signature::returning::<i32>("second")).unwrap();
  builder.append_block("entry").unwrap();
  let result = builder.const_i32(2).unwrap();
  builder.return_value(result).unwrap();
  let module = builder.finish().unwrap();
  assert!(module.function("first").is_some());
  assert!(module.function("second").is_some());
}

#[test]
fn raw_builder_roundtrip_preserves_current_function_contract()
{
  let mut raw = crate::xlil::Builder::new("RawInterop");
  raw.begin_function("identity", Type::I64, vec![Type::I64]).unwrap();
  raw.append_block("entry").unwrap();
  let mut builder = TypedBuilder::from_raw(raw);
  let parameter = builder.parameter::<i64>(0).unwrap();
  builder.return_value(parameter).unwrap();
  assert!(builder.finish().is_ok());
}

#[test]
fn into_raw_allows_specialized_producers_to_continue()
{
  let mut typed = begin_i32("raw_finish");
  let result = typed.const_i32(0).unwrap();
  typed.return_value(result).unwrap();
  let raw = typed.into_raw();
  assert_eq!(raw.current_function().unwrap().name, "raw_finish");
  assert!(raw.finish().is_ok());
}

#[test]
fn erased_arguments_keep_types_for_user_side_composition()
{
  let value = AnyValue::new(crate::xlil::ValueId(3), Type::I64);
  assert_eq!(value.id(), crate::xlil::ValueId(3));
  assert_eq!(value.value_type(), Type::I64);
  assert!(value.downcast::<i64>().is_ok());
  assert!(value.downcast::<i32>().is_err());
}

#[test]
fn panic_terminator_remains_available_through_typed_facade()
{
  let mut builder = TypedBuilder::new("Panic");
  builder.begin(Signature::void("abort")).unwrap();
  builder.append_block("entry").unwrap();
  builder.panic().unwrap();
  let text = module_to_string(&builder.finish().unwrap());
  assert!(text.contains("panic"));
}
