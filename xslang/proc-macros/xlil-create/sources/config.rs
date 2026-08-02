/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 */

use std::collections::HashSet;

use proc_macro2::TokenStream;
use quote::format_ident;
use syn::{Error, Expr, ExprLit, Ident, Lit, MetaNameValue, Token, parse::Parser, punctuated::Punctuated};

#[derive(Clone, Debug, Default)]
pub(crate) struct Config
{
    module: Option<String>,
    producer: Option<Ident>,
    text_producer: Option<Ident>,
}

impl Config
{
    pub(crate) fn parse(tokens: TokenStream) -> syn::Result<Self>
    {
        if tokens.is_empty()
        {
            return Ok(Self::default());
        }
        let values = Punctuated::<MetaNameValue, Token![,]>::parse_terminated.parse2(tokens)?;
        let mut config = Self::default();
        let mut seen = HashSet::new();
        for value in values
        {
            let Some(name) = value.path.get_ident().map(ToString::to_string)
            else
            {
                return Err(Error::new_spanned(
                    value.path,
                    "xlil_create option names cannot be qualified",
                ));
            };
            if !seen.insert(name.clone())
            {
                return Err(Error::new_spanned(
                    value.path,
                    format!("duplicate xlil_create option '{name}'"),
                ));
            }
            let text = string_value(&value.value)?;
            match name.as_str()
            {
                "module" =>
                {
                    if text.is_empty()
                    {
                        return Err(Error::new_spanned(
                            value.value,
                            "xlil_create module name cannot be empty",
                        ));
                    }
                    config.module = Some(text);
                }
                "producer" => config.producer = Some(identifier(&value.value, &text)?),
                "text" => config.text_producer = Some(identifier(&value.value, &text)?),
                _ =>
                {
                    return Err(Error::new_spanned(
                        value.path,
                        "unknown xlil_create option; expected module, producer, or text",
                    ));
                }
            }
        }
        Ok(config)
    }

    pub(crate) fn module(&self) -> Option<&str>
    {
        self.module.as_deref()
    }

    pub(crate) fn producer(&self, function: &Ident) -> Ident
    {
        self.producer
            .clone()
            .unwrap_or_else(|| format_ident!("{function}_xlil"))
    }

    pub(crate) fn text_producer(&self, function: &Ident) -> Ident
    {
        self.text_producer
            .clone()
            .unwrap_or_else(|| format_ident!("{function}_xlil_text"))
    }
}

fn string_value(expression: &Expr) -> syn::Result<String>
{
    let Expr::Lit(ExprLit {
        lit: Lit::Str(value), ..
    }) = expression
    else
    {
        return Err(Error::new_spanned(
            expression,
            "xlil_create option values must be string literals",
        ));
    };
    Ok(value.value())
}

fn identifier(expression: &Expr, value: &str) -> syn::Result<Ident>
{
    syn::parse_str::<Ident>(value)
        .map_err(|_| Error::new_spanned(expression, "xlil_create producer names must be Rust identifiers"))
}

#[cfg(test)]
mod tests
{
    use quote::quote;

    use super::*;

    #[test]
    fn empty_configuration_uses_conventional_names()
    {
        let config = Config::parse(TokenStream::new()).unwrap();
        let function = format_ident!("max");
        assert_eq!(config.producer(&function), "max_xlil");
        assert_eq!(config.text_producer(&function), "max_xlil_text");
        assert_eq!(config.module(), None);
    }

    #[test]
    fn explicit_configuration_preserves_all_names()
    {
        let config = Config::parse(quote!(module = "Math", producer = "build_max", text = "write_max")).unwrap();
        let function = format_ident!("max");
        assert_eq!(config.module(), Some("Math"));
        assert_eq!(config.producer(&function), "build_max");
        assert_eq!(config.text_producer(&function), "write_max");
    }

    #[test]
    fn duplicate_options_are_rejected()
    {
        let error = Config::parse(quote!(module = "A", module = "B")).unwrap_err();
        assert!(error.to_string().contains("duplicate xlil_create option 'module'"));
    }

    #[test]
    fn unknown_options_are_rejected()
    {
        let error = Config::parse(quote!(backend = "LLVM")).unwrap_err();
        assert!(error.to_string().contains("unknown xlil_create option"));
    }

    #[test]
    fn non_string_values_are_rejected()
    {
        let error = Config::parse(quote!(module = 7)).unwrap_err();
        assert!(error.to_string().contains("must be string literals"));
    }

    #[test]
    fn empty_module_name_is_rejected()
    {
        let error = Config::parse(quote!(module = "")).unwrap_err();
        assert!(error.to_string().contains("module name cannot be empty"));
    }

    #[test]
    fn invalid_producer_identifier_is_rejected()
    {
        let error = Config::parse(quote!(producer = "not-valid")).unwrap_err();
        assert!(error.to_string().contains("must be Rust identifiers"));
    }
}
