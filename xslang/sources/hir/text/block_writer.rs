/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 */

use std::fmt::Write;

use super::statement_writer::{write_desugared_statement, write_statement};
use super::{Block, DesugaredBlock, write_desugared_expression, write_expression};

pub(super) fn write_block(output: &mut String, block: &Block, indent: usize)
{
  for statement in &block.statements
  {
    write_statement(output, statement, indent);
  }
  if let Some(tail) = &block.tail
  {
    let pad = "  ".repeat(indent);
    let _ = writeln!(output, "{pad}tail");
    write_expression(output, tail, indent + 1);
  }
  let pad = "  ".repeat(indent.saturating_sub(1));
  let _ = writeln!(output, "{pad}.end");
}

pub(super) fn write_desugared_block(output: &mut String, block: &DesugaredBlock, indent: usize)
{
  for statement in &block.statements
  {
    write_desugared_statement(output, statement, indent);
  }
  if let Some(tail) = &block.tail
  {
    let pad = "  ".repeat(indent);
    let _ = writeln!(output, "{pad}tail");
    write_desugared_expression(output, tail, indent + 1);
  }
  let pad = "  ".repeat(indent.saturating_sub(1));
  let _ = writeln!(output, "{pad}.end");
}

pub(super) const fn mutability_name(mutable: bool) -> &'static str
{
  if mutable
  {
    "mutable"
  }
  else
  {
    "immutable"
  }
}
