/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 */

use super::*;

impl TypeChecker
{
  pub(super) fn check_block(&mut self, block: &Block, expected_tail: Option<&Type>)
  {
    let local_count = self.locals.len();
    self.scope_starts.push(local_count);
    for statement in &block.statements
    {
      self.check_statement(statement, self.return_type.clone().as_ref());
    }
    match (block.tail.as_deref(), expected_tail)
    {
      (Some(tail), Some(expected)) => self.check_expression_against_type(tail, expected),
      (None, Some(_)) => self.diagnostics
                             .push(Diagnostic { code: DiagnosticCode::MissingBlockValue,
                                                message: "if expression branch must produce a value".to_string(),
                                                span: block.span }),
      (Some(tail), None) => self.check_expression(tail),
      (None, None) =>
      {}
    }
    self.locals.truncate(local_count);
    self.scope_starts.pop();
  }
}
