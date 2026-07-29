/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 */

use serde::{Deserialize, Serialize};

#[derive(Clone, Debug, Default, Deserialize, Eq, PartialEq, Serialize)]
#[serde(default, deny_unknown_fields)]
pub struct Config
{
  pub deny_warnings: bool,
}

pub type ConfigError = toml::de::Error;

impl Config
{
  pub fn parse(text: &str) -> Result<Self, ConfigError>
  {
    toml::from_str(text)
  }
}

#[cfg(test)]
mod tests
{
  use super::*;

  #[test]
  fn parses_toml_and_rejects_unknown_fields()
  {
    assert!(Config::parse("deny_warnings = true\n").expect("config").deny_warnings);
    assert!(Config::parse("unknown = true\n").is_err());
  }
}
