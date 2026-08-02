/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 */

use super::{DiagnosticCode, Parser};
use crate::xlil::{Module, Type};

impl Parser<'_>
{
    pub(super) fn array_type(&mut self, module: &mut Module) -> bool
    {
        let line = self.line_number();
        let Some(record) = self.next_line()
        else
        {
            return self.invalid_array(line, "XLIL array type record is invalid");
        };
        let Some(record) = record.strip_prefix(".array %a")
        else
        {
            return self.invalid_array(line, "XLIL array type record is invalid");
        };
        let Some((id, definition)) = record.split_once(" : ")
        else
        {
            return self.invalid_array(line, "XLIL array type id is missing");
        };
        let Ok(id) = id.parse::<u32>()
        else
        {
            return self.invalid_array(line, "XLIL array type id is invalid");
        };
        if id != self.array_type_count
        {
            return self.invalid_array(line, "XLIL array type ids must be sequential");
        }
        let (element, length) = definition
            .rsplit_once(" x ")
            .map_or((definition, None), |(element, length)| (element, Some(length)));
        let Some(element) = self.type_name(element, line)
        else
        {
            return false;
        };
        let value_type = match length
        {
            Some(length) => match length.parse::<u64>()
            {
                Ok(length) => module.add_array_type(element, length),
                Err(_) => return self.invalid_array(line, "XLIL array length is invalid"),
            },
            None => module.add_dynamic_array_type(element),
        };
        if value_type != Some(Type::array(id))
        {
            return self.invalid_array(line, "XLIL array type is invalid or duplicated");
        }
        self.array_type_count += 1;
        true
    }

    fn invalid_array(&mut self, line: usize, message: &str) -> bool
    {
        self.report(DiagnosticCode::InvalidType, line, message);
        false
    }
}
