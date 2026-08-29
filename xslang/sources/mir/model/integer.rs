/*
 * SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
 * SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.0
 */

use crate::xlil::Type;

#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub enum IntegerConstant
{
    U8(u8),
    I8(i8),
    U16(u16),
    I16(i16),
    U32(u32),
    I32(i32),
    U64(u64),
    I64(i64),
    U128(u128),
    I128(i128),
}

impl IntegerConstant
{
    #[must_use]
    pub const fn value_type(self) -> Type
    {
        match self
        {
            Self::U8(_) => Type::U8,
            Self::I8(_) => Type::I8,
            Self::U16(_) => Type::U16,
            Self::I16(_) => Type::I16,
            Self::U32(_) => Type::U32,
            Self::I32(_) => Type::I32,
            Self::U64(_) => Type::U64,
            Self::I64(_) => Type::I64,
            Self::U128(_) => Type::U128,
            Self::I128(_) => Type::I128,
        }
    }

    #[must_use]
    pub const fn text_name(self) -> &'static str
    {
        crate::xlil::type_name(self.value_type())
    }

    #[must_use]
    pub fn decimal(self) -> String
    {
        match self
        {
            Self::U8(value) => value.to_string(),
            Self::I8(value) => value.to_string(),
            Self::U16(value) => value.to_string(),
            Self::I16(value) => value.to_string(),
            Self::U32(value) => value.to_string(),
            Self::I32(value) => value.to_string(),
            Self::U64(value) => value.to_string(),
            Self::I64(value) => value.to_string(),
            Self::U128(value) => value.to_string(),
            Self::I128(value) => value.to_string(),
        }
    }

    #[must_use]
    pub fn parse(value_type: Type, text: &str) -> Option<Self>
    {
        match value_type
        {
            Type::U8 => text.parse().ok().map(Self::U8),
            Type::I8 => text.parse().ok().map(Self::I8),
            Type::U16 => text.parse().ok().map(Self::U16),
            Type::I16 => text.parse().ok().map(Self::I16),
            Type::U32 => text.parse().ok().map(Self::U32),
            Type::I32 => text.parse().ok().map(Self::I32),
            Type::U64 => text.parse().ok().map(Self::U64),
            Type::I64 => text.parse().ok().map(Self::I64),
            Type::U128 => text.parse().ok().map(Self::U128),
            Type::I128 => text.parse().ok().map(Self::I128),
            _ => None,
        }
    }

    #[must_use]
    pub const fn bits(self) -> u128
    {
        match self
        {
            Self::U8(value) => value as u128,
            Self::I8(value) => value as u8 as u128,
            Self::U16(value) => value as u128,
            Self::I16(value) => value as u16 as u128,
            Self::U32(value) => value as u128,
            Self::I32(value) => value as u32 as u128,
            Self::U64(value) => value as u128,
            Self::I64(value) => value as u64 as u128,
            Self::U128(value) => value,
            Self::I128(value) => value as u128,
        }
    }

    #[must_use]
    pub fn from_bits(value_type: Type, bits: u128) -> Option<Self>
    {
        let width = value_type.integer_width()?;
        if width < 128 && bits >= (1_u128 << width)
        {
            return None;
        }
        Some(match value_type
        {
            Type::U8 => Self::U8(bits as u8),
            Type::I8 => Self::I8(bits as u8 as i8),
            Type::U16 => Self::U16(bits as u16),
            Type::I16 => Self::I16(bits as u16 as i16),
            Type::U32 => Self::U32(bits as u32),
            Type::I32 => Self::I32(bits as u32 as i32),
            Type::U64 => Self::U64(bits as u64),
            Type::I64 => Self::I64(bits as u64 as i64),
            Type::U128 => Self::U128(bits),
            Type::I128 => Self::I128(bits as i128),
            _ => return None,
        })
    }
}
