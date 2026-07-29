/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 */

use crate::{Config, NewlineStyle};

#[derive(Clone, Debug, Eq, PartialEq)]
pub struct FormatResult
{
  pub text: String,
  pub changed: bool,
}

pub fn format_source(source: &str, config: &Config) -> FormatResult
{
  let normalized = source.replace("\r\n", "\n").replace('\r', "\n");
  let mut lines = normalized.lines().map(str::trim_end).collect::<Vec<_>>();
  while lines.last().is_some_and(|line| line.is_empty())
  {
    lines.pop();
  }
  let mut output = lines.join("\n");
  output.push('\n');

  let newline = match config.newline_style
  {
    NewlineStyle::CrLf => "\r\n",
    NewlineStyle::Auto if source.contains("\r\n") => "\r\n",
    NewlineStyle::Auto | NewlineStyle::Lf => "\n",
  };
  if newline != "\n"
  {
    output = output.replace('\n', newline);
  }
  FormatResult { changed: output != source,
                 text: output }
}

#[cfg(test)]
mod tests
{
  use super::*;

  #[test]
  fn removes_trailing_space_and_normalizes_final_newline()
  {
    let result = format_source("fn main() {  \n  0;\t\n}\n\n", &Config::default());
    assert_eq!(result.text, "fn main() {\n  0;\n}\n");
    assert!(result.changed);
  }

  #[test]
  fn preserves_detected_crlf_and_is_idempotent()
  {
    let source = "fn main() {\r\n}\r\n";
    let first = format_source(source, &Config::default());
    assert_eq!(first.text, source);
    assert!(!first.changed);
    assert_eq!(format_source(&first.text, &Config::default()).text, source);
  }

  #[test]
  fn honors_explicit_lf()
  {
    let config = Config { newline_style: NewlineStyle::Lf,
                          ..Config::default() };
    assert_eq!(format_source("fn main() {}\r\n", &config).text, "fn main() {}\n");
  }
}
