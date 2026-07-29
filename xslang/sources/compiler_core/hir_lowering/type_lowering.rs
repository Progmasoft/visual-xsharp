/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 */

use super::*;

pub(super) fn lower_type(tree: &SyntaxTree, value: &SyntaxNode) -> declarations::TypeRef
{
  let lowered = lower_type_core(tree, value);
  if value.flags & OPTIONAL_TYPE != 0
  {
    declarations::TypeRef::Optional { element: Box::new(lowered) }
  }
  else
  {
    lowered
  }
}

fn lower_type_core(tree: &SyntaxTree, value: &SyntaxNode) -> declarations::TypeRef
{
  match value.kind
  {
    TYPE_UNIT => declarations::TypeRef::Unit,
    TYPE_NAMED => lower_named(tree, value),
    TYPE_GENERIC => lower_generic(tree, value),
    TYPE_REFERENCE | TYPE_MUTABLE_REFERENCE => lower_reference(tree, value),
    TYPE_ARRAY => lower_array(tree, value, None),
    TYPE_FIXED_ARRAY =>
    {
      let length = value.children
                        .get(1)
                        .and_then(|index| tree.nodes.get(*index))
                        .and_then(|length| length.text.replace('\'', "").parse::<u64>().ok());
      lower_array(tree, value, length)
    }
    TYPE_MAP => lower_map(tree, value),
    TYPE_TUPLE => tuple::lower_tuple_type(tree, value),
    _ => declarations::TypeRef::Named(value.text.clone()),
  }
}

fn lower_named(tree: &SyntaxTree, value: &SyntaxNode) -> declarations::TypeRef
{
  let name = path_text(tree, value);
  primitive(&name).map_or_else(|| declarations::TypeRef::Named(name), declarations::TypeRef::Primitive)
}

fn lower_generic(tree: &SyntaxTree, value: &SyntaxNode) -> declarations::TypeRef
{
  let mut children = value.children.iter().filter_map(|index| tree.nodes.get(*index));
  let base = children.next();
  if base.is_some_and(|base| path_text(tree, base) == "Optional") &&
     let Some(element) = children.next()
  {
    return declarations::TypeRef::Optional { element: Box::new(lower_type(tree, element)) };
  }
  declarations::TypeRef::Named(value.text.trim_end_matches('?').to_string())
}

fn lower_reference(tree: &SyntaxTree, value: &SyntaxNode) -> declarations::TypeRef
{
  let referent = value.children
                      .iter()
                      .filter_map(|index| tree.nodes.get(*index))
                      .find(|child| child.kind != 37)
                      .map(|referent| lower_type(tree, referent))
                      .unwrap_or_else(|| declarations::TypeRef::Named(String::new()));
  declarations::TypeRef::Reference { referent: Box::new(referent),
                                     mutable: value.kind == TYPE_MUTABLE_REFERENCE }
}

fn lower_array(tree: &SyntaxTree, value: &SyntaxNode, length: Option<u64>) -> declarations::TypeRef
{
  let element = value.children
                     .first()
                     .and_then(|index| tree.nodes.get(*index))
                     .map(|element| lower_type(tree, element))
                     .unwrap_or_else(|| declarations::TypeRef::Named(String::new()));
  declarations::TypeRef::Array { element: Box::new(element),
                                 length }
}

fn lower_map(tree: &SyntaxTree, value: &SyntaxNode) -> declarations::TypeRef
{
  let mut children = value.children.iter().filter_map(|index| tree.nodes.get(*index));
  let key = children.next()
                    .map(|key| lower_type(tree, key))
                    .unwrap_or_else(|| declarations::TypeRef::Named(String::new()));
  let value = children.next()
                      .map(|value| lower_type(tree, value))
                      .unwrap_or_else(|| declarations::TypeRef::Named(String::new()));
  declarations::TypeRef::Map { key: Box::new(key),
                               value: Box::new(value) }
}
