/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 */

use std::collections::HashMap;

use proc_macro2::TokenStream;
use quote::{format_ident, quote};
use syn::{
    BinOp, Block, Error, Expr, ExprBinary, ExprIf, ExprLit, ExprPath, ExprUnary, Ident, Lit, Local, Pat, Stmt, UnOp,
};

use crate::infer::{
    arithmetic_operation, comparison_operation, float_arithmetic_operation, infer_block_type, infer_expression_type,
    strip_groups,
};
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

#[derive(Clone)]
pub(crate) struct Binding
{
    pub(crate) binding: Ident,
    pub(crate) value_type: ValueType,
}

pub(crate) struct Lowerer
{
    crate_path: TokenStream,
    bindings: HashMap<String, Binding>,
    next_value: usize,
    next_block: usize,
}

impl Lowerer
{
    pub(crate) fn new(crate_path: TokenStream, parameters: Vec<Parameter>) -> Self
    {
        Self {
            crate_path,
            bindings: parameters
                .into_iter()
                .map(|parameter| {
                    (parameter.source.to_string(), Binding {
                        binding: parameter.binding,
                        value_type: parameter.value_type,
                    })
                })
                .collect(),
            next_value: 0,
            next_block: 0,
        }
    }

    pub(crate) fn lower_body(&mut self, block: &Block, return_type: ValueType) -> syn::Result<TokenStream>
    {
        self.lower_block_terminal(block, return_type, quote!(__xslang_entry))
    }

    fn lower_block_terminal(
        &mut self,
        block: &Block,
        return_type: ValueType,
        current_block: TokenStream,
    ) -> syn::Result<TokenStream>
    {
        let Some((last, prefix)) = block.stmts.split_last()
        else
        {
            if return_type == ValueType::Void
            {
                return Ok(quote!(__xslang_builder.return_void()?;));
            }
            return Err(Error::new_spanned(
                block,
                "value-producing xlil_create function has an empty body",
            ));
        };
        let mut statements = TokenStream::new();
        for statement in prefix
        {
            let Stmt::Local(local) = statement
            else
            {
                return Err(Error::new_spanned(
                    statement,
                    "xlil_create permits only immutable let bindings before the tail expression",
                ));
            };
            statements.extend(self.lower_local(local)?);
        }
        let expression = statement_expression(last)?;
        let terminal = self.lower_terminal(expression, return_type, current_block)?;
        Ok(quote! {
          #statements
          #terminal
        })
    }

    fn lower_local(&mut self, local: &Local) -> syn::Result<TokenStream>
    {
        let Some(initializer) = &local.init
        else
        {
            return Err(Error::new_spanned(
                local,
                "xlil_create let bindings require an initializer",
            ));
        };
        if initializer.diverge.is_some()
        {
            return Err(Error::new_spanned(
                local,
                "xlil_create does not lower let-else bindings",
            ));
        }
        let (identifier, explicit_type) = local_pattern(&local.pat)?;
        let value = self.lower_value(&initializer.expr, explicit_type)?;
        if let Some(expected) = explicit_type
        {
            require_type(&initializer.expr, value.value_type, expected)?;
        }
        let local_binding = format_ident!("__xslang_local_{}_{}", identifier, self.next_value);
        let value_statements = value.statements;
        let value_binding = value.binding;
        self.bindings.insert(identifier.to_string(), Binding {
            binding: local_binding.clone(),
            value_type: value.value_type,
        });
        Ok(quote! {
          #value_statements
          let #local_binding = #value_binding;
        })
    }

    fn lower_terminal(
        &mut self,
        expression: &Expr,
        return_type: ValueType,
        current_block: TokenStream,
    ) -> syn::Result<TokenStream>
    {
        let expression = strip_groups(expression);
        if let Expr::Block(nested) = expression
        {
            let outer = self.bindings.clone();
            let result = self.lower_block_terminal(&nested.block, return_type, current_block);
            self.bindings = outer;
            return result;
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
            return Err(Error::new_spanned(
                expression,
                "a void xlil_create function must end with the unit expression",
            ));
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

    fn lower_if(
        &mut self,
        branch: &ExprIf,
        return_type: ValueType,
        _current_block: TokenStream,
    ) -> syn::Result<TokenStream>
    {
        let Some((_, else_expression)) = &branch.else_branch
        else
        {
            return Err(Error::new_spanned(
                branch,
                "value-producing xlil_create if requires an else arm",
            ));
        };
        let condition = self.lower_value(&branch.cond, Some(ValueType::Bool))?;
        require_type(&branch.cond, condition.value_type, ValueType::Bool)?;

        let then_index = self.next_block;
        self.next_block += 1;
        let else_index = self.next_block;
        self.next_block += 1;
        let then_block = format_ident!("__xslang_then_{then_index}");
        let else_block = format_ident!("__xslang_else_{else_index}");
        let condition_block = format_ident!("__xslang_condition_{then_index}");
        let then_label = format!("then{then_index}");
        let else_label = format!("else{else_index}");

        let outer = self.bindings.clone();
        let then_tokens = self.lower_block_terminal(&branch.then_branch, return_type, quote!(#then_block))?;
        self.bindings = outer.clone();
        let else_tokens = self.lower_terminal(else_expression, return_type, quote!(#else_block))?;
        self.bindings = outer;
        let condition_statements = condition.statements;
        let condition_binding = condition.binding;

        Ok(quote! {
          #condition_statements
          let #condition_block = __xslang_builder.insertion_block()
            .expect("xlil_create always lowers conditions inside a block");
          let #then_block = __xslang_builder.append_block(#then_label)?;
          let #else_block = __xslang_builder.append_block(#else_label)?;
          __xslang_builder.position_at_end(#condition_block)?;
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
            Expr::Unary(unary) => self.lower_unary(unary, expected),
            Expr::If(branch) => self.lower_if_value(branch, expected),
            Expr::Block(block) => self.lower_block_value(&block.block, expected),
            other => Err(Error::new_spanned(
                other,
                "xlil_create does not yet lower this Rust expression",
            )),
        }
    }

    fn lower_path(&self, path: &ExprPath) -> syn::Result<Value>
    {
        let Some(identifier) = path.path.get_ident()
        else
        {
            return Err(Error::new_spanned(
                path,
                "xlil_create does not lower qualified value paths",
            ));
        };
        let Some(parameter) = self.bindings.get(&identifier.to_string())
        else
        {
            return Err(Error::new_spanned(
                path,
                "xlil_create could not resolve this parameter or local binding",
            ));
        };
        Ok(Value {
            statements: TokenStream::new(),
            binding: parameter.binding.clone(),
            value_type: parameter.value_type,
        })
    }

    fn lower_literal(&mut self, literal: &ExprLit, expected: Option<ValueType>) -> syn::Result<Value>
    {
        let binding = self.next_value_binding();
        match &literal.lit
        {
            Lit::Bool(value) =>
            {
                let value = value.value;
                Ok(Value {
                    statements: quote!(let #binding = __xslang_builder.const_bool(#value)?;),
                    binding,
                    value_type: ValueType::Bool,
                })
            }
            Lit::Int(value) =>
            {
                let value_type = expected
                    .filter(|candidate| candidate.is_integer())
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
                            return Err(Error::new_spanned(
                                value,
                                "integer literal exceeds its XLIL signed type",
                            ));
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
                Ok(Value {
                    statements,
                    binding,
                    value_type,
                })
            }
            Lit::Float(value) =>
            {
                let value_type = expected
                    .filter(|candidate| candidate.supports_float_arithmetic())
                    .or_else(|| match value.suffix()
                    {
                        "f32" => Some(ValueType::F32),
                        "f64" | "" => Some(ValueType::F64),
                        _ => None,
                    })
                    .ok_or_else(|| {
                        Error::new_spanned(value, "xlil_create float literal requires an F32 or F64 context")
                    })?;
                let statements = match value_type
                {
                    ValueType::F32 =>
                    {
                        let bits = value.base10_parse::<f32>()?.to_bits();
                        quote!(let #binding = __xslang_builder.const_f32_bits(#bits)?;)
                    }
                    ValueType::F64 =>
                    {
                        let bits = value.base10_parse::<f64>()?.to_bits();
                        quote!(let #binding = __xslang_builder.const_f64_bits(#bits)?;)
                    }
                    _ => unreachable!("floating context was checked above"),
                };
                Ok(Value {
                    statements,
                    binding,
                    value_type,
                })
            }
            _ => Err(Error::new_spanned(
                literal,
                "xlil_create currently supports integer, floating-point, and bool literals",
            )),
        }
    }

    fn lower_binary(&mut self, binary: &ExprBinary, expected: Option<ValueType>) -> syn::Result<Value>
    {
        if matches!(binary.op, BinOp::And(_) | BinOp::Or(_))
        {
            return self.lower_short_circuit(binary);
        }
        let comparison = comparison_operation(&binary.op);
        let is_comparison = comparison.is_some();
        let operation = comparison
            .or_else(|| arithmetic_operation(&binary.op))
            .ok_or_else(|| Error::new_spanned(binary.op, "xlil_create does not support this binary operator"))?;
        let operand_type = self.infer_operand_type(binary, expected)?;
        if !operand_type.is_integer() && !operand_type.supports_float_arithmetic()
        {
            return Err(Error::new_spanned(
                binary,
                "xlil_create binary operations require integer, F32, or F64 operands",
            ));
        }
        if operand_type.is_float() && !is_comparison && !float_arithmetic_operation(&binary.op)
        {
            return Err(Error::new_spanned(
                binary.op,
                "xlil_create floating-point expressions support +, -, *, /, and %",
            ));
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
        let instruction = if operand_type.is_integer()
        {
            quote! {
              let #binding = __xslang_builder.binary_integer(
                #crate_path::xlil::IntegerBinaryOperation::#operation,
                #type_tokens,
                #left_binding,
                #right_binding,
              )?;
            }
        }
        else if is_comparison
        {
            quote! {
              let #binding = __xslang_builder.compare_float(
                #crate_path::xlil::FloatComparisonOperation::#operation,
                #type_tokens,
                #left_binding,
                #right_binding,
              )?;
            }
        }
        else
        {
            quote! {
              let #binding = __xslang_builder.binary_float(
                #crate_path::xlil::FloatBinaryOperation::#operation,
                #type_tokens,
                #left_binding,
                #right_binding,
              )?;
            }
        };
        Ok(Value {
            statements: quote! {
              #left_statements
              #right_statements
              #instruction
            },
            binding,
            value_type,
        })
    }

    fn lower_unary(&mut self, unary: &ExprUnary, expected: Option<ValueType>) -> syn::Result<Value>
    {
        match unary.op
        {
            UnOp::Not(_) =>
            {
                let operand = self.lower_value(&unary.expr, Some(ValueType::Bool))?;
                require_type(&unary.expr, operand.value_type, ValueType::Bool)?;
                let binding = self.next_value_binding();
                let statements = operand.statements;
                let operand = operand.binding;
                Ok(Value {
                    statements: quote! {
                      #statements
                      let #binding = __xslang_builder.not_bool(#operand)?;
                    },
                    binding,
                    value_type: ValueType::Bool,
                })
            }
            UnOp::Neg(_) =>
            {
                let value_type = infer_expression_type(&unary.expr, &self.bindings)
                    .or(expected)
                    .filter(|candidate| candidate.is_integer() || candidate.supports_float_arithmetic())
                    .ok_or_else(|| Error::new_spanned(unary, "xlil_create cannot infer unary negation type"))?;
                let zero = self.lower_zero(value_type)?;
                let operand = self.lower_value(&unary.expr, Some(value_type))?;
                require_type(&unary.expr, operand.value_type, value_type)?;
                let binding = self.next_value_binding();
                let zero_statements = zero.statements;
                let operand_statements = operand.statements;
                let zero_binding = zero.binding;
                let operand_binding = operand.binding;
                let crate_path = &self.crate_path;
                let type_tokens = value_type.tokens(crate_path);
                let operation = if value_type.is_integer()
                {
                    quote! {
                      __xslang_builder.binary_integer(
                        #crate_path::xlil::IntegerBinaryOperation::Sub,
                        #type_tokens,
                        #zero_binding,
                        #operand_binding,
                      )?
                    }
                }
                else
                {
                    quote! {
                      __xslang_builder.binary_float(
                        #crate_path::xlil::FloatBinaryOperation::Sub,
                        #type_tokens,
                        #zero_binding,
                        #operand_binding,
                      )?
                    }
                };
                Ok(Value {
                    statements: quote! {
                      #zero_statements
                      #operand_statements
                      let #binding = #operation;
                    },
                    binding,
                    value_type,
                })
            }
            _ => Err(Error::new_spanned(unary, "xlil_create supports only ! and unary -")),
        }
    }

    fn lower_zero(&mut self, value_type: ValueType) -> syn::Result<Value>
    {
        let binding = self.next_value_binding();
        let crate_path = &self.crate_path;
        let statements = match value_type
        {
            ValueType::I32 => quote!(let #binding = __xslang_builder.const_i32(0)?;),
            ValueType::I64 => quote!(let #binding = __xslang_builder.const_i64(0)?;),
            ValueType::I8 | ValueType::I16 | ValueType::I128 =>
            {
                let type_tokens = value_type.tokens(crate_path);
                quote! {
                  let #binding = __xslang_builder.const_integer(
                    #crate_path::xlil::IntegerConstant::new(#type_tokens, 0)
                      .expect("zero fits every XLIL integer type"),
                  )?;
                }
            }
            ValueType::F32 => quote!(let #binding = __xslang_builder.const_f32_bits(0)?;),
            ValueType::F64 => quote!(let #binding = __xslang_builder.const_f64_bits(0)?;),
            _ =>
            {
                return Err(Error::new(
                    proc_macro2::Span::call_site(),
                    "xlil_create cannot construct zero for this type",
                ));
            }
        };
        Ok(Value {
            statements,
            binding,
            value_type,
        })
    }

    fn lower_block_value(&mut self, block: &Block, expected: Option<ValueType>) -> syn::Result<Value>
    {
        let outer = self.bindings.clone();
        let result = (|| {
            let Some((last, prefix)) = block.stmts.split_last()
            else
            {
                return Err(Error::new_spanned(block, "xlil_create value block cannot be empty"));
            };
            let mut statements = TokenStream::new();
            for statement in prefix
            {
                let Stmt::Local(local) = statement
                else
                {
                    return Err(Error::new_spanned(
                        statement,
                        "xlil_create value blocks permit only immutable let bindings",
                    ));
                };
                statements.extend(self.lower_local(local)?);
            }
            let expression = statement_expression(last)?;
            let value = self.lower_value(expression, expected)?;
            let value_statements = value.statements;
            Ok(Value {
                statements: quote! {
                  #statements
                  #value_statements
                },
                binding: value.binding,
                value_type: value.value_type,
            })
        })();
        self.bindings = outer;
        result
    }

    fn lower_if_value(&mut self, branch: &ExprIf, expected: Option<ValueType>) -> syn::Result<Value>
    {
        let Some((_, else_expression)) = &branch.else_branch
        else
        {
            return Err(Error::new_spanned(
                branch,
                "value-producing xlil_create if requires an else arm",
            ));
        };
        let inferred = infer_block_type(&branch.then_branch, &self.bindings)
            .or_else(|| infer_expression_type(else_expression, &self.bindings));
        let result_type = expected
            .or(inferred)
            .ok_or_else(|| Error::new_spanned(branch, "xlil_create cannot infer if expression result type"))?;
        if result_type == ValueType::Void
        {
            return Err(Error::new_spanned(branch, "xlil_create value if cannot produce void"));
        }
        let condition = self.lower_value(&branch.cond, Some(ValueType::Bool))?;
        require_type(&branch.cond, condition.value_type, ValueType::Bool)?;
        let outer = self.bindings.clone();
        let then_value = self.lower_block_value(&branch.then_branch, Some(result_type))?;
        self.bindings = outer.clone();
        let else_value = self.lower_value(else_expression, Some(result_type))?;
        self.bindings = outer;
        if then_value.value_type != result_type
        {
            return Err(Error::new_spanned(
                &branch.then_branch,
                format!(
                    "xlil_create type mismatch: expected {result_type:?}, found {:?}",
                    then_value.value_type
                ),
            ));
        }
        require_type(else_expression, else_value.value_type, result_type)?;

        let index = self.next_block;
        self.next_block += 3;
        let current = format_ident!("__xslang_if_current_{index}");
        let then_block = format_ident!("__xslang_if_then_{index}");
        let else_block = format_ident!("__xslang_if_else_{index}");
        let merge_block = format_ident!("__xslang_if_merge_{index}");
        let slot = format_ident!("__xslang_if_slot_{index}");
        let binding = self.next_value_binding();
        let condition_statements = condition.statements;
        let condition_binding = condition.binding;
        let then_statements = then_value.statements;
        let then_binding = then_value.binding;
        let else_statements = else_value.statements;
        let else_binding = else_value.binding;
        let type_tokens = result_type.tokens(&self.crate_path);
        let then_label = format!("if_then{index}");
        let else_label = format!("if_else{index}");
        let merge_label = format!("if_merge{index}");

        Ok(Value {
            statements: quote! {
              #condition_statements
              let #current = __xslang_builder.insertion_block()
                .expect("xlil_create always lowers expressions inside a block");
              let #slot = __xslang_builder.add_slot(#type_tokens)?;
              let #then_block = __xslang_builder.append_block(#then_label)?;
              let #else_block = __xslang_builder.append_block(#else_label)?;
              let #merge_block = __xslang_builder.append_block(#merge_label)?;
              __xslang_builder.position_at_end(#current)?;
              __xslang_builder.branch_if(#condition_binding, #then_block, #else_block)?;
              __xslang_builder.position_at_end(#then_block)?;
              #then_statements
              __xslang_builder.store(#slot, #then_binding)?;
              __xslang_builder.branch(#merge_block)?;
              __xslang_builder.position_at_end(#else_block)?;
              #else_statements
              __xslang_builder.store(#slot, #else_binding)?;
              __xslang_builder.branch(#merge_block)?;
              __xslang_builder.position_at_end(#merge_block)?;
              let #binding = __xslang_builder.load(#slot)?;
            },
            binding,
            value_type: result_type,
        })
    }

    fn lower_short_circuit(&mut self, binary: &ExprBinary) -> syn::Result<Value>
    {
        let left = self.lower_value(&binary.left, Some(ValueType::Bool))?;
        require_type(&binary.left, left.value_type, ValueType::Bool)?;
        let right = self.lower_value(&binary.right, Some(ValueType::Bool))?;
        require_type(&binary.right, right.value_type, ValueType::Bool)?;
        let index = self.next_block;
        self.next_block += 3;
        let current = format_ident!("__xslang_logic_current_{index}");
        let evaluate = format_ident!("__xslang_logic_evaluate_{index}");
        let short = format_ident!("__xslang_logic_short_{index}");
        let merge = format_ident!("__xslang_logic_merge_{index}");
        let slot = format_ident!("__xslang_logic_slot_{index}");
        let short_value = format_ident!("__xslang_logic_value_{index}");
        let binding = self.next_value_binding();
        let left_statements = left.statements;
        let left_binding = left.binding;
        let right_statements = right.statements;
        let right_binding = right.binding;
        let (then_target, else_target, constant) = match binary.op
        {
            BinOp::And(_) => (quote!(#evaluate), quote!(#short), false),
            BinOp::Or(_) => (quote!(#short), quote!(#evaluate), true),
            _ => unreachable!("caller checked short-circuit operator"),
        };
        let evaluate_label = format!("logic_evaluate{index}");
        let short_label = format!("logic_short{index}");
        let merge_label = format!("logic_merge{index}");
        let crate_path = &self.crate_path;
        Ok(Value {
            statements: quote! {
              #left_statements
              let #current = __xslang_builder.insertion_block()
                .expect("xlil_create always lowers expressions inside a block");
              let #slot = __xslang_builder.add_slot(#crate_path::xlil::Type::BOOL)?;
              let #evaluate = __xslang_builder.append_block(#evaluate_label)?;
              let #short = __xslang_builder.append_block(#short_label)?;
              let #merge = __xslang_builder.append_block(#merge_label)?;
              __xslang_builder.position_at_end(#current)?;
              __xslang_builder.branch_if(#left_binding, #then_target, #else_target)?;
              __xslang_builder.position_at_end(#evaluate)?;
              #right_statements
              __xslang_builder.store(#slot, #right_binding)?;
              __xslang_builder.branch(#merge)?;
              __xslang_builder.position_at_end(#short)?;
              let #short_value = __xslang_builder.const_bool(#constant)?;
              __xslang_builder.store(#slot, #short_value)?;
              __xslang_builder.branch(#merge)?;
              __xslang_builder.position_at_end(#merge)?;
              let #binding = __xslang_builder.load(#slot)?;
            },
            binding,
            value_type: ValueType::Bool,
        })
    }

    fn infer_operand_type(&self, binary: &ExprBinary, expected: Option<ValueType>) -> syn::Result<ValueType>
    {
        infer_expression_type(&binary.left, &self.bindings)
            .or_else(|| infer_expression_type(&binary.right, &self.bindings))
            .or_else(|| expected.filter(|candidate| candidate.is_integer() || candidate.supports_float_arithmetic()))
            .ok_or_else(|| Error::new_spanned(binary, "xlil_create could not infer an integer operand type"))
    }

    fn next_value_binding(&mut self) -> Ident
    {
        let index = self.next_value;
        self.next_value += 1;
        format_ident!("__xslang_value_{index}")
    }
}

fn statement_expression(statement: &Stmt) -> syn::Result<&Expr>
{
    match statement
    {
        Stmt::Expr(expression, None) => Ok(expression),
        Stmt::Expr(Expr::Return(value), Some(_)) => value
            .expr
            .as_deref()
            .ok_or_else(|| Error::new_spanned(value, "xlil_create requires a return value")),
        _ => Err(Error::new_spanned(
            statement,
            "xlil_create block must end with a tail expression or return statement",
        )),
    }
}

fn local_pattern(pattern: &Pat) -> syn::Result<(Ident, Option<ValueType>)>
{
    match pattern
    {
        Pat::Ident(identifier)
            if identifier.by_ref.is_none() && identifier.mutability.is_none() && identifier.subpat.is_none() =>
        {
            Ok((identifier.ident.clone(), None))
        }
        Pat::Type(typed) =>
        {
            let Pat::Ident(identifier) = typed.pat.as_ref()
            else
            {
                return Err(Error::new_spanned(
                    pattern,
                    "xlil_create typed let requires an identifier pattern",
                ));
            };
            if identifier.by_ref.is_some() || identifier.mutability.is_some() || identifier.subpat.is_some()
            {
                return Err(Error::new_spanned(
                    pattern,
                    "xlil_create let bindings must be immutable identifiers",
                ));
            }
            Ok((identifier.ident.clone(), Some(ValueType::parse(&typed.ty)?)))
        }
        _ => Err(Error::new_spanned(
            pattern,
            "xlil_create let bindings must be immutable identifiers",
        )),
    }
}

fn require_type(expression: &Expr, actual: ValueType, expected: ValueType) -> syn::Result<()>
{
    if actual == expected
    {
        Ok(())
    }
    else
    {
        Err(Error::new_spanned(
            expression,
            format!("xlil_create type mismatch: expected {expected:?}, found {actual:?}"),
        ))
    }
}
