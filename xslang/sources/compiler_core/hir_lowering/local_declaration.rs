/*
 * SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
 * SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.0
 */

use super::*;

pub(super) fn lower_one(
    tree: &SyntaxTree,
    declaration: &SyntaxNode,
    context: &LoweringContext,
    locals: &mut HashMap<String, Type>,
) -> Option<Statement>
{
    let name = first_child_kind(tree, declaration, IDENTIFIER)?;
    let initializer_node = declaration
        .children
        .iter()
        .filter_map(|index| tree.nodes.get(*index))
        .find(|child| child.kind >= EXPR_IDENTIFIER);
    let checked_type = if declaration.flags & INFERRED_TYPE != 0
    {
        expression_type(tree, initializer_node?, context, locals)?
    }
    else
    {
        let ty = declaration
            .children
            .iter()
            .filter_map(|index| tree.nodes.get(*index))
            .find(|child| (TYPE_NAMED..=TYPE_UNIT).contains(&child.kind))?;
        generic::checked_type_in_context(&lower_type(tree, ty), context)?
    };
    let checked_type = collection::refine_local_type(&checked_type, initializer_node);
    let initializer = match initializer_node
    {
        Some(value) => Some(lower_expression(tree, value, context, locals, Some(&checked_type))?),
        None => collection::default_array_initializer(&checked_type, span(declaration)?),
    };
    locals.insert(name.text.clone(), checked_type.clone());
    Some(Statement::Let {
        local: crate::hir::type_check::Local {
            name: name.text.clone(),
            ty: checked_type,
            mutable: declaration.flags & (IMMUTABLE | CONSTANT | STATIC_CONSTANT) == 0,
            span: span(declaration)?,
        },
        initializer,
    })
}

pub(super) fn lower_statements(
    tree: &SyntaxTree,
    statement: &SyntaxNode,
    context: &LoweringContext,
    locals: &mut HashMap<String, Type>,
) -> Option<Vec<Statement>>
{
    let declaration = if statement.kind == STMT_VARIABLE
    {
        statement.children.first().and_then(|index| tree.nodes.get(*index))?
    }
    else
    {
        statement
    };
    if declaration.kind == DECL_PATTERN_VARIABLE
    {
        return tuple::lower_pattern_declaration(tree, declaration, context, locals);
    }
    Some(vec![lower_one(tree, declaration, context, locals)?])
}
