/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 */

use super::*;

impl TypeChecker
{
  pub(super) fn check_result_propagation(&mut self, value: &Expression, span: Span)
  {
    let Some((_, error_type)) = self.result_parts_of_expression(value)
    else
    {
      self.diagnostics
          .push(Diagnostic { code: DiagnosticCode::ResultPropagationRequiresResult,
                             message: "Result propagation with '@' requires a Result<T, E> value".to_string(),
                             span });
      return;
    };
    let Some(return_type) = &self.return_type
    else
    {
      self.report_result_return_mismatch(span);
      return;
    };
    let Some((_, return_error_type)) = result_type_parts(return_type)
    else
    {
      self.report_result_return_mismatch(span);
      return;
    };
    if error_type != return_error_type
    {
      self.diagnostics
          .push(Diagnostic { code: DiagnosticCode::ResultPropagationReturnMismatch,
                             message: "Result propagation error type is not compatible with the function return \
                                       type"
                                            .to_string(),
                             span });
    }
  }

  pub(super) fn result_success_type(&self, value: &Expression) -> Option<Type>
  {
    self.result_parts_of_expression(value).map(|(success, _)| success)
  }

  pub(super) fn result_parts_of_expression(&self, value: &Expression) -> Option<(Type, Type)>
  {
    result_type_parts(&self.expression_type(value)?)
  }

  fn report_result_return_mismatch(&mut self, span: Span)
  {
    self.diagnostics
        .push(Diagnostic { code: DiagnosticCode::ResultPropagationReturnMismatch,
                           message:
                             "Result propagation requires the enclosing function to return Result<_, E>".to_string(),
                           span });
  }
}

pub(crate) fn result_type_parts(ty: &Type) -> Option<(Type, Type)>
{
  let Type::Named(name) = ty
  else
  {
    return None;
  };
  let generic = name.strip_prefix("Result<")
                    .or_else(|| name.strip_prefix("std::result::Result<"))
                    .or_else(|| name.strip_prefix("std.result.Result<"))?;
  let inner = generic.strip_suffix('>')?;
  if let Some((success, error)) = split_top_level_pair(inner)
  {
    return Some((parse_named_or_primitive(success.trim()), parse_named_or_primitive(error.trim())));
  }
  (inner.trim() == "()").then(|| (Type::Named("()".to_string()), standard_error()))
}

fn standard_error() -> Type
{
  Type::Named("Error".to_string())
}

fn split_top_level_pair(text: &str) -> Option<(&str, &str)>
{
  let mut depth = 0usize;
  for (index, ch) in text.char_indices()
  {
    match ch
    {
      '<' => depth += 1,
      '>' => depth = depth.saturating_sub(1),
      ',' if depth == 0 => return Some((&text[..index], &text[index + 1..])),
      _ =>
      {}
    }
  }
  None
}

fn parse_named_or_primitive(name: &str) -> Type
{
  let primitive = match name
  {
    "Bool" => Some(PrimitiveType::Bool),
    "Byte" => Some(PrimitiveType::Byte),
    "SByte" => Some(PrimitiveType::SByte),
    "Char" => Some(PrimitiveType::Char),
    "Short" => Some(PrimitiveType::Short),
    "Long" => Some(PrimitiveType::Long),
    "Int" => Some(PrimitiveType::Int),
    "Integer" => Some(PrimitiveType::Integer),
    "UShort" => Some(PrimitiveType::UShort),
    "ULong" => Some(PrimitiveType::ULong),
    "UInt" => Some(PrimitiveType::UInt),
    "UInteger" => Some(PrimitiveType::UInteger),
    "SFloat" => Some(PrimitiveType::SFloat),
    "LFloat" => Some(PrimitiveType::LFloat),
    "Float" => Some(PrimitiveType::Float),
    "Double" => Some(PrimitiveType::Double),
    "String" => Some(PrimitiveType::String),
    "Str" => Some(PrimitiveType::Str),
    _ => None,
  };
  primitive.map_or_else(|| Type::Named(name.to_string()), Type::Primitive)
}

#[cfg(test)]
mod tests
{
  use super::*;

  #[test]
  fn accepts_two_payloads_and_unit_shorthand()
  {
    assert_eq!(result_type_parts(&Type::Named("Result<(), Int>".to_string())),
               Some((Type::Named("()".to_string()), Type::Primitive(PrimitiveType::Int))));
    assert_eq!(result_type_parts(&Type::Named("Result<Int, ()>".to_string())),
               Some((Type::Primitive(PrimitiveType::Int), Type::Named("()".to_string()))));
    assert_eq!(result_type_parts(&Type::Named("Result<()>".to_string())),
               Some((Type::Named("()".to_string()), Type::Named("Error".to_string()))));
  }

  #[test]
  fn rejects_incomplete_single_payload_result()
  {
    assert_eq!(result_type_parts(&Type::Named("Result<Int>".to_string())), None);
  }
}
