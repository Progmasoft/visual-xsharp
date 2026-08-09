/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 */

use std::{env, fs, process::ExitCode};

use xsfmt::{Config, format_source};

fn main() -> ExitCode
{
  let mut check = false;
  let mut paths = Vec::new();
  for argument in env::args().skip(1)
  {
    if argument == "--check"
    {
      check = true;
    }
    else
    {
      paths.push(argument);
    }
  }
  if paths.is_empty()
  {
    eprintln!("usage: xsfmt [--check] <file.vxs>...");
    return ExitCode::from(2);
  }

  let config = Config::default();
  let mut failed = false;
  for path in paths
  {
    let source = match fs::read_to_string(&path)
    {
      Ok(source) => source,
      Err(error) =>
      {
        eprintln!("xsfmt: {path}: {error}");
        failed = true;
        continue;
      }
    };
    let result = format_source(&source, &config);
    if !result.changed
    {
      continue;
    }
    if check
    {
      eprintln!("xsfmt: {path} requires formatting");
      failed = true;
    }
    else if let Err(error) = fs::write(&path, result.text)
    {
      eprintln!("xsfmt: {path}: {error}");
      failed = true;
    }
  }
  if failed
  {
    ExitCode::FAILURE
  }
  else
  {
    ExitCode::SUCCESS
  }
}
