/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 */

use std::fmt;

use super::super::declarations::TypeRef;
use super::super::type_check::Type;

/// A structural error in an enum-data declaration hierarchy.
#[derive(Clone, Debug, Eq, PartialEq)]
pub enum EnumDataError
{
  /// A base type is not a named nominal type.
  InvalidBaseType,
  /// A named base does not exist.
  UnknownBase(String),
  /// A base exists but is not another enum-data type.
  InvalidBaseCategory(String),
  /// The inheritance graph contains a cycle.
  InheritanceCycle(String),
  /// The declaration does not define any typed variant.
  MissingTypedVariant,
  /// A payload-free variant name participates in an overload set.
  UntypedVariantOverload(String),
  /// Two distinct declarations provide the same name and payload type.
  DuplicatePayload
  {
    /// Overloaded variant name.
    name: String,
    /// Repeated payload type.
    payload: TypeRef,
  },
  /// A declaration repeats a payload-free variant.
  DuplicateUntypedVariant(String),
  /// The flattened variant count cannot be represented by the current tag.
  TagOverflow,
}

impl fmt::Display for EnumDataError
{
  fn fmt(&self, formatter: &mut fmt::Formatter<'_>) -> fmt::Result
  {
    match self
    {
      Self::InvalidBaseType => formatter.write_str("has a non-nominal enum data base"),
      Self::UnknownBase(name) => write!(formatter, "references unknown enum data base '{name}'"),
      Self::InvalidBaseCategory(name) => write!(formatter, "inherits non-enum-data type '{name}'"),
      Self::InheritanceCycle(name) => write!(formatter, "contains an enum data inheritance cycle through '{name}'"),
      Self::MissingTypedVariant => formatter.write_str("does not declare a typed variant"),
      Self::UntypedVariantOverload(name) => write!(formatter, "overloads payload-free variant '{name}'"),
      Self::DuplicatePayload { name,
                               payload, } =>
      {
        write!(formatter, "repeats variant '{name}' with payload type {payload:?}")
      }
      Self::DuplicateUntypedVariant(name) => write!(formatter, "repeats payload-free variant '{name}'"),
      Self::TagOverflow => formatter.write_str("has too many flattened variants for a u32 tag"),
    }
  }
}

impl std::error::Error for EnumDataError {}

/// A declaration-associated enum-data validation diagnostic.
#[derive(Clone, Debug, Eq, PartialEq)]
pub struct EnumDataDiagnostic
{
  /// Declaration whose hierarchy could not be resolved.
  pub type_name: String,
  /// Structural hierarchy error.
  pub error: EnumDataError,
}

impl fmt::Display for EnumDataDiagnostic
{
  fn fmt(&self, formatter: &mut fmt::Formatter<'_>) -> fmt::Result
  {
    write!(formatter, "enum data '{}': {}", self.type_name, self.error)
  }
}

/// Failure to select one constructor from a resolved overload set.
#[derive(Clone, Debug, Eq, PartialEq)]
pub enum VariantSelectionError
{
  /// The requested nominal type does not exist.
  UnknownType(String),
  /// The requested nominal type is not an enum-data type.
  NotEnumData(String),
  /// The hierarchy itself is invalid.
  InvalidHierarchy(EnumDataDiagnostic),
  /// No variant with the requested name is visible.
  UnknownVariant
  {
    /// Target enum-data type.
    type_name: String,
    /// Requested variant name.
    variant: String,
  },
  /// A typed overload set was invoked without a payload.
  PayloadRequired
  {
    /// Target enum-data type.
    type_name: String,
    /// Requested variant name.
    variant: String,
  },
  /// A payload-free variant was invoked with a payload.
  UnexpectedPayload
  {
    /// Target enum-data type.
    type_name: String,
    /// Requested variant name.
    variant: String,
  },
  /// Typed overloads exist, but none accepts the argument type.
  NoMatchingPayload
  {
    /// Target enum-data type.
    type_name: String,
    /// Requested variant name.
    variant: String,
    /// Actual payload type.
    actual: Type,
    /// Candidate payload types in deterministic declaration order.
    candidates: Vec<TypeRef>,
  },
}

impl fmt::Display for VariantSelectionError
{
  fn fmt(&self, formatter: &mut fmt::Formatter<'_>) -> fmt::Result
  {
    match self
    {
      Self::UnknownType(name) => write!(formatter, "unknown enum data type '{name}'"),
      Self::NotEnumData(name) => write!(formatter, "type '{name}' is not enum data"),
      Self::InvalidHierarchy(diagnostic) => diagnostic.fmt(formatter),
      Self::UnknownVariant { type_name,
                             variant, } => write!(formatter, "enum data '{type_name}' has no variant '{variant}'"),
      Self::PayloadRequired { type_name,
                              variant, } => write!(formatter,
                                                   "enum data variant '{type_name}::{variant}' requires a payload"),
      Self::UnexpectedPayload { type_name,
                                variant, } => write!(formatter,
                                                     "enum data variant '{type_name}::{variant}' does not accept a \
                                                      payload"),
      Self::NoMatchingPayload { type_name,
                                variant,
                                actual,
                                candidates, } => write!(formatter,
                                                        "enum data variant '{type_name}::{variant}' has no overload \
                                                         for {actual:?}; candidates: {candidates:?}"),
    }
  }
}

impl std::error::Error for VariantSelectionError {}
