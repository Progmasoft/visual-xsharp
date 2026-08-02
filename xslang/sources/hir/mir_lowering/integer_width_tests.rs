/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 */

use crate::hir::text::{function_to_xhir, parse_xhir_function};
use crate::hir::{Expression, Function, Literal, PrimitiveType, Span, Statement, Type, UnaryOperator};
use crate::mir::text::{function_to_xmir, parse_xmir_function};
use crate::xlil::module_to_string;
use crate::xlil::{Module, lowering::MirToXlilLowerer};

use super::HirToMirLowerer;

fn integer_function(name: &str, primitive: PrimitiveType, literal: &str, negative: bool) -> Function
{
    let span = Span::new(
        1,
        0,
        u32::try_from(literal.len()).expect("test literal length fits u32"),
    );
    let literal = Expression::Literal {
        literal: Literal::Integer(literal.to_string()),
        span,
    };
    let value = if negative
    {
        Expression::Unary {
            operator: UnaryOperator::Negative,
            operand: Box::new(literal),
            span,
        }
    }
    else
    {
        literal
    };
    Function {
        name: name.to_string(),
        return_type: Some(Type::Primitive(primitive)),
        locals: vec![],
        body: vec![Statement::Return {
            value: Some(value),
            span,
        }],
    }
}

#[test]
fn lowers_every_fixed_integer_width_to_canonical_xlil_bits()
{
    let cases = [
        ("byte_max", PrimitiveType::Byte, "255", false, "const.u8 0xff"),
        ("sbyte_min", PrimitiveType::SByte, "128", true, "const.i8 0x80"),
        ("short_min", PrimitiveType::Short, "32768", true, "const.i16 0x8000"),
        ("ushort_max", PrimitiveType::UShort, "65535", false, "const.u16 0xffff"),
        (
            "ulong_max",
            PrimitiveType::ULong,
            "4294967295",
            false,
            "const.u32 0xffffffff",
        ),
        (
            "uint_max",
            PrimitiveType::UInt,
            "18446744073709551615",
            false,
            "const.u64 0xffffffffffffffff",
        ),
        (
            "integer_min",
            PrimitiveType::Integer,
            "170141183460469231731687303715884105728",
            true,
            "const.i128 0x80000000000000000000000000000000",
        ),
        (
            "uinteger_max",
            PrimitiveType::UInteger,
            "340282366920938463463374607431768211455",
            false,
            "const.u128 0xffffffffffffffffffffffffffffffff",
        ),
    ];
    let mut module = Module::new("IntegerWidths");
    for (name, primitive, literal, negative, expected) in cases
    {
        let hir = integer_function(name, primitive, literal, negative);
        let xhir = function_to_xhir(&hir);
        let hir = parse_xhir_function(&xhir).expect("integer-width XHIR should round-trip");
        let mir = HirToMirLowerer::new()
            .lower_function(&hir)
            .expect("integer width should lower to MIR");
        assert!(crate::mir::verify::verify_function(&mir).is_empty());
        let xmir = function_to_xmir(&mir);
        let mir = parse_xmir_function(&xmir).expect("integer-width XMIR should round-trip");
        assert!(crate::mir::verify::verify_function(&mir).is_empty());
        let xlil = MirToXlilLowerer::new()
            .lower_function(&mir)
            .expect("integer width should lower to XLIL");
        module.functions.push(xlil);
        assert!(module_to_string(&module).contains(expected));
    }
}

#[test]
fn rejects_integer_literals_outside_their_declared_width()
{
    for function in [
        integer_function("bad_byte", PrimitiveType::Byte, "256", false),
        integer_function("bad_sbyte", PrimitiveType::SByte, "129", true),
        integer_function(
            "bad_uinteger",
            PrimitiveType::UInteger,
            "340282366920938463463374607431768211456",
            false,
        ),
    ]
    {
        assert!(HirToMirLowerer::new().lower_function(&function).is_err());
    }
}
