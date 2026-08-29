// SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
// SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.0

fn lower_discarded_expression(
    tree: &SyntaxTree,
    value: &SyntaxNode,
    context: &LoweringContext,
    locals: &HashMap<String, Type>,
    expected_type: Option<&Type>,
) -> Option<Expression>
{
    lower_expression(tree, value, context, locals, expected_type)
}

fn lower_builtin_macro_statement(tree: &SyntaxTree, statement: &SyntaxNode) -> Option<Statement>
{
    let call = statement
        .children
        .iter()
        .filter_map(|index| tree.nodes.get(*index))
        .find(|node| node.kind == EXPR_MACRO_CALL)?;
    let name = first_child_kind(tree, call, IDENTIFIER)?;
    let source_span = span(statement)?;
    (name.text == "panic").then_some(Statement::Panic {
        span: source_span,
    })
}

fn lower_statement_node(
    tree: &SyntaxTree,
    statement: &SyntaxNode,
    context: &LoweringContext,
    locals: &mut HashMap<String, Type>,
    return_type: Option<&Type>,
) -> Option<Vec<Statement>>
{
    let lowered = match statement.kind
    {
        STMT_RETURN =>
        {
            let value = match statement.children.first().and_then(|index| tree.nodes.get(*index))
            {
                Some(value) => Some(lower_expression(tree, value, context, locals, return_type)?),
                None => None,
            };
            Some(Statement::Return {
                value,
                span: span(statement)?,
            })
        }
        STMT_EXPRESSION if statement.children.len() == 1 =>
        {
            let expression = tree.nodes.get(statement.children[0])?;
            if tuple::is_tuple_assignment(tree, expression, locals)
            {
                return tuple::lower_tuple_assignment(tree, expression, context, locals).map(|value| vec![value]);
            }
            if collection::is_index_assignment(tree, expression)
            {
                return collection::lower_index_assignment(tree, expression, context, locals).map(|value| vec![value]);
            }
            let expected = if expression.kind == EXPR_ASSIGNMENT
            {
                expression
                    .children
                    .first()
                    .and_then(|index| tree.nodes.get(*index))
                    .map(|target| path_text(tree, target))
                    .and_then(|target| locals.get(&target))
            }
            else
            {
                None
            };
            lower_discarded_expression(tree, expression, context, locals, expected).map(Statement::Expr)
        }
        STMT_VARIABLE => return local_declaration::lower_statements(tree, statement, context, locals),
        STMT_IF => lower_if_statement(tree, statement, context, locals, return_type),
        STMT_FOR => lower_for_statement(tree, statement, context, locals, return_type),
        STMT_FOR_EACH => for_each::lower_for_each_statement(tree, statement, context, locals, return_type),
        STMT_WHILE => lower_while_statement(tree, statement, context, locals, return_type),
        STMT_LOOP => lower_loop_statement(tree, statement, context, locals, return_type),
        STMT_MATCH => lower_match_statement(tree, statement, context, locals, return_type),
        STMT_BREAK => Some(Statement::Break {
            span: span(statement)?,
        }),
        STMT_CONTINUE => Some(Statement::Continue {
            span: span(statement)?,
        }),
        STMT_MACRO_CALL => lower_builtin_macro_statement(tree, statement),
        _ => None,
    }?;
    Some(vec![lowered])
}

fn lower_hir_block(
    tree: &SyntaxTree,
    block: &SyntaxNode,
    context: &LoweringContext,
    locals: &mut HashMap<String, Type>,
    return_type: Option<&Type>,
    tail_type: Option<&Type>,
) -> Option<Block>
{
    if block.kind != STMT_BLOCK
    {
        return None;
    }
    let mut statements = Vec::with_capacity(block.children.len());
    let mut tail = None;
    for (position, child_index) in block.children.iter().enumerate()
    {
        let statement = tree.nodes.get(*child_index)?;
        if position + 1 == block.children.len() && statement.kind == STMT_EXPRESSION && statement.flags & DISCARDED == 0
        {
            let expression = tree.nodes.get(*statement.children.first()?)?;
            tail = Some(Box::new(lower_expression(
                tree, expression, context, locals, tail_type,
            )?));
        }
        else
        {
            statements.extend(lower_statement_node(tree, statement, context, locals, return_type)?);
        }
    }
    Some(Block {
        statements,
        tail,
        span: span(block)?,
    })
}

fn lower_if_branch(
    tree: &SyntaxTree,
    branch: &SyntaxNode,
    context: &LoweringContext,
    locals: &HashMap<String, Type>,
    return_type: Option<&Type>,
    else_block: Option<Block>,
) -> Option<Block>
{
    let condition = lower_expression(
        tree,
        tree.nodes.get(*branch.children.first()?)?,
        context,
        locals,
        Some(&Type::Primitive(PrimitiveType::Bool)),
    )?;
    let mut branch_locals = locals.clone();
    let then_block = lower_hir_block(
        tree,
        tree.nodes.get(*branch.children.get(1)?)?,
        context,
        &mut branch_locals,
        return_type,
        None,
    )?;
    let nested = Statement::If {
        condition,
        then_block,
        else_block,
        span: span(branch)?,
    };
    Some(Block {
        statements: vec![nested],
        tail: None,
        span: span(branch)?,
    })
}

fn lower_if_statement(
    tree: &SyntaxTree,
    statement: &SyntaxNode,
    context: &LoweringContext,
    locals: &HashMap<String, Type>,
    return_type: Option<&Type>,
) -> Option<Statement>
{
    let condition = lower_expression(
        tree,
        tree.nodes.get(*statement.children.first()?)?,
        context,
        locals,
        Some(&Type::Primitive(PrimitiveType::Bool)),
    )?;
    let mut then_locals = locals.clone();
    let then_block = lower_hir_block(
        tree,
        tree.nodes.get(*statement.children.get(1)?)?,
        context,
        &mut then_locals,
        return_type,
        None,
    )?;
    let mut else_block = match statement.children[2..]
        .last()
        .and_then(|index| tree.nodes.get(*index))
        .filter(|node| node.kind == STMT_BLOCK)
    {
        Some(block) =>
        {
            let mut branch_locals = locals.clone();
            Some(lower_hir_block(
                tree,
                block,
                context,
                &mut branch_locals,
                return_type,
                None,
            )?)
        }
        None => None,
    };
    for branch in statement.children[2..]
        .iter()
        .filter_map(|index| tree.nodes.get(*index))
        .filter(|node| node.kind == STMT_ELSE_IF)
        .rev()
    {
        else_block = Some(lower_if_branch(tree, branch, context, locals, return_type, else_block)?);
    }
    Some(Statement::If {
        condition,
        then_block,
        else_block,
        span: span(statement)?,
    })
}

fn lower_while_statement(
    tree: &SyntaxTree,
    statement: &SyntaxNode,
    context: &LoweringContext,
    locals: &HashMap<String, Type>,
    return_type: Option<&Type>,
) -> Option<Statement>
{
    let condition = lower_expression(
        tree,
        tree.nodes.get(*statement.children.first()?)?,
        context,
        locals,
        Some(&Type::Primitive(PrimitiveType::Bool)),
    )?;
    let mut body_locals = locals.clone();
    let mut body = lower_hir_block(
        tree,
        tree.nodes.get(*statement.children.get(1)?)?,
        context,
        &mut body_locals,
        return_type,
        None,
    )?;
    let condition = if statement.flags & POST_TEST_LOOP != 0
    {
        let loop_span = span(statement)?;
        body.statements.push(Statement::If {
            condition,
            then_block: Block {
                statements: Vec::new(),
                tail: None,
                span: loop_span,
            },
            else_block: Some(Block {
                statements: vec![Statement::Break {
                    span: loop_span,
                }],
                tail: None,
                span: loop_span,
            }),
            span: loop_span,
        });
        Expression::Literal {
            literal: Literal::Bool(true),
            span: span(statement)?,
        }
    }
    else
    {
        condition
    };
    Some(Statement::While {
        condition,
        body,
        span: span(statement)?,
    })
}

fn lower_loop_statement(
    tree: &SyntaxTree,
    statement: &SyntaxNode,
    context: &LoweringContext,
    locals: &HashMap<String, Type>,
    return_type: Option<&Type>,
) -> Option<Statement>
{
    let mut body_locals = locals.clone();
    let body = lower_hir_block(
        tree,
        tree.nodes.get(*statement.children.first()?)?,
        context,
        &mut body_locals,
        return_type,
        None,
    )?;
    Some(Statement::While {
        condition: Expression::Literal {
            literal: Literal::Bool(true),
            span: span(statement)?,
        },
        body,
        span: span(statement)?,
    })
}

fn lower_for_statement(
    tree: &SyntaxTree,
    statement: &SyntaxNode,
    context: &LoweringContext,
    locals: &HashMap<String, Type>,
    return_type: Option<&Type>,
) -> Option<Statement>
{
    let body_node = statement.children.last().and_then(|index| tree.nodes.get(*index))?;
    if body_node.kind != STMT_BLOCK
    {
        return None;
    }
    let mut for_locals = locals.clone();
    let mut cursor = 0usize;
    let initializer = if statement.flags & FOR_INITIALIZER != 0
    {
        let node = tree.nodes.get(*statement.children.get(cursor)?)?;
        cursor += 1;
        let lowered = if node.kind == DECL_VARIABLE
        {
            local_declaration::lower_one(tree, node, context, &mut for_locals)?
        }
        else
        {
            Statement::Expr(lower_discarded_expression(tree, node, context, &for_locals, None)?)
        };
        Some(Box::new(lowered))
    }
    else
    {
        None
    };
    let condition = if statement.flags & FOR_CONDITION != 0
    {
        let node = tree.nodes.get(*statement.children.get(cursor)?)?;
        cursor += 1;
        Some(lower_expression(
            tree,
            node,
            context,
            &for_locals,
            Some(&Type::Primitive(PrimitiveType::Bool)),
        )?)
    }
    else
    {
        None
    };
    let update = if statement.flags & FOR_UPDATE != 0
    {
        let node = tree.nodes.get(*statement.children.get(cursor)?)?;
        cursor += 1;
        Some(lower_discarded_expression(tree, node, context, &for_locals, None)?)
    }
    else
    {
        None
    };
    if statement.children.get(cursor).copied() != statement.children.last().copied()
    {
        return None;
    }
    let mut body_locals = for_locals;
    let body = lower_hir_block(tree, body_node, context, &mut body_locals, return_type, None)?;
    Some(Statement::For {
        initializer,
        condition,
        update,
        body,
        span: span(statement)?,
    })
}

fn lower_match_statement(
    tree: &SyntaxTree,
    statement: &SyntaxNode,
    context: &LoweringContext,
    locals: &HashMap<String, Type>,
    return_type: Option<&Type>,
) -> Option<Statement>
{
    let selector_node = tree.nodes.get(*statement.children.first()?)?;
    let selector_type = expression_type(tree, selector_node, context, locals)?;
    let selector = lower_expression(tree, selector_node, context, locals, Some(&selector_type))?;
    let arms = statement.children[1..]
        .iter()
        .map(|index| {
            let arm = tree.nodes.get(*index)?;
            if arm.kind != MATCH_ARM || arm.children.len() != 2
            {
                return None;
            }
            let pattern_node = tree.nodes.get(arm.children[0])?;
            let pattern = match_expression::lower_pattern(tree, pattern_node, context, locals, &selector_type)?;
            let mut arm_locals = locals.clone();
            match_expression::bind_pattern(&pattern, &mut arm_locals);
            let body = lower_hir_block(
                tree,
                tree.nodes.get(arm.children[1])?,
                context,
                &mut arm_locals,
                return_type,
                None,
            )?;
            Some(MatchArm {
                pattern,
                body,
                span: span(arm)?,
            })
        })
        .collect::<Option<Vec<_>>>()?;
    Some(Statement::Match {
        selector,
        selector_type,
        arms,
        span: span(statement)?,
    })
}

fn lower_body(
    tree: &SyntaxTree,
    function: &SyntaxNode,
    context: &LoweringContext,
    parameters: &[declarations::Parameter],
    return_type: Option<&Type>,
) -> Option<Vec<Statement>>
{
    let mut locals = parameters
        .iter()
        .filter_map(|parameter| checked_type(&parameter.ty).map(|ty| (parameter.name.clone(), ty)))
        .collect::<HashMap<_, _>>();
    let block = first_child_kind(tree, function, STMT_BLOCK)?;
    let block = lower_hir_block(tree, block, context, &mut locals, return_type, return_type)?;
    let mut body = block.statements;
    if let Some(tail) = block.tail
    {
        body.push(Statement::Return {
            value: Some(*tail),
            span: block.span,
        });
    }
    Some(body)
}

pub fn lower_declarations(tree: &SyntaxTree) -> Result<declarations::Module, LoweringError>
{
    program::lower_program(std::slice::from_ref(tree))
}

pub fn lower_program(trees: &[SyntaxTree]) -> Result<declarations::Module, LoweringError>
{
    program::lower_program(trees)
}

#[cfg(test)]
#[path = "enum_data_tests.rs"]
mod enum_data_tests;
#[cfg(test)]
#[path = "tests.rs"]
mod tests;
