/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 */

use super::*;

impl MirToXlilLowerer
{
  #[allow(clippy::too_many_arguments)]
  pub(super) fn lower_const_u16(&mut self,
                                local: mir::LocalId,
                                value: u16,
                                span: Span,
                                block: BlockId,
                                local_types: &HashMap<mir::LocalId, Option<Type>>,
                                values: &mut HashMap<mir::LocalId, ValueId>,
                                function: &mut Function)
  {
    match local_types.get(&local).copied().flatten()
    {
      Some(Type::U16) =>
      {
        if let Some(value_id) = function.add_const_u16(block, value)
        {
          values.insert(local, value_id);
        }
      }
      Some(_) => self.report(DiagnosticCode::UnsupportedLocalType,
                             "MIR const.u16 target local must have XLIL u16 type",
                             span),
      None => self.report(DiagnosticCode::MissingLocalType,
                          "MIR const.u16 target local has no XLIL value type",
                          span),
    }
  }

  #[allow(clippy::too_many_arguments)]
  pub(super) fn lower_const_integer(&mut self,
                                    local: mir::LocalId,
                                    value: mir::IntegerConstant,
                                    span: Span,
                                    block: BlockId,
                                    local_types: &HashMap<mir::LocalId, Option<Type>>,
                                    values: &mut HashMap<mir::LocalId, ValueId>,
                                    function: &mut Function)
  {
    let expected = value.value_type();
    match local_types.get(&local).copied().flatten()
    {
      Some(actual) if actual == expected =>
      {
        let constant = IntegerConstant::new(expected, value.bits()).expect("verified MIR integer fits its type");
        if let Some(value_id) = function.add_const_integer(block, constant)
        {
          values.insert(local, value_id);
        }
      }
      Some(_) => self.report(DiagnosticCode::UnsupportedLocalType,
                             "MIR integer constant target local has the wrong XLIL type",
                             span),
      None => self.report(DiagnosticCode::MissingLocalType,
                          "MIR integer constant target local has no XLIL value type",
                          span),
    }
  }

  #[allow(clippy::too_many_arguments)]
  pub(super) fn lower_binary_integer(&mut self,
                                     result: mir::LocalId,
                                     left: mir::LocalId,
                                     right: mir::LocalId,
                                     value_type: Type,
                                     operation: IntegerBinaryOperation,
                                     span: Span,
                                     block: BlockId,
                                     values: &mut HashMap<mir::LocalId, ValueId>,
                                     function: &mut Function)
  {
    let (Some(left_value), Some(right_value)) = (values.get(&left).copied(), values.get(&right).copied())
    else
    {
      self.report(DiagnosticCode::MissingLocalValue,
                  "MIR integer operation references a local without an XLIL value",
                  span);
      return;
    };
    let Some(value) = function.binary_integer(block, operation, value_type, left_value, right_value)
    else
    {
      self.report(DiagnosticCode::UnsupportedLocalType,
                  "MIR integer operation operands do not match its XLIL type",
                  span);
      return;
    };
    values.insert(result, value);
  }
}
