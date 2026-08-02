/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 */

use super::*;

impl HirToMirLowerer
{
    pub(super) fn report(&mut self, code: DiagnosticCode, message: impl Into<String>, span: Span)
    {
        self.diagnostics.push(Diagnostic {
            code,
            message: message.into(),
            span,
        });
    }

    pub(super) fn current_is_terminated(&self, lowered: &mut mir::Function) -> bool
    {
        self.current_block_mut(lowered).terminator.is_some()
    }
}
