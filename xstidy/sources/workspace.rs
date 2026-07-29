/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 */

use std::{
  collections::BTreeSet,
  fs, io,
  path::{Path, PathBuf},
};

use rayon::prelude::*;
use tracing::debug;
use walkdir::WalkDir;

use crate::{Diagnostic, lint_source};

#[derive(Debug)]
pub struct FileDiagnostics
{
  pub path: PathBuf,
  pub source: String,
  pub diagnostics: Vec<Diagnostic>,
}

#[derive(Debug)]
pub struct WorkspaceError
{
  pub path: PathBuf,
  pub error: io::Error,
}

pub fn collect_xs_files(paths: &[PathBuf]) -> Vec<PathBuf>
{
  let mut files = BTreeSet::new();
  for path in paths
  {
    if path.is_file()
    {
      if is_xs_file(path)
      {
        files.insert(path.clone());
      }
      continue;
    }
    for entry in WalkDir::new(path).follow_links(false)
                                   .into_iter()
                                   .filter_map(Result::ok)
    {
      if entry.file_type().is_file() && is_xs_file(entry.path())
      {
        files.insert(entry.into_path());
      }
    }
  }
  files.into_iter().collect()
}

pub fn lint_paths(paths: &[PathBuf]) -> Vec<Result<FileDiagnostics, WorkspaceError>>
{
  collect_xs_files(paths).into_par_iter()
                         .map(|path| {
                           debug!(path = %path.display(), "linting source");
                           match fs::read_to_string(&path)
                           {
                             Ok(source) =>
                             {
                               let diagnostics = lint_source(&source);
                               Ok(FileDiagnostics { path,
                                                    source,
                                                    diagnostics })
                             }
                             Err(error) => Err(WorkspaceError { path,
                                                                error }),
                           }
                         })
                         .collect()
}

fn is_xs_file(path: &Path) -> bool
{
  path.extension().is_some_and(|extension| extension == "xs")
}

#[cfg(test)]
mod tests
{
  use super::*;

  #[test]
  fn ignores_non_xs_inputs()
  {
    assert!(!is_xs_file(Path::new("README.md")));
    assert!(is_xs_file(Path::new("main.xs")));
  }
}
