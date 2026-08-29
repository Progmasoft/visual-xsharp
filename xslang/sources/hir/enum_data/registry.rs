/*
 * SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
 * SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.0
 */

use std::collections::{HashMap, HashSet};

use super::super::declarations::{EnumVariant, NominalKind, NominalType, TypeRef, type_ref_to_checked};
use super::super::type_check::Type;
use super::{EnumDataDiagnostic, EnumDataError, VariantSelectionError};

/// One inherited or directly declared variant with its flattened runtime tag.
#[derive(Clone, Debug, Eq, PartialEq)]
pub struct ResolvedVariant
{
    /// Declaration that originally introduced the variant.
    pub owner: String,
    /// Variant constructor name.
    pub name: String,
    /// Optional single payload type.
    pub payload: Option<TypeRef>,
    /// Deterministic tag in the resolved target hierarchy.
    pub tag: u32,
    /// Declaration-local tag retained for identity and diamond deduplication.
    pub declaration_tag: u32,
    /// Whether the target receives this variant through a base.
    pub inherited: bool,
}

impl ResolvedVariant
{
    fn declared(owner: &str, variant: &EnumVariant) -> Self
    {
        Self {
            owner: owner.to_string(),
            name: variant.name.clone(),
            payload: variant.payload.clone(),
            tag: 0,
            declaration_tag: variant.tag,
            inherited: false,
        }
    }
}

/// Validated registry for enum-data inheritance and overload selection.
#[derive(Clone, Debug, Default)]
pub struct EnumDataRegistry
{
    definitions: HashMap<String, NominalType>,
    variants: HashMap<String, Vec<ResolvedVariant>>,
    diagnostics: Vec<EnumDataDiagnostic>,
}

impl EnumDataRegistry
{
    /// Builds and validates all enum-data hierarchies in declaration order.
    #[must_use]
    pub fn build(declarations: &[NominalType]) -> Self
    {
        let definitions = declarations
            .iter()
            .cloned()
            .map(|declaration| (declaration.name.clone(), declaration))
            .collect::<HashMap<_, _>>();
        let mut registry = Self {
            definitions,
            variants: HashMap::new(),
            diagnostics: Vec::new(),
        };
        for declaration in declarations.iter().filter(|value| value.kind == NominalKind::EnumData)
        {
            if let Err(error) = registry.resolve_type(&declaration.name, &mut Vec::new())
            {
                registry.diagnostics.push(EnumDataDiagnostic {
                    type_name: declaration.name.clone(),
                    error,
                });
            }
        }
        registry
    }

    /// Returns every structural hierarchy diagnostic.
    #[must_use]
    pub fn diagnostics(&self) -> &[EnumDataDiagnostic]
    {
        &self.diagnostics
    }

    /// Returns true when every enum-data hierarchy is valid.
    #[must_use]
    pub fn is_valid(&self) -> bool
    {
        self.diagnostics.is_empty()
    }

    /// Returns flattened variants for a valid enum-data type.
    pub fn variants(&self, type_name: &str) -> Result<&[ResolvedVariant], VariantSelectionError>
    {
        let definition = self
            .definitions
            .get(type_name)
            .ok_or_else(|| VariantSelectionError::UnknownType(type_name.to_string()))?;
        if definition.kind != NominalKind::EnumData
        {
            return Err(VariantSelectionError::NotEnumData(type_name.to_string()));
        }
        if let Some(diagnostic) = self.diagnostics.iter().find(|value| value.type_name == type_name)
        {
            return Err(VariantSelectionError::InvalidHierarchy(diagnostic.clone()));
        }
        self.variants.get(type_name).map(Vec::as_slice).ok_or_else(|| {
            VariantSelectionError::InvalidHierarchy(EnumDataDiagnostic {
                type_name: type_name.to_string(),
                error: EnumDataError::MissingTypedVariant,
            })
        })
    }

    /// Selects a payload-free or typed variant constructor by exact payload type.
    pub fn select(
        &self,
        type_name: &str,
        variant_name: &str,
        payload: Option<&Type>,
    ) -> Result<&ResolvedVariant, VariantSelectionError>
    {
        let named = self
            .variants(type_name)?
            .iter()
            .filter(|candidate| candidate.name == variant_name)
            .collect::<Vec<_>>();
        if named.is_empty()
        {
            return Err(VariantSelectionError::UnknownVariant {
                type_name: type_name.to_string(),
                variant: variant_name.to_string(),
            });
        }
        match payload
        {
            None => named
                .iter()
                .copied()
                .find(|candidate| candidate.payload.is_none())
                .ok_or_else(|| VariantSelectionError::PayloadRequired {
                    type_name: type_name.to_string(),
                    variant: variant_name.to_string(),
                }),
            Some(actual) =>
            {
                let typed = named
                    .iter()
                    .copied()
                    .filter(|candidate| candidate.payload.is_some())
                    .collect::<Vec<_>>();
                if typed.is_empty()
                {
                    return Err(VariantSelectionError::UnexpectedPayload {
                        type_name: type_name.to_string(),
                        variant: variant_name.to_string(),
                    });
                }
                typed
                    .iter()
                    .copied()
                    .find(|candidate| candidate.payload.as_ref().and_then(type_ref_to_checked).as_ref() == Some(actual))
                    .ok_or_else(|| VariantSelectionError::NoMatchingPayload {
                        type_name: type_name.to_string(),
                        variant: variant_name.to_string(),
                        actual: actual.clone(),
                        candidates: typed.iter().filter_map(|candidate| candidate.payload.clone()).collect(),
                    })
            }
        }
    }

    /// Returns overloads visible under one variant name.
    pub fn overloads(&self, type_name: &str, variant_name: &str)
    -> Result<Vec<&ResolvedVariant>, VariantSelectionError>
    {
        let variants = self.variants(type_name)?;
        let overloads = variants
            .iter()
            .filter(|candidate| candidate.name == variant_name)
            .collect::<Vec<_>>();
        if overloads.is_empty()
        {
            Err(VariantSelectionError::UnknownVariant {
                type_name: type_name.to_string(),
                variant: variant_name.to_string(),
            })
        }
        else
        {
            Ok(overloads)
        }
    }

    fn resolve_type(&mut self, type_name: &str, stack: &mut Vec<String>)
    -> Result<Vec<ResolvedVariant>, EnumDataError>
    {
        if let Some(variants) = self.variants.get(type_name)
        {
            return Ok(variants.clone());
        }
        if stack.iter().any(|name| name == type_name)
        {
            return Err(EnumDataError::InheritanceCycle(type_name.to_string()));
        }
        let definition = self
            .definitions
            .get(type_name)
            .cloned()
            .ok_or_else(|| EnumDataError::UnknownBase(type_name.to_string()))?;
        if definition.kind != NominalKind::EnumData
        {
            return Err(EnumDataError::InvalidBaseCategory(type_name.to_string()));
        }
        stack.push(type_name.to_string());
        let result = self.collect_definition(&definition, stack);
        stack.pop();
        let variants = result?;
        self.variants.insert(type_name.to_string(), variants.clone());
        Ok(variants)
    }

    fn collect_definition(
        &mut self,
        definition: &NominalType,
        stack: &mut Vec<String>,
    ) -> Result<Vec<ResolvedVariant>, EnumDataError>
    {
        if !definition.variants.iter().any(|variant| variant.payload.is_some())
        {
            return Err(EnumDataError::MissingTypedVariant);
        }
        let mut variants = Vec::new();
        let mut inherited_identities = HashSet::new();
        for base in &definition.bases
        {
            let TypeRef::Named(base_name) = &base.ty
            else
            {
                return Err(EnumDataError::InvalidBaseType);
            };
            let Some(base_definition) = self.definitions.get(base_name)
            else
            {
                return Err(EnumDataError::UnknownBase(base_name.clone()));
            };
            if base_definition.kind != NominalKind::EnumData
            {
                return Err(EnumDataError::InvalidBaseCategory(base_name.clone()));
            }
            for mut inherited in self.resolve_type(base_name, stack)?
            {
                let identity = (inherited.owner.clone(), inherited.declaration_tag);
                if inherited_identities.insert(identity)
                {
                    inherited.inherited = true;
                    variants.push(inherited);
                }
            }
        }
        variants.extend(
            definition
                .variants
                .iter()
                .map(|variant| ResolvedVariant::declared(&definition.name, variant)),
        );
        validate_overloads(&variants)?;
        for (index, variant) in variants.iter_mut().enumerate()
        {
            variant.tag = u32::try_from(index).map_err(|_| EnumDataError::TagOverflow)?;
        }
        Ok(variants)
    }
}

fn validate_overloads(variants: &[ResolvedVariant]) -> Result<(), EnumDataError>
{
    let mut by_name = HashMap::<&str, Vec<&ResolvedVariant>>::new();
    for variant in variants
    {
        by_name.entry(&variant.name).or_default().push(variant);
    }
    for (name, overloads) in by_name
    {
        let untyped = overloads.iter().filter(|variant| variant.payload.is_none()).count();
        if untyped > 1
        {
            return Err(EnumDataError::DuplicateUntypedVariant(name.to_string()));
        }
        if untyped == 1 && overloads.len() > 1
        {
            return Err(EnumDataError::UntypedVariantOverload(name.to_string()));
        }
        let mut payloads = Vec::new();
        for variant in overloads
        {
            if let Some(payload) = &variant.payload &&
                payloads.contains(payload)
            {
                return Err(EnumDataError::DuplicatePayload {
                    name: name.to_string(),
                    payload: payload.clone(),
                });
            }
            if let Some(payload) = &variant.payload
            {
                payloads.push(payload.clone());
            }
        }
    }
    Ok(())
}
