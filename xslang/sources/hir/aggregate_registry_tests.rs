/*
 * SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
 * SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.0
 */

use super::aggregate_registry;
use super::declarations::{Base, EnumVariant, NominalKind, NominalType, TypeRef, Visibility};
use super::type_check::PrimitiveType;
use crate::compiler_core::SourceSpan;
use crate::xlil::Type;

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

fn variant(name: &str, payload: Option<PrimitiveType>, tag: u32) -> EnumVariant
{
    EnumVariant {
        name: name.to_string(),
        payload: payload.map(TypeRef::Primitive),
        tag,
        span: span(),
    }
}

fn enum_data(name: &str, bases: &[&str], variants: Vec<EnumVariant>) -> NominalType
{
    NominalType {
        name: name.to_string(),
        kind: NominalKind::EnumData,
        bases: bases
            .iter()
            .map(|name| Base {
                ty: TypeRef::Named((*name).to_string()),
                visibility: Visibility::Internal,
                is_virtual: false,
                span: span(),
            })
            .collect(),
        fields: Vec::new(),
        variants,
        span: span(),
    }
}

#[test]
fn enum_data_registers_tag_and_one_slot_per_typed_overload()
{
    let declaration = enum_data("Value", &[], vec![
        variant("Number", Some(PrimitiveType::Int), 0),
        variant("Number", Some(PrimitiveType::Long), 1),
        variant("Empty", None, 2),
    ]);
    let registry = aggregate_registry::build(&[declaration]).expect("valid enum data layout");
    assert_eq!(registry.layouts.len(), 1);
    assert_eq!(registry.layouts[0].name, "Value");
    assert_eq!(registry.layouts[0].fields, [Type::I32, Type::I64, Type::I32]);
    assert_eq!(registry.types["Value"], Type::aggregate(0));
    assert_eq!(registry.enum_data.len(), 1);
    assert_eq!(registry.enum_data[0].value_type, Type::aggregate(0));
    assert_eq!(registry.enum_data[0].variants.len(), 3);
    assert_eq!(registry.enum_data[0].variants[0].field, Some(1));
    assert_eq!(registry.enum_data[0].variants[1].field, Some(2));
    assert_eq!(registry.enum_data[0].variants[2].field, None);
}

#[test]
fn enum_data_layout_preserves_owner_tag_and_payload_identity()
{
    let declaration = enum_data("Value", &[], vec![
        variant("Number", Some(PrimitiveType::Int), 17),
        variant("Number", Some(PrimitiveType::Long), 23),
    ]);
    let registry = aggregate_registry::build(&[declaration]).unwrap();
    let int = &registry.enum_data[0].variants[0];
    let long = &registry.enum_data[0].variants[1];
    assert_eq!(
        (&int.owner, &int.name, int.tag, int.payload_type),
        (&"Value".to_string(), &"Number".to_string(), 0, Some(Type::I64))
    );
    assert_eq!(
        (&long.owner, &long.name, long.tag, long.payload_type),
        (&"Value".to_string(), &"Number".to_string(), 1, Some(Type::I32))
    );
}

#[test]
fn payload_free_enum_data_is_rejected_in_favor_of_a_regular_enum()
{
    let declaration = enum_data("Token", &[], vec![variant("Start", None, 0), variant("End", None, 1)]);
    assert!(aggregate_registry::build(&[declaration]).is_none());
}

#[test]
fn inherited_enum_data_layout_flattens_base_variants_before_derived_ones()
{
    let root = enum_data("Root", &[], vec![variant("RootValue", Some(PrimitiveType::Int), 0)]);
    let leaf = enum_data("Leaf", &["Root"], vec![
        variant("LeafValue", Some(PrimitiveType::Long), 0),
        variant("Empty", None, 1),
    ]);
    let registry = aggregate_registry::build(&[root, leaf]).unwrap();
    let leaf = registry.enum_data.iter().find(|layout| layout.name == "Leaf").unwrap();
    assert_eq!(
        leaf.variants
            .iter()
            .map(|variant| variant.owner.as_str())
            .collect::<Vec<_>>(),
        ["Root", "Leaf", "Leaf"]
    );
    assert_eq!(leaf.variants.iter().map(|variant| variant.tag).collect::<Vec<_>>(), [
        0, 1, 2
    ]);
    assert_eq!(leaf.variants.iter().map(|variant| variant.field).collect::<Vec<_>>(), [
        Some(1),
        Some(2),
        None
    ]);
    let fields = &registry.layouts[leaf.value_type.registry_id as usize].fields;
    assert_eq!(fields, &[Type::I32, Type::I64, Type::I32]);
}

#[test]
fn enum_and_enum_data_receive_distinct_aggregate_registry_ids()
{
    let regular = NominalType {
        name: "Color".to_string(),
        kind: NominalKind::Enum,
        bases: Vec::new(),
        fields: Vec::new(),
        variants: vec![variant("Red", None, 0)],
        span: span(),
    };
    let value = enum_data("Value", &[], vec![variant("Number", Some(PrimitiveType::Long), 0)]);
    let registry = aggregate_registry::build(&[regular, value]).unwrap();
    assert_eq!(registry.types["Color"], Type::aggregate(0));
    assert_eq!(registry.types["Value"], Type::aggregate(1));
    assert_eq!(registry.layouts[0].fields, [Type::I32]);
    assert_eq!(registry.layouts[1].fields, [Type::I32, Type::I32]);
}

#[test]
fn invalid_enum_data_hierarchy_does_not_create_partial_layouts()
{
    let regular = NominalType {
        name: "Color".to_string(),
        kind: NominalKind::Enum,
        bases: Vec::new(),
        fields: Vec::new(),
        variants: vec![variant("Red", None, 0)],
        span: span(),
    };
    let invalid = enum_data("Value", &["Color"], vec![variant("Empty", None, 0)]);
    assert!(aggregate_registry::build(&[regular, invalid]).is_none());
}

#[test]
fn duplicate_exact_overloads_reject_aggregate_registry()
{
    let invalid = enum_data("Value", &[], vec![
        variant("Number", Some(PrimitiveType::Long), 0),
        variant("Number", Some(PrimitiveType::Long), 1),
    ]);
    assert!(aggregate_registry::build(&[invalid]).is_none());
}

#[test]
fn inherited_overloads_keep_distinct_payload_slots()
{
    let root = enum_data("Root", &[], vec![variant("Number", Some(PrimitiveType::Int), 0)]);
    let leaf = enum_data("Leaf", &["Root"], vec![variant("Number", Some(PrimitiveType::Long), 0)]);
    let registry = aggregate_registry::build(&[root, leaf]).unwrap();
    let leaf = registry.enum_data.iter().find(|layout| layout.name == "Leaf").unwrap();
    assert_eq!(leaf.variants.len(), 2);
    assert_eq!(leaf.variants[0].payload_type, Some(Type::I64));
    assert_eq!(leaf.variants[1].payload_type, Some(Type::I32));
    assert_ne!(leaf.variants[0].field, leaf.variants[1].field);
}

#[test]
fn lookup_requires_complete_variant_identity()
{
    let declaration = enum_data("Value", &[], vec![
        variant("Number", Some(PrimitiveType::Int), 0),
        variant("Number", Some(PrimitiveType::Long), 1),
    ]);
    let registry = aggregate_registry::build(&[declaration]).unwrap();
    let lookup = |name: &str| registry.enum_data.iter().find(|layout| layout.name == name);
    assert!(lookup("Value").is_some());
    assert!(lookup("Missing").is_none());
    assert_eq!(
        lookup("Value")
            .and_then(|layout| layout.variant("Value", "Number", 0))
            .and_then(|variant| variant.payload_type),
        Some(Type::I64)
    );
    assert_eq!(
        lookup("Value")
            .and_then(|layout| layout.variant("Value", "Number", 1))
            .and_then(|variant| variant.payload_type),
        Some(Type::I32)
    );
    let layout = lookup("Value").unwrap();
    assert!(layout.variant("Other", "Number", 0).is_none());
    assert!(layout.variant("Value", "Other", 0).is_none());
    assert!(layout.variant("Value", "Number", 9).is_none());
}

#[test]
fn multiple_enum_data_types_keep_independent_tag_sequences()
{
    let first = enum_data("First", &[], vec![
        variant("A", Some(PrimitiveType::Long), 99),
        variant("B", None, 100),
    ]);
    let second = enum_data("Second", &[], vec![
        variant("C", Some(PrimitiveType::Int), 44),
        variant("D", Some(PrimitiveType::Long), 45),
    ]);
    let registry = aggregate_registry::build(&[first, second]).unwrap();
    assert_eq!(
        registry
            .enum_data
            .iter()
            .find(|layout| layout.name == "First")
            .unwrap()
            .variants
            .iter()
            .map(|variant| variant.tag)
            .collect::<Vec<_>>(),
        [0, 1]
    );
    assert_eq!(
        registry
            .enum_data
            .iter()
            .find(|layout| layout.name == "Second")
            .unwrap()
            .variants
            .iter()
            .map(|variant| variant.tag)
            .collect::<Vec<_>>(),
        [0, 1]
    );
}

#[test]
fn enum_data_registry_ids_follow_preceding_data_and_enum_layouts()
{
    use super::declarations::Field;
    let point = NominalType {
        name: "Point".to_string(),
        kind: NominalKind::Data,
        bases: Vec::new(),
        fields: vec![Field {
            name: "x".to_string(),
            ty: TypeRef::Primitive(PrimitiveType::Long),
            mutable: false,
            span: span(),
        }],
        variants: Vec::new(),
        span: span(),
    };
    let color = NominalType {
        name: "Color".to_string(),
        kind: NominalKind::Enum,
        bases: Vec::new(),
        fields: Vec::new(),
        variants: vec![variant("Red", None, 0)],
        span: span(),
    };
    let value = enum_data("Value", &[], vec![variant("Number", Some(PrimitiveType::Long), 0)]);
    let registry = aggregate_registry::build(&[value, color, point]).unwrap();
    assert_eq!(registry.types["Point"], Type::aggregate(0));
    assert_eq!(registry.types["Color"], Type::aggregate(1));
    assert_eq!(registry.types["Value"], Type::aggregate(2));
    assert_eq!(
        registry
            .enum_data
            .iter()
            .find(|layout| layout.name == "Value")
            .unwrap()
            .value_type,
        Type::aggregate(2)
    );
}
