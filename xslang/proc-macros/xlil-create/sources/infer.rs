/*
 * SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
 * SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.0
 */

//! Local type inference and operator classification for macro lowering.

use std::collections::HashMap;

use proc_macro2::Ident;
use quote::format_ident;
use syn::{BinOp, Block, Expr, ExprLit, Lit, Stmt, UnOp};

use crate::lower::Binding;
use crate::types::ValueType;

pub(crate) fn strip_groups(mut expression: &Expr) -> &Expr
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

pub(crate) fn infer_expression_type(expression: &Expr, bindings: &HashMap<String, Binding>) -> Option<ValueType>
{
    match strip_groups(expression)
    {
        Expr::Path(path) => path
            .path
            .get_ident()
            .and_then(|identifier| bindings.get(&identifier.to_string()))
            .map(|binding| binding.value_type),
        Expr::Lit(ExprLit {
            lit: Lit::Bool(_), ..
        }) => Some(ValueType::Bool),
        Expr::Lit(ExprLit {
            lit: Lit::Float(value),
            ..
        }) => match value.suffix()
        {
            "f32" => Some(ValueType::F32),
            "f64" | "" => Some(ValueType::F64),
            _ => None,
        },
        Expr::Binary(binary) if comparison_operation(&binary.op).is_some() => Some(ValueType::Bool),
        Expr::Binary(binary) if matches!(binary.op, BinOp::And(_) | BinOp::Or(_)) => Some(ValueType::Bool),
        Expr::Binary(binary) =>
        {
            infer_expression_type(&binary.left, bindings).or_else(|| infer_expression_type(&binary.right, bindings))
        }
        Expr::Unary(unary) => match unary.op
        {
            UnOp::Not(_) => Some(ValueType::Bool),
            UnOp::Neg(_) => infer_expression_type(&unary.expr, bindings),
            _ => None,
        },
        Expr::If(branch) => infer_block_type(&branch.then_branch, bindings).or_else(|| {
            branch
                .else_branch
                .as_ref()
                .and_then(|(_, value)| infer_expression_type(value, bindings))
        }),
        Expr::Block(block) => infer_block_type(&block.block, bindings),
        _ => None,
    }
}

pub(crate) fn infer_block_type(block: &Block, bindings: &HashMap<String, Binding>) -> Option<ValueType>
{
    block.stmts.last().and_then(|statement| match statement
    {
        Stmt::Expr(expression, None) => infer_expression_type(expression, bindings),
        Stmt::Expr(Expr::Return(value), Some(_)) => value
            .expr
            .as_deref()
            .and_then(|expression| infer_expression_type(expression, bindings)),
        _ => None,
    })
}

pub(crate) fn arithmetic_operation(operation: &BinOp) -> Option<Ident>
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

pub(crate) fn comparison_operation(operation: &BinOp) -> Option<Ident>
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

pub(crate) fn float_arithmetic_operation(operation: &BinOp) -> bool
{
    matches!(
        operation,
        BinOp::Add(_) | BinOp::Sub(_) | BinOp::Mul(_) | BinOp::Div(_) | BinOp::Rem(_)
    )
}

#[cfg(test)]
mod tests
{
    use quote::quote;
    use syn::parse2;

    use super::*;

    fn expression(tokens: proc_macro2::TokenStream) -> Expr
    {
        parse2(tokens).unwrap()
    }

    #[test]
    fn group_stripping_reaches_the_inner_expression()
    {
        let value = expression(quote!((true)));
        assert!(matches!(strip_groups(&value), Expr::Lit(_)));
    }

    #[test]
    fn literal_and_unary_types_are_inferred()
    {
        let bindings = HashMap::new();
        assert_eq!(
            infer_expression_type(&expression(quote!(true)), &bindings),
            Some(ValueType::Bool)
        );
        assert_eq!(
            infer_expression_type(&expression(quote!(1.5f32)), &bindings),
            Some(ValueType::F32)
        );
        assert_eq!(
            infer_expression_type(&expression(quote!(-1.5f64)), &bindings),
            Some(ValueType::F64)
        );
        assert_eq!(
            infer_expression_type(&expression(quote!(!false)), &bindings),
            Some(ValueType::Bool)
        );
    }

    #[test]
    fn registered_binding_type_is_inferred()
    {
        let mut bindings = HashMap::new();
        bindings.insert("value".into(), Binding {
            binding: format_ident!("generated"),
            value_type: ValueType::I32,
        });
        assert_eq!(
            infer_expression_type(&expression(quote!(value)), &bindings),
            Some(ValueType::I32)
        );
        assert_eq!(
            infer_expression_type(&expression(quote!(value + 1)), &bindings),
            Some(ValueType::I32)
        );
    }

    #[test]
    fn comparisons_and_short_circuit_are_boolean()
    {
        let bindings = HashMap::new();
        assert_eq!(
            infer_expression_type(&expression(quote!(1 < 2)), &bindings),
            Some(ValueType::Bool)
        );
        assert_eq!(
            infer_expression_type(&expression(quote!(true && false)), &bindings),
            Some(ValueType::Bool)
        );
    }

    #[test]
    fn operator_tables_cover_the_supported_subset()
    {
        let Expr::Binary(add) = expression(quote!(1 + 2))
        else
        {
            unreachable!()
        };
        let Expr::Binary(equal) = expression(quote!(1 == 2))
        else
        {
            unreachable!()
        };
        assert_eq!(arithmetic_operation(&add.op).unwrap().to_string(), "Add");
        assert_eq!(comparison_operation(&equal.op).unwrap().to_string(), "Equal");
        assert!(float_arithmetic_operation(&add.op));
        assert!(!float_arithmetic_operation(&equal.op));
    }
}
