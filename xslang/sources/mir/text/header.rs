/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 */

/// Latest XMIR text version emitted by the canonical writer.
pub const SUPPORTED_XMIR_VERSION: u32 = 1;

/// Returns whether an XMIR text version can be read.
#[must_use]
pub const fn is_supported_xmir_version(version: u32) -> bool
{
  matches!(version, 0 | SUPPORTED_XMIR_VERSION)
}

/// Version and function identity read from the beginning of an XMIR document.
#[derive(Clone, Debug, Eq, PartialEq)]
pub struct XmirDocumentHeader
{
  /// Declared XMIR text format version.
  pub version: u32,
  /// Declared function name.
  pub function: String,
}

/// Parses the leading version and function records without parsing a body.
#[must_use]
pub fn parse_xmir_header(text: &str) -> Option<XmirDocumentHeader>
{
  let mut lines = text.lines();
  let version = lines.next()?.strip_prefix(".xmir version ")?.parse().ok()?;
  let function = lines.next()?.strip_prefix("function ")?.to_string();
  Some(XmirDocumentHeader { version,
                            function })
}
