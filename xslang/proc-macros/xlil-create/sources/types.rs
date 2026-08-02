/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 */

use proc_macro2::TokenStream;
use quote::quote;
use syn::{Error, ReturnType, Type as RustType, TypePath};

#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub(crate) enum ValueType
{
    Void,
    Bool,
    I8,
    I16,
    I32,
    I64,
    I128,
    F16,
    F32,
    F64,
    F128,
}

impl ValueType
{
    pub(crate) fn parse(rust_type: &RustType) -> syn::Result<Self>
    {
        let RustType::Path(TypePath {
            qself: None,
            path,
        }) = rust_type
        else
        {
            return Err(Error::new_spanned(rust_type, "xlil_create requires a named value type"));
        };
        let Some(identifier) = path.segments.last().map(|segment| &segment.ident)
        else
        {
            return Err(Error::new_spanned(rust_type, "xlil_create requires a named value type"));
        };
        match identifier.to_string().as_str()
        {
            "bool" => Ok(Self::Bool),
            "i8" | "I8" => Ok(Self::I8),
            "i16" | "I16" => Ok(Self::I16),
            "i32" | "I32" => Ok(Self::I32),
            "i64" | "I64" => Ok(Self::I64),
            "i128" | "I128" => Ok(Self::I128),
            "F16" => Ok(Self::F16),
            "f32" | "F32" => Ok(Self::F32),
            "f64" | "F64" => Ok(Self::F64),
            "F128" => Ok(Self::F128),
            _ => Err(Error::new_spanned(
                rust_type,
                "xlil_create requires bool or an xslang::xlil::types numeric type",
            )),
        }
    }

    pub(crate) fn parse_return(output: &ReturnType) -> syn::Result<Self>
    {
        match output
        {
            ReturnType::Default => Ok(Self::Void),
            ReturnType::Type(_, value_type) => Self::parse(value_type),
        }
    }

    pub(crate) fn tokens(self, crate_path: &TokenStream) -> TokenStream
    {
        match self
        {
            Self::Void => quote!(#crate_path::xlil::Type::VOID),
            Self::Bool => quote!(#crate_path::xlil::Type::BOOL),
            Self::I8 => quote!(#crate_path::xlil::Type::I8),
            Self::I16 => quote!(#crate_path::xlil::Type::I16),
            Self::I32 => quote!(#crate_path::xlil::Type::I32),
            Self::I64 => quote!(#crate_path::xlil::Type::I64),
            Self::I128 => quote!(#crate_path::xlil::Type::I128),
            Self::F16 => quote!(#crate_path::xlil::Type::F16),
            Self::F32 => quote!(#crate_path::xlil::Type::F32),
            Self::F64 => quote!(#crate_path::xlil::Type::F64),
            Self::F128 => quote!(#crate_path::xlil::Type::F128),
        }
    }

    pub(crate) const fn is_integer(self) -> bool
    {
        matches!(self, Self::I8 | Self::I16 | Self::I32 | Self::I64 | Self::I128)
    }

    pub(crate) const fn is_float(self) -> bool
    {
        matches!(self, Self::F16 | Self::F32 | Self::F64 | Self::F128)
    }

    pub(crate) const fn supports_float_arithmetic(self) -> bool
    {
        matches!(self, Self::F32 | Self::F64)
    }
}
