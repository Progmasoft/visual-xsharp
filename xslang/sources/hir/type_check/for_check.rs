/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 */

use super::{Block, Diagnostic, DiagnosticCode, Expression, Local, Span, Statement, Type, TypeChecker};

impl TypeChecker
{
  pub(super) fn check_condition(&mut self, condition: &Expression, span: Span)
  {
    self.check_expression(condition);
    if self.expression_type(condition) != Some(Type::Primitive(super::PrimitiveType::Bool))
    {
      self.diagnostics
          .push(Diagnostic { code: DiagnosticCode::ConditionTypeMismatch,
                             message: "loop or branch condition must have type Bool".to_string(),
                             span });
    }
  }

  pub(super) fn check_for_statement(&mut self,
                                    initializer: Option<&Statement>,
                                    condition: Option<&Expression>,
                                    update: Option<&Expression>,
                                    body: &Block,
                                    span: Span,
                                    return_type: Option<&Type>)
  {
    let local_count = self.locals.len();
    self.scope_starts.push(local_count);
    if let Some(initializer) = initializer
    {
      self.check_statement(initializer, return_type);
    }
    if let Some(condition) = condition
    {
      self.check_condition(condition, span);
    }
    self.loop_depth += 1;
    self.check_block(body, None);
    self.loop_depth -= 1;
    if let Some(update) = update
    {
      self.check_expression(update);
    }
    self.locals.truncate(local_count);
    self.scope_starts.pop();
  }

  pub(super) fn check_for_each_statement(&mut self,
                                         binding: &Local,
                                         iterable: &Expression,
                                         iterable_type: &Type,
                                         body: &Block,
                                         span: Span)
  {
    self.check_expression(iterable);
    self.check_expression_against_type(iterable, iterable_type);
    let Type::Array { element, .. } = iterable_type
    else
    {
      self.diagnostics
          .push(Diagnostic { code: DiagnosticCode::ForEachTypeMismatch,
                             message: "for-each currently requires a built-in array iterable".to_string(),
                             span });
      return;
    };
    if binding.ty != **element
    {
      self.diagnostics
          .push(Diagnostic { code: DiagnosticCode::ForEachTypeMismatch,
                             message: "for-each binding type does not match the array element type".to_string(),
                             span: binding.span });
    }
    let local_count = self.locals.len();
    self.scope_starts.push(local_count);
    self.locals.push(binding.clone());
    self.loop_depth += 1;
    self.check_block(body, None);
    self.loop_depth -= 1;
    self.locals.truncate(local_count);
    self.scope_starts.pop();
  }
}
