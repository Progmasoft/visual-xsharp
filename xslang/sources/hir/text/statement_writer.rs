/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 */

use std::fmt::Write;

use super::{
    DesugaredStatement, Statement, match_pattern_name, mutability_name, type_name, write_block, write_desugared_block,
    write_desugared_expression, write_expression,
};

pub(super) fn write_statement(output: &mut String, statement: &Statement, indent: usize)
{
    let pad = "  ".repeat(indent);
    match statement
    {
        Statement::Let {
            local,
            initializer,
        } =>
        {
            let _ = writeln!(output, "{pad}let {}", local.name);
            let _ = writeln!(output, "{pad}  type {}", type_name(&local.ty));
            let _ = writeln!(output, "{pad}  mutability {}", mutability_name(local.mutable));
            if let Some(initializer) = initializer
            {
                let _ = writeln!(output, "{pad}  initializer");
                write_expression(output, initializer, indent + 2);
            }
        }
        Statement::Expr(expression) =>
        {
            let _ = writeln!(output, "{pad}expression");
            write_expression(output, expression, indent + 1);
        }
        Statement::AssignIndex {
            target,
            index,
            value,
            element_type,
            ..
        } =>
        {
            let _ = writeln!(output, "{pad}assign_index {target}");
            let _ = writeln!(output, "{pad}  type {}", type_name(element_type));
            let _ = writeln!(output, "{pad}  index");
            write_expression(output, index, indent + 2);
            let _ = writeln!(output, "{pad}  value");
            write_expression(output, value, indent + 2);
            let _ = writeln!(output, "{pad}.end");
        }
        Statement::AssignTupleElement {
            target,
            index,
            value,
            tuple_type,
            element_type,
            ..
        } =>
        {
            let _ = writeln!(output, "{pad}assign_tuple_element {target} {index}");
            let _ = writeln!(output, "{pad}  tuple_type {}", type_name(tuple_type));
            let _ = writeln!(output, "{pad}  element_type {}", type_name(element_type));
            let _ = writeln!(output, "{pad}  value");
            write_expression(output, value, indent + 2);
            let _ = writeln!(output, "{pad}.end");
        }
        Statement::Return {
            value, ..
        } =>
        {
            let _ = writeln!(output, "{pad}return");
            if let Some(value) = value
            {
                write_expression(output, value, indent + 1);
            }
        }
        Statement::If {
            condition,
            then_block,
            else_block,
            ..
        } =>
        {
            write_conditional(output, condition, then_block, else_block.as_ref(), indent);
        }
        Statement::While {
            condition,
            body,
            ..
        } =>
        {
            let _ = writeln!(output, "{pad}while");
            let _ = writeln!(output, "{pad}  condition");
            write_expression(output, condition, indent + 2);
            let _ = writeln!(output, "{pad}  body");
            write_block(output, body, indent + 2);
        }
        Statement::For {
            initializer,
            condition,
            update,
            body,
            ..
        } =>
        {
            let _ = writeln!(output, "{pad}for");
            if let Some(initializer) = initializer
            {
                let _ = writeln!(output, "{pad}  initializer");
                write_statement(output, initializer, indent + 2);
            }
            if let Some(condition) = condition
            {
                let _ = writeln!(output, "{pad}  condition");
                write_expression(output, condition, indent + 2);
            }
            if let Some(update) = update
            {
                let _ = writeln!(output, "{pad}  update");
                write_expression(output, update, indent + 2);
            }
            let _ = writeln!(output, "{pad}  body");
            write_block(output, body, indent + 2);
        }
        Statement::ForEach {
            binding,
            iterable,
            iterable_type,
            body,
            ..
        } =>
        {
            let _ = writeln!(output, "{pad}for_each {}", binding.name);
            let _ = writeln!(output, "{pad}  type {}", type_name(&binding.ty));
            let _ = writeln!(output, "{pad}  iterable_type {}", type_name(iterable_type));
            let _ = writeln!(output, "{pad}  iterable");
            write_expression(output, iterable, indent + 2);
            let _ = writeln!(output, "{pad}  body");
            write_block(output, body, indent + 2);
        }
        Statement::Match {
            selector,
            selector_type,
            arms,
            ..
        } =>
        {
            let _ = writeln!(output, "{pad}match {}", type_name(selector_type));
            let _ = writeln!(output, "{pad}  selector");
            write_expression(output, selector, indent + 2);
            for arm in arms
            {
                let _ = writeln!(output, "{pad}  arm {}", match_pattern_name(&arm.pattern));
                let _ = writeln!(output, "{pad}    body");
                write_block(output, &arm.body, indent + 3);
            }
            let _ = writeln!(output, "{pad}.end");
        }
        Statement::Break {
            ..
        } =>
        {
            let _ = writeln!(output, "{pad}break");
        }
        Statement::Continue {
            ..
        } =>
        {
            let _ = writeln!(output, "{pad}continue");
        }
        Statement::Panic {
            ..
        } =>
        {
            let _ = writeln!(output, "{pad}panic");
        }
    }
}

fn write_conditional(
    output: &mut String,
    condition: &super::Expression,
    then_block: &super::Block,
    else_block: Option<&super::Block>,
    indent: usize,
)
{
    let pad = "  ".repeat(indent);
    let _ = writeln!(output, "{pad}if");
    let _ = writeln!(output, "{pad}  condition");
    write_expression(output, condition, indent + 2);
    let _ = writeln!(output, "{pad}  then");
    write_block(output, then_block, indent + 2);
    if let Some(else_block) = else_block
    {
        let _ = writeln!(output, "{pad}  else");
        write_block(output, else_block, indent + 2);
    }
}

pub(super) fn write_desugared_statement(output: &mut String, statement: &DesugaredStatement, indent: usize)
{
    let pad = "  ".repeat(indent);
    match statement
    {
        DesugaredStatement::Let {
            local,
            initializer,
        } =>
        {
            let _ = writeln!(output, "{pad}let {}", local.name);
            let _ = writeln!(output, "{pad}  type {}", type_name(&local.ty));
            let _ = writeln!(output, "{pad}  mutability {}", mutability_name(local.mutable));
            if let Some(initializer) = initializer
            {
                let _ = writeln!(output, "{pad}  initializer");
                write_desugared_expression(output, initializer, indent + 2);
            }
        }
        DesugaredStatement::Expr(expression) =>
        {
            let _ = writeln!(output, "{pad}expression");
            write_desugared_expression(output, expression, indent + 1);
        }
        DesugaredStatement::AssignIndex {
            target,
            index,
            value,
            element_type,
            ..
        } =>
        {
            let _ = writeln!(output, "{pad}assign_index {target}");
            let _ = writeln!(output, "{pad}  type {}", type_name(element_type));
            let _ = writeln!(output, "{pad}  index");
            write_desugared_expression(output, index, indent + 2);
            let _ = writeln!(output, "{pad}  value");
            write_desugared_expression(output, value, indent + 2);
            let _ = writeln!(output, "{pad}.end");
        }
        DesugaredStatement::AssignTupleElement {
            target,
            index,
            value,
            tuple_type,
            element_type,
            ..
        } =>
        {
            let _ = writeln!(output, "{pad}assign_tuple_element {target} {index}");
            let _ = writeln!(output, "{pad}  tuple_type {}", type_name(tuple_type));
            let _ = writeln!(output, "{pad}  element_type {}", type_name(element_type));
            let _ = writeln!(output, "{pad}  value");
            write_desugared_expression(output, value, indent + 2);
            let _ = writeln!(output, "{pad}.end");
        }
        DesugaredStatement::Return {
            value, ..
        } =>
        {
            let _ = writeln!(output, "{pad}return");
            if let Some(value) = value
            {
                write_desugared_expression(output, value, indent + 1);
            }
        }
        DesugaredStatement::If {
            condition,
            then_block,
            else_block,
            ..
        } =>
        {
            let _ = writeln!(output, "{pad}if");
            let _ = writeln!(output, "{pad}  condition");
            write_desugared_expression(output, condition, indent + 2);
            let _ = writeln!(output, "{pad}  then");
            write_desugared_block(output, then_block, indent + 2);
            if let Some(else_block) = else_block
            {
                let _ = writeln!(output, "{pad}  else");
                write_desugared_block(output, else_block, indent + 2);
            }
        }
        DesugaredStatement::While {
            condition,
            body,
            ..
        } =>
        {
            let _ = writeln!(output, "{pad}while");
            let _ = writeln!(output, "{pad}  condition");
            write_desugared_expression(output, condition, indent + 2);
            let _ = writeln!(output, "{pad}  body");
            write_desugared_block(output, body, indent + 2);
        }
        DesugaredStatement::For {
            initializer,
            condition,
            update,
            body,
            ..
        } =>
        {
            let _ = writeln!(output, "{pad}for");
            if let Some(initializer) = initializer
            {
                let _ = writeln!(output, "{pad}  initializer");
                write_desugared_statement(output, initializer, indent + 2);
            }
            if let Some(condition) = condition
            {
                let _ = writeln!(output, "{pad}  condition");
                write_desugared_expression(output, condition, indent + 2);
            }
            if let Some(update) = update
            {
                let _ = writeln!(output, "{pad}  update");
                write_desugared_expression(output, update, indent + 2);
            }
            let _ = writeln!(output, "{pad}  body");
            write_desugared_block(output, body, indent + 2);
        }
        DesugaredStatement::ForEach {
            binding,
            iterable,
            iterable_type,
            body,
            ..
        } =>
        {
            let _ = writeln!(output, "{pad}for_each {}", binding.name);
            let _ = writeln!(output, "{pad}  type {}", type_name(&binding.ty));
            let _ = writeln!(output, "{pad}  iterable_type {}", type_name(iterable_type));
            let _ = writeln!(output, "{pad}  iterable");
            write_desugared_expression(output, iterable, indent + 2);
            let _ = writeln!(output, "{pad}  body");
            write_desugared_block(output, body, indent + 2);
        }
        DesugaredStatement::Match {
            selector,
            selector_type,
            arms,
            ..
        } =>
        {
            let _ = writeln!(output, "{pad}match {}", type_name(selector_type));
            let _ = writeln!(output, "{pad}  selector");
            write_desugared_expression(output, selector, indent + 2);
            for arm in arms
            {
                let _ = writeln!(output, "{pad}  arm {}", match_pattern_name(&arm.pattern));
                let _ = writeln!(output, "{pad}    body");
                write_desugared_block(output, &arm.body, indent + 3);
            }
            let _ = writeln!(output, "{pad}.end");
        }
        DesugaredStatement::Break {
            ..
        } =>
        {
            let _ = writeln!(output, "{pad}break");
        }
        DesugaredStatement::Continue {
            ..
        } =>
        {
            let _ = writeln!(output, "{pad}continue");
        }
        DesugaredStatement::Panic {
            ..
        } =>
        {
            let _ = writeln!(output, "{pad}panic");
        }
    }
}
