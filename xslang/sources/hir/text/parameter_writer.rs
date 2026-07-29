/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 */

use crate::hir::type_check::Function;

use super::function_to_xhir;

#[must_use]
pub fn function_to_xhir_with_parameters(function: &Function, parameter_count: usize) -> String
{
  assert!(parameter_count <= function.locals.len(),
          "XHIR parameter count exceeds local table");
  if parameter_count == 0
  {
    return function_to_xhir(function);
  }
  let document = function_to_xhir(function);
  let section_start = document.find("  locals\n")
                              .expect("parameters must occur in the canonical local table");
  let records_start = section_start + "  locals\n".len();
  let relative_end = document[records_start..].find("  .end\n")
                                              .expect("canonical local table must end");
  let records_end = records_start + relative_end;
  let records = document[records_start..records_end].lines().collect::<Vec<_>>();
  assert!(parameter_count <= records.len(),
          "XHIR parameter count exceeds serialized local records");

  let mut output = String::with_capacity(document.len() + 24);
  output.push_str(&document[..section_start]);
  output.push_str("  parameters\n");
  for record in &records[..parameter_count]
  {
    output.push_str(&record.replacen("    local ", "    parameter ", 1));
    output.push('\n');
  }
  output.push_str("  .end\n");
  if records.len() > parameter_count
  {
    output.push_str("  locals\n");
    for record in &records[parameter_count..]
    {
      output.push_str(record);
      output.push('\n');
    }
    output.push_str("  .end\n");
  }
  output.push_str(&document[records_end + "  .end\n".len()..]);
  output
}

#[cfg(test)]
mod tests
{
  use super::*;
  use crate::hir::{Local, PrimitiveType, Span, Type};

  #[test]
  fn separates_leading_parameters_from_function_locals()
  {
    let local = |name: &str| Local { name: name.to_string(),
                                     ty: Type::Primitive(PrimitiveType::Long),
                                     mutable: false,
                                     span: Span::new(0, 0, 1) };
    let function = Function { name: "add".to_string(),
                              return_type: Some(Type::Primitive(PrimitiveType::Long)),
                              locals: vec![local("left"), local("right"), local("result")],
                              body: vec![] };
    let text = function_to_xhir_with_parameters(&function, 2);
    assert!(text.contains("parameters\n    parameter left: Long immutable\n    parameter right: Long immutable"));
    assert!(text.contains("locals\n    local result: Long immutable"));
  }
}
