/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 */

use crate::compiler_core::SourceSpan;
use crate::hir::declarations::{Base, EnumVariant, NominalKind, NominalType, TypeRef, Visibility};
use crate::hir::type_check::{PrimitiveType, Type};

use super::*;

fn span() -> SourceSpan
{
    SourceSpan {
        file_id: 0,
        start_offset: 0,
        end_offset: 1,
        start_line: 1,
        start_column: 0,
        end_line: 1,
        end_column: 1,
    }
}

fn primitive(value: PrimitiveType) -> TypeRef
{
    TypeRef::Primitive(value)
}

fn variant(name: &str, payload: Option<TypeRef>, tag: u32) -> EnumVariant
{
    EnumVariant {
        name: name.to_string(),
        payload,
        tag,
        span: span(),
    }
}

fn base(name: &str) -> Base
{
    Base {
        ty: TypeRef::Named(name.to_string()),
        visibility: Visibility::Internal,
        is_virtual: false,
        span: span(),
    }
}

fn invalid_base(ty: TypeRef) -> Base
{
    Base {
        ty,
        visibility: Visibility::Internal,
        is_virtual: false,
        span: span(),
    }
}

fn enum_data(name: &str, bases: &[&str], variants: Vec<EnumVariant>) -> NominalType
{
    NominalType {
        name: name.to_string(),
        kind: NominalKind::EnumData,
        bases: bases.iter().map(|name| base(name)).collect(),
        fields: Vec::new(),
        variants,
        span: span(),
    }
}

fn nominal(name: &str, kind: NominalKind) -> NominalType
{
    NominalType {
        name: name.to_string(),
        kind,
        bases: Vec::new(),
        fields: Vec::new(),
        variants: Vec::new(),
        span: span(),
    }
}

fn number_variants() -> Vec<EnumVariant>
{
    vec![
        variant("Number", Some(primitive(PrimitiveType::Int)), 0),
        variant("Number", Some(primitive(PrimitiveType::Long)), 1),
        variant("Text", Some(primitive(PrimitiveType::String)), 2),
        variant("Empty", None, 3),
    ]
}

#[test]
fn builds_direct_overload_sets_in_declaration_order()
{
    let registry = EnumDataRegistry::build(&[enum_data("Value", &[], number_variants())]);
    assert!(registry.is_valid());
    assert!(registry.diagnostics().is_empty());
    let variants = registry.variants("Value").unwrap();
    assert_eq!(variants.len(), 4);
    assert_eq!(variants.iter().map(|value| value.tag).collect::<Vec<_>>(), [0, 1, 2, 3]);
    assert!(variants.iter().all(|value| !value.inherited));
}

#[test]
fn selects_overload_by_exact_primitive_payload()
{
    let registry = EnumDataRegistry::build(&[enum_data("Value", &[], number_variants())]);
    let int = registry
        .select("Value", "Number", Some(&Type::Primitive(PrimitiveType::Int)))
        .unwrap();
    let long = registry
        .select("Value", "Number", Some(&Type::Primitive(PrimitiveType::Long)))
        .unwrap();
    assert_eq!(int.tag, 0);
    assert_eq!(long.tag, 1);
    assert_eq!(int.payload, Some(primitive(PrimitiveType::Int)));
    assert_eq!(long.payload, Some(primitive(PrimitiveType::Long)));
}

#[test]
fn selects_payload_free_variant_without_argument()
{
    let registry = EnumDataRegistry::build(&[enum_data("Value", &[], number_variants())]);
    let selected = registry.select("Value", "Empty", None).unwrap();
    assert_eq!(selected.tag, 3);
    assert_eq!(selected.payload, None);
}

#[test]
fn reports_payload_required_for_typed_variant()
{
    let registry = EnumDataRegistry::build(&[enum_data("Value", &[], number_variants())]);
    assert_eq!(
        registry.select("Value", "Number", None),
        Err(VariantSelectionError::PayloadRequired {
            type_name: "Value".to_string(),
            variant: "Number".to_string()
        })
    );
}

#[test]
fn reports_unexpected_payload_for_payload_free_variant()
{
    let registry = EnumDataRegistry::build(&[enum_data("Value", &[], number_variants())]);
    assert_eq!(
        registry.select("Value", "Empty", Some(&Type::Primitive(PrimitiveType::Int))),
        Err(VariantSelectionError::UnexpectedPayload {
            type_name: "Value".to_string(),
            variant: "Empty".to_string()
        })
    );
}

#[test]
fn reports_candidate_types_when_no_payload_matches()
{
    let registry = EnumDataRegistry::build(&[enum_data("Value", &[], number_variants())]);
    assert_eq!(
        registry.select("Value", "Number", Some(&Type::Primitive(PrimitiveType::Integer))),
        Err(VariantSelectionError::NoMatchingPayload {
            type_name: "Value".to_string(),
            variant: "Number".to_string(),
            actual: Type::Primitive(PrimitiveType::Integer),
            candidates: vec![primitive(PrimitiveType::Int), primitive(PrimitiveType::Long)]
        })
    );
}

#[test]
fn reports_unknown_type_and_regular_enum()
{
    let declarations = [nominal("Color", NominalKind::Enum)];
    let registry = EnumDataRegistry::build(&declarations);
    assert_eq!(
        registry.variants("Missing"),
        Err(VariantSelectionError::UnknownType("Missing".to_string()))
    );
    assert_eq!(
        registry.variants("Color"),
        Err(VariantSelectionError::NotEnumData("Color".to_string()))
    );
}

#[test]
fn reports_unknown_variant_name()
{
    let registry = EnumDataRegistry::build(&[enum_data("Value", &[], number_variants())]);
    assert_eq!(
        registry.select("Value", "Missing", None),
        Err(VariantSelectionError::UnknownVariant {
            type_name: "Value".to_string(),
            variant: "Missing".to_string()
        })
    );
}

#[test]
fn exposes_one_named_overload_set()
{
    let registry = EnumDataRegistry::build(&[enum_data("Value", &[], number_variants())]);
    let overloads = registry.overloads("Value", "Number").unwrap();
    assert_eq!(overloads.len(), 2);
    assert_eq!(overloads[0].payload, Some(primitive(PrimitiveType::Int)));
    assert_eq!(overloads[1].payload, Some(primitive(PrimitiveType::Long)));
}

#[test]
fn flattens_transitive_bases_before_derived_variants()
{
    let declarations = [
        enum_data("Root", &[], vec![variant(
            "RootValue",
            Some(primitive(PrimitiveType::Int)),
            0,
        )]),
        enum_data("Middle", &["Root"], vec![variant(
            "MiddleValue",
            Some(primitive(PrimitiveType::Long)),
            0,
        )]),
        enum_data("Leaf", &["Middle"], vec![variant(
            "LeafValue",
            Some(primitive(PrimitiveType::Integer)),
            0,
        )]),
    ];
    let registry = EnumDataRegistry::build(&declarations);
    let variants = registry.variants("Leaf").unwrap();
    assert_eq!(variants.iter().map(|value| value.name.as_str()).collect::<Vec<_>>(), [
        "RootValue",
        "MiddleValue",
        "LeafValue"
    ]);
    assert_eq!(variants.iter().map(|value| value.tag).collect::<Vec<_>>(), [0, 1, 2]);
    assert!(variants[0].inherited);
    assert!(variants[1].inherited);
    assert!(!variants[2].inherited);
}

#[test]
fn supports_overloads_across_base_and_derived_declarations()
{
    let declarations = [
        enum_data("Base", &[], vec![variant(
            "Number",
            Some(primitive(PrimitiveType::Int)),
            0,
        )]),
        enum_data("Derived", &["Base"], vec![variant(
            "Number",
            Some(primitive(PrimitiveType::Long)),
            0,
        )]),
    ];
    let registry = EnumDataRegistry::build(&declarations);
    let overloads = registry.overloads("Derived", "Number").unwrap();
    assert_eq!(overloads.len(), 2);
    assert_eq!(overloads[0].owner, "Base");
    assert_eq!(overloads[1].owner, "Derived");
    assert!(overloads[0].inherited);
    assert!(!overloads[1].inherited);
}

#[test]
fn preserves_base_order_under_multiple_inheritance()
{
    let declarations = [
        enum_data("Left", &[], vec![variant(
            "LeftValue",
            Some(primitive(PrimitiveType::Int)),
            0,
        )]),
        enum_data("Right", &[], vec![variant(
            "RightValue",
            Some(primitive(PrimitiveType::Long)),
            0,
        )]),
        enum_data("Combined", &["Right", "Left"], vec![variant(
            "Own",
            Some(primitive(PrimitiveType::Integer)),
            0,
        )]),
    ];
    let registry = EnumDataRegistry::build(&declarations);
    let variants = registry.variants("Combined").unwrap();
    assert_eq!(variants.iter().map(|value| value.owner.as_str()).collect::<Vec<_>>(), [
        "Right", "Left", "Combined"
    ]);
}

#[test]
fn deduplicates_a_shared_ancestor_in_a_diamond()
{
    let declarations = [
        enum_data("Root", &[], vec![variant(
            "RootValue",
            Some(primitive(PrimitiveType::Int)),
            0,
        )]),
        enum_data("Left", &["Root"], vec![variant(
            "LeftValue",
            Some(primitive(PrimitiveType::Long)),
            0,
        )]),
        enum_data("Right", &["Root"], vec![variant(
            "RightValue",
            Some(primitive(PrimitiveType::Integer)),
            0,
        )]),
        enum_data("Diamond", &["Left", "Right"], vec![variant(
            "Own",
            Some(primitive(PrimitiveType::Short)),
            0,
        )]),
    ];
    let registry = EnumDataRegistry::build(&declarations);
    let variants = registry.variants("Diamond").unwrap();
    assert_eq!(variants.iter().filter(|value| value.owner == "Root").count(), 1);
    assert_eq!(variants.iter().map(|value| value.name.as_str()).collect::<Vec<_>>(), [
        "RootValue",
        "LeftValue",
        "RightValue",
        "Own"
    ]);
}

#[test]
fn rejects_duplicate_typed_payload_in_one_declaration()
{
    let declaration = enum_data("Value", &[], vec![
        variant("Number", Some(primitive(PrimitiveType::Int)), 0),
        variant("Number", Some(primitive(PrimitiveType::Int)), 1),
    ]);
    let registry = EnumDataRegistry::build(&[declaration]);
    assert_eq!(registry.diagnostics(), [EnumDataDiagnostic {
        type_name: "Value".to_string(),
        error: EnumDataError::DuplicatePayload {
            name: "Number".to_string(),
            payload: primitive(PrimitiveType::Int)
        }
    }]);
}

#[test]
fn rejects_duplicate_typed_payload_from_distinct_bases()
{
    let declarations = [
        enum_data("Left", &[], vec![variant(
            "Number",
            Some(primitive(PrimitiveType::Int)),
            0,
        )]),
        enum_data("Right", &[], vec![variant(
            "Number",
            Some(primitive(PrimitiveType::Int)),
            0,
        )]),
        enum_data("Combined", &["Left", "Right"], vec![variant(
            "Own",
            Some(primitive(PrimitiveType::Long)),
            0,
        )]),
    ];
    let registry = EnumDataRegistry::build(&declarations);
    assert!(registry.diagnostics().iter().any(|diagnostic| {
        diagnostic.type_name == "Combined" && matches!(diagnostic.error, EnumDataError::DuplicatePayload { .. })
    }));
}

#[test]
fn rejects_repeated_payload_free_variant()
{
    let declaration = enum_data("Value", &[], vec![
        variant("Some", Some(primitive(PrimitiveType::Int)), 0),
        variant("None", None, 1),
        variant("None", None, 2),
    ]);
    let registry = EnumDataRegistry::build(&[declaration]);
    assert_eq!(
        registry.diagnostics()[0].error,
        EnumDataError::DuplicateUntypedVariant("None".to_string())
    );
}

#[test]
fn rejects_payload_free_and_typed_overload_mix()
{
    let declaration = enum_data("Value", &[], vec![
        variant("Item", None, 0),
        variant("Item", Some(primitive(PrimitiveType::Int)), 1),
    ]);
    let registry = EnumDataRegistry::build(&[declaration]);
    assert_eq!(
        registry.diagnostics()[0].error,
        EnumDataError::UntypedVariantOverload("Item".to_string())
    );
}

#[test]
fn rejects_enum_data_without_a_direct_typed_variant()
{
    let declaration = enum_data("EmptyLike", &[], vec![variant("None", None, 0)]);
    let registry = EnumDataRegistry::build(&[declaration]);
    assert_eq!(registry.diagnostics()[0], EnumDataDiagnostic {
        type_name: "EmptyLike".to_string(),
        error: EnumDataError::MissingTypedVariant
    });
}

#[test]
fn rejects_unknown_base()
{
    let declaration = enum_data("Value", &["Missing"], vec![variant(
        "Own",
        Some(primitive(PrimitiveType::Int)),
        0,
    )]);
    let registry = EnumDataRegistry::build(&[declaration]);
    assert_eq!(
        registry.diagnostics()[0].error,
        EnumDataError::UnknownBase("Missing".to_string())
    );
}

#[test]
fn rejects_regular_enum_base()
{
    let declarations = [
        nominal("Color", NominalKind::Enum),
        enum_data("Value", &["Color"], vec![variant(
            "Own",
            Some(primitive(PrimitiveType::Int)),
            0,
        )]),
    ];
    let registry = EnumDataRegistry::build(&declarations);
    assert_eq!(
        registry.diagnostics()[0].error,
        EnumDataError::InvalidBaseCategory("Color".to_string())
    );
}

#[test]
fn rejects_non_nominal_base()
{
    let mut declaration = enum_data("Value", &[], vec![variant(
        "Own",
        Some(primitive(PrimitiveType::Int)),
        0,
    )]);
    declaration.bases.push(invalid_base(primitive(PrimitiveType::Int)));
    let registry = EnumDataRegistry::build(&[declaration]);
    assert_eq!(registry.diagnostics()[0].error, EnumDataError::InvalidBaseType);
}

#[test]
fn rejects_direct_inheritance_cycle()
{
    let declarations = [
        enum_data("Left", &["Right"], vec![variant(
            "LeftValue",
            Some(primitive(PrimitiveType::Int)),
            0,
        )]),
        enum_data("Right", &["Left"], vec![variant(
            "RightValue",
            Some(primitive(PrimitiveType::Long)),
            0,
        )]),
    ];
    let registry = EnumDataRegistry::build(&declarations);
    assert!(
        registry
            .diagnostics()
            .iter()
            .any(|diagnostic| { matches!(diagnostic.error, EnumDataError::InheritanceCycle(_)) })
    );
}

#[test]
fn selects_nominal_payload_by_nominal_identity()
{
    let declaration = enum_data("Event", &[], vec![
        variant("Data", Some(TypeRef::Named("User".to_string())), 0),
        variant("Data", Some(TypeRef::Named("Message".to_string())), 1),
    ]);
    let registry = EnumDataRegistry::build(&[declaration]);
    let user = registry
        .select("Event", "Data", Some(&Type::Named("User".to_string())))
        .unwrap();
    let message = registry
        .select("Event", "Data", Some(&Type::Named("Message".to_string())))
        .unwrap();
    assert_eq!(user.tag, 0);
    assert_eq!(message.tag, 1);
}

#[test]
fn nominal_payload_selection_does_not_use_structural_compatibility()
{
    let declaration = enum_data("Event", &[], vec![variant(
        "Data",
        Some(TypeRef::Named("User".to_string())),
        0,
    )]);
    let registry = EnumDataRegistry::build(&[declaration]);
    assert!(matches!(
        registry.select("Event", "Data", Some(&Type::Named("OtherUser".to_string()))),
        Err(VariantSelectionError::NoMatchingPayload { .. })
    ));
}

#[test]
fn supports_optional_and_result_payload_identity()
{
    let optional = TypeRef::Optional {
        element: Box::new(primitive(PrimitiveType::Int)),
    };
    let result = TypeRef::Result {
        success: Box::new(primitive(PrimitiveType::Int)),
        error: Box::new(TypeRef::Named("Error".to_string())),
    };
    let declaration = enum_data("Container", &[], vec![
        variant("Value", Some(optional.clone()), 0),
        variant("Value", Some(result.clone()), 1),
    ]);
    let registry = EnumDataRegistry::build(&[declaration]);
    let optional_type = Type::Optional {
        element: Box::new(Type::Primitive(PrimitiveType::Int)),
    };
    let result_type = Type::Result {
        success: Box::new(Type::Primitive(PrimitiveType::Int)),
        error: Box::new(Type::Named("Error".to_string())),
    };
    assert_eq!(
        registry
            .select("Container", "Value", Some(&optional_type))
            .unwrap()
            .payload,
        Some(optional)
    );
    assert_eq!(
        registry
            .select("Container", "Value", Some(&result_type))
            .unwrap()
            .payload,
        Some(result)
    );
}
