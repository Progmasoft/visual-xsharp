/*
 * SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
 * SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.0
 */

use super::*;
use crate::compiler_core::SourceSpan;
use crate::hir::declarations::{Base, EnumVariant, Field, NominalKind, NominalType, TypeRef, Visibility};
use crate::hir::type_check::{Local, ObjectField};

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

fn enum_data(name: &str, bases: &[&str], variants: Vec<EnumVariant>) -> NominalType
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

fn integer(value: &str) -> Expression
{
    Expression::Literal {
        literal: Literal::Integer(value.to_string()),
        span: span(),
    }
}

fn constructor(
    enum_type: &str,
    owner: &str,
    variant: &str,
    tag: u32,
    payload: Option<Expression>,
    payload_type: Option<Type>,
) -> Expression
{
    Expression::EnumData {
        enum_type: enum_type.to_string(),
        owner: owner.to_string(),
        variant: variant.to_string(),
        tag,
        payload: payload.map(Box::new),
        payload_type: payload_type.map(Box::new),
        span: span(),
    }
}

fn returning(name: &str, value: Expression) -> Function
{
    Function {
        name: name.to_string(),
        return_type: Some(Type::Named("Value".to_string())),
        locals: Vec::new(),
        body: vec![Statement::Return {
            value: Some(value),
            span: span(),
        }],
    }
}

fn value_declaration() -> NominalType
{
    enum_data("Value", &[], vec![
        variant("Number", Some(PrimitiveType::Int), 0),
        variant("Number", Some(PrimitiveType::Long), 1),
        variant("Empty", None, 2),
    ])
}

fn point_declaration() -> NominalType
{
    NominalType {
        name: "Point".to_string(),
        kind: NominalKind::Data,
        bases: Vec::new(),
        fields: vec![
            Field {
                name: "x".to_string(),
                ty: TypeRef::Primitive(PrimitiveType::Long),
                mutable: false,
                span: source_span(),
            },
            Field {
                name: "y".to_string(),
                ty: TypeRef::Primitive(PrimitiveType::Long),
                mutable: false,
                span: source_span(),
            },
        ],
        variants: Vec::new(),
        span: source_span(),
    }
}

fn point_value(x: &str, y: &str) -> Expression
{
    Expression::Object {
        nominal_type: "Point".to_string(),
        fields: vec![
            ObjectField {
                name: "x".to_string(),
                value: integer(x),
                span: span(),
            },
            ObjectField {
                name: "y".to_string(),
                value: integer(y),
                span: span(),
            },
        ],
        span: span(),
    }
}

#[test]
fn lowers_int_payload_with_tag_and_inactive_long_zero()
{
    let declaration = value_declaration();
    let function = returning(
        "make_int",
        constructor(
            "Value",
            "Value",
            "Number",
            0,
            Some(integer("7")),
            Some(Type::Primitive(PrimitiveType::Int)),
        ),
    );
    let lowered = HirToMirLowerer::new()
        .with_nominal_types(std::slice::from_ref(&declaration))
        .lower_function(&function)
        .expect("typed enum data constructor should lower");
    assert_eq!(lowered.return_type, XlilType::aggregate(0));
    assert!(matches!(lowered.blocks[0].statements[0], mir::Statement::ConstI32 {
        value: 0,
        ..
    }));
    assert!(matches!(lowered.blocks[0].statements[1], mir::Statement::ConstI64 {
        value: 7,
        ..
    }));
    assert!(matches!(
        lowered.blocks[0].statements[2],
        mir::Statement::ConstInteger {
            value: mir::IntegerConstant::I32(0),
            ..
        }
    ));
    assert!(matches!(&lowered.blocks[0].statements[3],
                   mir::Statement::Aggregate { value_type, field_types, fields, .. }
                     if *value_type == XlilType::aggregate(0) &&
                        field_types == &[XlilType::I32, XlilType::I64, XlilType::I32] && fields.len() == 3));
    assert!(crate::mir::verify::verify_function(&lowered).is_empty());
}

#[test]
fn lowers_long_overload_into_its_distinct_payload_slot()
{
    let declaration = value_declaration();
    let function = returning(
        "make_long",
        constructor(
            "Value",
            "Value",
            "Number",
            1,
            Some(integer("7")),
            Some(Type::Primitive(PrimitiveType::Long)),
        ),
    );
    let lowered = HirToMirLowerer::new()
        .with_nominal_types(std::slice::from_ref(&declaration))
        .lower_function(&function)
        .unwrap();
    assert!(matches!(lowered.blocks[0].statements[0], mir::Statement::ConstI32 {
        value: 1,
        ..
    }));
    assert!(matches!(
        lowered.blocks[0].statements[1],
        mir::Statement::ConstInteger {
            value: mir::IntegerConstant::I64(0),
            ..
        }
    ));
    assert!(matches!(lowered.blocks[0].statements[2], mir::Statement::ConstI32 {
        value: 7,
        ..
    }));
    assert!(crate::mir::verify::verify_function(&lowered).is_empty());
}

#[test]
fn lowers_payload_free_variant_with_zeroed_payload_storage()
{
    let declaration = value_declaration();
    let function = returning("make_empty", constructor("Value", "Value", "Empty", 2, None, None));
    let lowered = HirToMirLowerer::new()
        .with_nominal_types(std::slice::from_ref(&declaration))
        .lower_function(&function)
        .unwrap();
    assert!(matches!(lowered.blocks[0].statements[0], mir::Statement::ConstI32 {
        value: 2,
        ..
    }));
    assert!(matches!(
        lowered.blocks[0].statements[1],
        mir::Statement::ConstInteger {
            value: mir::IntegerConstant::I64(0),
            ..
        }
    ));
    assert!(matches!(
        lowered.blocks[0].statements[2],
        mir::Statement::ConstInteger {
            value: mir::IntegerConstant::I32(0),
            ..
        } | mir::Statement::ConstI32 {
            value: 0,
            ..
        }
    ));
}

#[test]
fn rejects_payload_metadata_that_disagrees_with_selected_overload()
{
    let declaration = value_declaration();
    let function = returning(
        "bad",
        constructor(
            "Value",
            "Value",
            "Number",
            0,
            Some(integer("7")),
            Some(Type::Primitive(PrimitiveType::Long)),
        ),
    );
    let diagnostics = HirToMirLowerer::new()
        .with_nominal_types(std::slice::from_ref(&declaration))
        .lower_function(&function)
        .unwrap_err();
    assert!(
        diagnostics
            .iter()
            .any(|diagnostic| diagnostic.code == DiagnosticCode::UnsupportedType &&
                diagnostic.message.contains("payload"))
    );
}

#[test]
fn rejects_unknown_variant_owner_and_tag_metadata()
{
    let declaration = value_declaration();
    let function = returning(
        "bad",
        constructor(
            "Value",
            "Other",
            "Number",
            99,
            Some(integer("7")),
            Some(Type::Primitive(PrimitiveType::Long)),
        ),
    );
    let diagnostics = HirToMirLowerer::new()
        .with_nominal_types(std::slice::from_ref(&declaration))
        .lower_function(&function)
        .unwrap_err();
    assert!(
        diagnostics
            .iter()
            .any(|diagnostic| diagnostic.code == DiagnosticCode::UnsupportedExpression &&
                diagnostic.message.contains("inconsistent tag"))
    );
}

#[test]
fn lowers_enum_data_mir_through_xlil_registry_and_verifier()
{
    let declaration = value_declaration();
    let registry = crate::hir::aggregate_registry::build(std::slice::from_ref(&declaration)).unwrap();
    let function = returning(
        "make_long",
        constructor(
            "Value",
            "Value",
            "Number",
            1,
            Some(integer("11")),
            Some(Type::Primitive(PrimitiveType::Long)),
        ),
    );
    let mir = HirToMirLowerer::new()
        .with_nominal_types(std::slice::from_ref(&declaration))
        .lower_function(&function)
        .unwrap();
    let xlil = crate::xlil::lowering::MirToXlilLowerer::new()
        .lower_function(&mir)
        .unwrap();
    let mut module = crate::xlil::Module::new("EnumData");
    for layout in &registry.layouts
    {
        assert_eq!(
            module.add_aggregate_type(layout.name.clone(), layout.fields.clone()),
            Some(layout.value_type)
        );
    }
    module.add_function(xlil);
    assert!(crate::xlil::verify_module(&module).is_empty());
    let text = crate::xlil::module_to_string(&module);
    assert!(text.contains(".type %t0 Value : (i32, i64, i32)"));
    assert!(text.contains(":%t0 = aggregate "));
    let parsed = crate::xlil::parse_module(&text).unwrap();
    assert!(crate::xlil::verify_module(&parsed).is_empty());
    assert_eq!(crate::xlil::module_to_string(&parsed), text);
}

#[test]
fn inherited_constructor_uses_flattened_derived_tag_and_payload_slot()
{
    let root = enum_data("Root", &[], vec![variant("Number", Some(PrimitiveType::Int), 0)]);
    let leaf = enum_data("Value", &["Root"], vec![variant(
        "Number",
        Some(PrimitiveType::Long),
        0,
    )]);
    let function = returning(
        "make_root_number",
        constructor(
            "Value",
            "Root",
            "Number",
            0,
            Some(integer("9")),
            Some(Type::Primitive(PrimitiveType::Int)),
        ),
    );
    let mir = HirToMirLowerer::new()
        .with_nominal_types(&[root, leaf])
        .lower_function(&function)
        .unwrap();
    assert!(matches!(mir.blocks[0].statements[0], mir::Statement::ConstI32 {
        value: 0,
        ..
    }));
    assert!(matches!(mir.blocks[0].statements[1], mir::Statement::ConstI64 {
        value: 9,
        ..
    }));
    assert!(crate::mir::verify::verify_function(&mir).is_empty());
}

#[test]
fn enum_data_parameter_remains_one_first_class_aggregate_parameter()
{
    let declaration = value_declaration();
    let function = Function {
        name: "identity".to_string(),
        return_type: Some(Type::Named("Value".to_string())),
        locals: vec![Local {
            name: "value".to_string(),
            ty: Type::Named("Value".to_string()),
            mutable: false,
            span: span(),
        }],
        body: vec![Statement::Return {
            value: Some(Expression::Local {
                name: "value".to_string(),
                span: span(),
            }),
            span: span(),
        }],
    };
    let mir = HirToMirLowerer::new()
        .with_nominal_types(std::slice::from_ref(&declaration))
        .lower_function_with_parameters(&function, 1)
        .unwrap();
    assert_eq!(mir.parameters.len(), 1);
    assert_eq!(mir.parameters[0].value_type, XlilType::aggregate(0));
    assert_eq!(mir.return_type, XlilType::aggregate(0));
    assert!(crate::mir::verify::verify_function(&mir).is_empty());
}

#[test]
fn nominal_data_payload_lowers_as_nested_aggregate_slot()
{
    let point = point_declaration();
    let wrapped = NominalType {
        name: "Value".to_string(),
        kind: NominalKind::EnumData,
        bases: Vec::new(),
        fields: Vec::new(),
        variants: vec![
            EnumVariant {
                name: "Point".to_string(),
                payload: Some(TypeRef::Named("Point".to_string())),
                tag: 0,
                span: source_span(),
            },
            variant("Empty", None, 1),
        ],
        span: source_span(),
    };
    let function = returning(
        "wrap_point",
        constructor(
            "Value",
            "Value",
            "Point",
            0,
            Some(point_value("3", "4")),
            Some(Type::Named("Point".to_string())),
        ),
    );
    let registry = crate::hir::aggregate_registry::build(&[point.clone(), wrapped.clone()]).unwrap();
    assert_eq!(registry.layouts[0].fields, [XlilType::I32, XlilType::I32]);
    assert_eq!(registry.layouts[1].fields, [XlilType::I32, XlilType::aggregate(0)]);
    let mir = HirToMirLowerer::new()
        .with_nominal_types(&[point, wrapped])
        .lower_function(&function)
        .unwrap();
    assert_eq!(mir.return_type, XlilType::aggregate(1));
    assert!(mir.blocks[0].statements.iter().any(|statement| matches!(statement,
                                       mir::Statement::Aggregate { value_type, field_types, .. }
                                         if *value_type == XlilType::aggregate(0) &&
                                            field_types == &[XlilType::I32, XlilType::I32])));
    assert!(mir.blocks[0].statements.iter().any(|statement| matches!(statement,
                                       mir::Statement::Aggregate { value_type, field_types, .. }
                                         if *value_type == XlilType::aggregate(1) &&
                                            field_types == &[XlilType::I32, XlilType::aggregate(0)])));
    assert!(crate::mir::verify::verify_function(&mir).is_empty());
}

#[test]
fn payload_free_variant_recursively_zeros_inactive_nominal_payload()
{
    let point = point_declaration();
    let wrapped = NominalType {
        name: "Value".to_string(),
        kind: NominalKind::EnumData,
        bases: Vec::new(),
        fields: Vec::new(),
        variants: vec![
            EnumVariant {
                name: "Point".to_string(),
                payload: Some(TypeRef::Named("Point".to_string())),
                tag: 0,
                span: source_span(),
            },
            variant("Empty", None, 1),
        ],
        span: source_span(),
    };
    let function = returning("empty", constructor("Value", "Value", "Empty", 1, None, None));
    let mir = HirToMirLowerer::new()
        .with_nominal_types(&[point, wrapped])
        .lower_function(&function)
        .unwrap();
    let zero_count = mir.blocks[0]
        .statements
        .iter()
        .filter(|statement| {
            matches!(
                statement,
                mir::Statement::ConstInteger {
                    value: mir::IntegerConstant::I32(0),
                    ..
                } | mir::Statement::ConstI32 {
                    value: 0,
                    ..
                }
            )
        })
        .count();
    assert!(zero_count >= 2);
    assert!(mir.blocks[0].statements.iter().any(|statement| matches!(statement,
                                       mir::Statement::Aggregate { value_type, .. }
                                         if *value_type == XlilType::aggregate(0))));
    assert!(crate::mir::verify::verify_function(&mir).is_empty());
}

#[test]
fn enum_data_local_binding_stores_and_loads_the_complete_value()
{
    let declaration = value_declaration();
    let value_type = Type::Named("Value".to_string());
    let function = Function {
        name: "local_flow".to_string(),
        return_type: Some(value_type.clone()),
        locals: vec![Local {
            name: "value".to_string(),
            ty: value_type,
            mutable: false,
            span: span(),
        }],
        body: vec![
            Statement::Let {
                local: Local {
                    name: "value".to_string(),
                    ty: Type::Named("Value".to_string()),
                    mutable: false,
                    span: span(),
                },
                initializer: Some(constructor("Value", "Value", "Empty", 2, None, None)),
            },
            Statement::Return {
                value: Some(Expression::Local {
                    name: "value".to_string(),
                    span: span(),
                }),
                span: span(),
            },
        ],
    };
    let mir = HirToMirLowerer::new()
        .with_nominal_types(std::slice::from_ref(&declaration))
        .lower_function(&function)
        .unwrap();
    assert!(
        mir.blocks[0]
            .statements
            .iter()
            .any(|statement| matches!(statement, mir::Statement::StoreLocal { .. }))
    );
    assert!(
        mir.blocks[0]
            .statements
            .iter()
            .any(|statement| matches!(statement, mir::Statement::LoadLocal { .. }))
    );
    assert!(crate::mir::verify::verify_function(&mir).is_empty());
}

#[test]
fn bool_payload_and_inactive_float_slot_use_canonical_values()
{
    let declaration = enum_data("Value", &[], vec![
        variant("Value", Some(PrimitiveType::Bool), 0),
        variant("Value", Some(PrimitiveType::Float), 1),
    ]);
    let function = returning(
        "wrap_bool",
        constructor(
            "Value",
            "Value",
            "Value",
            0,
            Some(Expression::Literal {
                literal: Literal::Bool(true),
                span: span(),
            }),
            Some(Type::Primitive(PrimitiveType::Bool)),
        ),
    );
    let mir = HirToMirLowerer::new()
        .with_nominal_types(std::slice::from_ref(&declaration))
        .lower_function(&function)
        .unwrap();
    assert!(
        mir.blocks[0]
            .statements
            .iter()
            .any(|statement| matches!(statement, mir::Statement::ConstBool {
                value: true,
                ..
            }))
    );
    assert!(
        mir.blocks[0]
            .statements
            .iter()
            .any(|statement| matches!(statement, mir::Statement::ConstF64 {
                bits: 0,
                ..
            }))
    );
    assert!(crate::mir::verify::verify_function(&mir).is_empty());
}

#[test]
fn float_payload_and_inactive_str_slot_lower_without_target_apis()
{
    let declaration = enum_data("Value", &[], vec![
        variant("Value", Some(PrimitiveType::Float), 0),
        variant("Value", Some(PrimitiveType::Str), 1),
    ]);
    let function = returning(
        "wrap_float",
        constructor(
            "Value",
            "Value",
            "Value",
            0,
            Some(Expression::Literal {
                literal: Literal::Float("3.5".to_string()),
                span: span(),
            }),
            Some(Type::Primitive(PrimitiveType::Float)),
        ),
    );
    let mir = HirToMirLowerer::new()
        .with_nominal_types(std::slice::from_ref(&declaration))
        .lower_function(&function)
        .unwrap();
    assert!(
        mir.blocks[0]
            .statements
            .iter()
            .any(|statement| matches!(statement, mir::Statement::ConstF64 { bits, .. }
                                                  if *bits == 3.5_f64.to_bits()))
    );
    assert!(
        mir.blocks[0]
            .statements
            .iter()
            .any(|statement| matches!(statement, mir::Statement::ConstStr { units, .. } if units.is_empty()))
    );
    assert!(crate::mir::verify::verify_function(&mir).is_empty());
}

#[test]
fn str_payload_preserves_utf32_code_points_in_enum_data_slot()
{
    let declaration = enum_data("Value", &[], vec![
        EnumVariant {
            name: "Text".to_string(),
            payload: Some(TypeRef::Reference {
                referent: Box::new(TypeRef::Primitive(PrimitiveType::Str)),
                mutable: false,
            }),
            tag: 0,
            span: source_span(),
        },
        variant("Empty", None, 1),
    ]);
    let function = returning(
        "wrap_text",
        constructor(
            "Value",
            "Value",
            "Text",
            0,
            Some(Expression::Literal {
                literal: Literal::String("Aß".to_string()),
                span: span(),
            }),
            Some(Type::Reference {
                referent: Box::new(Type::Primitive(PrimitiveType::Str)),
                mutable: false,
            }),
        ),
    );
    let mir = HirToMirLowerer::new()
        .with_nominal_types(std::slice::from_ref(&declaration))
        .lower_function(&function)
        .unwrap();
    assert!(
        mir.blocks[0]
            .statements
            .iter()
            .any(|statement| matches!(statement, mir::Statement::ConstStr { units, .. }
                                                  if units == &[0x41, 0xDF]))
    );
    let xlil = crate::xlil::lowering::MirToXlilLowerer::new()
        .lower_function(&mir)
        .unwrap();
    assert!(matches!(&xlil.blocks[0].instructions[1],
                   crate::xlil::Instruction::ConstStr { units, .. } if units == &[0x41, 0xDF]));
}

#[test]
fn enum_data_value_type_remains_target_independent_before_xlil()
{
    let declaration = value_declaration();
    let function = returning("empty", constructor("Value", "Value", "Empty", 2, None, None));
    let mir = HirToMirLowerer::new()
        .with_nominal_types(std::slice::from_ref(&declaration))
        .lower_function(&function)
        .unwrap();
    assert_eq!(mir.return_type.kind, TypeKind::Aggregate);
    assert_eq!(mir.return_type.registry_id, 0);
    assert!(mir.locals.iter().all(|local| local.value_type.is_some()));
    assert!(mir.blocks.iter().all(|block| block.terminator.is_some()));
}

#[test]
fn reports_inactive_f16_payload_until_canonical_constant_support_exists()
{
    let declaration = enum_data("Value", &[], vec![
        variant("Half", Some(PrimitiveType::SFloat), 0),
        variant("Empty", None, 1),
    ]);
    let function = returning("empty", constructor("Value", "Value", "Empty", 1, None, None));
    let diagnostics = HirToMirLowerer::new()
        .with_nominal_types(std::slice::from_ref(&declaration))
        .lower_function(&function)
        .unwrap_err();
    assert!(
        diagnostics
            .iter()
            .any(|diagnostic| diagnostic.code == DiagnosticCode::UnsupportedType &&
                diagnostic.message.contains("canonical zero"))
    );
}

#[test]
fn reports_inactive_f128_payload_until_canonical_constant_support_exists()
{
    let declaration = enum_data("Value", &[], vec![
        variant("Wide", Some(PrimitiveType::Double), 0),
        variant("Empty", None, 1),
    ]);
    let function = returning("empty", constructor("Value", "Value", "Empty", 1, None, None));
    let diagnostics = HirToMirLowerer::new()
        .with_nominal_types(std::slice::from_ref(&declaration))
        .lower_function(&function)
        .unwrap_err();
    assert!(
        diagnostics
            .iter()
            .any(|diagnostic| diagnostic.message.contains("inactive enum data payload"))
    );
}

#[test]
fn reports_owned_string_inactive_payload_until_string_runtime_lowering_exists()
{
    let declaration = enum_data("Value", &[], vec![
        variant("Text", Some(PrimitiveType::String), 0),
        variant("Empty", None, 1),
    ]);
    let function = returning("empty", constructor("Value", "Value", "Empty", 1, None, None));
    let diagnostics = HirToMirLowerer::new()
        .with_nominal_types(std::slice::from_ref(&declaration))
        .lower_function(&function)
        .unwrap_err();
    assert!(
        diagnostics
            .iter()
            .any(|diagnostic| diagnostic.code == DiagnosticCode::UnsupportedType &&
                diagnostic.message.contains("canonical zero"))
    );
}

#[test]
fn rejects_value_when_expected_nominal_type_uses_another_registry_entry()
{
    let first = enum_data("Value", &[], vec![
        variant("Number", Some(PrimitiveType::Long), 0),
        variant("Empty", None, 1),
    ]);
    let second = enum_data("Other", &[], vec![
        variant("Number", Some(PrimitiveType::Long), 0),
        variant("Empty", None, 1),
    ]);
    let function = Function {
        name: "bad".to_string(),
        return_type: Some(Type::Named("Other".to_string())),
        locals: Vec::new(),
        body: vec![Statement::Return {
            value: Some(constructor("Value", "Value", "Empty", 1, None, None)),
            span: span(),
        }],
    };
    let diagnostics = HirToMirLowerer::new()
        .with_nominal_types(&[first, second])
        .lower_function(&function)
        .unwrap_err();
    assert!(
        diagnostics
            .iter()
            .any(|diagnostic| diagnostic.code == DiagnosticCode::UnsupportedType)
    );
}
