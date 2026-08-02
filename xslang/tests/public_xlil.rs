// SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
// SPDX-License-Identifier: MPL-2.0

//! Public XLIL producer API integration test.

use std::io::Cursor;

use xslang::rust::XSResult;
use xslang::xlil::*;

#[test]
fn public_rust_api_builds_verifies_and_roundtrips_a_module()
{
  let mut builder = Builder::new("PublicRustProducer");
  builder.declare_function("Import", Type::I32, vec![Type::I32])
         .expect("public API should add declarations");
  builder.begin_function("main", Type::I32, Vec::new())
         .expect("public API should begin definitions");
  builder.append_block("entry").expect("public API should append blocks");
  let argument = builder.const_i32(7).expect("public API should append const.i32");
  let result = builder.call("Import", vec![argument], Type::I32)
                      .expect("public API should append calls")
                      .expect("non-void calls should produce values");
  builder.return_value(Some(result))
         .expect("public API should append a return");
  let module = builder.finish().expect("public API should verify completed modules");

  let text = module_to_string(&module);
  let parsed = parse_module(&text).expect("public writer output should parse");
  assert!(verify_module(&parsed).is_empty());
  assert_eq!(parsed, module);
}

#[test]
fn public_builder_reports_invalid_state_and_duplicate_functions()
{
  let mut builder = Builder::new("Diagnostics");
  assert_eq!(builder.const_i32(0), Err(BuildError::NoCurrentFunction));
  builder.begin_function("main", Type::I32, Vec::new()).unwrap();
  assert_eq!(builder.const_i32(0), Err(BuildError::NoInsertionBlock));
  assert_eq!(builder.declare_function("main", Type::I32, Vec::new()),
             Err(BuildError::DuplicateFunction("main".to_owned())));
}

#[test]
fn checked_calls_derive_the_registered_signature()
{
  let mut builder = Builder::new("CheckedCalls");
  builder.declare_function("Sink", Type::VOID, vec![Type::I32]).unwrap();
  builder.declare_function("Identity", Type::I32, vec![Type::I32])
         .unwrap();
  builder.begin_function("main", Type::I32, Vec::new()).unwrap();
  builder.append_block("entry").unwrap();
  let argument = builder.const_i32(17).unwrap();
  builder.call_void("Sink", vec![argument]).unwrap();
  let result = builder.call_value("Identity", vec![argument]).unwrap();
  builder.return_value(Some(result)).unwrap();

  let module = builder.finish().unwrap();
  let main = module.function("main").unwrap();
  assert_eq!(main.value(result).unwrap().value_type, Type::I32);
  assert_eq!(main.block(BlockId(0)).unwrap().instructions[0].opcode(), "const.i32");
}

#[test]
fn checked_calls_report_signature_mismatches_early()
{
  let mut builder = Builder::new("CheckedErrors");
  builder.declare_function("Import", Type::I32, vec![Type::I64]).unwrap();
  builder.begin_function("main", Type::I32, Vec::new()).unwrap();
  builder.append_block("entry").unwrap();
  let argument = builder.const_i32(1).unwrap();
  assert!(matches!(builder.call_checked("Missing", vec![]),
                   Err(BuildError::UnknownFunction(_))));
  assert!(matches!(builder.call_checked("Import", vec![]),
                   Err(BuildError::ArgumentCount { .. })));
  assert!(matches!(builder.call_checked("Import", vec![argument]),
                   Err(BuildError::ArgumentType { .. })));
  assert!(matches!(builder.call_void("Import", vec![argument]),
                   Err(BuildError::UnexpectedCallResult { .. })));
}

#[test]
fn verified_and_stream_apis_preserve_the_model()
{
  let text = ".xlil version 1\n.xlil module Streams\n.func main : () -> i32\nbb0.entry:\n  %r0:i32 = const.i32 0\n  \
              ret %r0\n.end\n";
  let verified = parse_verified(text).unwrap();
  assert_eq!(verified.function("main").unwrap().return_type, Type::I32);
  assert_eq!(verified.to_text(), text);

  let mut bytes = Vec::new();
  write_verified(&verified, &mut bytes).unwrap();
  let reread = read_verified(Cursor::new(bytes)).unwrap();
  assert_eq!(reread, verified);
}

#[test]
fn rust_result_alias_accepts_xlil_errors()
{
  fn load(text: &str) -> XSResult<VerifiedModule>
  {
    Ok(parse_verified(text)?)
  }

  let error = load("not xlil").unwrap_err();
  assert!(error.to_string().contains("XLIL parsing failed"));
}
