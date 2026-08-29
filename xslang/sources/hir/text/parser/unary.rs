/*
 * SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
 * SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.0
 */

use super::*;

impl Parser<'_>
{
    pub(super) fn unary_operator(&mut self, name: &str) -> Option<UnaryOperator>
    {
        match name
        {
            "logical_not" => Some(UnaryOperator::LogicalNot),
            "positive" => Some(UnaryOperator::Positive),
            "negative" => Some(UnaryOperator::Negative),
            _ =>
            {
                self.report(format!("unknown unary operator '{name}'"));
                None
            }
        }
    }
}
