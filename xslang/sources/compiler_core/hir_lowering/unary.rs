/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 */

use super::*;

pub(super) fn lower_unary_expression(tree: &SyntaxTree,
                                     value: &SyntaxNode,
                                     context: &LoweringContext,
                                     locals: &HashMap<String, Type>,
                                     expected_type: Option<&Type>,
                                     source_span: Span)
                                     -> Option<Expression>
{
  if matches!(value.token_kind, TOKEN_PLUS_PLUS | TOKEN_MINUS_MINUS)
  {
    let target = tree.nodes.get(value.children[0])?;
    if target.kind != EXPR_IDENTIFIER
    {
      return None;
    }
    return Some(Expression::Update { target: path_text(tree, target),
                                     operator: if value.token_kind == TOKEN_PLUS_PLUS
                                     {
                                       UpdateOperator::Increment
                                     }
                                     else
                                     {
                                       UpdateOperator::Decrement
                                     },
                                     position: if value.flags & PREFIX_UPDATE != 0
                                     {
                                       UpdatePosition::Prefix
                                     }
                                     else
                                     {
                                       UpdatePosition::Postfix
                                     },
                                     span: source_span });
  }
  let operator = match value.token_kind
  {
    TOKEN_BANG => UnaryOperator::LogicalNot,
    TOKEN_PLUS => UnaryOperator::Positive,
    TOKEN_MINUS => UnaryOperator::Negative,
    _ => return None,
  };
  let bool_type = Type::Primitive(PrimitiveType::Bool);
  let operand_type = if operator == UnaryOperator::LogicalNot
  {
    Some(&bool_type)
  }
  else
  {
    expected_type
  };
  let operand = lower_expression(tree, tree.nodes.get(value.children[0])?, context, locals, operand_type)?;
  Some(Expression::Unary { operator,
                           operand: Box::new(operand),
                           span: source_span })
}
