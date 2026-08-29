/*
 * SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
 * SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.0
 */

use super::*;
use crate::compiler_core::SourceSpan;
use crate::hir::declarations::{Base, EnumVariant, NominalKind, NominalType, TypeRef, Visibility};

fn span() -> Span
{
    Span::new(1, 0, 1)
}

fn source_span() -> SourceSpan
{
    SourceSpan {
        file_id: 1,
        start_offset: 0,
        end_offset: 1,
        start_line: 1,
        start_column: 0,
        end_line: 1,
        end_column: 1,
    }
}

fn variant(name: &str, payload: Option<PrimitiveType>, tag: u32) -> EnumVariant
{
    EnumVariant {
        name: name.to_string(),
        payload: payload.map(TypeRef::Primitive),
        tag,
        span: source_span(),
    }
}

fn declaration(name: &str, bases: &[&str], variants: Vec<EnumVariant>) -> NominalType
{
    NominalType {
        name: name.to_string(),
        kind: NominalKind::EnumData,
        bases: bases
            .iter()
            .map(|base| Base {
                ty: TypeRef::Named((*base).to_string()),
                visibility: Visibility::Internal,
                is_virtual: false,
                span: source_span(),
            })
            .collect(),
        fields: Vec::new(),
        variants,
        span: source_span(),
    }
}

fn value_declaration() -> NominalType
{
    declaration("Value", &[], vec![
        variant("Number", Some(PrimitiveType::Long), 0),
        variant("Wide", Some(PrimitiveType::Int), 1),
        variant("Empty", None, 2),
    ])
}

fn pattern(
    owner: &str,
    variant: &str,
    tag: u32,
    binding: Option<&str>,
    payload_type: Option<PrimitiveType>,
) -> MatchPattern
{
    MatchPattern::EnumDataVariant {
        enum_type: "Value".to_string(),
        owner: owner.to_string(),
        variant: variant.to_string(),
        tag,
        binding: binding.map(str::to_string),
        payload_type: payload_type.map(Type::Primitive),
    }
}

fn local(name: &str) -> Expression
{
    Expression::Local {
        name: name.to_string(),
        span: span(),
    }
}

fn integer(value: &str) -> Expression
{
    Expression::Literal {
        literal: Literal::Integer(value.to_string()),
        span: span(),
    }
}

fn returning(pattern: MatchPattern, value: Expression) -> MatchArm
{
    MatchArm {
        pattern,
        body: Block {
            statements: vec![Statement::Return {
                value: Some(value),
                span: span(),
            }],
            tail: None,
            span: span(),
        },
        span: span(),
    }
}

fn yielding(pattern: MatchPattern, value: Expression) -> MatchArm
{
    MatchArm {
        pattern,
        body: Block {
            statements: Vec::new(),
            tail: Some(Box::new(value)),
            span: span(),
        },
        span: span(),
    }
}

fn function(name: &str, arms: Vec<MatchArm>, expression: bool) -> Function
{
    let match_value = Expression::Match {
        selector: Box::new(local("input")),
        selector_type: Box::new(Type::Named("Value".to_string())),
        arms: arms.clone(),
        result_type: Box::new(Type::Primitive(PrimitiveType::Long)),
        span: span(),
    };
    Function {
        name: name.to_string(),
        return_type: Some(Type::Primitive(PrimitiveType::Long)),
        locals: vec![crate::hir::Local {
            name: "input".to_string(),
            ty: Type::Named("Value".to_string()),
            mutable: false,
            span: span(),
        }],
        body: if expression
        {
            vec![Statement::Return {
                value: Some(match_value),
                span: span(),
            }]
        }
        else
        {
            vec![Statement::Match {
                selector: local("input"),
                selector_type: Type::Named("Value".to_string()),
                arms,
                span: span(),
            }]
        },
    }
}

fn lower(function: &Function, declarations: &[NominalType]) -> mir::Function
{
    HirToMirLowerer::new()
        .with_nominal_types(declarations)
        .lower_function_with_parameters(function, 1)
        .unwrap_or_else(|diagnostics| panic!("enum data match should lower: {diagnostics:#?}"))
}

fn lower_to_module(function: &Function, declarations: &[NominalType]) -> crate::xlil::Module
{
    let registry = crate::hir::aggregate_registry::build(declarations).expect("aggregate registry");
    let mir = lower(function, declarations);
    assert!(crate::mir::verify::verify_function(&mir).is_empty());
    let function = crate::xlil::lowering::MirToXlilLowerer::new()
        .lower_function(&mir)
        .expect("MIR to XLIL lowering");
    let mut module = crate::xlil::Module::new("EnumMatch");
    for layout in &registry.layouts
    {
        assert_eq!(
            module.add_aggregate_type(layout.name.clone(), layout.fields.clone()),
            Some(layout.value_type)
        );
    }
    module.add_function(function);
    assert!(crate::xlil::verify_module(&module).is_empty());
    module
}

#[test]
fn lowers_enum_data_tag_test_to_extract_compare_and_branch()
{
    let declaration = value_declaration();
    let function = function(
        "select",
        vec![
            returning(
                pattern("Value", "Number", 0, None, Some(PrimitiveType::Long)),
                integer("7"),
            ),
            returning(MatchPattern::Else, integer("9")),
        ],
        false,
    );
    let mir = lower(&function, &[declaration]);
    let statements = &mir.blocks[0].statements;
    assert!(matches!(statements[0], mir::Statement::Extract {
        field: 0,
        field_type: XlilType::I32,
        ..
    }));
    assert!(
        statements
            .iter()
            .any(|statement| matches!(statement, mir::Statement::ConstI32 {
                value: 0,
                ..
            }))
    );
    assert!(
        statements
            .iter()
            .any(|statement| matches!(statement, mir::Statement::EqI32 { .. }))
    );
    assert!(matches!(
        mir.blocks[0].terminator,
        Some(mir::Terminator::BranchIf { .. })
    ));
    assert!(crate::mir::verify::verify_function(&mir).is_empty());
}

#[test]
fn lowers_payload_binding_from_the_selected_slot()
{
    let declaration = value_declaration();
    let function = function(
        "unwrap",
        vec![
            returning(
                pattern("Value", "Number", 0, Some("number"), Some(PrimitiveType::Long)),
                local("number"),
            ),
            returning(MatchPattern::Else, integer("0")),
        ],
        false,
    );
    let mir = lower(&function, &[declaration]);
    assert!(mir.blocks.iter().flat_map(|block| &block.statements).any(|statement| {
        matches!(statement, mir::Statement::Extract {
            field: 1,
            field_type: XlilType::I32,
            ..
        })
    }));
    assert!(crate::mir::verify::verify_function(&mir).is_empty());
}

#[test]
fn int_payload_binding_uses_its_distinct_i64_slot()
{
    let declaration = value_declaration();
    let function = function(
        "unwrap_wide",
        vec![
            returning(
                pattern("Value", "Wide", 1, Some("wide"), Some(PrimitiveType::Int)),
                Expression::Binary {
                    operator: BinaryOperator::Equal,
                    left: Box::new(local("wide")),
                    right: Box::new(integer("0")),
                    span: span(),
                },
            ),
            returning(MatchPattern::Else, integer("0")),
        ],
        false,
    );
    let diagnostics = HirToMirLowerer::new()
        .with_nominal_types(std::slice::from_ref(&declaration))
        .lower_function_with_parameters(&function, 1)
        .expect_err("Bool return must not match Long function return");
    assert!(
        diagnostics
            .iter()
            .any(|diagnostic| diagnostic.code == DiagnosticCode::UnsupportedType)
    );
}

#[test]
fn exhaustive_match_uses_the_final_variant_as_fallthrough()
{
    let declaration = value_declaration();
    let function = function(
        "all",
        vec![
            returning(
                pattern("Value", "Number", 0, None, Some(PrimitiveType::Long)),
                integer("1"),
            ),
            returning(
                pattern("Value", "Wide", 1, None, Some(PrimitiveType::Int)),
                integer("2"),
            ),
            returning(pattern("Value", "Empty", 2, None, None), integer("3")),
        ],
        false,
    );
    let mir = lower(&function, &[declaration]);
    let branches = mir
        .blocks
        .iter()
        .filter(|block| matches!(block.terminator, Some(mir::Terminator::BranchIf { .. })))
        .count();
    assert_eq!(
        branches, 2,
        "the exhaustive final arm does not require a third tag comparison"
    );
    assert!(crate::mir::verify::verify_function(&mir).is_empty());
}

#[test]
fn match_expression_merges_bound_payload_value()
{
    let declaration = value_declaration();
    let function = function(
        "value",
        vec![
            yielding(
                pattern("Value", "Number", 0, Some("number"), Some(PrimitiveType::Long)),
                local("number"),
            ),
            yielding(MatchPattern::Else, integer("4")),
        ],
        true,
    );
    let mir = lower(&function, &[declaration]);
    assert!(
        mir.blocks
            .iter()
            .flat_map(|block| &block.statements)
            .any(|statement| matches!(statement, mir::Statement::StoreLocal { .. }))
    );
    assert!(
        mir.blocks
            .iter()
            .flat_map(|block| &block.statements)
            .any(|statement| matches!(statement, mir::Statement::LoadLocal { .. }))
    );
    assert!(crate::mir::verify::verify_function(&mir).is_empty());
}

#[test]
fn inherited_variant_uses_flattened_tag_and_payload_slot()
{
    let root = declaration("Root", &[], vec![variant("Number", Some(PrimitiveType::Long), 8)]);
    let value = declaration("Value", &["Root"], vec![variant(
        "Text",
        Some(PrimitiveType::String),
        4,
    )]);
    let function = function(
        "inherited",
        vec![
            returning(
                pattern("Root", "Number", 0, Some("number"), Some(PrimitiveType::Long)),
                local("number"),
            ),
            returning(
                pattern("Value", "Text", 1, None, Some(PrimitiveType::String)),
                integer("0"),
            ),
        ],
        false,
    );
    let mir = lower(&function, &[root, value]);
    assert!(mir.blocks.iter().flat_map(|block| &block.statements).any(|statement| {
        matches!(statement, mir::Statement::Extract {
            field: 1,
            ..
        })
    }));
    assert!(crate::mir::verify::verify_function(&mir).is_empty());
}

#[test]
fn rejects_spoofed_enum_data_pattern_metadata_before_branching()
{
    let declaration = value_declaration();
    let function = function(
        "bad",
        vec![
            returning(
                pattern("Other", "Number", 99, None, Some(PrimitiveType::Long)),
                integer("1"),
            ),
            returning(MatchPattern::Else, integer("0")),
        ],
        false,
    );
    let diagnostics = HirToMirLowerer::new()
        .with_nominal_types(&[declaration])
        .lower_function_with_parameters(&function, 1)
        .expect_err("forged pattern metadata must fail");
    assert!(
        diagnostics
            .iter()
            .any(|diagnostic| diagnostic.code == DiagnosticCode::UnsupportedExpression &&
                diagnostic.message.contains("metadata"))
    );
}

#[test]
fn lowers_enum_data_match_through_canonical_xlil_text()
{
    let declaration = value_declaration();
    let function = function(
        "select",
        vec![
            returning(
                pattern("Value", "Number", 0, None, Some(PrimitiveType::Long)),
                integer("7"),
            ),
            returning(MatchPattern::Else, integer("9")),
        ],
        false,
    );
    let module = lower_to_module(&function, &[declaration]);
    let text = crate::xlil::module_to_string(&module);
    assert!(text.contains(" = extract %r0, 0"));
    assert!(text.contains(" = eq.i32 "));
    assert!(text.contains("br_if %r"));
    let parsed = crate::xlil::parse_module(&text).expect("canonical enum match XLIL should parse");
    assert!(crate::xlil::verify_module(&parsed).is_empty());
    assert_eq!(crate::xlil::module_to_string(&parsed), text);
}

#[test]
fn payload_free_pattern_does_not_extract_an_inactive_payload_slot()
{
    let declaration = value_declaration();
    let function = function(
        "empty",
        vec![
            returning(pattern("Value", "Empty", 2, None, None), integer("1")),
            returning(MatchPattern::Else, integer("0")),
        ],
        false,
    );
    let mir = lower(&function, &[declaration]);
    let extracts = mir
        .blocks
        .iter()
        .flat_map(|block| &block.statements)
        .filter(|statement| matches!(statement, mir::Statement::Extract { .. }))
        .count();
    assert_eq!(extracts, 1, "only the tag is extracted for a payload-free pattern");
}
