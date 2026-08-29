/*
 * SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
 * SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.0
 */

use super::*;

impl Parser<'_>
{
    pub(super) fn enum_data_expression(&mut self, record: &str) -> Option<Expression>
    {
        let (identity, metadata) = record.split_once(" owner ")?;
        let (enum_type, variant) = identity.rsplit_once("::")?;
        let (owner, metadata) = metadata.split_once(" tag ")?;
        let (tag, payload_name) = metadata.split_once(" payload ")?;
        let tag = match tag.parse::<u32>()
        {
            Ok(value) => value,
            Err(_) =>
            {
                self.report(format!("invalid enum-data tag '{tag}'"));
                return None;
            }
        };
        let payload_type = if payload_name == "()"
        {
            None
        }
        else
        {
            Some(Box::new(
                self.parse_type(payload_name)
                    .unwrap_or(Type::Named(payload_name.to_string())),
            ))
        };
        self.index += 1;
        let payload = if self.current().as_deref() == Some("value")
        {
            self.index += 1;
            self.expression().map(Box::new)
        }
        else
        {
            None
        };
        if self.current().as_deref() == Some(".end")
        {
            self.index += 1;
        }
        else
        {
            self.report("enum-data expression is missing .end".to_string());
        }
        Some(Expression::EnumData {
            enum_type: enum_type.to_string(),
            owner: owner.to_string(),
            variant: variant.to_string(),
            tag,
            payload,
            payload_type,
            span: span(),
        })
    }
}
