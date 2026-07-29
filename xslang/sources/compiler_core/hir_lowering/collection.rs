/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 */

use super::*;
use crate::hir::type_check::MapEntry;

const EXPR_INDEX: u32 = 68;
const EXPR_ARRAY_LITERAL: u32 = 76;
const EXPR_MAP_LITERAL: u32 = 107;
const EXPR_SET_LITERAL: u32 = 109;

pub(super) const fn is_expression(kind: u32) -> bool
{
  matches!(kind,
           EXPR_INDEX | EXPR_ARRAY_LITERAL | EXPR_MAP_LITERAL | EXPR_SET_LITERAL)
}

pub(super) const fn is_set_literal(kind: u32) -> bool
{
  kind == EXPR_SET_LITERAL
}

pub(super) fn default_array_initializer(value_type: &Type, span: Span) -> Option<Expression>
{
  let Type::Array { element,
                    length: Some(length), } = value_type
  else
  {
    return None;
  };
  let count = usize::try_from(*length).ok()?;
  let elements = (0..count).map(|_| default_element(element, span))
                           .collect::<Option<Vec<_>>>()?;
  Some(Expression::Array { elements,
                           span })
}

pub(super) fn refine_local_type(value_type: &Type, initializer: Option<&SyntaxNode>) -> Type
{
  match (value_type, initializer)
  {
    (Type::Array { element,
                   length: None, },
     Some(value))
      if is_set_literal(value.kind) =>
    {
      Type::Set { element: element.clone() }
    }
    _ => value_type.clone(),
  }
}

pub(super) fn is_index_assignment(tree: &SyntaxTree, node: &SyntaxNode) -> bool
{
  node.kind == EXPR_ASSIGNMENT &&
  node.children
      .first()
      .and_then(|index| tree.nodes.get(*index))
      .is_some_and(|target| target.kind == EXPR_INDEX)
}

pub(super) fn lower_index_assignment(tree: &SyntaxTree,
                                     assignment: &SyntaxNode,
                                     context: &LoweringContext,
                                     locals: &HashMap<String, Type>)
                                     -> Option<Statement>
{
  if assignment.token_kind != TOKEN_ASSIGN || assignment.children.len() != 3
  {
    return None;
  }
  let target = tree.nodes.get(assignment.children[0])?;
  let collection = tree.nodes.get(*target.children.first()?)?;
  if collection.kind != EXPR_IDENTIFIER || target.children.len() != 2
  {
    return None;
  }
  let name = path_text(tree, collection);
  let Type::Array { element, .. } = locals.get(&name)?
  else
  {
    return None;
  };
  let index = super::lower_expression(tree,
                                      tree.nodes.get(target.children[1])?,
                                      context,
                                      locals,
                                      Some(&Type::Primitive(PrimitiveType::Int)))?;
  let value = super::lower_expression(tree,
                                      tree.nodes.get(assignment.children[2])?,
                                      context,
                                      locals,
                                      Some(element))?;
  Some(Statement::AssignIndex { target: name,
                                index,
                                value,
                                element_type: element.as_ref().clone(),
                                span: super::span(assignment)? })
}

pub(super) fn array_member_type(tree: &SyntaxTree,
                                member: &SyntaxNode,
                                context: &LoweringContext,
                                locals: &HashMap<String, Type>)
                                -> Option<Type>
{
  if member.kind != EXPR_MEMBER_ACCESS || member.children.len() != 2
  {
    return None;
  }
  let receiver = tree.nodes.get(member.children[0])?;
  let Type::Array { element,
                    length, } = super::expression_type(tree, receiver, context, locals)?
  else
  {
    return None;
  };
  match path_text(tree, tree.nodes.get(member.children[1])?).as_str()
  {
    "count" | "capacity" | "start_index" | "end_index" => Some(Type::Primitive(PrimitiveType::Int)),
    "is_empty" => Some(Type::Primitive(PrimitiveType::Bool)),
    "first" | "last" if length != Some(0) => Some(*element),
    _ => None,
  }
}

pub(super) fn lower_array_member(tree: &SyntaxTree,
                                 member: &SyntaxNode,
                                 context: &LoweringContext,
                                 locals: &HashMap<String, Type>,
                                 span: Span)
                                 -> Option<Expression>
{
  let receiver_node = tree.nodes.get(*member.children.first()?)?;
  let array_type = super::expression_type(tree, receiver_node, context, locals)?;
  let Type::Array { element,
                    length, } = &array_type
  else
  {
    return None;
  };
  let name = path_text(tree, tree.nodes.get(*member.children.get(1)?)?);
  match name.as_str()
  {
    "count" | "capacity" | "end_index" => match length
    {
      Some(length) => integer_literal(*length, span),
      None => Some(Expression::ArrayLength { collection: Box::new(super::lower_expression(tree,
                                                                                           receiver_node,
                                                                                           context,
                                                                                           locals,
                                                                                           Some(&array_type))?),
                                              span }),
    },
    "start_index" => integer_literal(0, span),
    "is_empty" => match length
    {
      Some(length) => Some(Expression::Literal { literal: Literal::Bool(*length == 0),
                                                 span }),
      None => Some(Expression::Binary { operator: BinaryOperator::Equal,
                                        left: Box::new(Expression::ArrayLength {
                                          collection: Box::new(super::lower_expression(tree,
                                                                                       receiver_node,
                                                                                       context,
                                                                                       locals,
                                                                                       Some(&array_type))?),
                                          span,
                                        }),
                                        right: Box::new(integer_literal(0, span)?),
                                        span }),
    },
    "first" | "last" =>
    {
      let receiver = super::lower_expression(tree, receiver_node, context, locals, Some(&array_type))?;
      let index = match (name.as_str(), length)
      {
        ("first", _) => integer_literal(0, span)?,
        ("last", Some(length)) => integer_literal(length.checked_sub(1)?, span)?,
        ("last", None) => Expression::Binary {
          operator: BinaryOperator::Sub,
          left: Box::new(Expression::ArrayLength { collection: Box::new(receiver.clone()),
                                                   span }),
          right: Box::new(integer_literal(1, span)?),
          span,
        },
        _ => return None,
      };
      Some(Expression::Index { collection: Box::new(receiver),
                               index: Box::new(index),
                               element_type: element.clone(),
                               span })
    }
    _ => None,
  }
}

fn integer_literal(value: u64, span: Span) -> Option<Expression>
{
  i64::try_from(value).ok()
                      .map(|value| Expression::Literal { literal: Literal::Integer(value.to_string()),
                                                         span })
}

pub(super) fn expression_type(tree: &SyntaxTree,
                              value: &SyntaxNode,
                              context: &LoweringContext,
                              locals: &HashMap<String, Type>)
                              -> Option<Type>
{
  match value.kind
  {
    EXPR_ARRAY_LITERAL =>
    {
      let first = tree.nodes.get(*value.children.first()?)?;
      let element = super::expression_type(tree, first, context, locals)?;
      value.children
           .iter()
           .skip(1)
           .all(|index| {
             tree.nodes
                 .get(*index)
                 .and_then(|node| super::expression_type(tree, node, context, locals))
                 .as_ref() ==
             Some(&element)
           })
           .then(|| Type::Array { element: Box::new(element),
                                  length: None })
    }
    EXPR_SET_LITERAL =>
    {
      homogeneous_type(tree, value, context, locals).map(|element| Type::Set { element: Box::new(element) })
    }
    EXPR_MAP_LITERAL => map_type(tree, value, context, locals),
    EXPR_INDEX =>
    {
      let collection = tree.nodes.get(*value.children.first()?)?;
      match super::expression_type(tree, collection, context, locals)?
      {
        Type::Array { element, .. } => Some(*element),
        _ => None,
      }
    }
    _ => None,
  }
}

pub(super) fn lower_expression(tree: &SyntaxTree,
                               value: &SyntaxNode,
                               context: &LoweringContext,
                               locals: &HashMap<String, Type>,
                               expected_type: Option<&Type>,
                               span: Span)
                               -> Option<Expression>
{
  match value.kind
  {
    EXPR_ARRAY_LITERAL => lower_array(tree, value, context, locals, expected_type, span),
    EXPR_SET_LITERAL => lower_set(tree, value, context, locals, expected_type, span),
    EXPR_MAP_LITERAL => lower_map(tree, value, context, locals, expected_type, span),
    EXPR_INDEX if value.children.len() == 2 =>
    {
      let collection_node = tree.nodes.get(value.children[0])?;
      let collection_type = super::expression_type(tree, collection_node, context, locals)?;
      let Type::Array { element, .. } = &collection_type
      else
      {
        return None;
      };
      let collection = super::lower_expression(tree, collection_node, context, locals, Some(&collection_type))?;
      let index = super::lower_expression(tree,
                                          tree.nodes.get(value.children[1])?,
                                          context,
                                          locals,
                                          Some(&Type::Primitive(PrimitiveType::Int)))?;
      Some(Expression::Index { collection: Box::new(collection),
                               index: Box::new(index),
                               element_type: element.clone(),
                               span })
    }
    _ => None,
  }
}

fn homogeneous_type(tree: &SyntaxTree,
                    value: &SyntaxNode,
                    context: &LoweringContext,
                    locals: &HashMap<String, Type>)
                    -> Option<Type>
{
  let first = tree.nodes.get(*value.children.first()?)?;
  let element = super::expression_type(tree, first, context, locals)?;
  value.children
       .iter()
       .skip(1)
       .all(|index| {
         tree.nodes
             .get(*index)
             .and_then(|node| super::expression_type(tree, node, context, locals))
             .as_ref() ==
         Some(&element)
       })
       .then_some(element)
}

fn lower_array(tree: &SyntaxTree,
               value: &SyntaxNode,
               context: &LoweringContext,
               locals: &HashMap<String, Type>,
               expected_type: Option<&Type>,
               span: Span)
               -> Option<Expression>
{
  let inferred = expression_type(tree, value, context, locals);
  let array_type = expected_type.or(inferred.as_ref())?;
  let Type::Array { element,
                    length, } = array_type
  else
  {
    return None;
  };
  let retained = length.map_or(value.children.len(), |length| {
                         usize::try_from(length).unwrap_or(usize::MAX).min(value.children.len())
                       });
  let mut elements =
    value.children[..retained].iter()
                              .map(|index| {
                                super::lower_expression(tree, tree.nodes.get(*index)?, context, locals, Some(element))
                              })
                              .collect::<Option<Vec<_>>>()?;
  if let Some(length) = length
  {
    let target = usize::try_from(*length).ok()?;
    while elements.len() < target
    {
      elements.push(default_element(element, span)?);
    }
  }
  Some(Expression::Array { elements,
                           span })
}

fn lower_set(tree: &SyntaxTree,
             value: &SyntaxNode,
             context: &LoweringContext,
             locals: &HashMap<String, Type>,
             expected_type: Option<&Type>,
             span: Span)
             -> Option<Expression>
{
  let inferred = expression_type(tree, value, context, locals);
  let set_type = expected_type.or(inferred.as_ref())?;
  let Type::Set { element } = set_type
  else
  {
    return None;
  };
  let elements =
    value.children
         .iter()
         .map(|index| super::lower_expression(tree, tree.nodes.get(*index)?, context, locals, Some(element)))
         .collect::<Option<Vec<_>>>()?;
  Some(Expression::Set { elements,
                         span })
}

fn default_element(element: &Type, span: Span) -> Option<Expression>
{
  let literal = match element
  {
    Type::Primitive(PrimitiveType::Byte |
                    PrimitiveType::SByte |
                    PrimitiveType::Short |
                    PrimitiveType::Long |
                    PrimitiveType::Int |
                    PrimitiveType::Integer |
                    PrimitiveType::UShort |
                    PrimitiveType::ULong |
                    PrimitiveType::UInt |
                    PrimitiveType::UInteger) => Literal::Integer("0".to_string()),
    Type::Primitive(PrimitiveType::SFloat | PrimitiveType::LFloat | PrimitiveType::Float | PrimitiveType::Double) =>
    {
      Literal::Float("0.0".to_string())
    }
    Type::Primitive(PrimitiveType::Char) => Literal::Char(0),
    _ => return None,
  };
  Some(Expression::Literal { literal,
                             span })
}

fn lower_map(tree: &SyntaxTree,
             node: &SyntaxNode,
             context: &LoweringContext,
             locals: &HashMap<String, Type>,
             expected_type: Option<&Type>,
             span: Span)
             -> Option<Expression>
{
  let inferred = map_type(tree, node, context, locals);
  let Type::Map { key,
                  value, } = expected_type.or(inferred.as_ref())?
  else
  {
    return None;
  };
  let entries = node.children
                    .iter()
                    .map(|index| {
                      let entry = tree.nodes.get(*index)?;
                      Some(MapEntry { key: super::lower_expression(tree,
                                                                   tree.nodes.get(*entry.children.first()?)?,
                                                                   context,
                                                                   locals,
                                                                   Some(key))?,
                                      value: super::lower_expression(tree,
                                                                     tree.nodes.get(*entry.children.get(1)?)?,
                                                                     context,
                                                                     locals,
                                                                     Some(value))?,
                                      span: super::span(entry)? })
                    })
                    .collect::<Option<Vec<_>>>()?;
  Some(Expression::Map { entries,
                         span })
}

fn map_type(tree: &SyntaxTree,
            node: &SyntaxNode,
            context: &LoweringContext,
            locals: &HashMap<String, Type>)
            -> Option<Type>
{
  let first = tree.nodes.get(*node.children.first()?)?;
  let key = super::expression_type(tree, tree.nodes.get(*first.children.first()?)?, context, locals)?;
  let value = super::expression_type(tree, tree.nodes.get(*first.children.get(1)?)?, context, locals)?;
  node.children
      .iter()
      .skip(1)
      .all(|index| {
        let Some(entry) = tree.nodes.get(*index)
        else
        {
          return false;
        };
        super::expression_type(tree, &tree.nodes[entry.children[0]], context, locals).as_ref() == Some(&key) &&
        super::expression_type(tree, &tree.nodes[entry.children[1]], context, locals).as_ref() == Some(&value)
      })
      .then(|| Type::Map { key: Box::new(key),
                           value: Box::new(value) })
}
