/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 */

use super::*;

fn enum_variant<'a>(
    tree: &SyntaxTree,
    value: &SyntaxNode,
    context: &'a LoweringContext,
) -> Option<(&'a declarations::NominalType, &'a declarations::EnumVariant)>
{
    let path = path_text(tree, value);
    let (enum_name, variant_name) = path.rsplit_once("::")?;
    let declaration = context.nominal_types.get(enum_name)?;
    matches!(declaration.kind, declarations::NominalKind::Enum).then_some(())?;
    let variant = declaration
        .variants
        .iter()
        .find(|variant| variant.name == variant_name)?;
    Some((declaration, variant))
}

pub(super) fn enum_variant_type(tree: &SyntaxTree, value: &SyntaxNode, context: &LoweringContext) -> Option<Type>
{
    let (declaration, _) = enum_variant(tree, value, context)?;
    Some(Type::Named(declaration.name.clone()))
}

pub(super) fn enum_variant_literal(
    tree: &SyntaxTree,
    value: &SyntaxNode,
    context: &LoweringContext,
    span: Span,
) -> Option<Expression>
{
    let (declaration, variant) = enum_variant(tree, value, context)?;
    Some(Expression::Literal {
        literal: Literal::EnumVariant {
            enum_type: declaration.name.clone(),
            variant: variant.name.clone(),
            tag: variant.tag,
        },
        span,
    })
}

fn enum_data_path(tree: &SyntaxTree, value: &SyntaxNode) -> Option<(String, String)>
{
    let path = path_text(tree, value);
    let (enum_type, variant) = path.rsplit_once("::")?;
    Some((enum_type.to_string(), variant.to_string()))
}

pub(super) fn enum_data_variant_type(tree: &SyntaxTree, value: &SyntaxNode, context: &LoweringContext) -> Option<Type>
{
    let (enum_type, variant) = enum_data_path(tree, value)?;
    context.enum_data.select(&enum_type, &variant, None).ok()?;
    Some(Type::Named(enum_type))
}

pub(super) fn enum_data_variant_literal(
    tree: &SyntaxTree,
    value: &SyntaxNode,
    context: &LoweringContext,
    source_span: Span,
) -> Option<Expression>
{
    let (enum_type, variant_name) = enum_data_path(tree, value)?;
    let variant = context.enum_data.select(&enum_type, &variant_name, None).ok()?;
    Some(Expression::EnumData {
        enum_type,
        owner: variant.owner.clone(),
        variant: variant.name.clone(),
        tag: variant.tag,
        payload: None,
        payload_type: None,
        span: source_span,
    })
}

pub(super) fn enum_data_constructor_type(
    tree: &SyntaxTree,
    call: &SyntaxNode,
    callee: &SyntaxNode,
    context: &LoweringContext,
    locals: &HashMap<String, Type>,
) -> Option<Type>
{
    if call.children.len() != 2
    {
        return None;
    }
    let (enum_type, variant_name) = enum_data_path(tree, callee)?;
    let argument = tree.nodes.get(call.children[1])?;
    let argument_type = expression_type::expression_type(tree, argument, context, locals)?;
    context
        .enum_data
        .select(&enum_type, &variant_name, Some(&argument_type))
        .ok()?;
    Some(Type::Named(enum_type))
}

pub(super) fn lower_enum_data_constructor(
    tree: &SyntaxTree,
    call: &SyntaxNode,
    context: &LoweringContext,
    locals: &HashMap<String, Type>,
    expected_type: Option<&Type>,
    source_span: Span,
) -> Option<Expression>
{
    if call.children.len() != 2
    {
        return None;
    }
    let callee = tree.nodes.get(call.children[0])?;
    let (enum_type, variant_name) = enum_data_path(tree, callee)?;
    if expected_type.is_some_and(|expected| expected != &Type::Named(enum_type.clone()))
    {
        return None;
    }
    let argument_node = tree.nodes.get(call.children[1])?;
    let argument_type = expression_type::expression_type(tree, argument_node, context, locals)?;
    let selected = context
        .enum_data
        .select(&enum_type, &variant_name, Some(&argument_type))
        .ok()?;
    let payload_type = selected.payload.as_ref().and_then(declarations::type_ref_to_checked)?;
    let payload = lower_expression(tree, argument_node, context, locals, Some(&payload_type))?;
    Some(Expression::EnumData {
        enum_type,
        owner: selected.owner.clone(),
        variant: selected.name.clone(),
        tag: selected.tag,
        payload: Some(Box::new(payload)),
        payload_type: Some(Box::new(payload_type)),
        span: source_span,
    })
}

pub(super) fn lower_nominal_type(
    tree: &SyntaxTree,
    value: &SyntaxNode,
) -> Result<declarations::NominalType, LoweringError>
{
    let name = first_child_kind(tree, value, IDENTIFIER).ok_or(LoweringError::MissingIdentifier)?;
    let bases = value
        .children
        .iter()
        .filter_map(|index| tree.nodes.get(*index))
        .filter(|child| child.kind == BASE_SPECIFIER)
        .map(|base| {
            let ty = base
                .children
                .iter()
                .filter_map(|index| tree.nodes.get(*index))
                .find(|child| (TYPE_NAMED..=TYPE_UNIT).contains(&child.kind) || child.kind == TYPE_MAP)
                .ok_or(LoweringError::MissingParameterType)?;
            let visibility = match base.visibility
            {
                1 => declarations::Visibility::Public,
                2 => declarations::Visibility::Private,
                3 => declarations::Visibility::Protected,
                _ => declarations::Visibility::Internal,
            };
            Ok(declarations::Base {
                ty: lower_type(tree, ty),
                visibility,
                is_virtual: base.flags & VIRTUAL != 0,
                span: base.span.clone(),
            })
        })
        .collect::<Result<Vec<_>, _>>()?;
    let fields = value
        .children
        .iter()
        .filter_map(|index| tree.nodes.get(*index))
        .filter(|child| matches!(child.kind, CLASS_FIELD | DATA_FIELD))
        .map(|field| {
            let name = first_child_kind(tree, field, IDENTIFIER).ok_or(LoweringError::MissingIdentifier)?;
            let ty = field
                .children
                .iter()
                .filter_map(|index| tree.nodes.get(*index))
                .find(|child| (TYPE_NAMED..=TYPE_UNIT).contains(&child.kind) || child.kind == TYPE_MAP)
                .ok_or(LoweringError::MissingParameterType)?;
            Ok(declarations::Field {
                name: name.text.clone(),
                ty: lower_type(tree, ty),
                mutable: field.flags & (IMMUTABLE | CONSTANT | STATIC_CONSTANT) == 0,
                span: field.span.clone(),
            })
        })
        .collect::<Result<Vec<_>, _>>()?;
    let variants = value
        .children
        .iter()
        .filter_map(|index| tree.nodes.get(*index))
        .filter(|child| child.kind == ENUM_VARIANT)
        .enumerate()
        .map(|(tag, variant)| {
            let name = first_child_kind(tree, variant, IDENTIFIER).ok_or(LoweringError::MissingIdentifier)?;
            let payload = variant
                .children
                .iter()
                .filter_map(|index| tree.nodes.get(*index))
                .find(|child| (TYPE_NAMED..=TYPE_UNIT).contains(&child.kind) || child.kind == TYPE_MAP)
                .map(|child| lower_type(tree, child));
            Ok(declarations::EnumVariant {
                name: name.text.clone(),
                payload,
                tag: u32::try_from(tag).map_err(|_| LoweringError::InvalidRoot)?,
                span: variant.span.clone(),
            })
        })
        .collect::<Result<Vec<_>, _>>()?;
    Ok(declarations::NominalType {
        name: name.text.clone(),
        kind: match value.kind
        {
            DECL_CLASS => declarations::NominalKind::Class,
            DECL_INTERFACE => declarations::NominalKind::Interface,
            DECL_ENUM if value.flags & DATA_ENUM != 0 => declarations::NominalKind::EnumData,
            DECL_ENUM => declarations::NominalKind::Enum,
            _ => declarations::NominalKind::Data,
        },
        bases,
        fields,
        variants,
        span: value.span.clone(),
    })
}

fn path_segments(tree: &SyntaxTree, value: &SyntaxNode) -> Option<(String, Vec<String>)>
{
    if value.kind == EXPR_IDENTIFIER
    {
        return Some((path_text(tree, value), Vec::new()));
    }
    if value.kind != EXPR_MEMBER_ACCESS || value.children.len() != 2
    {
        return None;
    }
    let (root, mut fields) = path_segments(tree, tree.nodes.get(value.children[0])?)?;
    fields.push(path_text(tree, tree.nodes.get(value.children[1])?));
    Some((root, fields))
}

fn field_path(
    tree: &SyntaxTree,
    value: &SyntaxNode,
    context: &LoweringContext,
    locals: &HashMap<String, Type>,
) -> Option<crate::hir::type_check::FieldPath>
{
    let (root, fields) = path_segments(tree, value)?;
    let mut current_type = locals.get(&root)?.clone();
    let mut mutable = false;
    for field_name in &fields
    {
        let Type::Named(type_name) = current_type
        else
        {
            return None;
        };
        let definition = context.nominal_types.get(&type_name)?;
        let fields = declarations::resolved_fields(definition, &context.nominal_types).ok()?;
        let field = fields.into_iter().find(|field| field.name == *field_name)?;
        current_type = declarations::type_ref_to_checked(&field.ty)?;
        mutable = field.mutable;
    }
    Some(crate::hir::type_check::FieldPath {
        root,
        fields,
        ty: current_type,
        mutable,
        span: span(value)?,
    })
}

pub(super) fn lower_field_expression(
    tree: &SyntaxTree,
    value: &SyntaxNode,
    context: &LoweringContext,
    locals: &HashMap<String, Type>,
) -> Option<Expression>
{
    Some(Expression::Field {
        path: field_path(tree, value, context, locals)?,
    })
}

pub(super) fn member_type(
    tree: &SyntaxTree,
    value: &SyntaxNode,
    context: &LoweringContext,
    locals: &HashMap<String, Type>,
) -> Option<Type>
{
    if value.kind != EXPR_MEMBER_ACCESS || value.children.len() != 2
    {
        return None;
    }
    let receiver = tree.nodes.get(value.children[0])?;
    let Type::Named(owner) = expression_type::expression_type(tree, receiver, context, locals)?
    else
    {
        return None;
    };
    let name = path_text(tree, tree.nodes.get(value.children[1])?);
    let definition = context.nominal_types.get(&owner)?;
    declarations::resolved_fields(definition, &context.nominal_types)
        .ok()?
        .into_iter()
        .find(|field| field.name == name)
        .and_then(|field| declarations::type_ref_to_checked(&field.ty))
}

pub(super) fn lower_member_expression(
    tree: &SyntaxTree,
    value: &SyntaxNode,
    context: &LoweringContext,
    locals: &HashMap<String, Type>,
    source_span: Span,
) -> Option<Expression>
{
    let receiver_node = tree.nodes.get(*value.children.first()?)?;
    let Type::Named(owner) = expression_type::expression_type(tree, receiver_node, context, locals)?
    else
    {
        return None;
    };
    let name = path_text(tree, tree.nodes.get(*value.children.get(1)?)?);
    let definition = context.nominal_types.get(&owner)?;
    let field = declarations::resolved_fields(definition, &context.nominal_types)
        .ok()?
        .into_iter()
        .find(|field| field.name == name)?;
    let field_type = declarations::type_ref_to_checked(&field.ty)?;
    let receiver = lower_expression(tree, receiver_node, context, locals, Some(&Type::Named(owner.clone())))?;
    Some(Expression::Member {
        receiver: Box::new(receiver),
        owner,
        name,
        field_type: Box::new(field_type),
        span: source_span,
    })
}

fn optional_member_parts(
    tree: &SyntaxTree,
    value: &SyntaxNode,
    context: &LoweringContext,
    locals: &HashMap<String, Type>,
) -> Option<(Type, String, String, Type)>
{
    if value.kind != EXPR_OPTIONAL_MEMBER_ACCESS || value.children.len() != 2
    {
        return None;
    }
    let receiver = tree.nodes.get(value.children[0])?;
    let Type::Optional {
        element,
    } = expression_type::expression_type(tree, receiver, context, locals)?
    else
    {
        return None;
    };
    let Type::Named(owner) = element.as_ref()
    else
    {
        return None;
    };
    let name = path_text(tree, tree.nodes.get(value.children[1])?);
    let definition = context.nominal_types.get(owner)?;
    let field_type = declarations::resolved_fields(definition, &context.nominal_types)
        .ok()?
        .into_iter()
        .find(|field| field.name == name)
        .and_then(|field| declarations::type_ref_to_checked(&field.ty))?;
    Some((
        Type::Optional {
            element: Box::new(Type::Named(owner.clone())),
        },
        owner.clone(),
        name,
        field_type,
    ))
}

pub(super) fn optional_member_type(
    tree: &SyntaxTree,
    value: &SyntaxNode,
    context: &LoweringContext,
    locals: &HashMap<String, Type>,
) -> Option<Type>
{
    let (_, _, _, field_type) = optional_member_parts(tree, value, context, locals)?;
    Some(Type::Optional {
        element: Box::new(field_type),
    })
}

pub(super) fn lower_optional_member_expression(
    tree: &SyntaxTree,
    value: &SyntaxNode,
    context: &LoweringContext,
    locals: &HashMap<String, Type>,
    expected_type: Option<&Type>,
    source_span: Span,
) -> Option<Expression>
{
    let (receiver_type, owner, name, field_type) = optional_member_parts(tree, value, context, locals)?;
    let result_type = Type::Optional {
        element: Box::new(field_type.clone()),
    };
    if expected_type.is_some_and(|expected| expected != &result_type)
    {
        return None;
    }
    let receiver_node = tree.nodes.get(value.children[0])?;
    let receiver = lower_expression(tree, receiver_node, context, locals, Some(&receiver_type))?;
    Some(Expression::OptionalMember {
        receiver: Box::new(receiver),
        owner,
        name,
        field_type: Box::new(field_type),
        result_type: Box::new(result_type),
        span: source_span,
    })
}

pub(super) fn lower_field_assignment(
    tree: &SyntaxTree,
    value: &SyntaxNode,
    context: &LoweringContext,
    locals: &HashMap<String, Type>,
    source_span: Span,
) -> Option<Expression>
{
    let target = field_path(tree, tree.nodes.get(value.children[0])?, context, locals)?;
    let assigned = lower_expression(
        tree,
        tree.nodes.get(value.children[2])?,
        context,
        locals,
        Some(&target.ty),
    )?;
    let assigned = if value.token_kind == TOKEN_ASSIGN
    {
        assigned
    }
    else
    {
        let operator = match value.token_kind
        {
            TOKEN_PLUS_ASSIGN => BinaryOperator::Add,
            TOKEN_MINUS_ASSIGN => BinaryOperator::Sub,
            TOKEN_STAR_ASSIGN => BinaryOperator::Mul,
            TOKEN_SLASH_ASSIGN => BinaryOperator::Div,
            TOKEN_PERCENT_ASSIGN => BinaryOperator::Rem,
            TOKEN_AMPERSAND_ASSIGN => BinaryOperator::BitAnd,
            TOKEN_PIPE_ASSIGN => BinaryOperator::BitOr,
            TOKEN_CARET_ASSIGN => BinaryOperator::BitXor,
            _ => return None,
        };
        Expression::Binary {
            operator,
            left: Box::new(Expression::Field {
                path: target.clone(),
            }),
            right: Box::new(assigned),
            span: source_span,
        }
    };
    Some(Expression::AssignField {
        target,
        value: Box::new(assigned),
        span: source_span,
    })
}

pub(super) fn lower_object_expression(
    tree: &SyntaxTree,
    value: &SyntaxNode,
    context: &LoweringContext,
    locals: &HashMap<String, Type>,
    expected_type: Option<&Type>,
    source_span: Span,
) -> Option<Expression>
{
    let (nominal_type, first_field) = if value.kind == EXPR_TYPED_OBJECT_LITERAL
    {
        (path_text(tree, tree.nodes.get(*value.children.first()?)?), 1)
    }
    else
    {
        let Type::Named(name) = expected_type?
        else
        {
            return None;
        };
        (name.clone(), 0)
    };
    let definition = context.nominal_types.get(&nominal_type)?;
    let definition_fields = declarations::resolved_fields(definition, &context.nominal_types).ok()?;
    let fields = value.children[first_field..]
        .iter()
        .map(|index| {
            let field = tree.nodes.get(*index)?;
            if field.kind != OBJECT_FIELD
            {
                return None;
            }
            let name = tree.nodes.get(*field.children.first()?)?;
            let definition = definition_fields.iter().find(|candidate| candidate.name == name.text)?;
            let expected = declarations::type_ref_to_checked(&definition.ty)?;
            let expression = tree.nodes.get(*field.children.get(1)?)?;
            Some(crate::hir::type_check::ObjectField {
                name: name.text.clone(),
                value: lower_expression(tree, expression, context, locals, Some(&expected))?,
                span: span(field)?,
            })
        })
        .collect::<Option<Vec<_>>>()?;
    Some(Expression::Object {
        nominal_type,
        fields,
        span: source_span,
    })
}
