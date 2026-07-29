/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 */

use super::*;

#[test]
fn parses_aggregate_type_registry()
{
  let text = ".xlil version 1\n.xlil module Geometry\n.type %t0 Point : (i32, i32)\n.type %t1 Line : (%t0, \
              %t0)\n.extern midpoint : (%t1) -> %t0\n";
  let module = parse_module(text).expect("aggregate registry should parse");

  assert_eq!(module.aggregate_types.len(), 2);
  assert_eq!(module.aggregate_types[0].name, "Point");
  assert_eq!(module.aggregate_types[1].fields, vec![Type::aggregate(0),
                                                    Type::aggregate(0)]);
  assert_eq!(module.functions[0].return_type, Type::aggregate(0));
  assert_eq!(crate::xlil::module_to_string(&module), text);
}

#[test]
fn rejects_forward_aggregate_field_reference()
{
  let text = ".xlil version 1\n.xlil module Geometry\n.type %t0 Line : (%t1, %t1)\n.type %t1 Point : (i32, i32)\n";

  assert!(parse_module(text).is_err());
}

#[test]
fn verifier_rejects_unknown_aggregate_signature_type()
{
  let mut module = Module::new("Broken");
  module.add_function(Function::declaration("missing", Type::aggregate(7), vec![]));

  assert!(crate::xlil::verify_module(&module).iter().any(|diagnostic| {
                                                      diagnostic.code ==
                                                      crate::xlil::VerifyDiagnosticCode::InvalidAggregateType
                                                    }));
}

#[test]
fn roundtrips_aggregate_and_extract_registers()
{
  let text = ".xlil version 1\n.xlil module Geometry\n.type %t0 Point : (i32, i32)\n.func point_x : (i32, i32) -> \
              i32\n.param %r0:i32\n.param %r1:i32\nbb0.entry:\n  %r2:%t0 = aggregate %r0, %r1\n  %r3:i32 = extract \
              %r2, 0\n  ret %r3\n.end\n";
  let module = parse_module(text).expect("aggregate instructions should parse");

  assert!(crate::xlil::verify_module(&module).is_empty());
  assert_eq!(crate::xlil::module_to_string(&module), text);
}

#[test]
fn roundtrips_fixed_array_registry_and_registers()
{
  let text = ".xlil version 1\n.xlil module Arrays\n.array %a0 : i32 x 3\n.func second : (i32, i32, i32) -> \
              i32\n.param %r0:i32\n.param %r1:i32\n.param %r2:i32\nbb0.entry:\n  %r3:%a0 = array %r0, %r1, %r2\n  \
              %r4:i32 = extract.array %r3, 1\n  ret %r4\n.end\n";
  let module = parse_module(text).expect("fixed array XLIL should parse");

  assert_eq!(module.array_types.len(), 1);
  assert_eq!(module.array_types[0].element_type, Type::I32);
  assert_eq!(module.array_types[0].length, Some(3));
  assert!(crate::xlil::verify_module(&module).is_empty());
  assert_eq!(crate::xlil::module_to_string(&module), text);
}

#[test]
fn roundtrips_runtime_array_registry_length_and_access()
{
  let text = ".xlil version 1\n.xlil module Arrays\n.array %a0 : i32\n.func inspect : (%a0, i64) -> i64\n.param \
              %r0:%a0\n.param %r1:i64\nbb0.entry:\n  %r2:i32 = array.get %r0, %r1\n  %r3:%a0 = array.set %r0, %r1, \
              %r2\n  %r4:i64 = len.array %r3\n  ret %r4\n.end\n";
  let module = parse_module(text).expect("runtime array XLIL should parse");

  assert_eq!(module.array_types[0].length, None);
  assert!(crate::xlil::verify_module(&module).is_empty());
  assert_eq!(crate::xlil::module_to_string(&module), text);
}

#[test]
fn verifier_rejects_wrong_fixed_array_length()
{
  let text = ".xlil version 1\n.xlil module Arrays\n.array %a0 : i32 x 2\n.func bad : (i32) -> %a0\n.param \
              %r0:i32\nbb0.entry:\n  %r1:%a0 = array %r0\n  ret %r1\n.end\n";
  let module = parse_module(text).expect("structurally valid fixed array should parse");

  assert!(crate::xlil::verify_module(&module).iter()
                                             .any(|diagnostic| {
                                               diagnostic.code == crate::xlil::VerifyDiagnosticCode::InvalidArrayType
                                             }));
}

#[test]
fn verifier_rejects_aggregate_field_type_mismatch()
{
  let text = ".xlil version 1\n.xlil module Geometry\n.type %t0 Point : (i32, i32)\n.func bad : (i64, i32) -> \
              %t0\n.param %r0:i64\n.param %r1:i32\nbb0.entry:\n  %r2:%t0 = aggregate %r0, %r1\n  ret %r2\n.end\n";
  let module = parse_module(text).expect("structurally valid aggregate should parse");

  assert!(!crate::xlil::verify_module(&module).is_empty());
}
use crate::xlil::module_to_string;

#[test]
fn parses_function_declaration()
{
  let module =
    parse_module(".xlil version 0\n.xlil module App\n.extern xs$App$External : (i64) -> i64\n").expect("parse should \
                                                                                                        succeed");

  assert_eq!(module.name, "App");
  assert_eq!(module.functions.len(), 1);
  assert!(!module.functions[0].is_definition);
  assert_eq!(module.functions[0].parameters, vec![Type::I64]);
}

#[test]
fn parses_explicit_parameter_records()
{
  let text =
    ".xlil version 1\n.xlil module App\n.func Identity : (i64) -> i64\n.param %r0:i64\nbb0.entry:\n  ret %r0\n.end\n";
  let module = parse_module(text).expect("parameterized function should parse");

  assert_eq!(module.functions[0].parameter_value(0), Some(ValueId(0)));
  assert_eq!(module.functions[0].values.len(), 1);
}

#[test]
fn rejects_missing_version_header()
{
  let diagnostics =
    parse_module(".xlil module App\n.extern xs$App$External : (i64) -> i64\n").expect_err("parse must fail");

  assert_eq!(diagnostics[0].code, DiagnosticCode::InvalidVersionHeader);
}

#[test]
fn rejects_empty_input()
{
  let diagnostics = parse_module("").expect_err("empty input must fail");

  assert_eq!(diagnostics[0].code, DiagnosticCode::EmptyInput);
}

#[test]
fn rejects_unsupported_version()
{
  let diagnostics = parse_module(".xlil version 2\n.xlil module App\n").expect_err("unsupported version must fail");

  assert_eq!(diagnostics[0].code, DiagnosticCode::InvalidVersionHeader);
  assert!(diagnostics[0].message.contains("version 2 is not supported"));
}

#[test]
fn rejects_invalid_version_number()
{
  let diagnostics = parse_module(".xlil version current\n.xlil module App\n").expect_err("invalid version must fail");

  assert_eq!(diagnostics[0].code, DiagnosticCode::InvalidVersionHeader);
  assert!(diagnostics[0].message.contains("version number is invalid"));
}

#[test]
fn roundtrips_const_i64_function()
{
  let text = ".xlil version 1\n.xlil module App\n.func xs$App$Value : () -> i64\nbb0.entry:\n  %r0:i64 = const.i64 \
              42\n  ret %r0\n.end\n";

  let module = parse_module(text).expect("parse should succeed");

  assert_eq!(module_to_string(&module), text);
}

#[test]
fn rejects_untyped_i64_constant_opcode()
{
  let text =
    ".xlil version 1\n.xlil module App\n.func Value : () -> i64\nbb0.entry:\n  %r0:i64 = const 42\n  ret %r0\n.end\n";

  let diagnostics = parse_module(text).expect_err("untyped XLIL constant must fail");

  assert_eq!(diagnostics[0].code, DiagnosticCode::InvalidInstruction);
}

#[test]
fn roundtrips_const_i32_function()
{
  let text =
    ".xlil version 1\n.xlil module App\n.func main : () -> i32\nbb0.entry:\n  %r0:i32 = const.i32 0\n  ret %r0\n.end\n";
  let parsed = parse_module(text).expect("XLIL must parse");
  assert_eq!(crate::xlil::module_to_string(&parsed), text);
}

#[test]
fn roundtrips_explicit_utf32_string_constants()
{
  let text = ".xlil version 1\n.xlil module Strings\n.func little : () -> str\nbb0.entry:\n  %r0:str = const.str \
              utf32le [0x0000004c, 0x00000065, 0x00000069]\n  ret %r0\n.end\n.func big : () -> str\nbb0.entry:\n  \
              %r0:str = const.str utf32be [0x0000004c, 0x00000065, 0x00000069]\n  ret %r0\n.end\n";
  let module = parse_module(text).expect("explicit UTF-16 strings should parse");

  assert!(crate::xlil::verify_module(&module).is_empty());
  assert_eq!(module_to_string(&module), text);
}

#[test]
fn roundtrips_u16_code_unit_constant()
{
  let text = ".xlil version 1\n.xlil module Character\n.func omega : () -> u16\nbb0.entry:\n  %r0:u16 = const.u16 \
              0x03a9\n  ret %r0\n.end\n";
  let module = parse_module(text).expect("u16 constant should parse");

  assert!(crate::xlil::verify_module(&module).is_empty());
  assert_eq!(module_to_string(&module), text);
}

#[test]
fn roundtrips_all_fixed_integer_bit_patterns()
{
  let text = ".xlil version 1\n.xlil module Integers\n.func values : () -> void\nbb0.entry:\n  %r0:u8 = const.u8 \
              0xff\n  %r1:i8 = const.i8 0x80\n  %r2:i16 = const.i16 0x8000\n  %r3:u32 = const.u32 0xffffffff\n  \
              %r4:u64 = const.u64 0xffffffffffffffff\n  %r5:i128 = const.i128 0x80000000000000000000000000000000\n  \
              %r6:u128 = const.u128 0xffffffffffffffffffffffffffffffff\n  ret\n.end\n";
  let module = parse_module(text).expect("integer constants should parse");

  assert!(crate::xlil::verify_module(&module).is_empty());
  assert_eq!(module_to_string(&module), text);
}

#[test]
fn rejects_integer_bit_pattern_with_wrong_width()
{
  let text = ".xlil version 1\n.xlil module Integers\n.func bad : () -> u8\nbb0.entry:\n  %r0:u8 = const.u8 0x0001\n  \
              ret %r0\n.end\n";
  assert!(parse_module(text).is_err());
}

#[test]
fn rejects_untagged_or_malformed_utf32_string_constants()
{
  for instruction in ["%r0:str = const.str utf32 [0x00000041]",
                      "%r0:str = const.str utf32le [0x0000d800]"]
  {
    let text = format!(".xlil version 1\n.xlil module Strings\n.func value : () -> str\nbb0.entry:\n  \
                        {instruction}\n  ret %r0\n.end\n");
    assert!(parse_module(&text).is_err(),
            "invalid instruction was accepted: {instruction}");
  }
}

#[test]
fn roundtrips_call_function()
{
  let text =
    ".xlil version 1\n.xlil module App\n.func xs$App$Call : () -> i64\nbb0.entry:\n  %r0:i64 = const.i64 7\n  %r1:i64 \
     = call xs$App$Callee(%r0)\n  ret %r1\n.end\n";

  let module = parse_module(text).expect("parse should succeed");

  assert_eq!(module_to_string(&module), text);
}

#[test]
fn roundtrips_add_i64_function()
{
  let text = ".xlil version 1\n.xlil module App\n.func xs$App$Add : () -> i64\nbb0.entry:\n  %r0:i64 = const.i64 2\n  \
              %r1:i64 = const.i64 3\n  %r2:i64 = add.i64 %r0, %r1\n  ret %r2\n.end\n";

  let module = parse_module(text).expect("parse should succeed");

  assert_eq!(module_to_string(&module), text);
}

#[test]
fn roundtrips_sub_i64_function()
{
  let text = ".xlil version 1\n.xlil module App\n.func xs$App$Sub : () -> i64\nbb0.entry:\n  %r0:i64 = const.i64 8\n  \
              %r1:i64 = const.i64 3\n  %r2:i64 = sub.i64 %r0, %r1\n  ret %r2\n.end\n";

  let module = parse_module(text).expect("parse should succeed");

  assert_eq!(module_to_string(&module), text);
}

#[test]
fn roundtrips_mul_i64_function()
{
  let text = ".xlil version 1\n.xlil module App\n.func xs$App$Mul : () -> i64\nbb0.entry:\n  %r0:i64 = const.i64 6\n  \
              %r1:i64 = const.i64 7\n  %r2:i64 = mul.i64 %r0, %r1\n  ret %r2\n.end\n";

  let module = parse_module(text).expect("parse should succeed");

  assert_eq!(module_to_string(&module), text);
}

#[test]
fn roundtrips_eq_i64_function()
{
  let text = ".xlil version 1\n.xlil module App\n.func xs$App$Eq : () -> bool\nbb0.entry:\n  %r0:i64 = const.i64 7\n  \
              %r1:i64 = const.i64 7\n  %r2:bool = eq.i64 %r0, %r1\n  ret %r2\n.end\n";

  let module = parse_module(text).expect("parse should succeed");

  assert_eq!(module_to_string(&module), text);
}

#[test]
fn roundtrips_i32_instruction_family()
{
  let text =
    ".xlil version 1\n.xlil module App\n.func xs$App$I32 : () -> bool\nbb0.entry:\n  %r0:i32 = const.i32 2\n  %r1:i32 \
     = const.i32 3\n  %r2:i32 = add.i32 %r0, %r1\n  %r3:i32 = sub.i32 %r0, %r1\n  %r4:i32 = mul.i32 %r0, %r1\n  \
     %r5:bool = eq.i32 %r0, %r1\n  %r6:bool = lt.i32 %r0, %r1\n  %r7:bool = le.i32 %r0, %r1\n  %r8:bool = gt.i32 %r0, \
     %r1\n  %r9:bool = ge.i32 %r0, %r1\n  ret %r9\n.end\n";

  let module = parse_module(text).expect("parse should succeed");

  assert_eq!(module_to_string(&module), text);
}

#[test]
fn roundtrips_const_bool_function()
{
  let text = ".xlil version 1\n.xlil module App\n.func xs$App$Truth : () -> bool\nbb0.entry:\n  %r0:bool = const.bool \
              true\n  ret %r0\n.end\n";

  let module = parse_module(text).expect("parse should succeed");

  assert_eq!(module_to_string(&module), text);
}

#[test]
fn roundtrips_branch_if_function()
{
  let text = ".xlil version 1\n.xlil module App\n.func xs$App$BranchIf : () -> void\nbb0.entry:\n  %r0:bool = \
              const.bool true\n  br_if %r0, bb1, bb2\nbb1.then:\n  ret\nbb2.else:\n  ret\n.end\n";

  let module = parse_module(text).expect("parse should succeed");

  assert_eq!(module_to_string(&module), text);
}

#[test]
fn roundtrips_panic_function()
{
  let text = ".xlil version 1\n.xlil module App\n.func xs$App$Panic : () -> void\nbb0.entry:\n  panic\n.end\n";

  let module = parse_module(text).expect("parse should succeed");

  assert_eq!(module_to_string(&module), text);
}

#[test]
fn roundtrips_stack_slot_memory()
{
  let text = ".xlil version 1\n.xlil module App\n.func Memory : () -> i32\n.slot %s0:i32\nbb0.entry:\n  %r0:i32 = \
              const.i32 7\n  store %r0, %s0\n  %r1:i32 = load %s0\n  ret %r1\n.end\n";

  let module = parse_module(text).expect("parse should succeed");

  assert!(crate::xlil::verify_module(&module).is_empty());
  assert_eq!(module_to_string(&module), text);
}

#[test]
fn roundtrips_f32_and_f64_constant_bits()
{
  let text = ".xlil version 1\n.xlil module FloatConstants\n.func F32Value : () -> f32\nbb0.entry:\n  %r0:f32 = \
              const.f32 0x3fc00000\n  ret %r0\n.end\n.func F64Value : () -> f64\nbb0.entry:\n  %r0:f64 = const.f64 \
              0x3ff8000000000000\n  ret %r0\n.end\n";
  let module = parse_module(text).expect("floating constants should parse");

  assert!(crate::xlil::verify_module(&module).is_empty());
  assert_eq!(module_to_string(&module), text);
}

#[test]
fn rejects_legacy_plain_value_ids()
{
  let text = ".xlil version 1\n.xlil module App\n.func xs$App$Legacy : () -> i64\nbb0.entry:\n  %0:i64 = const.i64 \
              42\n  ret %0\n.end\n";

  let diagnostics = parse_module(text).expect_err("legacy plain value ids must fail");

  assert_eq!(diagnostics[0].code, DiagnosticCode::InvalidValueId);
}

#[test]
fn parses_missing_terminator_marker()
{
  let text =
    ".xlil version 1\n.xlil module App\n.func xs$App$Broken : () -> void\nbb0.entry:\n  .missing_terminator\n.end\n";

  let module = parse_module(text).expect("parse should succeed");

  assert_eq!(module.functions[0].blocks[0].terminator, None);
}

#[test]
fn rejects_unknown_type()
{
  let diagnostics =
    parse_module(".xlil version 1\n.xlil module App\n.extern bad : () -> nope\n").expect_err("parse must fail");

  assert_eq!(diagnostics[0].code, DiagnosticCode::InvalidType);
}

#[test]
fn roundtrips_branch_function()
{
  let text = ".xlil version 1\n.xlil module App\n.func xs$App$Branch : () -> void\nbb0.entry:\n  br bb1\nbb1.exit:\n  \
              ret\n.end\n";

  let module = parse_module(text).expect("parse should succeed");

  assert_eq!(module_to_string(&module), text);
}
