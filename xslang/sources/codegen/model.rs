/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 */

use std::collections::BTreeMap;

use crate::mono::{MonoEntry, MonoPlan};

#[derive(Clone, Debug, Eq, PartialEq)]
pub struct CodegenFunction
{
    pub symbol_name: String,
    pub cache_key: String,
}

#[derive(Clone, Debug, Eq, PartialEq)]
pub struct CodegenUnit
{
    pub name: String,
    pub functions: Vec<CodegenFunction>,
}

#[derive(Clone, Debug, Eq, PartialEq)]
pub struct CodegenPlan
{
    pub units: Vec<CodegenUnit>,
}

#[must_use]
pub fn split_codegen_units(plan: &MonoPlan) -> CodegenPlan
{
    let mut units: BTreeMap<String, Vec<CodegenFunction>> = BTreeMap::new();
    for entry in &plan.entries
    {
        units.entry(unit_name(entry)).or_default().push(CodegenFunction {
            symbol_name: entry.symbol_name.clone(),
            cache_key: entry.cache_key.clone(),
        });
    }
    CodegenPlan {
        units: units
            .into_iter()
            .map(|(name, mut functions)| {
                functions.sort_by(|left, right| left.symbol_name.cmp(&right.symbol_name));
                CodegenUnit {
                    name,
                    functions,
                }
            })
            .collect(),
    }
}

fn unit_name(entry: &MonoEntry) -> String
{
    format!("cgu${}", entry.source_module.replace('.', "$"))
}

#[cfg(test)]
mod tests
{
    use super::*;
    use crate::mono::{GenericInstance, TypeArgument, create_mono_plan};

    fn arg(name: &str) -> TypeArgument
    {
        TypeArgument {
            name: name.to_string(),
        }
    }

    fn instance(module_path: &str, function_name: &str, type_arguments: Vec<TypeArgument>) -> GenericInstance
    {
        GenericInstance {
            module_path: module_path.to_string(),
            function_name: function_name.to_string(),
            type_arguments,
        }
    }

    #[test]
    fn groups_monomorphized_functions_by_module_path()
    {
        let mono = create_mono_plan([
            instance("App", "Main", vec![]),
            instance("App.Math", "Add", vec![arg("Int")]),
            instance("App.Math", "Add", vec![arg("Str")]),
        ]);
        let plan = split_codegen_units(&mono);
        assert_eq!(plan.units.len(), 2);
        assert_eq!(plan.units[0].name, "cgu$App");
        assert_eq!(plan.units[1].name, "cgu$App$Math");
        assert_eq!(plan.units[1].functions.len(), 2);
    }

    #[test]
    fn sorts_functions_inside_codegen_unit()
    {
        let mono = create_mono_plan([instance("App", "Zed", vec![]), instance("App", "Alpha", vec![])]);
        let plan = split_codegen_units(&mono);
        assert_eq!(plan.units[0].functions[0].symbol_name, "xs$App$Alpha");
        assert_eq!(plan.units[0].functions[1].symbol_name, "xs$App$Zed");
    }
}
