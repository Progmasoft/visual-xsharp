/*
 * SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
 * SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.0
 */

use super::{DiagnosticCode, Parser};
use crate::xlil::{SUPPORTED_XLIL_VERSION, is_supported_xlil_version};

impl Parser<'_>
{
    pub(super) fn version_header(&mut self) -> bool
    {
        let Some(version) = self.next_line()
        else
        {
            self.report(DiagnosticCode::EmptyInput, 1, "XLIL input is empty");
            return false;
        };
        let Some(version) = version.strip_prefix(".xlil version ")
        else
        {
            self.report(
                DiagnosticCode::InvalidVersionHeader,
                1,
                "XLIL version header is invalid",
            );
            return false;
        };
        match version.parse::<u32>()
        {
            Ok(version) if is_supported_xlil_version(version) => true,
            Ok(version) =>
            {
                self.report(
                    DiagnosticCode::InvalidVersionHeader,
                    1,
                    &format!(
                        "XLIL version {version} is not supported; supported versions are 0 and \
                         {SUPPORTED_XLIL_VERSION}"
                    ),
                );
                false
            }
            Err(_) =>
            {
                self.report(
                    DiagnosticCode::InvalidVersionHeader,
                    1,
                    "XLIL version number is invalid",
                );
                false
            }
        }
    }
}
