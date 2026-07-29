/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 */

use super::{DiagnosticCode, Parser};
use crate::xlil::{Module, Type, TypeKind};

impl Parser<'_>
{
  pub(super) fn aggregate_type(&mut self, module: &mut Module) -> bool
  {
    let line = self.line_number();
    let Some(record) = self.next_line()
    else
    {
      return self.invalid_aggregate(line, "XLIL aggregate type record is invalid");
    };
    let Some(record) = record.strip_prefix(".type %t")
    else
    {
      return self.invalid_aggregate(line, "XLIL aggregate type record is invalid");
    };
    let Some((id, definition)) = record.split_once(' ')
    else
    {
      return self.invalid_aggregate(line, "XLIL aggregate type id is missing");
    };
    let Ok(id) = id.parse::<u32>()
    else
    {
      return self.invalid_aggregate(line, "XLIL aggregate type id is invalid");
    };
    if id != self.aggregate_type_count
    {
      return self.invalid_aggregate(line, "XLIL aggregate type ids must be sequential");
    }
    let Some((name, fields)) = definition.split_once(" : (")
    else
    {
      return self.invalid_aggregate(line, "XLIL aggregate type definition is invalid");
    };
    let Some(fields) = fields.strip_suffix(')')
    else
    {
      return self.invalid_aggregate(line, "XLIL aggregate field list is invalid");
    };
    let Some(fields) = self.aggregate_fields(fields, line)
    else
    {
      return false;
    };
    if module.add_aggregate_type(name, fields) != Some(Type::aggregate(id))
    {
      return self.invalid_aggregate(line, "XLIL aggregate type name is invalid or duplicated");
    }
    self.aggregate_type_count += 1;
    true
  }

  fn aggregate_fields(&mut self, fields: &str, line: usize) -> Option<Vec<Type>>
  {
    let mut parsed = vec![];
    if fields.is_empty()
    {
      return Some(parsed);
    }
    for field in fields.split(", ")
    {
      let field = self.type_name(field, line)?;
      if field.kind == TypeKind::Void
      {
        self.invalid_aggregate(line, "XLIL aggregate fields cannot have void type");
        return None;
      }
      parsed.push(field);
    }
    Some(parsed)
  }

  fn invalid_aggregate(&mut self, line: usize, message: &str) -> bool
  {
    self.report(DiagnosticCode::InvalidType, line, message);
    false
  }
}
