// SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
// SPDX-License-Identifier: MPL-2.0

//! Public XLIL producer API integration test.

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
