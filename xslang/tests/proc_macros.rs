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
