// SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
// SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.0

fn write_desugared_expression(output: &mut String, expression: &DesugaredExpression, indent: usize)
{
    let pad = "  ".repeat(indent);
    match expression
    {
        DesugaredExpression::Literal {
            literal, ..
        } =>
        {
            let _ = writeln!(output, "{pad}literal {}", literal_name(literal));
        }
        DesugaredExpression::Local {
            name, ..
        } =>
        {
            let _ = writeln!(output, "{pad}local {name}");
        }
        DesugaredExpression::Field {
            path,
        } =>
        {
            nominal::write_field(output, path, &pad);
        }
        DesugaredExpression::Member {
            receiver,
            owner,
            name,
            field_type,
            ..
        } =>
        {
            nominal::write_desugared_member(output, receiver, owner, name, field_type, indent);
        }
        DesugaredExpression::Object {
            nominal_type,
            fields,
            ..
        } =>
        {
            let _ = writeln!(output, "{pad}object {nominal_type}");
            for field in fields
            {
                let _ = writeln!(output, "{pad}  field {}", field.name);
                write_desugared_expression(output, &field.value, indent + 2);
            }
            let _ = writeln!(output, "{pad}.end");
        }
        DesugaredExpression::EnumData {
            ..
        } =>
        {
            enum_data::write_desugared_expression(output, expression, indent);
        }
        DesugaredExpression::Array {
            elements, ..
        } =>
        {
            let _ = writeln!(output, "{pad}array");
            for element in elements
            {
                let _ = writeln!(output, "{pad}  element");
                write_desugared_expression(output, element, indent + 2);
            }
            let _ = writeln!(output, "{pad}.end");
        }
        DesugaredExpression::Set {
            elements, ..
        } =>
        {
            let _ = writeln!(output, "{pad}set");
            for element in elements
            {
                let _ = writeln!(output, "{pad}  element");
                write_desugared_expression(output, element, indent + 2);
            }
            let _ = writeln!(output, "{pad}.end");
        }
        DesugaredExpression::Map {
            entries, ..
        } =>
        {
            let _ = writeln!(output, "{pad}map");
            for entry in entries
            {
                let _ = writeln!(output, "{pad}  entry");
                let _ = writeln!(output, "{pad}    key");
                write_desugared_expression(output, &entry.key, indent + 3);
                let _ = writeln!(output, "{pad}    value");
                write_desugared_expression(output, &entry.value, indent + 3);
            }
            let _ = writeln!(output, "{pad}.end");
        }
        DesugaredExpression::Tuple {
            fields,
            tuple_type,
            ..
        } =>
        {
            let _ = writeln!(output, "{pad}tuple {}", type_name(tuple_type));
            for field in fields
            {
                let name = field.name.as_deref().unwrap_or("positional");
                let _ = writeln!(output, "{pad}  field {name}");
                write_desugared_expression(output, &field.value, indent + 2);
            }
            let _ = writeln!(output, "{pad}.end");
        }
        DesugaredExpression::TupleElement {
            tuple,
            index,
            element_type,
            ..
        } =>
        {
            let _ = writeln!(output, "{pad}tuple_element {index} {}", type_name(element_type));
            write_desugared_expression(output, tuple, indent + 1);
            let _ = writeln!(output, "{pad}.end");
        }
        DesugaredExpression::Index {
            collection,
            index,
            element_type,
            ..
        } =>
        {
            let _ = writeln!(output, "{pad}index {}", type_name(element_type));
            let _ = writeln!(output, "{pad}  collection");
            write_desugared_expression(output, collection, indent + 2);
            let _ = writeln!(output, "{pad}  offset");
            write_desugared_expression(output, index, indent + 2);
            let _ = writeln!(output, "{pad}.end");
        }
        DesugaredExpression::ArrayLength {
            collection, ..
        } =>
        {
            let _ = writeln!(output, "{pad}array_length");
            write_desugared_expression(output, collection, indent + 1);
            let _ = writeln!(output, "{pad}.end");
        }
        DesugaredExpression::Assign {
            target,
            value,
            ..
        } =>
        {
            let _ = writeln!(output, "{pad}assign {target}");
            write_desugared_expression(output, value, indent + 1);
        }
        DesugaredExpression::AssignField {
            target,
            value,
            ..
        } =>
        {
            let mutability = if target.mutable
            {
                "mutable"
            }
            else
            {
                "immutable"
            };
            let _ = writeln!(
                output,
                "{pad}assign_field {mutability} {} : {}",
                field_path_name(target),
                type_name(&target.ty)
            );
            write_desugared_expression(output, value, indent + 1);
        }
        DesugaredExpression::Update {
            target,
            operator,
            position,
            ..
        } =>
        {
            let _ = writeln!(
                output,
                "{pad}update {} {} {target}",
                update_position_name(*position),
                update_operator_name(*operator)
            );
        }
        DesugaredExpression::Binary {
            operator,
            left,
            right,
            ..
        } =>
        {
            let _ = writeln!(output, "{pad}binary {}", binary_operator_name(*operator));
            let _ = writeln!(output, "{pad}  left");
            write_desugared_expression(output, left, indent + 2);
            let _ = writeln!(output, "{pad}  right");
            write_desugared_expression(output, right, indent + 2);
        }
        DesugaredExpression::Unary {
            operator,
            operand,
            ..
        } =>
        {
            let _ = writeln!(output, "{pad}unary {}", unary_operator_name(*operator));
            let _ = writeln!(output, "{pad}  operand");
            write_desugared_expression(output, operand, indent + 2);
        }
        DesugaredExpression::OptionalUnwrap {
            value,
            element_type,
            ..
        } =>
        {
            let _ = writeln!(output, "{pad}optional_unwrap {}", type_name(element_type));
            let _ = writeln!(output, "{pad}  value");
            write_desugared_expression(output, value, indent + 2);
        }
        DesugaredExpression::OptionalCoalesceAssign {
            target,
            value,
            optional_type,
            ..
        } =>
        {
            let _ = writeln!(
                output,
                "{pad}optional_coalesce_assign {target} : {}",
                type_name(optional_type)
            );
            let _ = writeln!(output, "{pad}  value");
            write_desugared_expression(output, value, indent + 2);
        }
        DesugaredExpression::OptionalMember {
            receiver,
            owner,
            name,
            field_type,
            result_type,
            ..
        } =>
        {
            let _ = writeln!(
                output,
                "{pad}optional_member {owner}::{name} : {} -> {}",
                type_name(field_type),
                type_name(result_type)
            );
            let _ = writeln!(output, "{pad}  receiver");
            write_desugared_expression(output, receiver, indent + 2);
        }
        DesugaredExpression::ResultMatch {
            value,
            success_binding,
            error_binding,
            success_type,
            error_type,
            ..
        } =>
        {
            let _ = writeln!(output, "{pad}result_match");
            let _ = writeln!(output, "{pad}  ok {success_binding}: {}", type_name(success_type));
            let _ = writeln!(output, "{pad}  error {error_binding}: {}", type_name(error_type));
            let _ = writeln!(output, "{pad}  value");
            write_desugared_expression(output, value, indent + 2);
        }
        DesugaredExpression::Call {
            function,
            arguments,
            parameter_types,
            return_type,
            ..
        } =>
        {
            let parameters = parameter_types.iter().map(type_name).collect::<Vec<_>>().join(", ");
            let _ = writeln!(
                output,
                "{pad}call {function} : ({parameters}) -> {}",
                type_name(return_type)
            );
            for argument in arguments
            {
                let _ = writeln!(output, "{pad}  argument");
                write_desugared_expression(output, argument, indent + 2);
            }
        }
        DesugaredExpression::If {
            condition,
            then_block,
            else_block,
            result_type,
            ..
        } =>
        {
            let _ = writeln!(output, "{pad}if_expression {}", type_name(result_type));
            let _ = writeln!(output, "{pad}  condition");
            write_desugared_expression(output, condition, indent + 2);
            let _ = writeln!(output, "{pad}  then");
            write_desugared_block(output, then_block, indent + 2);
            let _ = writeln!(output, "{pad}  else");
            write_desugared_block(output, else_block, indent + 2);
        }
        DesugaredExpression::Match {
            selector,
            selector_type,
            arms,
            result_type,
            ..
        } =>
        {
            let _ = writeln!(
                output,
                "{pad}match_expression {} selector {}",
                type_name(result_type),
                type_name(selector_type)
            );
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
    }
}

fn write_function_header(output: &mut String, name: &str, return_type: Option<&Type>)
{
    let _ = writeln!(output, ".xhir version 1");
    let _ = writeln!(output, "function {name}");
    let _ = writeln!(output, "  signature");
    match return_type
    {
        Some(return_type) =>
        {
            let _ = writeln!(output, "    returns {}", type_name(return_type));
        }
        None =>
        {
            let _ = writeln!(output, "    returns void");
        }
    }
    let _ = writeln!(output, "  .end");
}

fn write_locals(output: &mut String, locals: &[super::type_check::Local])
{
    let _ = writeln!(output, "  locals");
    for local in locals
    {
        let _ = writeln!(
            output,
            "    local {}: {} {}",
            local.name,
            type_name(&local.ty),
            mutability_name(local.mutable)
        );
    }
    let _ = writeln!(output, "  .end");
}

#[cfg(test)]
#[path = "tests.rs"]
mod tests;
