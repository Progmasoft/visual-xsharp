/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 */

use super::*;

pub(super) fn lower_signature(tree: &SyntaxTree,
                              value: &SyntaxNode,
                              owner: &str,
                              ordinal: usize)
                              -> Result<(declarations::Function, String, bool), LoweringError>
{
  let mut function = signature::lower_function(tree, value)?;
  let source_name = function.name.clone();
  let is_static = value.flags & STATIC != 0;
  function.name = format!("xs$method${owner}${source_name}${ordinal}");
  if !is_static
  {
    function.parameters
            .insert(0, declarations::Parameter { name: "self".to_string(),
                                                 ty:
                                                   declarations::TypeRef::Named(owner.to_string()),
                                                 span: value.span.clone() });
  }
  Ok((function, source_name, is_static))
}
