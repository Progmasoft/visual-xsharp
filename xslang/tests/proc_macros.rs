// SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
// SPDX-License-Identifier: MPL-2.0

//! Public procedural-macro integration tests.

#![cfg(feature = "proc-macros")]

use xslang::xlil::types::{F16, I16};
use xslang::xlil::{Type, module_to_string, verify_module};

#[xslang::xlil_create]
fn max(a: i64, b: i64) -> i64
{
  if a > b
  {
    a
  }
  else
  {
    b
  }
}

#[xslang::xlil_create]
fn increment(value: i32) -> i32
{
  value + 1
}

#[xslang::xlil_create]
fn narrow_identity(value: I16) -> I16
{
  value
}

#[xslang::xlil_create]
fn half_identity(value: F16) -> F16
{
  value
}

#[xslang::xlil_create]
fn local_arithmetic(value: i64) -> i64
{
  let doubled = value * 2;
  let adjusted: i64 = doubled + 3;
  adjusted
}

#[xslang::xlil_create]
fn negate_i32(value: i32) -> i32
{
  -value
}

#[xslang::xlil_create]
fn invert_bool(value: bool) -> bool
{
  !value
}

#[xslang::xlil_create]
fn float_score(value: f64) -> f64
{
  let scaled = value * 2.5;
  scaled - 1.0
}

#[xslang::xlil_create]
fn float_is_positive(value: f32) -> bool
{
  value > 0.0
}

#[xslang::xlil_create]
#[allow(clippy::let_and_return)]
fn choose_i32(condition: bool, left: i32, right: i32) -> i32
{
  let selected = if condition
  {
    let offset: i32 = 1;
    left + offset
  }
  else
  {
    right
  };
  selected
}

#[xslang::xlil_create]
fn both(left: bool, right: bool) -> bool
{
  left && right
}

#[xslang::xlil_create]
fn either(left: bool, right: bool) -> bool
{
  left || right
}

#[xslang::xlil_create]
fn conditional_short_circuit(left: bool, right: bool) -> i32
{
  if left && right
  {
    1
  }
  else
  {
    0
  }
}

#[xslang::xlil_create(module = "configured.math",
                      producer = "build_configured_sum",
                      text = "write_configured_sum")]
fn configured_sum(left: i64, right: i64) -> i64
{
  left + right
}

#[test]
fn attributed_function_keeps_its_rust_behavior()
{
  assert_eq!(max(4, 9), 9);
  assert_eq!(increment(41), 42);
  assert_eq!(narrow_identity(7), 7);
  let half = F16::from_bits(0x3c00);
  assert_eq!(half_identity(half), half);
}

#[test]
fn companion_producer_lowers_conditional_control_flow()
{
  let module = max_xlil().expect("macro-generated XLIL should verify");
  assert!(verify_module(&module).is_empty());
  let function = module.function("max").expect("generated module should contain max");
  assert_eq!(function.return_type, Type::I64);
  assert_eq!(function.parameters, vec![Type::I64, Type::I64]);

  let text = module_to_string(&module);
  assert!(text.contains("gt.i64 %r0, %r1"));
  assert!(text.contains("br_if %r2, bb1, bb2"));
  assert!(text.contains("ret %r0"));
  assert!(text.contains("ret %r1"));
}

#[test]
fn companion_producer_lowers_contextual_integer_literals()
{
  let module = increment_xlil().expect("macro-generated XLIL should verify");
  let text = module_to_string(&module);
  assert!(text.contains("%r1:i32 = const.i32 1"));
  assert!(text.contains("%r2:i32 = add.i32 %r0, %r1"));
}

#[test]
fn explicit_rust_types_select_xlil_signature_types()
{
  let narrow = narrow_identity_xlil().unwrap();
  let narrow = narrow.function("narrow_identity").unwrap();
  assert_eq!(narrow.parameters, vec![Type::I16]);
  assert_eq!(narrow.return_type, Type::I16);

  let half = half_identity_xlil().unwrap();
  let half = half.function("half_identity").unwrap();
  assert_eq!(half.parameters, vec![Type::F16]);
  assert_eq!(half.return_type, Type::F16);
}

#[test]
fn companion_producer_preserves_immutable_local_data_flow()
{
  assert_eq!(local_arithmetic(4), 11);
  let module = local_arithmetic_xlil().expect("local bindings should lower");
  assert!(verify_module(&module).is_empty());
  let text = local_arithmetic_xlil_text().expect("canonical text producer should succeed");
  assert!(text.starts_with(".xlil version 1\n.xlil module "));
  assert!(text.contains("mul.i64 %r0, %r1"));
  assert!(text.contains("add.i64 %r2, %r3"));
  assert!(text.contains("ret %r4"));
}

#[test]
fn companion_producer_lowers_numeric_and_boolean_unary_operations()
{
  assert_eq!(negate_i32(13), -13);
  assert!(!invert_bool(true));

  let integer = negate_i32_xlil_text().unwrap();
  assert!(integer.contains("const.i32 0"));
  assert!(integer.contains("sub.i32"));

  let boolean = invert_bool_xlil_text().unwrap();
  assert!(boolean.contains("not.bool %r0"));
  assert!(verify_module(&invert_bool_xlil().unwrap()).is_empty());
}

#[test]
fn companion_producer_lowers_supported_float_operations()
{
  assert_eq!(float_score(4.0), 9.0);
  assert!(float_is_positive(2.0));

  let arithmetic = float_score_xlil_text().unwrap();
  assert!(arithmetic.contains("const.f64"));
  assert!(arithmetic.contains("mul.f64"));
  assert!(arithmetic.contains("sub.f64"));

  let comparison = float_is_positive_xlil_text().unwrap();
  assert!(comparison.contains("const.f32"));
  assert!(comparison.contains("gt.f32"));
  assert!(comparison.contains("ret %r2"));
}

#[test]
fn value_if_uses_a_typed_slot_and_merge_block()
{
  assert_eq!(choose_i32(true, 4, 9), 5);
  assert_eq!(choose_i32(false, 4, 9), 9);

  let module = choose_i32_xlil().unwrap();
  assert!(verify_module(&module).is_empty());
  let text = module_to_string(&module);
  assert!(text.contains(".slot %s0:i32"));
  assert!(text.contains(", %s0"));
  assert!(text.contains("load %s0"));
  assert!(text.contains("if_merge"));
}

#[test]
fn boolean_operators_preserve_short_circuit_control_flow()
{
  assert!(!both(true, false));
  assert!(either(false, true));

  let and_text = both_xlil_text().unwrap();
  assert!(and_text.contains("logic_evaluate"));
  assert!(and_text.contains("logic_short"));
  assert!(and_text.contains("const.bool false"));
  assert!(and_text.contains("br_if %r0"));

  let or_text = either_xlil_text().unwrap();
  assert!(or_text.contains("const.bool true"));
  assert!(verify_module(&either_xlil().unwrap()).is_empty());
}

#[test]
fn short_circuit_value_can_feed_a_terminal_conditional()
{
  assert_eq!(conditional_short_circuit(true, true), 1);
  assert_eq!(conditional_short_circuit(true, false), 0);
  let module = conditional_short_circuit_xlil().unwrap();
  assert!(verify_module(&module).is_empty());
  let text = conditional_short_circuit_xlil_text().unwrap();
  assert!(text.contains("logic_merge"));
  assert!(text.contains("then"));
  assert!(text.contains("else"));
}

#[test]
fn attribute_configuration_controls_public_companion_names()
{
  assert_eq!(configured_sum(8, 13), 21);
  let module = build_configured_sum().unwrap();
  assert_eq!(module.name, "configured.math");
  assert!(verify_module(&module).is_empty());

  let text = write_configured_sum().unwrap();
  assert!(text.contains(".xlil module configured.math"));
  assert!(text.contains(".func configured_sum : (i64, i64) -> i64"));
  assert!(text.contains("add.i64 %r0, %r1"));
}
