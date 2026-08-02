/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 */

use proc_macro_crate::{FoundCrate, crate_name};
use proc_macro2::TokenStream;
use quote::{format_ident, quote};
use syn::{Error, FnArg, ItemFn, Pat, Signature, parse2};

use crate::lower::{Lowerer, Parameter};
use crate::types::ValueType;

pub(crate) fn expand(attribute: TokenStream, item: TokenStream) -> TokenStream
{
  match expand_checked(attribute, item)
  {
    Ok(tokens) => tokens,
    Err(error) => error.into_compile_error(),
  }
}

fn expand_checked(attribute: TokenStream, item: TokenStream) -> syn::Result<TokenStream>
{
  if !attribute.is_empty()
  {
    return Err(Error::new_spanned(attribute, "xlil_create does not accept attribute arguments"));
  }

  let function: ItemFn = parse2(item)?;
  validate_signature(&function.sig)?;
  let crate_path = xslang_path()?;
  let parameters = parameters(&function.sig)?;
  let return_type = ValueType::parse_return(&function.sig.output)?;
  let mut lowerer = Lowerer::new(crate_path.clone(), parameters.clone());
  let body = lowerer.lower_body(&function.block, return_type)?;

  let original_name = &function.sig.ident;
  let producer_name = format_ident!("{}_xlil", original_name);
  let visibility = &function.vis;
  let parameter_types = parameters.iter()
                                  .map(|parameter| parameter.value_type.tokens(&crate_path));
  let parameter_bindings = parameters.iter().enumerate().map(|(index, parameter)| {
                                                          let binding = &parameter.binding;
                                                          quote!(let #binding = __xslang_builder.parameter(#index)?;)
                                                        });
  let return_tokens = return_type.tokens(&crate_path);
  let documentation = format!("Builds the XLIL module generated from [`{original_name}`].");

  Ok(quote! {
    #function

    #[doc = #documentation]
    #visibility fn #producer_name()
      -> ::core::result::Result<#crate_path::xlil::Module, #crate_path::xlil::BuildError>
    {
      let mut __xslang_builder = #crate_path::xlil::Builder::new(
        ::core::concat!(::core::module_path!(), "::", ::core::stringify!(#original_name))
      );
      __xslang_builder.begin_function(
        ::core::stringify!(#original_name),
        #return_tokens,
        ::std::vec![#(#parameter_types),*],
      )?;
      let __xslang_entry = __xslang_builder.append_block("entry")?;
      #(#parameter_bindings)*
      #body
      __xslang_builder.finish()
    }
  })
}

fn validate_signature(signature: &Signature) -> syn::Result<()>
{
  if signature.constness.is_some() ||
     signature.asyncness.is_some() ||
     signature.unsafety.is_some() ||
     signature.abi.is_some()
  {
    return Err(Error::new_spanned(signature, "xlil_create requires a synchronous, safe Rust function"));
  }
  if !signature.generics.params.is_empty() || signature.generics.where_clause.is_some()
  {
    return Err(Error::new_spanned(&signature.generics, "xlil_create does not yet lower generics"));
  }
  if signature.variadic.is_some()
  {
    return Err(Error::new_spanned(signature, "xlil_create does not lower variadic functions"));
  }
  Ok(())
}

fn parameters(signature: &Signature) -> syn::Result<Vec<Parameter>>
{
  signature.inputs
           .iter()
           .enumerate()
           .map(|(index, argument)| {
             let FnArg::Typed(argument) = argument
             else
             {
               return Err(Error::new_spanned(argument, "xlil_create can only be applied to free functions"));
             };
             let Pat::Ident(pattern) = argument.pat.as_ref()
             else
             {
               return Err(Error::new_spanned(&argument.pat,
                                             "xlil_create parameters must use simple identifier patterns"));
             };
             if pattern.by_ref.is_some() || pattern.subpat.is_some()
             {
               return Err(Error::new_spanned(pattern, "xlil_create parameters cannot use ref or subpatterns"));
             }
             Ok(Parameter { source: pattern.ident.clone(),
                            binding: format_ident!("__xslang_parameter_{index}"),
                            value_type: ValueType::parse(&argument.ty)? })
           })
           .collect()
}

fn xslang_path() -> syn::Result<TokenStream>
{
  match crate_name("xslang")
  {
    Ok(FoundCrate::Itself) => Ok(quote!(crate)),
    Ok(FoundCrate::Name(name)) =>
    {
      let identifier = format_ident!("{}", name.replace('-', "_"));
      Ok(quote!(::#identifier))
    }
    Err(error) => Err(Error::new(proc_macro2::Span::call_site(),
                                 format!("xlil_create could not locate the xslang dependency: {error}"))),
  }
}
