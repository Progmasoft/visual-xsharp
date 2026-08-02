/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 */

use super::*;
use crate::hir::type_check::TupleFieldValue;

impl Parser<'_>
{
    pub(super) fn assign_tuple_element_statement(&mut self) -> Statement
    {
        let record = self.current().unwrap_or_default();
        let record = record.strip_prefix("assign_tuple_element ").unwrap_or_default();
        let (target, index) = record.rsplit_once(' ').unwrap_or((record, "0"));
        let index = index.parse().unwrap_or(0);
        self.index += 1;
        let tuple_record = self.current().unwrap_or_default();
        let tuple_type = tuple_record
            .strip_prefix("tuple_type ")
            .and_then(|value| self.parse_type(value))
            .unwrap_or(Type::Unit);
        self.index += 1;
        let element_record = self.current().unwrap_or_default();
        let element_type = element_record
            .strip_prefix("element_type ")
            .and_then(|value| self.parse_type(value))
            .unwrap_or(Type::Unit);
        self.index += 1;
        self.consume_expression_field("value");
        let value = self.expression().unwrap_or(Expression::Literal {
            literal: Literal::None,
            span: span(),
        });
        if self.current().as_deref() == Some(".end")
        {
            self.index += 1;
        }
        else
        {
            self.report("unterminated tuple element assignment".to_string());
        }
        Statement::AssignTupleElement {
            target: target.to_string(),
            index,
            value,
            tuple_type,
            element_type,
            span: span(),
        }
    }

    pub(super) fn tuple_expression(&mut self, type_text: &str) -> Option<Expression>
    {
        let tuple_type = self.parse_type(type_text)?;
        let Type::Tuple {
            fields: expected,
        } = &tuple_type
        else
        {
            self.report("tuple expression requires a tuple type".to_string());
            return None;
        };
        self.index += 1;
        let mut fields = Vec::new();
        while let Some(line) = self.current()
        {
            if line == ".end"
            {
                self.index += 1;
                if fields.len() != expected.len()
                {
                    self.report("tuple expression field count does not match its type".to_string());
                    return None;
                }
                return Some(Expression::Tuple {
                    fields,
                    tuple_type: Box::new(tuple_type),
                    span: span(),
                });
            }
            let Some(name) = line.strip_prefix("field ")
            else
            {
                self.report(format!("expected tuple field, got '{line}'"));
                return None;
            };
            self.index += 1;
            fields.push(TupleFieldValue {
                name: (name != "positional").then(|| name.to_string()),
                value: self.expression()?,
                span: span(),
            });
        }
        self.report("unterminated tuple expression".to_string());
        None
    }

    pub(super) fn tuple_element_expression(&mut self, record: &str) -> Option<Expression>
    {
        let (index, element_type) = record.split_once(' ')?;
        let index = index.parse().ok()?;
        let element_type = self.parse_type(element_type)?;
        self.index += 1;
        let tuple = self.expression()?;
        if self.current().as_deref() != Some(".end")
        {
            self.report("unterminated tuple element expression".to_string());
            return None;
        }
        self.index += 1;
        Some(Expression::TupleElement {
            tuple: Box::new(tuple),
            index,
            element_type: Box::new(element_type),
            span: span(),
        })
    }
}
