/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 */

use super::*;

pub(super) fn lower_match_expression(tree: &SyntaxTree,
                                     expression: &SyntaxNode,
                                     context: &LoweringContext,
                                     locals: &HashMap<String, Type>,
                                     result_type: Type)
                                     -> Option<Expression>
{
  let selector_node = tree.nodes.get(*expression.children.first()?)?;
  let selector_type = expression_type(tree, selector_node, context, locals)?;
  let selector = lower_expression(tree, selector_node, context, locals, Some(&selector_type))?;
  let arms =
    expression.children[1..].iter()
                            .map(|index| lower_arm(tree, *index, context, locals, &selector_type, &result_type))
                            .collect::<Option<Vec<_>>>()?;
  Some(Expression::Match { selector: Box::new(selector),
                           selector_type: Box::new(selector_type),
                           arms,
                           result_type: Box::new(result_type),
                           span: span(expression)? })
}

fn lower_arm(tree: &SyntaxTree,
             index: usize,
             context: &LoweringContext,
             locals: &HashMap<String, Type>,
             selector_type: &Type,
             result_type: &Type)
             -> Option<MatchArm>
{
  let arm = tree.nodes.get(index)?;
  if arm.kind != MATCH_ARM || arm.children.len() != 2
  {
    return None;
  }
  let pattern_node = tree.nodes.get(arm.children[0])?;
  let pattern = lower_pattern(tree, pattern_node, context, locals, selector_type)?;
  let mut arm_locals = locals.clone();
  bind_pattern(&pattern, &mut arm_locals);
  let body = lower_hir_block(tree,
                             tree.nodes.get(arm.children[1])?,
                             context,
                             &mut arm_locals,
                             None,
                             Some(result_type))?;
  body.tail.as_ref()?;
  Some(MatchArm { pattern,
                  body,
                  span: span(arm)? })
}

pub(super) fn lower_pattern(tree: &SyntaxTree,
                            pattern_node: &SyntaxNode,
                            context: &LoweringContext,
                            locals: &HashMap<String, Type>,
                            selector_type: &Type)
                            -> Option<MatchPattern>
{
  if pattern_node.kind == PATTERN_ELSE
  {
    return Some(MatchPattern::Else);
  }
  if pattern_node.kind == PATTERN_LITERAL
  {
    let literal_node = tree.nodes.get(*pattern_node.children.first()?)?;
    let Expression::Literal { literal, .. } =
      lower_expression(tree, literal_node, context, locals, Some(selector_type))?
    else
    {
      return None;
    };
    return Some(MatchPattern::Literal(literal));
  }
  if pattern_node.kind != PATTERN_ENUM_VARIANT
  {
    return None;
  }
  if let Type::Result { success,
                        error, } = selector_type
  {
    let path = tree.nodes.get(*pattern_node.children.first()?)?;
    let name = path_text(tree, path);
    let success_variant = match name.as_str()
    {
      "Ok" | "std::result::Ok" => true,
      "Error" | "std::result::Error" => false,
      _ => return None,
    };
    let payload_type = if success_variant
    {
      success.as_ref()
    }
    else
    {
      error.as_ref()
    };
    let binding = match pattern_node.children.get(1).and_then(|index| tree.nodes.get(*index))
    {
      None => None,
      Some(pattern) if pattern.kind == PATTERN_ELSE => None,
      Some(pattern) if pattern.kind == PATTERN_IDENTIFIER => Some(path_text(tree, pattern)),
      _ => return None,
    };
    if pattern_node.children.len() > 2
    {
      return None;
    }
    return Some(MatchPattern::ResultVariant { success: success_variant,
                                              binding,
                                              payload_type: payload_type.clone() });
  }
  if let Type::Named(enum_type) = selector_type &&
     context.enum_data.variants(enum_type).is_ok()
  {
    return lower_enum_data_pattern(tree, pattern_node, context, enum_type);
  }
  let Expression::Literal { literal, .. } =
    nominal::enum_variant_literal(tree, pattern_node, context, span(pattern_node)?)?
  else
  {
    return None;
  };
  Some(MatchPattern::Literal(literal))
}

pub(super) fn bind_pattern(pattern: &MatchPattern, locals: &mut HashMap<String, Type>)
{
  match pattern
  {
    MatchPattern::ResultVariant { binding: Some(binding),
                                  payload_type,
                                  .. } =>
    {
      locals.insert(binding.clone(), payload_type.clone());
    }
    MatchPattern::EnumDataVariant { binding: Some(binding),
                                    payload_type: Some(payload_type),
                                    .. } =>
    {
      locals.insert(binding.clone(), payload_type.clone());
    }
    _ =>
    {}
  }
}

fn lower_enum_data_pattern(tree: &SyntaxTree,
                           pattern_node: &SyntaxNode,
                           context: &LoweringContext,
                           enum_type: &str)
                           -> Option<MatchPattern>
{
  let path = tree.nodes.get(*pattern_node.children.first()?)?;
  let path = path_text(tree, path);
  let (path_owner, variant_name) = path.rsplit_once("::")?;
  if path_owner != enum_type && !context.nominal_types.contains_key(path_owner)
  {
    return None;
  }
  let binding_node = pattern_node.children.get(1).and_then(|index| tree.nodes.get(*index));
  if pattern_node.children.len() > 2
  {
    return None;
  }
  let (binding, annotated_type) = lower_enum_data_binding(tree, binding_node)?;
  let candidates = context.enum_data.overloads(enum_type, variant_name).ok()?;
  let selected = if let Some(payload_type) = annotated_type.as_ref()
  {
    context.enum_data
           .select(enum_type, variant_name, Some(payload_type))
           .ok()?
  }
  else
  {
    let visible = candidates.into_iter()
                            .filter(|candidate| path_owner == enum_type || candidate.owner == path_owner)
                            .collect::<Vec<_>>();
    if visible.len() != 1
    {
      return None;
    }
    visible[0]
  };
  if path_owner != enum_type && selected.owner != path_owner
  {
    return None;
  }
  let payload_type = selected.payload.as_ref().and_then(declarations::type_ref_to_checked);
  if binding.is_some() && payload_type.is_none()
  {
    return None;
  }
  Some(MatchPattern::EnumDataVariant { enum_type: enum_type.to_string(),
                                       owner: selected.owner.clone(),
                                       variant: selected.name.clone(),
                                       tag: selected.tag,
                                       binding,
                                       payload_type })
}

fn lower_enum_data_binding(tree: &SyntaxTree, binding: Option<&SyntaxNode>) -> Option<(Option<String>, Option<Type>)>
{
  let Some(binding) = binding
  else
  {
    return Some((None, None));
  };
  match binding.kind
  {
    PATTERN_ELSE => Some((None, None)),
    PATTERN_IDENTIFIER => Some((Some(path_text(tree, binding)), None)),
    PATTERN_TYPED =>
    {
      let pattern = binding.children.first().and_then(|index| tree.nodes.get(*index))?;
      let ty = binding.children.get(1).and_then(|index| tree.nodes.get(*index))?;
      let name = match pattern.kind
      {
        PATTERN_ELSE => None,
        PATTERN_IDENTIFIER => Some(path_text(tree, pattern)),
        _ => return None,
      };
      Some((name, declarations::type_ref_to_checked(&lower_type(tree, ty))))
    }
    _ => None,
  }
}
