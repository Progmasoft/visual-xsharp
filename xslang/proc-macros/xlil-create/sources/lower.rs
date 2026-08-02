/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 */

use std::collections::HashMap;

use proc_macro2::TokenStream;
use quote::{format_ident, quote};
use syn::{BinOp, Block, Error, Expr, ExprBinary, ExprIf, ExprLit, ExprPath, Ident, Lit, Stmt};

use crate::types::ValueType;

#[derive(Clone)]
pub(crate) struct Parameter
{
  pub(crate) source: Ident,
  pub(crate) binding: Ident,
  pub(crate) value_type: ValueType,
}

struct Value
{
  statements: TokenStream,
  binding: Ident,
  value_type: ValueType,
}

pub(crate) struct Lowerer
{
  crate_path: TokenStream,
  parameters: HashMap<String, Parameter>,
  next_value: usize,
  next_block: usize,
}

impl Lowerer
{
  pub(crate) fn new(crate_path: TokenStream, parameters: Vec<Parameter>) -> Self
  {
    Self { crate_path,
           parameters: parameters.into_iter()
                                 .map(|parameter| (parameter.source.to_string(), parameter))
                                 .collect(),
           next_value: 0,
           next_block: 0 }
  }

  pub(crate) fn lower_body(&mut self, block: &Block, return_type: ValueType) -> syn::Result<TokenStream>
  {
    let expression = terminal_expression(block)?;
    self.lower_terminal(expression, return_type, quote!(__xslang_entry))
  }

  fn lower_terminal(&mut self,
                    expression: &Expr,
                    return_type: ValueType,
                    current_block: TokenStream)
                    -> syn::Result<TokenStream>
  {
    let expression = strip_groups(expression);
    if let Expr::Block(nested) = expression
    {
      return self.lower_terminal(terminal_expression(&nested.block)?, return_type, current_block);
    }
    if let Expr::If(branch) = expression
    {
      return self.lower_if(branch, return_type, current_block);
    }
    if return_type == ValueType::Void
    {
      if matches!(expression, Expr::Tuple(tuple) if tuple.elems.is_empty())
      {
        return Ok(quote!(__xslang_builder.return_void()?;));
      }
      return Err(Error::new_spanned(expression,
                                    "a void xlil_create function must end with the unit expression"));
    }

    let value = self.lower_value(expression, Some(return_type))?;
    require_type(expression, value.value_type, return_type)?;
    let statements = value.statements;
    let binding = value.binding;
    Ok(quote! {
      #statements
      __xslang_builder.return_value(::core::option::Option::Some(#binding))?;
    })
  }

  fn lower_if(&mut self,
              branch: &ExprIf,
              return_type: ValueType,
              current_block: TokenStream)
              -> syn::Result<TokenStream>
  {
    let Some((_, else_expression)) = &branch.else_branch
    else
    {
      return Err(Error::new_spanned(branch, "value-producing xlil_create if requires an else arm"));
    };
    let condition = self.lower_value(&branch.cond, Some(ValueType::Bool))?;
    require_type(&branch.cond, condition.value_type, ValueType::Bool)?;

    let then_index = self.next_block;
    self.next_block += 1;
    let else_index = self.next_block;
    self.next_block += 1;
    let then_block = format_ident!("__xslang_then_{then_index}");
    let else_block = format_ident!("__xslang_else_{else_index}");
    let then_label = format!("then{then_index}");
    let else_label = format!("else{else_index}");

    let then_expression = terminal_expression(&branch.then_branch)?;
    let then_tokens = self.lower_terminal(then_expression, return_type, quote!(#then_block))?;
    let else_tokens = self.lower_terminal(else_expression, return_type, quote!(#else_block))?;
    let condition_statements = condition.statements;
    let condition_binding = condition.binding;

    Ok(quote! {
      #condition_statements
      let #then_block = __xslang_builder.append_block(#then_label)?;
      let #else_block = __xslang_builder.append_block(#else_label)?;
      __xslang_builder.position_at_end(#current_block)?;
      __xslang_builder.branch_if(#condition_binding, #then_block, #else_block)?;
      __xslang_builder.position_at_end(#then_block)?;
      #then_tokens
      __xslang_builder.position_at_end(#else_block)?;
      #else_tokens
    })
  }

  fn lower_value(&mut self, expression: &Expr, expected: Option<ValueType>) -> syn::Result<Value>
  {
    match strip_groups(expression)
    {
      Expr::Path(path) => self.lower_path(path),
      Expr::Lit(literal) => self.lower_literal(literal, expected),
      Expr::Binary(binary) => self.lower_binary(binary, expected),
      other => Err(Error::new_spanned(other,
                                      "xlil_create currently lowers parameters, literals, and integer binary \
                                       expressions")),
    }
  }

  fn lower_path(&self, path: &ExprPath) -> syn::Result<Value>
  {
    let Some(identifier) = path.path.get_ident()
    else
    {
      return Err(Error::new_spanned(path, "xlil_create does not lower qualified value paths"));
    };
    let Some(parameter) = self.parameters.get(&identifier.to_string())
    else
    {
      return Err(Error::new_spanned(path, "xlil_create only resolves function parameters in expressions"));
    };
    Ok(Value { statements: TokenStream::new(),
               binding: parameter.binding.clone(),
               value_type: parameter.value_type })
  }

  fn lower_literal(&mut self, literal: &ExprLit, expected: Option<ValueType>) -> syn::Result<Value>
  {
    let binding = self.next_value_binding();
    match &literal.lit
    {
      Lit::Bool(value) =>
      {
        let value = value.value;
        Ok(Value { statements: quote!(let #binding = __xslang_builder.const_bool(#value)?;),
                   binding,
                   value_type: ValueType::Bool })
      }
      Lit::Int(value) =>
      {
        let value_type = expected.filter(|candidate| candidate.is_integer())
                                 .unwrap_or(ValueType::I64);
        let statements = match value_type
        {
          ValueType::I8 | ValueType::I16 | ValueType::I128 =>
          {
            let parsed = value.base10_parse::<u128>()?;
            let maximum = match value_type
            {
              ValueType::I8 => i8::MAX as u128,
              ValueType::I16 => i16::MAX as u128,
              ValueType::I128 => i128::MAX as u128,
              _ => unreachable!("narrow integer variants were matched above"),
            };
            if parsed > maximum
            {
              return Err(Error::new_spanned(value, "integer literal exceeds its XLIL signed type"));
            }
            let crate_path = &self.crate_path;
            let type_tokens = value_type.tokens(crate_path);
            quote! {
              let #binding = __xslang_builder.const_integer(
                #crate_path::xlil::IntegerConstant::new(#type_tokens, #parsed)
                  .expect("xlil_create validated the integer width"),
              )?;
            }
          }
          ValueType::I32 =>
          {
            let parsed = value.base10_parse::<i32>()?;
            quote!(let #binding = __xslang_builder.const_i32(#parsed)?;)
          }
          ValueType::I64 =>
          {
            let parsed = value.base10_parse::<i64>()?;
            quote!(let #binding = __xslang_builder.const_i64(#parsed)?;)
          }
          _ => unreachable!("integer context was checked above"),
        };
        Ok(Value { statements,
                   binding,
                   value_type })
      }
      _ => Err(Error::new_spanned(literal, "xlil_create currently supports integer and bool literals")),
    }
  }

  fn lower_binary(&mut self, binary: &ExprBinary, expected: Option<ValueType>) -> syn::Result<Value>
  {
    let comparison = comparison_operation(&binary.op);
    let is_comparison = comparison.is_some();
    let operation = comparison.or_else(|| arithmetic_operation(&binary.op)).ok_or_else(|| {
                                                                              Error::new_spanned(&binary.op,
                                                                                                 "xlil_create does \
                                                                                                  not support this \
                                                                                                  binary operator")
                                                                            })?;
    let operand_type = self.infer_operand_type(binary, expected)?;
    if !operand_type.is_integer()
    {
      return Err(Error::new_spanned(binary,
                                    "xlil_create binary operations currently require integer operands"));
    }
    let left = self.lower_value(&binary.left, Some(operand_type))?;
    let right = self.lower_value(&binary.right, Some(operand_type))?;
    require_type(&binary.left, left.value_type, operand_type)?;
    require_type(&binary.right, right.value_type, operand_type)?;
    let binding = self.next_value_binding();
    let left_statements = left.statements;
    let right_statements = right.statements;
    let left_binding = left.binding;
    let right_binding = right.binding;
    let crate_path = &self.crate_path;
    let type_tokens = operand_type.tokens(crate_path);
    let value_type = if is_comparison
    {
      ValueType::Bool
    }
    else
    {
      operand_type
    };
    Ok(Value { statements: quote! {
                 #left_statements
                 #right_statements
                 let #binding = __xslang_builder.binary_integer(
                   #crate_path::xlil::IntegerBinaryOperation::#operation,
                   #type_tokens,
                   #left_binding,
                   #right_binding,
                 )?;
               },
               binding,
               value_type })
  }

  fn infer_operand_type(&self, binary: &ExprBinary, expected: Option<ValueType>) -> syn::Result<ValueType>
  {
    infer_expression_type(&binary.left, &self.parameters).or_else(|| {
                                                           infer_expression_type(&binary.right, &self.parameters)
                                                         })
                                                         .or_else(|| {
                                                           expected.filter(|candidate| candidate.is_integer())
                                                         })
                                                         .ok_or_else(|| {
                                                           Error::new_spanned(binary,
                                                                              "xlil_create could not infer an integer \
                                                                               operand type")
                                                         })
  }

  fn next_value_binding(&mut self) -> Ident
  {
    let index = self.next_value;
    self.next_value += 1;
    format_ident!("__xslang_value_{index}")
  }
}

fn terminal_expression(block: &Block) -> syn::Result<&Expr>
{
  match block.stmts.as_slice()
  {
    [Stmt::Expr(expression, None)] => Ok(expression),
    [Stmt::Expr(Expr::Return(value), Some(_))] =>
    {
      value.expr
           .as_deref()
           .ok_or_else(|| Error::new_spanned(value, "xlil_create requires a return value"))
    }
    _ => Err(Error::new_spanned(block,
                                "xlil_create currently requires one tail expression or return statement")),
  }
}

fn strip_groups(mut expression: &Expr) -> &Expr
{
  loop
  {
    expression = match expression
    {
      Expr::Group(group) => &group.expr,
      Expr::Paren(parenthesized) => &parenthesized.expr,
      _ => return expression,
    };
  }
}

fn infer_expression_type(expression: &Expr, parameters: &HashMap<String, Parameter>) -> Option<ValueType>
{
  match strip_groups(expression)
  {
    Expr::Path(path) => path.path
                            .get_ident()
                            .and_then(|identifier| parameters.get(&identifier.to_string()))
                            .map(|parameter| parameter.value_type),
    Expr::Lit(ExprLit { lit: Lit::Bool(_), .. }) => Some(ValueType::Bool),
    Expr::Binary(binary) if comparison_operation(&binary.op).is_some() => Some(ValueType::Bool),
    Expr::Binary(binary) =>
    {
      infer_expression_type(&binary.left, parameters).or_else(|| infer_expression_type(&binary.right, parameters))
    }
    _ => None,
  }
}

fn arithmetic_operation(operation: &BinOp) -> Option<Ident>
{
  Some(format_ident!("{}", match operation
       {
         BinOp::Add(_) => "Add",
         BinOp::Sub(_) => "Sub",
         BinOp::Mul(_) => "Mul",
         BinOp::Div(_) => "Div",
         BinOp::Rem(_) => "Rem",
         BinOp::BitAnd(_) => "BitAnd",
         BinOp::BitOr(_) => "BitOr",
         BinOp::BitXor(_) => "BitXor",
         BinOp::Shl(_) => "ShiftLeft",
         BinOp::Shr(_) => "ShiftRight",
         _ => return None,
       }))
}

fn comparison_operation(operation: &BinOp) -> Option<Ident>
{
  Some(format_ident!("{}", match operation
       {
         BinOp::Eq(_) => "Equal",
         BinOp::Ne(_) => "NotEqual",
         BinOp::Lt(_) => "Less",
         BinOp::Le(_) => "LessEqual",
         BinOp::Gt(_) => "Greater",
         BinOp::Ge(_) => "GreaterEqual",
         _ => return None,
       }))
}

fn require_type(expression: &Expr, actual: ValueType, expected: ValueType) -> syn::Result<()>
{
  if actual == expected
  {
    Ok(())
  }
  else
  {
    Err(Error::new_spanned(expression,
                           format!("xlil_create type mismatch: expected {expected:?}, found {actual:?}")))
  }
}
