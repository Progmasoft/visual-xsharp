/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 */

use super::*;

impl TypeChecker
{
  pub(super) fn find_local(&self, name: &str) -> Option<&Local>
  {
    self.locals.iter().rev().find(|local| local.name == name)
  }

  pub(super) fn check_index_assignment(&mut self,
                                       target: &str,
                                       index: &Expression,
                                       value: &Expression,
                                       element_type: &Type,
                                       span: Span)
  {
    let Some(local) = self.find_local(target).cloned()
    else
    {
      self.diagnostics.push(Diagnostic { code: DiagnosticCode::UnknownLocal,
                                         message: format!("unknown array local '{target}'"),
                                         span });
      return;
    };
    if !matches!(local.ty, Type::Array { ref element, .. } if element.as_ref() == element_type)
    {
      self.report_collection_mismatch(span,
                                      "indexed assignment target must be an array of the recorded element type");
    }
    if !matches!(self.expression_type(index), Some(Type::Primitive(primitive)) if is_supported_integer(primitive))
    {
      self.report_collection_mismatch(span, "array assignment index must have an integer type");
    }
    self.check_expression(index);
    self.check_expression_against_type(value, element_type);
  }

  pub(super) fn report_collection_mismatch(&mut self, span: Span, message: &str)
  {
    self.diagnostics
        .push(Diagnostic { code: DiagnosticCode::LiteralTypeMismatch,
                           message: message.to_string(),
                           span });
  }
}
