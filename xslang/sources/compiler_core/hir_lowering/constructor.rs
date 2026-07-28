/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <xs-lang.chess031@slmails.com>
 * SPDX-License-Identifier: MPL-2.0
 */

use super::*;
use std::collections::HashSet;

pub(super) const CLASS_CONSTRUCTOR: u32 = 17;

pub(super) fn lower_signature(tree: &SyntaxTree,
                              value: &SyntaxNode,
                              owner: &str,
                              ordinal: usize)
                              -> Result<declarations::Function, LoweringError>
{
  let parameters = value.children
                        .iter()
                        .filter_map(|index| tree.nodes.get(*index))
                        .filter(|child| child.kind == PARAMETER)
                        .map(|parameter| signature::lower_parameter(tree, parameter))
                        .collect::<Result<_, _>>()?;
  Ok(declarations::Function { name: format!("xs$ctor${owner}${ordinal}"),
                              parameters,
                              return_type: declarations::TypeRef::Named(owner.to_string()),
                              flags: value.flags,
                              span: value.span.clone(),
                              body_present: true,
                              body: None })
}

pub(super) fn lower_body(tree: &SyntaxTree,
                         constructor: &SyntaxNode,
                         context: &LoweringContext,
                         parameters: &[declarations::Parameter],
                         owner: &str)
                         -> Option<Vec<Statement>>
{
  let owner_type = Type::Named(owner.to_string());
  let mut locals = parameters.iter()
                             .filter_map(|parameter| checked_type(&parameter.ty).map(|ty| (parameter.name.clone(), ty)))
                             .collect::<HashMap<_, _>>();
  locals.insert("self".to_string(), owner_type.clone());
  let block = first_child_kind(tree, constructor, STMT_BLOCK)?;
  let block = lower_hir_block(tree, block, context, &mut locals, None, None)?;
  if block.tail.is_some()
  {
    return None;
  }
  let required = required_fields(owner, context)?;
  let assigned = assigned_after(&block.statements, &HashSet::new(), &required);
  if !required.is_subset(&assigned)
  {
    return None;
  }
  let constructor_span = span(constructor)?;
  let mut body = Vec::with_capacity(block.statements.len() + 2);
  body.push(Statement::Let { local: crate::hir::type_check::Local { name: "self".to_string(),
                                                                    ty: owner_type,
                                                                    mutable: true,
                                                                    span: constructor_span },
                             initializer: None });
  body.extend(block.statements);
  body.push(Statement::Return { value: Some(Expression::Local { name: "self".to_string(),
                                                                span: constructor_span }),
                                span: constructor_span });
  Some(body)
}

fn required_fields(owner: &str, context: &LoweringContext) -> Option<HashSet<String>>
{
  let definition = context.nominal_types.get(owner)?;
  let mut required = HashSet::new();
  collect_required_fields(definition, context, &[], &mut Vec::new(), &mut required)?;
  Some(required)
}

fn collect_required_fields(definition: &declarations::NominalType,
                           context: &LoweringContext,
                           prefix: &[String],
                           visiting: &mut Vec<String>,
                           required: &mut HashSet<String>)
                           -> Option<()>
{
  if visiting.contains(&definition.name)
  {
    return None;
  }
  visiting.push(definition.name.clone());
  for field in declarations::resolved_fields(definition, &context.nominal_types).ok()?
  {
    let mut path = prefix.to_vec();
    path.push(field.name);
    match declarations::type_ref_to_checked(&field.ty)?
    {
      Type::Primitive(_) =>
      {
        required.insert(path.join("."));
      }
      Type::Named(name) =>
      {
        let nested = context.nominal_types.get(&name)?;
        collect_required_fields(nested, context, &path, visiting, required)?;
      }
      _ => return None,
    }
  }
  visiting.pop();
  Some(())
}

fn assigned_after(statements: &[Statement], incoming: &HashSet<String>, required: &HashSet<String>) -> HashSet<String>
{
  let mut assigned = incoming.clone();
  for statement in statements
  {
    match statement
    {
      Statement::Expr(Expression::AssignField { target, .. }) if target.root == "self" =>
      {
        let path = target.fields.join(".");
        if matches!(target.ty, Type::Named(_))
        {
          assigned.extend(required.iter()
                                  .filter(|field| field.starts_with(&format!("{path}.")))
                                  .cloned());
        }
        else
        {
          assigned.insert(path);
        }
      }
      Statement::If { then_block,
                      else_block,
                      .. } =>
      {
        let then_assigned = assigned_after(&then_block.statements, &assigned, required);
        let else_assigned = else_block.as_ref()
                                      .map_or_else(|| assigned.clone(),
                                                   |block| assigned_after(&block.statements, &assigned, required));
        assigned = then_assigned.intersection(&else_assigned).cloned().collect();
      }
      Statement::Match { arms, .. } if !arms.is_empty() =>
      {
        let mut arm_states = arms.iter()
                                 .map(|arm| assigned_after(&arm.body.statements, &assigned, required));
        if let Some(first) = arm_states.next()
        {
          assigned = arm_states.fold(first, |common, arm| common.intersection(&arm).cloned().collect());
        }
      }
      Statement::Return { .. } => break,
      _ =>
      {}
    }
  }
  assigned
}

pub(super) fn resolve<'a>(tree: &SyntaxTree,
                          call: &SyntaxNode,
                          name: &str,
                          context: &'a LoweringContext,
                          locals: &HashMap<String, Type>,
                          expected_type: Option<&Type>)
                          -> Option<&'a CallSignature>
{
  let candidates = context.constructors.get(name)?;
  let arity = call.children.len().checked_sub(1)?;
  let mut matching =
    candidates.iter()
              .filter(|candidate| candidate.parameters.len() == arity)
              .filter(|candidate| expected_type.is_none_or(|expected| expected == &candidate.return_type))
              .collect::<Vec<_>>();
  if matching.len() == 1
  {
    return matching.pop();
  }
  let argument_types = call.children[1..].iter()
                                         .map(|index| expression_type(tree, tree.nodes.get(*index)?, context, locals))
                                         .collect::<Option<Vec<_>>>()?;
  matching.retain(|candidate| candidate.parameters == argument_types);
  (matching.len() == 1).then(|| matching[0])
}

fn constructed_type(tree: &SyntaxTree, call: &SyntaxNode) -> Option<Type>
{
  let ty = tree.nodes.get(*call.children.first()?)?;
  checked_type(&lower_type(tree, ty))
}

pub(super) fn new_expression_type(tree: &SyntaxTree,
                                  call: &SyntaxNode,
                                  context: &LoweringContext,
                                  locals: &HashMap<String, Type>)
                                  -> Option<Type>
{
  let Type::Named(name) = constructed_type(tree, call)?
  else
  {
    return None;
  };
  let signature = resolve(tree, call, &name, context, locals, Some(&Type::Named(name.clone())))?;
  Some(signature.return_type.clone())
}

pub(super) fn lower_new_expression(tree: &SyntaxTree,
                                   call: &SyntaxNode,
                                   context: &LoweringContext,
                                   locals: &HashMap<String, Type>,
                                   expected_type: Option<&Type>,
                                   source_span: Span)
                                   -> Option<Expression>
{
  let Type::Named(name) = constructed_type(tree, call)?
  else
  {
    return None;
  };
  let constructed = Type::Named(name.clone());
  let expected_type = expected_type.or(Some(&constructed));
  let signature = resolve(tree, call, &name, context, locals, expected_type)?;
  let arguments = call.children[1..].iter()
                                    .zip(&signature.parameters)
                                    .map(|(index, parameter)| {
                                      lower_expression(tree, tree.nodes.get(*index)?, context, locals, Some(parameter))
                                    })
                                    .collect::<Option<Vec<_>>>()?;
  Some(Expression::Call { function: signature.symbol.clone(),
                          arguments,
                          parameter_types: signature.parameters.clone(),
                          return_type: Box::new(signature.return_type.clone()),
                          span: source_span })
}
