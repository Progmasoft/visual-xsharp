/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 */

use std::collections::BTreeSet;

#[derive(Clone, Debug, Eq, Ord, PartialEq, PartialOrd)]
pub struct TypeArgument
{
  pub name: String,
}

#[derive(Clone, Debug, Eq, Ord, PartialEq, PartialOrd)]
pub struct GenericInstance
{
  pub module_path: String,
  pub function_name: String,
  pub type_arguments: Vec<TypeArgument>,
}

#[derive(Clone, Debug, Eq, PartialEq)]
pub struct MonoEntry
{
  pub source_module: String,
  pub source_function: String,
  pub symbol_name: String,
  pub cache_key: String,
}

#[derive(Clone, Debug, Eq, PartialEq)]
pub struct MonoPlan
{
  pub entries: Vec<MonoEntry>,
}

#[must_use]
pub fn create_mono_plan(instances: impl IntoIterator<Item = GenericInstance>) -> MonoPlan
{
  let unique: BTreeSet<_> = instances.into_iter().collect();
  let entries = unique.into_iter().map(|instance| mono_entry(&instance)).collect();
  MonoPlan { entries }
}

fn mono_entry(instance: &GenericInstance) -> MonoEntry
{
  let type_suffix = instance.type_arguments
                            .iter()
                            .map(|argument| sanitize_symbol_part(&argument.name))
                            .collect::<Vec<_>>()
                            .join("$");
  let source = format!("{}::{}", instance.module_path, instance.function_name);
  let symbol_name = if type_suffix.is_empty()
  {
    format!("xs${}${}",
            sanitize_symbol_part(&instance.module_path),
            sanitize_symbol_part(&instance.function_name))
  }
  else
  {
    format!("xs${}${}${type_suffix}",
            sanitize_symbol_part(&instance.module_path),
            sanitize_symbol_part(&instance.function_name))
  };
  let cache_key = if type_suffix.is_empty()
  {
    source.clone()
  }
  else
  {
    format!("{source}<{type_suffix}>")
  };
  MonoEntry { source_module: instance.module_path.clone(),
              source_function: instance.function_name.clone(),
              symbol_name,
              cache_key }
}

fn sanitize_symbol_part(part: &str) -> String
{
  part.chars()
      .map(|character| {
        if character.is_ascii_alphanumeric()
        {
          character
        }
        else
        {
          '$'
        }
      })
      .collect()
}

#[cfg(test)]
mod tests
{
  use super::*;

  fn arg(name: &str) -> TypeArgument
  {
    TypeArgument { name: name.to_string() }
  }

  fn instance(module_path: &str, function_name: &str, type_arguments: Vec<TypeArgument>) -> GenericInstance
  {
    GenericInstance { module_path: module_path.to_string(),
                      function_name: function_name.to_string(),
                      type_arguments }
  }

  #[test]
  fn creates_deterministic_unique_entries()
  {
    let plan = create_mono_plan([instance("App.Core", "Keep", vec![arg("Int")]),
                                 instance("App.Core", "Keep", vec![arg("Int")]),
                                 instance("App.Core", "Keep", vec![arg("Str")])]);
    assert_eq!(plan.entries.len(), 2);
    assert_eq!(plan.entries[0].cache_key, "App.Core::Keep<Int>");
    assert_eq!(plan.entries[1].cache_key, "App.Core::Keep<Str>");
  }

  #[test]
  fn stable_symbol_name_uses_module_function_and_types()
  {
    let plan = create_mono_plan([instance("App.Core", "Map", vec![arg("List<Int>"), arg("Str")])]);
    assert_eq!(plan.entries[0].symbol_name, "xs$App$Core$Map$List$Int$$Str");
  }

  #[test]
  fn supports_non_generic_concrete_function()
  {
    let plan = create_mono_plan([instance("App", "Main", vec![])]);
    assert_eq!(plan.entries[0].cache_key, "App::Main");
    assert_eq!(plan.entries[0].symbol_name, "xs$App$Main");
  }
}
