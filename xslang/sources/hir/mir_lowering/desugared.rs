/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 */

use super::*;

impl HirToMirLowerer
{
  pub(super) fn surface_statement_from_desugared(&mut self, statement: &DesugaredStatement) -> Option<Statement>
  {
    match statement
    {
      DesugaredStatement::Let { local,
                                initializer, } =>
      {
        let initializer = initializer.as_ref()
                                     .and_then(|expression| self.surface_expression_from_desugared(expression));
        Some(Statement::Let { local: local.clone(),
                              initializer })
      }
      DesugaredStatement::Expr(expression) => self.surface_expression_from_desugared(expression).map(Statement::Expr),
      DesugaredStatement::AssignIndex { target,
                                        index,
                                        value,
                                        element_type,
                                        span, } =>
      {
        Some(Statement::AssignIndex { target: target.clone(),
                                      index: self.surface_expression_from_desugared(index)?,
                                      value: self.surface_expression_from_desugared(value)?,
                                      element_type: element_type.clone(),
                                      span: *span })
      }
      DesugaredStatement::AssignTupleElement { target,
                                               index,
                                               value,
                                               tuple_type,
                                               element_type,
                                               span, } =>
      {
        Some(Statement::AssignTupleElement { target: target.clone(),
                                             index: *index,
                                             value: self.surface_expression_from_desugared(value)?,
                                             tuple_type: tuple_type.clone(),
                                             element_type: element_type.clone(),
                                             span: *span })
      }
      DesugaredStatement::Return { value,
                                   span, } =>
      {
        let value = value.as_ref()
                         .and_then(|expression| self.surface_expression_from_desugared(expression));
        Some(Statement::Return { value,
                                 span: *span })
      }
      DesugaredStatement::If { condition,
                               then_block,
                               else_block,
                               span, } =>
      {
        Some(Statement::If { condition: self.surface_expression_from_desugared(condition)?,
                             then_block: self.surface_block_from_desugared(then_block)?,
                             else_block: else_block.as_ref()
                                                   .and_then(|block| self.surface_block_from_desugared(block)),
                             span: *span })
      }
      DesugaredStatement::While { condition,
                                  body,
                                  span, } =>
      {
        Some(Statement::While { condition: self.surface_expression_from_desugared(condition)?,
                                body: self.surface_block_from_desugared(body)?,
                                span: *span })
      }
      DesugaredStatement::For { initializer,
                                condition,
                                update,
                                body,
                                span, } =>
      {
        Some(Statement::For { initializer: match initializer
                              {
                                Some(statement) => Some(Box::new(self.surface_statement_from_desugared(statement)?)),
                                None => None,
                              },
                              condition: match condition
                              {
                                Some(expression) => Some(self.surface_expression_from_desugared(expression)?),
                                None => None,
                              },
                              update: match update
                              {
                                Some(expression) => Some(self.surface_expression_from_desugared(expression)?),
                                None => None,
                              },
                              body: self.surface_block_from_desugared(body)?,
                              span: *span })
      }
      DesugaredStatement::ForEach { binding,
                                    iterable,
                                    iterable_type,
                                    body,
                                    span, } =>
      {
        Some(Statement::ForEach { binding: binding.clone(),
                                  iterable: self.surface_expression_from_desugared(iterable)?,
                                  iterable_type: iterable_type.clone(),
                                  body: self.surface_block_from_desugared(body)?,
                                  span: *span })
      }
      DesugaredStatement::Match { selector,
                                  selector_type,
                                  arms,
                                  span, } =>
      {
        let arms = arms.iter()
                       .map(|arm| {
                         Some(MatchArm { pattern: arm.pattern.clone(),
                                         body: self.surface_block_from_desugared(&arm.body)?,
                                         span: arm.span })
                       })
                       .collect::<Option<Vec<_>>>()?;
        Some(Statement::Match { selector: self.surface_expression_from_desugared(selector)?,
                                selector_type: selector_type.clone(),
                                arms,
                                span: *span })
      }
      DesugaredStatement::Break { span } => Some(Statement::Break { span: *span }),
      DesugaredStatement::Continue { span } => Some(Statement::Continue { span: *span }),
      DesugaredStatement::Panic { span } => Some(Statement::Panic { span: *span }),
    }
  }

  pub(super) fn surface_expression_from_desugared(&mut self, expression: &DesugaredExpression) -> Option<Expression>
  {
    match expression
    {
      DesugaredExpression::Literal { literal,
                                     span, } => Some(Expression::Literal { literal: literal.clone(),
                                                                           span: *span }),
      DesugaredExpression::Local { name,
                                   span, } => Some(Expression::Local { name: name.clone(),
                                                                       span: *span }),
      DesugaredExpression::Field { path } => Some(Expression::Field { path: path.clone() }),
      DesugaredExpression::Member { receiver,
                                    owner,
                                    name,
                                    field_type,
                                    span, } =>
      {
        Some(Expression::Member { receiver: Box::new(self.surface_expression_from_desugared(receiver)?),
                                  owner: owner.clone(),
                                  name: name.clone(),
                                  field_type: field_type.clone(),
                                  span: *span })
      }
      DesugaredExpression::Object { nominal_type,
                                    fields,
                                    span, } =>
      {
        Some(Expression::Object { nominal_type: nominal_type.clone(),
                                  fields: fields.iter()
                                                .map(|field| {
                                                  Some(crate::hir::type_check::ObjectField {
                                                    name: field.name.clone(),
                                                    value: self.surface_expression_from_desugared(&field.value)?,
                                                    span: field.span,
                                                  })
                                                })
                                                .collect::<Option<Vec<_>>>()?,
                                  span: *span })
      }
      DesugaredExpression::Array { elements,
                                   span, } =>
      {
        Some(Expression::Array { elements: elements.iter()
                                                   .map(|value| self.surface_expression_from_desugared(value))
                                                   .collect::<Option<Vec<_>>>()?,
                                 span: *span })
      }
      DesugaredExpression::Set { elements,
                                 span, } =>
      {
        Some(Expression::Set { elements: elements.iter()
                                                 .map(|value| self.surface_expression_from_desugared(value))
                                                 .collect::<Option<Vec<_>>>()?,
                               span: *span })
      }
      DesugaredExpression::Map { entries,
                                 span, } => Some(Expression::Map { entries:
                                                                     entries.iter()
                                                                            .map(|entry| {
                                                                              Some(crate::hir::type_check::MapEntry {
                            key: self.surface_expression_from_desugared(&entry.key)?,
                            value: self.surface_expression_from_desugared(&entry.value)?,
                            span: entry.span,
                          })
                                                                            })
                                                                            .collect::<Option<Vec<_>>>()?,
                                                                   span: *span }),
      DesugaredExpression::Tuple { fields,
                                   tuple_type,
                                   span, } =>
      {
        Some(Expression::Tuple { fields: fields.iter()
                                               .map(|field| {
                                                 Some(crate::hir::type_check::TupleFieldValue {
                          name: field.name.clone(),
                          value: self.surface_expression_from_desugared(&field.value)?,
                          span: field.span,
                        })
                                               })
                                               .collect::<Option<Vec<_>>>()?,
                                 tuple_type: tuple_type.clone(),
                                 span: *span })
      }
      DesugaredExpression::TupleElement { tuple,
                                          index,
                                          element_type,
                                          span, } =>
      {
        Some(Expression::TupleElement { tuple: Box::new(self.surface_expression_from_desugared(tuple)?),
                                        index: *index,
                                        element_type: element_type.clone(),
                                        span: *span })
      }
      DesugaredExpression::Index { collection,
                                   index,
                                   element_type,
                                   span, } =>
      {
        Some(Expression::Index { collection: Box::new(self.surface_expression_from_desugared(collection)?),
                                 index: Box::new(self.surface_expression_from_desugared(index)?),
                                 element_type: element_type.clone(),
                                 span: *span })
      }
      DesugaredExpression::ArrayLength { collection,
                                         span, } =>
      {
        Some(Expression::ArrayLength { collection: Box::new(self.surface_expression_from_desugared(collection)?),
                                       span: *span })
      }
      DesugaredExpression::Assign { target,
                                    value,
                                    span, } =>
      {
        self.surface_expression_from_desugared(value)
            .map(|value| Expression::Assign { target: target.clone(),
                                              value: Box::new(value),
                                              span: *span })
      }
      DesugaredExpression::AssignField { target,
                                         value,
                                         span, } =>
      {
        self.surface_expression_from_desugared(value)
            .map(|value| Expression::AssignField { target: target.clone(),
                                                   value: Box::new(value),
                                                   span: *span })
      }
      DesugaredExpression::Update { target,
                                    operator,
                                    position,
                                    span, } => Some(Expression::Update { target: target.clone(),
                                                                         operator: *operator,
                                                                         position: *position,
                                                                         span: *span }),
      DesugaredExpression::Binary { operator,
                                    left,
                                    right,
                                    span, } =>
      {
        Some(Expression::Binary { operator: *operator,
                                  left: Box::new(self.surface_expression_from_desugared(left)?),
                                  right: Box::new(self.surface_expression_from_desugared(right)?),
                                  span: *span })
      }
      DesugaredExpression::Unary { operator,
                                   operand,
                                   span, } =>
      {
        Some(Expression::Unary { operator: *operator,
                                 operand: Box::new(self.surface_expression_from_desugared(operand)?),
                                 span: *span })
      }
      DesugaredExpression::ResultMatch { span, .. } =>
      {
        self.report(DiagnosticCode::UnsupportedExpression,
                    "desugared Result match lowering awaits MIR Result control-flow support",
                    *span);
        None
      }
      DesugaredExpression::Call { function,
                                  arguments,
                                  parameter_types,
                                  return_type,
                                  span, } =>
      {
        Some(Expression::Call { function: function.clone(),
                                arguments: arguments.iter()
                                                    .map(|value| self.surface_expression_from_desugared(value))
                                                    .collect::<Option<Vec<_>>>()?,
                                parameter_types: parameter_types.clone(),
                                return_type: return_type.clone(),
                                span: *span })
      }
      DesugaredExpression::If { condition,
                                then_block,
                                else_block,
                                result_type,
                                span, } =>
      {
        Some(Expression::If { condition: Box::new(self.surface_expression_from_desugared(condition)?),
                              then_block: Box::new(self.surface_block_from_desugared(then_block)?),
                              else_block: Box::new(self.surface_block_from_desugared(else_block)?),
                              result_type: result_type.clone(),
                              span: *span })
      }
      DesugaredExpression::Match { selector,
                                   selector_type,
                                   arms,
                                   result_type,
                                   span, } =>
      {
        Some(Expression::Match { selector: Box::new(self.surface_expression_from_desugared(selector)?),
                                 selector_type: selector_type.clone(),
                                 arms: arms.iter()
                                           .map(|arm| {
                                             Some(MatchArm { pattern: arm.pattern.clone(),
                                                             body: self.surface_block_from_desugared(&arm.body)?,
                                                             span: arm.span })
                                           })
                                           .collect::<Option<Vec<_>>>()?,
                                 result_type: result_type.clone(),
                                 span: *span })
      }
    }
  }

  pub(super) fn surface_block_from_desugared(&mut self, block: &DesugaredBlock) -> Option<Block>
  {
    let statements = block.statements
                          .iter()
                          .map(|statement| self.surface_statement_from_desugared(statement))
                          .collect::<Option<Vec<_>>>()?;
    let tail = match &block.tail
    {
      Some(expression) => Some(Box::new(self.surface_expression_from_desugared(expression)?)),
      None => None,
    };
    Some(Block { statements,
                 tail,
                 span: block.span })
  }
}
