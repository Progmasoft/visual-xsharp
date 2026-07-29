/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 */

use std::fmt;

use serde::{Deserialize, Serialize};

#[derive(Clone, Copy, Debug, Default, Deserialize, Eq, PartialEq, Serialize)]
#[serde(rename_all = "snake_case")]
pub enum NewlineStyle
{
  #[default]
  Auto,
  Lf,
  CrLf,
}

#[derive(Clone, Debug, Deserialize, Eq, PartialEq, Serialize)]
#[serde(default, deny_unknown_fields)]
pub struct Config
{
  pub max_width: u16,
  pub indent_width: u8,
  pub use_tabs: bool,
  pub newline_style: NewlineStyle,
}

impl Default for Config
{
  fn default() -> Self
  {
    Self { max_width: 120,
           indent_width: 2,
           use_tabs: false,
           newline_style: NewlineStyle::Auto }
  }
}

#[derive(Debug)]
pub enum ConfigError
{
  Toml(toml::de::Error),
  MaxWidth(u16),
  IndentWidth(u8),
}

impl Config
{
  pub fn parse(text: &str) -> Result<Self, ConfigError>
  {
    let config: Self = toml::from_str(text).map_err(ConfigError::Toml)?;
    config.validate()?;
    Ok(config)
  }

  pub fn validate(&self) -> Result<(), ConfigError>
  {
    if !(40..=320).contains(&self.max_width)
    {
      return Err(ConfigError::MaxWidth(self.max_width));
    }
    if !(1..=16).contains(&self.indent_width)
    {
      return Err(ConfigError::IndentWidth(self.indent_width));
    }
    Ok(())
  }
}

impl fmt::Display for ConfigError
{
  fn fmt(&self, formatter: &mut fmt::Formatter<'_>) -> fmt::Result
  {
    match self
    {
      Self::Toml(error) => write!(formatter, "invalid xsfmt TOML: {error}"),
      Self::MaxWidth(value) => write!(formatter, "max_width must be between 40 and 320, got {value}"),
      Self::IndentWidth(value) => write!(formatter, "indent_width must be between 1 and 16, got {value}"),
    }
  }
}

impl std::error::Error for ConfigError {}

#[cfg(test)]
mod tests
{
  use super::*;

  #[test]
  fn parses_partial_toml_over_defaults()
  {
    let config = Config::parse("max_width = 100\nnewline_style = \"lf\"\n").expect("valid config");
    assert_eq!(config.max_width, 100);
    assert_eq!(config.indent_width, 2);
    assert_eq!(config.newline_style, NewlineStyle::Lf);
  }

  #[test]
  fn rejects_unknown_fields_and_invalid_ranges()
  {
    assert!(matches!(Config::parse("unknown = true\n"), Err(ConfigError::Toml(_))));
    assert!(matches!(Config::parse("max_width = 20\n"), Err(ConfigError::MaxWidth(20))));
    assert!(matches!(Config::parse("indent_width = 0\n"), Err(ConfigError::IndentWidth(0))));
  }
}
