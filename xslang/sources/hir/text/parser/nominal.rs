/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 */

use super::*;

impl Parser<'_>
{
  pub(super) fn field_expression(&mut self, record: &str) -> Option<Expression>
  {
    let path = self.field_path(record)?;
    self.index += 1;
    Some(Expression::Field { path })
  }

  pub(super) fn member_expression(&mut self, record: &str) -> Option<Expression>
  {
    let Some((member, field_type)) = record.split_once(" : ")
    else
    {
      self.report(format!("invalid member record '{record}'"));
      return None;
    };
    let Some((owner, name)) = member.rsplit_once('.')
    else
    {
      self.report(format!("member record requires an owner and field name: '{record}'"));
      return None;
    };
    if owner.is_empty() || name.is_empty()
    {
      self.report(format!("invalid member name '{member}'"));
      return None;
    }
    let field_type = self.parse_type(field_type)
                         .unwrap_or(Type::Named(field_type.to_string()));
    self.index += 1;
    let receiver = self.expression()?;
    if self.current().as_deref() != Some(".end")
    {
      self.report("member expression requires a closing .end record".to_string());
      return None;
    }
    self.index += 1;
    Some(Expression::Member { receiver: Box::new(receiver),
                              owner: owner.to_string(),
                              name: name.to_string(),
                              field_type: Box::new(field_type),
                              span: span() })
  }

  pub(super) fn assign_field_expression(&mut self, record: &str) -> Option<Expression>
  {
    let target = self.field_path(record)?;
    self.index += 1;
    let value = self.expression()?;
    Some(Expression::AssignField { target,
                                   value: Box::new(value),
                                   span: span() })
  }

  pub(super) fn object_expression(&mut self, nominal_type: &str) -> Option<Expression>
  {
    let nominal_type = nominal_type.to_string();
    self.index += 1;
    let mut fields = Vec::new();
    while let Some(line) = self.current()
    {
      if line == ".end"
      {
        self.index += 1;
        return Some(Expression::Object { nominal_type,
                                         fields,
                                         span: span() });
      }
      let Some(name) = line.strip_prefix("field ")
      else
      {
        self.report(format!("expected object field, got '{line}'"));
        return None;
      };
      let name = name.to_string();
      self.index += 1;
      fields.push(ObjectField { name,
                                value: self.expression()?,
                                span: span() });
    }
    self.report("unterminated object expression".to_string());
    None
  }

  fn field_path(&mut self, record: &str) -> Option<FieldPath>
  {
    let Some((path, ty)) = record.split_once(" : ")
    else
    {
      self.report(format!("invalid field path record '{record}'"));
      return None;
    };
    let Some((mutability, path)) = path.split_once(' ')
    else
    {
      self.report(format!("missing field mutability in '{record}'"));
      return None;
    };
    let mutable = match mutability
    {
      "mutable" => true,
      "immutable" => false,
      _ =>
      {
        self.report(format!("invalid field mutability '{mutability}'"));
        return None;
      }
    };
    let mut segments = path.split('.');
    let root = segments.next()?.to_string();
    let fields = segments.map(ToString::to_string).collect::<Vec<_>>();
    if root.is_empty() || fields.iter().any(String::is_empty)
    {
      self.report(format!("invalid field path '{path}'"));
      return None;
    }
    let ty = self.parse_type(ty).unwrap_or(Type::Named(ty.to_string()));
    Some(FieldPath { root,
                     fields,
                     ty,
                     mutable,
                     span: span() })
  }
}
