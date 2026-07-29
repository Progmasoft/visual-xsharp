/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 */

use super::*;

pub(super) fn lower_parameter(tree: &SyntaxTree, value: &SyntaxNode) -> Result<declarations::Parameter, LoweringError>
{
  let name = first_child_kind(tree, value, IDENTIFIER).ok_or(LoweringError::MissingIdentifier)?;
  let ty = value.children
                .iter()
                .filter_map(|index| tree.nodes.get(*index))
                .find(|child| child.kind != IDENTIFIER)
                .ok_or(LoweringError::MissingParameterType)?;
  Ok(declarations::Parameter { name: name.text.clone(),
                               ty: lower_type(tree, ty),
                               span: value.span.clone() })
}

pub(super) fn lower_function(tree: &SyntaxTree, value: &SyntaxNode) -> Result<declarations::Function, LoweringError>
{
  let name = first_child_kind(tree, value, IDENTIFIER).ok_or(LoweringError::MissingIdentifier)?;
  let parameters = value.children
                        .iter()
                        .filter_map(|index| tree.nodes.get(*index))
                        .filter(|child| child.kind == PARAMETER)
                        .map(|parameter| lower_parameter(tree, parameter))
                        .collect::<Result<_, _>>()?;
  let return_type = value.children
                         .iter()
                         .filter_map(|index| tree.nodes.get(*index))
                         .find(|child| child.flags & RETURN_TYPE != 0)
                         .map_or(declarations::TypeRef::Unit, |ty| lower_type(tree, ty));
  Ok(declarations::Function { name: name.text.clone(),
                              parameters,
                              return_type,
                              flags: value.flags,
                              span: value.span.clone(),
                              body_present: first_child_kind(tree, value, STMT_BLOCK).is_some(),
                              body: None })
}
