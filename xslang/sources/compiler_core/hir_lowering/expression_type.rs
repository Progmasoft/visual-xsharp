/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <xs-lang.chess031@slmails.com>
 * SPDX-License-Identifier: MPL-2.0
 */

use super::*;

pub(super) fn expression_type(tree: &SyntaxTree,
                              value: &SyntaxNode,
                              context: &LoweringContext,
                              locals: &HashMap<String, Type>)
                              -> Option<Type>
{
  match value.kind
  {
    kind if collection::is_expression(kind) => collection::expression_type(tree, value, context, locals),
    EXPR_LITERAL if value.text == "true" || value.text == "false" => Some(Type::Primitive(PrimitiveType::Bool)),
    EXPR_LITERAL if value.token_kind == TOKEN_INTEGER => Some(Type::Primitive(PrimitiveType::Int)),
    EXPR_LITERAL if value.token_kind == TOKEN_FLOAT => Some(Type::Primitive(PrimitiveType::Float)),
    EXPR_LITERAL if value.token_kind == TOKEN_CHARACTER => Some(Type::Primitive(PrimitiveType::Char)),
    EXPR_LITERAL if value.token_kind == TOKEN_STRING => Some(Type::Primitive(PrimitiveType::Str)),
    EXPR_IDENTIFIER =>
    {
      nominal::enum_variant_type(tree, value, context).or_else(|| locals.get(&path_text(tree, value)).cloned())
    }
    EXPR_MEMBER_ACCESS =>
    {
      tuple::tuple_element_type(tree, value, context, locals).or_else(|| {
                                                               collection::array_member_type(tree, value, context,
                                                                                             locals)
                                                             })
                                                             .or_else(|| {
                                                               nominal::member_type(tree, value, context, locals)
                                                             })
    }
    EXPR_TUPLE => tuple::tuple_expression_type(tree, value, context, locals),
    EXPR_NEW => constructor::new_expression_type(tree, value, context, locals),
    EXPR_CALL =>
    {
      let callee = tree.nodes.get(*value.children.first()?)?;
      let signature = if callee.kind == EXPR_GENERIC_QUALIFIER
      {
        call::resolve_generic_function(tree, value, callee, context, locals, None)
      }
      else
      {
        let name = path_text(tree, callee);
        call::resolve_function(tree, value, &name, context, locals, None)
      }?;
      Some(signature.return_type.clone())
    }
    EXPR_METHOD_CALL =>
    {
      call::lower_method_call(tree, value, context, locals, None, span(value)?).and_then(|expression| match expression
                                                                               {
                                                                                 Expression::Call { return_type,
                                                                                                    .. } =>
                                                                                 {
                                                                                   Some(*return_type)
                                                                                 }
                                                                                 _ => None,
                                                                               })
    }
    EXPR_BINARY
      if matches!(value.token_kind,
                  TOKEN_EQUAL |
                  TOKEN_NOT_EQUAL |
                  TOKEN_GREATER |
                  TOKEN_GREATER_EQUAL |
                  TOKEN_LESS |
                  TOKEN_LESS_EQUAL |
                  TOKEN_LOGICAL_AND |
                  TOKEN_LOGICAL_OR) =>
    {
      Some(Type::Primitive(PrimitiveType::Bool))
    }
    EXPR_BINARY => value.children
                        .first()
                        .and_then(|index| tree.nodes.get(*index))
                        .and_then(|operand| expression_type(tree, operand, context, locals))
                        .or_else(|| {
                          value.children
                               .last()
                               .and_then(|index| tree.nodes.get(*index))
                               .and_then(|operand| expression_type(tree, operand, context, locals))
                        })
                        .or(Some(Type::Primitive(PrimitiveType::Int))),
    EXPR_UNARY if value.token_kind == TOKEN_BANG => Some(Type::Primitive(PrimitiveType::Bool)),
    EXPR_UNARY => expression_type(tree, tree.nodes.get(*value.children.first()?)?, context, locals),
    _ => None,
  }
}
