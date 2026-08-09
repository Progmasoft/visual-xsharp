/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 */

use std::{path::PathBuf, process::ExitCode};

use ariadne::{Label, Report, ReportKind, Source};
use clap::Parser;
use tracing::Level;
use xstidy::{FileDiagnostics, lint_paths};

#[derive(Debug, Parser)]
#[command(name = "xstidy", about = "Experimental Visual X# source linter")]
struct Arguments
{
  /// Emit one JSON diagnostic per line.
  #[arg(long)]
  json: bool,

  /// Enable execution tracing.
  #[arg(long)]
  verbose: bool,

  /// Visual X# files or directories to inspect recursively.
  #[arg(required = true)]
  paths: Vec<PathBuf>,
}

fn main() -> ExitCode
{
  let arguments = Arguments::parse();
  if arguments.verbose
  {
    tracing_subscriber::fmt().with_max_level(Level::DEBUG)
                             .with_writer(std::io::stderr)
                             .init();
  }

  let mut failed = false;
  for result in lint_paths(&arguments.paths)
  {
    match result
    {
      Ok(file) if file.diagnostics.is_empty() =>
      {}
      Ok(file) =>
      {
        failed = true;
        print_diagnostics(&file, arguments.json);
      }
      Err(error) =>
      {
        failed = true;
        eprintln!("xstidy: {}: {}", error.path.display(), error.error);
      }
    }
  }
  ExitCode::from(u8::from(failed))
}

fn print_diagnostics(file: &FileDiagnostics, json: bool)
{
  for diagnostic in &file.diagnostics
  {
    if json
    {
      println!("{}",
               serde_json::to_string(diagnostic).expect("diagnostic serialization"));
      continue;
    }
    let name = file.path.display().to_string();
    let span = diagnostic.byte_start..diagnostic.byte_end;
    Report::build(ReportKind::Warning, (name.as_str(), span.clone()))
      .with_code(diagnostic.code)
      .with_message(diagnostic.message)
      .with_label(Label::new((name.as_str(), span)).with_message("legacy constructor spelling"))
      .with_help("write constructor calls as `new Type(...)`")
      .finish()
      .eprint((name.as_str(), Source::from(file.source.as_str())))
      .expect("write diagnostic");
  }
}
