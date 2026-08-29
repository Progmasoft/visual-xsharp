/*
 * SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
 * SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.0
 */

use super::*;

pub(super) fn lower_tuple_type(tree: &SyntaxTree, value: &SyntaxNode) -> declarations::TypeRef
{
    declarations::TypeRef::Tuple {
        fields: value
            .children
            .iter()
            .filter_map(|index| tree.nodes.get(*index))
            .map(|field| {
                if field.kind != TUPLE_FIELD
                {
                    return declarations::TupleFieldRef {
                        name: None,
                        ty: lower_type(tree, field),
                    };
                }
                let name = field
                    .children
                    .first()
                    .and_then(|index| tree.nodes.get(*index))
                    .map(|name| name.text.clone());
                let ty = field
                    .children
                    .get(1)
                    .and_then(|index| tree.nodes.get(*index))
                    .map(|ty| lower_type(tree, ty))
                    .unwrap_or_else(|| declarations::TypeRef::Named(String::new()));
                declarations::TupleFieldRef {
                    name,
                    ty,
                }
            })
            .collect(),
    }
}

pub(super) fn lower_pattern_declaration(
    tree: &SyntaxTree,
    declaration: &SyntaxNode,
    context: &LoweringContext,
    locals: &mut HashMap<String, Type>,
) -> Option<Vec<Statement>>
{
    let pattern = declaration.children.first().and_then(|index| tree.nodes.get(*index))?;
    let initializer_node = declaration.children.last().and_then(|index| tree.nodes.get(*index))?;
    if declaration.children.len() < 2
    {
        return None;
    }
    let inferred_type = expression_type(tree, initializer_node, context, locals)?;
    let (pattern, tuple_type) = typed_pattern(tree, pattern, context, &inferred_type)?;
    let Type::Tuple {
        ..
    } = tuple_type
    else
    {
        return None;
    };
    let declaration_span = span(declaration)?;
    let hidden_name = format!("$tuple_pattern_{}", declaration.span.start_offset);
    let hidden = crate::hir::type_check::Local {
        name: hidden_name.clone(),
        ty: tuple_type.clone(),
        mutable: false,
        span: declaration_span,
    };
    let initializer = lower_expression(tree, initializer_node, context, locals, Some(&tuple_type))?;
    let mut statements = vec![Statement::Let {
        local: hidden,
        initializer: Some(initializer),
    }];
    let source = Expression::Local {
        name: hidden_name.clone(),
        span: declaration_span,
    };
    locals.insert(hidden_name, tuple_type.clone());
    let mut names = std::collections::HashSet::new();
    let mut state = PatternBindingState {
        context,
        locals,
        names: &mut names,
        statements: &mut statements,
    };
    lower_pattern_bindings(tree, pattern, &tuple_type, source, &mut state)?;
    Some(statements)
}

fn typed_pattern<'a>(
    tree: &'a SyntaxTree,
    pattern: &'a SyntaxNode,
    context: &LoweringContext,
    inferred_type: &Type,
) -> Option<(&'a SyntaxNode, Type)>
{
    if pattern.kind != PATTERN_TYPED
    {
        return Some((pattern, inferred_type.clone()));
    }
    let inner = pattern.children.first().and_then(|index| tree.nodes.get(*index))?;
    let ty = pattern.children.get(1).and_then(|index| tree.nodes.get(*index))?;
    let declared = generic::checked_type_in_context(&lower_type(tree, ty), context)?;
    (declared == *inferred_type).then_some((inner, declared))
}

struct PatternBindingState<'a>
{
    context: &'a LoweringContext,
    locals: &'a mut HashMap<String, Type>,
    names: &'a mut std::collections::HashSet<String>,
    statements: &'a mut Vec<Statement>,
}

fn lower_pattern_bindings(
    tree: &SyntaxTree,
    pattern: &SyntaxNode,
    pattern_type: &Type,
    source: Expression,
    state: &mut PatternBindingState<'_>,
) -> Option<()>
{
    let Type::Tuple {
        fields,
    } = pattern_type
    else
    {
        return None;
    };
    if pattern.kind != PATTERN_TUPLE || pattern.children.len() != fields.len()
    {
        return None;
    }
    for (index, (child, field)) in pattern.children.iter().zip(fields).enumerate()
    {
        let child = tree.nodes.get(*child)?;
        let (child, child_type) = typed_pattern(tree, child, state.context, &field.ty)?;
        let child_span = span(child)?;
        let projection = Expression::TupleElement {
            tuple: Box::new(source.clone()),
            index: u32::try_from(index).ok()?,
            element_type: Box::new(child_type.clone()),
            span: child_span,
        };
        match child.kind
        {
            PATTERN_ELSE =>
            {}
            PATTERN_IDENTIFIER =>
            {
                let name = first_child_kind(tree, child, IDENTIFIER)?.text.clone();
                if state.locals.contains_key(&name) || !state.names.insert(name.clone())
                {
                    return None;
                }
                state.locals.insert(name.clone(), child_type.clone());
                state.statements.push(Statement::Let {
                    local: crate::hir::type_check::Local {
                        name,
                        ty: child_type,
                        mutable: true,
                        span: child_span,
                    },
                    initializer: Some(projection),
                });
            }
            PATTERN_TUPLE =>
            {
                lower_pattern_bindings(tree, child, &child_type, projection, state)?;
            }
            _ => return None,
        }
    }
    Some(())
}

pub(super) fn is_tuple_assignment(tree: &SyntaxTree, value: &SyntaxNode, locals: &HashMap<String, Type>) -> bool
{
    assignment_parts(tree, value, locals).is_some()
}

pub(super) fn lower_tuple_assignment(
    tree: &SyntaxTree,
    value: &SyntaxNode,
    context: &LoweringContext,
    locals: &HashMap<String, Type>,
) -> Option<Statement>
{
    let (target, tuple_type, index, element_type) = assignment_parts(tree, value, locals)?;
    let assigned = lower_expression(
        tree,
        tree.nodes.get(value.children[2])?,
        context,
        locals,
        Some(&element_type),
    )?;
    Some(Statement::AssignTupleElement {
        target,
        index,
        value: assigned,
        tuple_type,
        element_type,
        span: span(value)?,
    })
}

fn assignment_parts(
    tree: &SyntaxTree,
    value: &SyntaxNode,
    locals: &HashMap<String, Type>,
) -> Option<(String, Type, u32, Type)>
{
    (value.kind == EXPR_ASSIGNMENT && value.token_kind == TOKEN_ASSIGN && value.children.len() == 3).then_some(())?;
    let member = tree.nodes.get(value.children[0])?;
    (member.kind == EXPR_MEMBER_ACCESS && member.children.len() == 2).then_some(())?;
    let receiver = tree.nodes.get(member.children[0])?;
    (receiver.kind == EXPR_IDENTIFIER).then_some(())?;
    let target = path_text(tree, receiver);
    let tuple_type = locals.get(&target)?.clone();
    let Type::Tuple {
        fields,
    } = &tuple_type
    else
    {
        return None;
    };
    let selector = path_text(tree, tree.nodes.get(member.children[1])?);
    let position = selector.parse::<usize>().ok().or_else(|| {
        fields
            .iter()
            .position(|field| field.name.as_deref() == Some(selector.as_str()))
    })?;
    let element_type = fields.get(position)?.ty.clone();
    Some((target, tuple_type, u32::try_from(position).ok()?, element_type))
}

pub(super) fn tuple_expression_type(
    tree: &SyntaxTree,
    value: &SyntaxNode,
    context: &LoweringContext,
    locals: &HashMap<String, Type>,
) -> Option<Type>
{
    (value.kind == EXPR_TUPLE && !value.children.is_empty()).then_some(())?;
    let fields = value
        .children
        .iter()
        .map(|index| {
            let field = tree.nodes.get(*index)?;
            let (name, expression) = tuple_field(tree, field)?;
            Some(crate::hir::type_check::TupleFieldType {
                name,
                ty: expression_type(tree, expression, context, locals)?,
            })
        })
        .collect::<Option<Vec<_>>>()?;
    Some(Type::Tuple {
        fields,
    })
}

pub(super) fn lower_tuple_expression(
    tree: &SyntaxTree,
    value: &SyntaxNode,
    context: &LoweringContext,
    locals: &HashMap<String, Type>,
    expected_type: Option<&Type>,
    source_span: Span,
) -> Option<Expression>
{
    let tuple_type = expected_type
        .cloned()
        .or_else(|| tuple_expression_type(tree, value, context, locals))?;
    let Type::Tuple {
        fields: expected_fields,
    } = &tuple_type
    else
    {
        return None;
    };
    if expected_fields.len() != value.children.len()
    {
        return None;
    }
    let fields = value
        .children
        .iter()
        .zip(expected_fields)
        .map(|(index, expected)| {
            let field = tree.nodes.get(*index)?;
            let (name, expression) = tuple_field(tree, field)?;
            if name != expected.name
            {
                return None;
            }
            Some(crate::hir::type_check::TupleFieldValue {
                name,
                value: lower_expression(tree, expression, context, locals, Some(&expected.ty))?,
                span: span(field)?,
            })
        })
        .collect::<Option<Vec<_>>>()?;
    Some(Expression::Tuple {
        fields,
        tuple_type: Box::new(tuple_type),
        span: source_span,
    })
}

pub(super) fn tuple_element_type(
    tree: &SyntaxTree,
    value: &SyntaxNode,
    context: &LoweringContext,
    locals: &HashMap<String, Type>,
) -> Option<Type>
{
    let (_, _, _, element_type) = element_parts(tree, value, context, locals)?;
    Some(element_type)
}

pub(super) fn lower_tuple_element(
    tree: &SyntaxTree,
    value: &SyntaxNode,
    context: &LoweringContext,
    locals: &HashMap<String, Type>,
    source_span: Span,
) -> Option<Expression>
{
    let (receiver, tuple_type, index, element_type) = element_parts(tree, value, context, locals)?;
    Some(Expression::TupleElement {
        tuple: Box::new(lower_expression(tree, receiver, context, locals, Some(&tuple_type))?),
        index,
        element_type: Box::new(element_type),
        span: source_span,
    })
}

fn element_parts<'a>(
    tree: &'a SyntaxTree,
    value: &'a SyntaxNode,
    context: &LoweringContext,
    locals: &HashMap<String, Type>,
) -> Option<(&'a SyntaxNode, Type, u32, Type)>
{
    (value.kind == EXPR_MEMBER_ACCESS && value.children.len() == 2).then_some(())?;
    let receiver = tree.nodes.get(value.children[0])?;
    let tuple_type = expression_type(tree, receiver, context, locals)?;
    let Type::Tuple {
        fields,
    } = &tuple_type
    else
    {
        return None;
    };
    let member = path_text(tree, tree.nodes.get(value.children[1])?);
    let index = member.parse::<usize>().ok().or_else(|| {
        fields
            .iter()
            .position(|field| field.name.as_deref() == Some(member.as_str()))
    })?;
    let element_type = fields.get(index)?.ty.clone();
    Some((receiver, tuple_type, u32::try_from(index).ok()?, element_type))
}

fn tuple_field<'a>(tree: &'a SyntaxTree, field: &'a SyntaxNode) -> Option<(Option<String>, &'a SyntaxNode)>
{
    if field.kind == TUPLE_FIELD
    {
        let name = field
            .children
            .first()
            .and_then(|index| tree.nodes.get(*index))?
            .text
            .clone();
        let expression = field.children.get(1).and_then(|index| tree.nodes.get(*index))?;
        Some((Some(name), expression))
    }
    else
    {
        Some((None, field))
    }
}
