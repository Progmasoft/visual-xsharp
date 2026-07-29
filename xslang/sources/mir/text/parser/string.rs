/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 */

use super::{Parser, span};
use crate::mir::Statement;
use crate::xlil::StrComparisonOperation;

impl Parser<'_>
{
  pub(super) fn const_str_statement(&mut self) -> Statement
  {
    let local = self.const_target();
    let line = self.current();
    let units = match line.as_deref().and_then(|value| value.strip_prefix("value "))
    {
      Some(value) =>
      {
        self.index += 1;
        crate::text::parse_units(value).unwrap_or_else(|| {
                                         self.report("invalid const.str UTF-16 code units".to_string());
                                         Vec::new()
                                       })
      }
      None =>
      {
        self.report("missing const.str value".to_string());
        Vec::new()
      }
    };
    Statement::ConstStr { local,
                          units,
                          span: span() }
  }

  pub(super) fn str_comparison_statement(&mut self, instruction: &str) -> Statement
  {
    let operation = instruction.strip_suffix(".str")
                               .and_then(StrComparisonOperation::parse_text_stem)
                               .expect("guarded Str comparison must parse");
    Statement::CompareStr { operation,
                            result: self.binary_local(instruction, "result"),
                            left: self.binary_local(instruction, "left"),
                            right: self.binary_local(instruction, "right"),
                            span: span() }
  }
}
