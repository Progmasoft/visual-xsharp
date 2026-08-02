/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 */

use super::*;

#[derive(Clone)]
pub(super) struct CallSignature
{
    pub(super) symbol: String,
    pub(super) parameters: Vec<Type>,
    pub(super) return_type: Type,
}

#[derive(Clone)]
pub(super) struct LoweringContext
{
    pub(super) calls: HashMap<String, Vec<CallSignature>>,
    pub(super) generic_calls: HashMap<String, Vec<CallSignature>>,
    pub(super) constructors: HashMap<String, Vec<CallSignature>>,
    pub(super) methods: HashMap<(String, String), Vec<CallSignature>>,
    pub(super) nominal_types: HashMap<String, declarations::NominalType>,
    pub(super) enum_data: crate::hir::enum_data::EnumDataRegistry,
    pub(super) type_substitutions: HashMap<String, Type>,
}

pub(super) fn resolve_function<'a>(
    tree: &SyntaxTree,
    call: &SyntaxNode,
    name: &str,
    context: &'a LoweringContext,
    locals: &HashMap<String, Type>,
    expected_type: Option<&Type>,
) -> Option<&'a CallSignature>
{
    resolve_candidates(
        tree,
        &call.children[1..],
        context.calls.get(name)?,
        context,
        locals,
        expected_type,
        0,
    )
}

pub(super) fn resolve_generic_function<'a>(
    tree: &SyntaxTree,
    call: &SyntaxNode,
    callee: &SyntaxNode,
    context: &'a LoweringContext,
    locals: &HashMap<String, Type>,
    expected_type: Option<&Type>,
) -> Option<&'a CallSignature>
{
    let key = generic::specialized_call_key(tree, callee, &context.type_substitutions)?;
    resolve_candidates(
        tree,
        &call.children[1..],
        context.generic_calls.get(&key)?,
        context,
        locals,
        expected_type,
        0,
    )
}

pub(super) fn lower_method_call(
    tree: &SyntaxTree,
    call: &SyntaxNode,
    context: &LoweringContext,
    locals: &HashMap<String, Type>,
    expected_type: Option<&Type>,
    source_span: Span,
) -> Option<Expression>
{
    if call.children.len() < 2
    {
        return None;
    }
    let receiver_node = tree.nodes.get(call.children[0])?;
    let receiver_type = expression_type(tree, receiver_node, context, locals)?;
    let Type::Named(owner) = &receiver_type
    else
    {
        return None;
    };
    let method_name = path_text(tree, tree.nodes.get(call.children[1])?);
    let candidates = context.methods.get(&(owner.clone(), method_name))?;
    let signature = resolve_candidates(tree, &call.children[2..], candidates, context, locals, expected_type, 1)?;
    if signature.parameters.first() != Some(&receiver_type)
    {
        return None;
    }
    let mut arguments = Vec::with_capacity(call.children.len() - 1);
    arguments.push(lower_expression(
        tree,
        receiver_node,
        context,
        locals,
        Some(&receiver_type),
    )?);
    arguments.extend(
        call.children[2..]
            .iter()
            .zip(&signature.parameters[1..])
            .map(|(index, parameter)| lower_expression(tree, tree.nodes.get(*index)?, context, locals, Some(parameter)))
            .collect::<Option<Vec<_>>>()?,
    );
    Some(Expression::Call {
        function: signature.symbol.clone(),
        arguments,
        parameter_types: signature.parameters.clone(),
        return_type: Box::new(signature.return_type.clone()),
        span: source_span,
    })
}

fn resolve_candidates<'a>(
    tree: &SyntaxTree,
    arguments: &[usize],
    candidates: &'a [CallSignature],
    context: &LoweringContext,
    locals: &HashMap<String, Type>,
    expected_type: Option<&Type>,
    implicit_parameters: usize,
) -> Option<&'a CallSignature>
{
    let mut matching = candidates
        .iter()
        .filter(|candidate| candidate.parameters.len() == arguments.len() + implicit_parameters)
        .filter(|candidate| expected_type.is_none_or(|expected| expected == &candidate.return_type))
        .collect::<Vec<_>>();
    if matching.len() == 1
    {
        return matching.pop();
    }
    let argument_types = arguments
        .iter()
        .map(|index| expression_type(tree, tree.nodes.get(*index)?, context, locals))
        .collect::<Option<Vec<_>>>()?;
    matching.retain(|candidate| candidate.parameters[implicit_parameters..] == argument_types);
    (matching.len() == 1).then(|| matching[0])
}

pub(super) fn signature(function: &declarations::Function) -> Option<CallSignature>
{
    Some(CallSignature {
        symbol: function.name.clone(),
        parameters: function
            .parameters
            .iter()
            .map(|parameter| checked_type(&parameter.ty))
            .collect::<Option<Vec<_>>>()?,
        return_type: checked_type(&function.return_type)?,
    })
}

pub(super) fn has_duplicate_signatures(registry: &HashMap<String, Vec<CallSignature>>) -> bool
{
    registry.values().any(|signatures| {
        signatures.iter().enumerate().any(|(index, signature)| {
            signatures[index + 1..]
                .iter()
                .any(|other| signature.parameters == other.parameters)
        })
    })
}

pub(super) fn has_duplicate_method_signatures(registry: &HashMap<(String, String), Vec<CallSignature>>) -> bool
{
    registry.values().any(|signatures| {
        signatures.iter().enumerate().any(|(index, signature)| {
            signatures[index + 1..]
                .iter()
                .any(|other| signature.parameters == other.parameters)
        })
    })
}
