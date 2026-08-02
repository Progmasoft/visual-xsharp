/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 */

use crate::hir::async_check::Span;
use crate::hir::symbols::Import;

pub(super) fn parse_import_line(line: &str) -> Option<Import>
{
    let rest = line.strip_prefix("import ")?;
    if let Some(module) = rest.strip_prefix("module ")
    {
        return Some(Import::Module {
            module: module.to_string(),
            span: span(),
        });
    }
    if let Some(module) = rest.strip_prefix("all from ")
    {
        return Some(Import::All {
            module: module.to_string(),
            span: span(),
        });
    }
    let (left, module) = rest.rsplit_once(" from ")?;
    if let Some((name, alias)) = left.split_once(" as ")
    {
        return Some(Import::Selected {
            module: module.to_string(),
            name: name.to_string(),
            alias: Some(alias.to_string()),
            span: span(),
        });
    }
    Some(Import::Selected {
        module: module.to_string(),
        name: left.to_string(),
        alias: None,
        span: span(),
    })
}

pub(super) const fn span() -> Span
{
    Span::new(0, 0, 0)
}
