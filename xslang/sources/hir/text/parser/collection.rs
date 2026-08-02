/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 */

use super::*;
use crate::hir::type_check::MapEntry;

impl Parser<'_>
{
    pub(super) fn assign_index_statement(&mut self) -> Statement
    {
        let target = self
            .current()
            .and_then(|line| line.strip_prefix("assign_index ").map(ToString::to_string))
            .unwrap_or_default();
        self.index += 1;
        let element_type = self
            .current()
            .and_then(|line| line.strip_prefix("type ").map(ToString::to_string))
            .and_then(|name| self.parse_type(&name))
            .unwrap_or(Type::Unit);
        self.index += 1;
        self.consume_expression_field("index");
        let index = self.expression().unwrap_or(Expression::Literal {
            literal: Literal::None,
            span: span(),
        });
        self.consume_expression_field("value");
        let value = self.expression().unwrap_or(Expression::Literal {
            literal: Literal::None,
            span: span(),
        });
        if self.current().as_deref() == Some(".end")
        {
            self.index += 1;
        }
        Statement::AssignIndex {
            target,
            index,
            value,
            element_type,
            span: span(),
        }
    }

    pub(super) fn array_expression(&mut self) -> Option<Expression>
    {
        self.index += 1;
        let mut elements = Vec::new();
        while let Some(line) = self.current()
        {
            if line == ".end"
            {
                self.index += 1;
                return Some(Expression::Array {
                    elements,
                    span: span(),
                });
            }
            if line != "element"
            {
                self.report(format!("expected array element, got '{line}'"));
                return None;
            }
            self.index += 1;
            elements.push(self.expression()?);
        }
        self.report("unterminated array expression".to_string());
        None
    }

    pub(super) fn map_expression(&mut self) -> Option<Expression>
    {
        self.index += 1;
        let mut entries = Vec::new();
        while let Some(line) = self.current()
        {
            if line == ".end"
            {
                self.index += 1;
                return Some(Expression::Map {
                    entries,
                    span: span(),
                });
            }
            if line != "entry"
            {
                self.report(format!("expected map entry, got '{line}'"));
                return None;
            }
            self.index += 1;
            self.consume_expression_field("key");
            let key = self.expression()?;
            self.consume_expression_field("value");
            let value = self.expression()?;
            entries.push(MapEntry {
                key,
                value,
                span: span(),
            });
        }
        self.report("unterminated map expression".to_string());
        None
    }

    pub(super) fn set_expression(&mut self) -> Option<Expression>
    {
        self.index += 1;
        let mut elements = Vec::new();
        while let Some(line) = self.current()
        {
            if line == ".end"
            {
                self.index += 1;
                return Some(Expression::Set {
                    elements,
                    span: span(),
                });
            }
            if line != "element"
            {
                self.report(format!("expected set element, got '{line}'"));
                return None;
            }
            self.index += 1;
            elements.push(self.expression()?);
        }
        self.report("unterminated set expression".to_string());
        None
    }

    pub(super) fn index_expression(&mut self, element_type: &str) -> Option<Expression>
    {
        let element_type = self.parse_type(element_type)?;
        self.index += 1;
        self.consume_expression_field("collection");
        let collection = self.expression()?;
        self.consume_expression_field("offset");
        let index = self.expression()?;
        if self.current().as_deref() != Some(".end")
        {
            self.report("unterminated index expression".to_string());
            return None;
        }
        self.index += 1;
        Some(Expression::Index {
            collection: Box::new(collection),
            index: Box::new(index),
            element_type: Box::new(element_type),
            span: span(),
        })
    }

    pub(super) fn array_length_expression(&mut self) -> Option<Expression>
    {
        self.index += 1;
        let collection = self.expression()?;
        if self.current().as_deref() != Some(".end")
        {
            self.report("unterminated array length expression".to_string());
            return None;
        }
        self.index += 1;
        Some(Expression::ArrayLength {
            collection: Box::new(collection),
            span: span(),
        })
    }
}
