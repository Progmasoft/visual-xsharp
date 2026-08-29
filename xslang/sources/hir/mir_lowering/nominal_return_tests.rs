/*
 * SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
 * SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.0
 */

use super::*;
use crate::compiler_core::SourceSpan;
use crate::hir::declarations::{Field, NominalKind, NominalType, TypeRef};
use crate::hir::type_check::ObjectField;

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
        start_column: 1,
        end_line: 1,
        end_column: 2,
    }
}

#[test]
fn lowers_nominal_object_return_to_first_class_mir_aggregate()
{
    let point = NominalType {
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
    };
    let literal = |name: &str, value: &str| ObjectField {
        name: name.to_string(),
        value: Expression::Literal {
            literal: Literal::Integer(value.to_string()),
            span: span(),
        },
        span: span(),
    };
    let function = Function {
        name: "make_point".to_string(),
        return_type: Some(Type::Named("Point".to_string())),
        locals: Vec::new(),
        body: vec![Statement::Return {
            value: Some(Expression::Object {
                nominal_type: "Point".to_string(),
                fields: vec![literal("x", "7"), literal("y", "9")],
                span: span(),
            }),
            span: span(),
        }],
    };

    let mir = HirToMirLowerer::new()
        .with_nominal_types(std::slice::from_ref(&point))
        .lower_function(&function)
        .expect("nominal return should lower");

    assert_eq!(mir.return_type, XlilType::aggregate(0));
    assert!(matches!(&mir.blocks[0].statements[2],
                   mir::Statement::Aggregate { value_type, fields, field_types, .. }
                     if *value_type == XlilType::aggregate(0) && fields.len() == 2 &&
                        field_types == &[XlilType::I32, XlilType::I32]));
    assert!(crate::mir::verify::verify_function(&mir).is_empty());
    let lowered = crate::xlil::lowering::MirToXlilLowerer::new()
        .lower_function(&mir)
        .expect("aggregate MIR should lower");
    assert!(matches!(
        lowered.blocks[0].instructions[2],
        crate::xlil::Instruction::Aggregate { .. }
    ));
}
