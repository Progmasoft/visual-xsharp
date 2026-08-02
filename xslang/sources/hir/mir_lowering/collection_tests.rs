/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 */

use super::*;
use crate::hir::collection_registry::{ArrayLayout, CollectionRegistry};

const fn span() -> Span
{
    Span::new(1, 0, 1)
}

fn integer(value: &str) -> Expression
{
    Expression::Literal {
        literal: Literal::Integer(value.to_string()),
        span: span(),
    }
}

#[test]
fn lowers_fixed_array_literal_and_index_through_xlil()
{
    let source_type = Type::Array {
        element: Box::new(Type::Primitive(PrimitiveType::Long)),
        length: Some(3),
    };
    let registry = CollectionRegistry {
        arrays: vec![ArrayLayout {
            source_type: source_type.clone(),
            value_type: XlilType::array(0),
            element_type: XlilType::I32,
            length: Some(3),
        }],
    };
    let function = Function {
        name: "main".to_string(),
        return_type: Some(Type::Primitive(PrimitiveType::Long)),
        locals: vec![],
        body: vec![
            Statement::Let {
                local: crate::hir::type_check::Local {
                    name: "values".to_string(),
                    ty: source_type.clone(),
                    mutable: false,
                    span: span(),
                },
                initializer: Some(Expression::Array {
                    elements: vec![integer("2"), integer("3"), integer("4")],
                    span: span(),
                }),
            },
            Statement::Return {
                value: Some(Expression::Index {
                    collection: Box::new(Expression::Local {
                        name: "values".to_string(),
                        span: span(),
                    }),
                    index: Box::new(integer("1")),
                    element_type: Box::new(Type::Primitive(PrimitiveType::Long)),
                    span: span(),
                }),
                span: span(),
            },
        ],
    };

    assert!(
        crate::hir::type_check::TypeChecker::new()
            .check_function(&function)
            .is_empty()
    );
    let mir = HirToMirLowerer::new()
        .with_collection_types(&registry)
        .lower_function(&function)
        .expect("fixed array HIR should lower");
    assert!(crate::mir::verify::verify_function(&mir).is_empty());
    let xlil = crate::xlil::lowering::MirToXlilLowerer::new()
        .lower_function(&mir)
        .expect("fixed array MIR should lower");
    let mut module = crate::xlil::Module::new("Arrays");
    assert_eq!(module.add_array_type(XlilType::I32, 3), Some(XlilType::array(0)));
    module.add_function(xlil);
    assert!(crate::xlil::verify_module(&module).is_empty());
    let text = crate::xlil::module_to_string(&module);
    assert!(text.contains("%r3:%a0 = array"));
    assert!(text.contains("extract.array"));
    assert_eq!(
        crate::xlil::parse_module(&text).expect("fixed array XLIL round-trip"),
        module
    );
}

#[test]
fn lowers_dynamic_array_get_and_set_through_xlil()
{
    let source_type = Type::Array {
        element: Box::new(Type::Primitive(PrimitiveType::Long)),
        length: Some(3),
    };
    let registry = CollectionRegistry {
        arrays: vec![ArrayLayout {
            source_type: source_type.clone(),
            value_type: XlilType::array(0),
            element_type: XlilType::I32,
            length: Some(3),
        }],
    };
    let values = crate::hir::type_check::Local {
        name: "values".to_string(),
        ty: source_type.clone(),
        mutable: true,
        span: span(),
    };
    let index = crate::hir::type_check::Local {
        name: "index".to_string(),
        ty: Type::Primitive(PrimitiveType::Int),
        mutable: false,
        span: span(),
    };
    let index_local = || Expression::Local {
        name: "index".to_string(),
        span: span(),
    };
    let function = Function {
        name: "main".to_string(),
        return_type: Some(Type::Primitive(PrimitiveType::Long)),
        locals: vec![],
        body: vec![
            Statement::Let {
                local: values,
                initializer: Some(Expression::Array {
                    elements: vec![integer("2"), integer("3"), integer("4")],
                    span: span(),
                }),
            },
            Statement::Let {
                local: index,
                initializer: Some(integer("1")),
            },
            Statement::AssignIndex {
                target: "values".to_string(),
                index: index_local(),
                value: integer("7"),
                element_type: Type::Primitive(PrimitiveType::Long),
                span: span(),
            },
            Statement::Return {
                value: Some(Expression::Index {
                    collection: Box::new(Expression::Local {
                        name: "values".to_string(),
                        span: span(),
                    }),
                    index: Box::new(index_local()),
                    element_type: Box::new(Type::Primitive(PrimitiveType::Long)),
                    span: span(),
                }),
                span: span(),
            },
        ],
    };

    assert!(
        crate::hir::type_check::TypeChecker::new()
            .check_function(&function)
            .is_empty()
    );
    let mir = HirToMirLowerer::new()
        .with_collection_types(&registry)
        .lower_function(&function)
        .expect("dynamic array access should lower");
    assert!(
        mir.blocks[0]
            .statements
            .iter()
            .any(|statement| matches!(statement, mir::Statement::ArraySet { .. }))
    );
    assert!(
        mir.blocks[0]
            .statements
            .iter()
            .any(|statement| matches!(statement, mir::Statement::ArrayGet { .. }))
    );
    let xlil = crate::xlil::lowering::MirToXlilLowerer::new()
        .lower_function(&mir)
        .expect("dynamic array MIR should lower");
    let mut module = crate::xlil::Module::new("Arrays");
    assert_eq!(module.add_array_type(XlilType::I32, 3), Some(XlilType::array(0)));
    module.add_function(xlil);
    assert!(crate::xlil::verify_module(&module).is_empty());
    let text = crate::xlil::module_to_string(&module);
    assert!(text.contains("array.set"));
    assert!(text.contains("array.get"));
    assert_eq!(
        crate::xlil::parse_module(&text).expect("dynamic array XLIL round-trip"),
        module
    );
}
