/*
 * SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
 * SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.1
 */

// Only the scalar/aggregate type descriptor is retained for the historical
// MIR analyses. Executable XLIL records and all construction APIs are gone.
mod types;

pub use types::{Type, TypeKind};

/// Aggregate layout retained only for parsing historical XMIR documents.
#[derive(Clone, Debug, Eq, PartialEq)]
pub struct AggregateType
{
    pub id: u32,
    pub name: String,
    pub fields: Vec<Type>,
}

/// Array layout retained only for parsing historical XMIR documents.
#[derive(Clone, Debug, Eq, PartialEq)]
pub struct ArrayType
{
    pub id: u32,
    pub element_type: Type,
    pub length: Option<u64>,
}
