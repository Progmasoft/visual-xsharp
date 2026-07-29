/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 */

use super::{Parser, span};
use crate::{
  mir::{IntegerConstant, Statement},
  xlil::type_from_name,
};

pub(super) fn is_integer_constant(kind: &str) -> bool
{
  matches!(kind,
           "const.u8" | "const.i8" | "const.i16" | "const.u32" | "const.u64" | "const.u128" | "const.i128")
}

impl Parser<'_>
{
  pub(super) fn const_u16_statement(&mut self) -> Statement
  {
    let local = self.const_target();
    let line = self.current();
    let value = match line.as_deref().and_then(|value| value.strip_prefix("value 0x"))
    {
      Some(hexadecimal) if hexadecimal.len() == 4 =>
      {
        self.index += 1;
        u16::from_str_radix(hexadecimal, 16).unwrap_or_else(|_| {
                                              self.report("invalid const.u16 value".to_string());
                                              0
                                            })
      }
      _ =>
      {
        self.report("missing const.u16 value".to_string());
        0
      }
    };
    Statement::ConstU16 { local,
                          value,
                          span: span() }
  }

  pub(super) fn integer_constant_statement(&mut self, kind: &str) -> Statement
  {
    let local = self.const_target();
    let value_type = kind.strip_prefix("const.").and_then(type_from_name);
    let line = self.current();
    let value = line.as_deref().and_then(|line| line.strip_prefix("value "));
    if value.is_some()
    {
      self.index += 1;
    }
    let value = value_type.and_then(|value_type| value.and_then(|value| IntegerConstant::parse(value_type, value)));
    let Some(value) = value
    else
    {
      self.report(format!("invalid {kind} value"));
      return Statement::ConstInteger { local,
                                       value: IntegerConstant::U8(0),
                                       span: span() };
    };
    Statement::ConstInteger { local,
                              value,
                              span: span() }
  }
}
