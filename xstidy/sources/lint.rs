/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 */

use codespan::Files;
use serde::{Deserialize, Serialize};

#[derive(Clone, Copy, Debug, Deserialize, Eq, PartialEq, Serialize)]
#[serde(rename_all = "lowercase")]
pub enum Severity
{
  Warning,
}

#[derive(Clone, Debug, Deserialize, Eq, PartialEq, Serialize)]
pub struct Diagnostic
{
  pub code: &'static str,
  pub severity: Severity,
  pub message: &'static str,
  pub line: usize,
  pub column: usize,
  pub byte_start: usize,
  pub byte_end: usize,
}

#[derive(Clone, Copy, Debug, Eq, PartialEq)]
enum ScanState
{
  Code,
  LineComment,
  String(char),
}

pub fn lint_source(source: &str) -> Vec<Diagnostic>
{
  let characters = source.char_indices().collect::<Vec<_>>();
  let mut matches = Vec::new();
  let mut state = ScanState::Code;
  let mut escaped = false;
  let mut index = 0;

  while index < characters.len()
  {
    let (offset, character) = characters[index];
    let next = characters.get(index + 1).map(|entry| entry.1);
    match state
    {
      ScanState::Code if character == '/' && next == Some('/') =>
      {
        state = ScanState::LineComment;
      }
      ScanState::Code if character == '"' || character == '\'' =>
      {
        state = ScanState::String(character);
        escaped = false;
      }
      ScanState::Code if source[offset..].starts_with("::new") && follows_call(source, offset + 5) =>
      {
        matches.push(offset);
      }
      ScanState::LineComment if character == '\n' => state = ScanState::Code,
      ScanState::String(quote) if !escaped && character == quote => state = ScanState::Code,
      ScanState::String(_) =>
      {
        escaped = !escaped && character == '\\';
        if character != '\\'
        {
          escaped = false;
        }
      }
      _ =>
      {}
    }
    index += 1;
  }

  let mut files = Files::new();
  let file_id = files.add("<memory>", source);
  matches.into_iter()
         .map(|byte_start| {
           let byte_index = u32::try_from(byte_start).expect("X# source exceeds the v0 diagnostic limit");
           let location = files.location(file_id, byte_index)
                               .expect("scanner produced a valid offset");
           Diagnostic { code: "XT0001",
                        severity: Severity::Warning,
                        message: "constructor calls use `new Type(...)`, not `Type::new(...)`",
                        line: location.line.to_usize() + 1,
                        column: location.column.to_usize() + 1,
                        byte_start,
                        byte_end: byte_start + "::new".len() }
         })
         .collect()
}

fn follows_call(source: &str, offset: usize) -> bool
{
  source[offset..].trim_start().starts_with('(')
}

#[cfg(test)]
mod tests
{
  use super::*;

  #[test]
  fn reports_legacy_constructor_syntax()
  {
    let diagnostics = lint_source("fn main() {\n  value := Type::new();\n}\n");
    assert_eq!(diagnostics.len(), 1);
    assert_eq!(diagnostics[0].code, "XT0001");
    assert_eq!((diagnostics[0].line, diagnostics[0].column), (2, 16));
  }

  #[test]
  fn ignores_comments_strings_and_associated_methods()
  {
    let source = "// Type::new()\ntext := \"Type::new()\";\nvalue := Type::new_value();\nvalue := new Type();\n";
    assert!(lint_source(source).is_empty());
  }
}
