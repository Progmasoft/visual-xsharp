/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 */

use super::async_check::Span;
use super::match_model::MatchPattern;
use super::type_check::{
  BinaryOperator, Block, Expression, FieldPath, Function, Literal, Local, PrimitiveType, Statement, Type,
  UnaryOperator, UpdateOperator, UpdatePosition, literal_matches_type, result_type_parts,
};

mod expression_type;

use expression_type::{literal_default_type, unary_expression_type};

#[derive(Clone, Debug, Eq, PartialEq)]
pub enum DiagnosticCode
{
  RequiresResult,
  ReturnMismatch,
  UnknownLocal,
}

#[derive(Clone, Debug, Eq, PartialEq)]
pub struct Diagnostic
{
  pub code: DiagnosticCode,
  pub message: String,
  pub span: Span,
}

#[derive(Clone, Debug, Eq, PartialEq)]
pub enum DesugaredExpression
{
  Literal
  {
    literal: Literal, span: Span
  },
  Local
  {
    name: String, span: Span
  },
  Field
  {
    path: FieldPath
  },
  Member
  {
    receiver: Box<DesugaredExpression>,
    owner: String,
    name: String,
    field_type: Box<Type>,
    span: Span,
  },
  Object
  {
    nominal_type: String,
    fields: Vec<DesugaredObjectField>,
    span: Span,
  },
  Array
  {
    elements: Vec<DesugaredExpression>,
    span: Span,
  },
  Set
  {
    elements: Vec<DesugaredExpression>,
    span: Span,
  },
  Map
  {
    entries: Vec<DesugaredMapEntry>,
    span: Span,
  },
  Tuple
  {
    fields: Vec<DesugaredTupleField>,
    tuple_type: Box<Type>,
    span: Span,
  },
  TupleElement
  {
    tuple: Box<DesugaredExpression>,
    index: u32,
    element_type: Box<Type>,
    span: Span,
  },
  Index
  {
    collection: Box<DesugaredExpression>,
    index: Box<DesugaredExpression>,
    element_type: Box<Type>,
    span: Span,
  },
  ArrayLength
  {
    collection: Box<DesugaredExpression>,
    span: Span,
  },
  Assign
  {
    target: String,
    value: Box<DesugaredExpression>,
    span: Span,
  },
  AssignField
  {
    target: FieldPath,
    value: Box<DesugaredExpression>,
    span: Span,
  },
  Update
  {
    target: String,
    operator: UpdateOperator,
    position: UpdatePosition,
    span: Span,
  },
  Binary
  {
    operator: BinaryOperator,
    left: Box<DesugaredExpression>,
    right: Box<DesugaredExpression>,
    span: Span,
  },
  Unary
  {
    operator: UnaryOperator,
    operand: Box<DesugaredExpression>,
    span: Span,
  },
  Call
  {
    function: String,
    arguments: Vec<DesugaredExpression>,
    parameter_types: Vec<Type>,
    return_type: Box<Type>,
    span: Span,
  },
  If
  {
    condition: Box<DesugaredExpression>,
    then_block: Box<DesugaredBlock>,
    else_block: Box<DesugaredBlock>,
    result_type: Box<Type>,
    span: Span,
  },
  Match
  {
    selector: Box<DesugaredExpression>,
    selector_type: Box<Type>,
    arms: Vec<DesugaredMatchArm>,
    result_type: Box<Type>,
    span: Span,
  },
  ResultMatch
  {
    value: Box<DesugaredExpression>,
    success_binding: String,
    error_binding: String,
    success_type: Type,
    error_type: Type,
    span: Span,
  },
}

#[derive(Clone, Debug, Eq, PartialEq)]
pub struct DesugaredMapEntry
{
  pub key: DesugaredExpression,
  pub value: DesugaredExpression,
  pub span: Span,
}

#[derive(Clone, Debug, Eq, PartialEq)]
pub struct DesugaredTupleField
{
  pub name: Option<String>,
  pub value: DesugaredExpression,
  pub span: Span,
}

#[derive(Clone, Debug, Eq, PartialEq)]
pub struct DesugaredObjectField
{
  pub name: String,
  pub value: DesugaredExpression,
  pub span: Span,
}

#[derive(Clone, Debug, Eq, PartialEq)]
pub enum DesugaredStatement
{
  Let
  {
    local: Local,
    initializer: Option<DesugaredExpression>,
  },
  Expr(DesugaredExpression),
  AssignIndex
  {
    target: String,
    index: DesugaredExpression,
    value: DesugaredExpression,
    element_type: Type,
    span: Span,
  },
  AssignTupleElement
  {
    target: String,
    index: u32,
    value: DesugaredExpression,
    tuple_type: Type,
    element_type: Type,
    span: Span,
  },
  Return
  {
    value: Option<DesugaredExpression>,
    span: Span,
  },
  If
  {
    condition: DesugaredExpression,
    then_block: DesugaredBlock,
    else_block: Option<DesugaredBlock>,
    span: Span,
  },
  While
  {
    condition: DesugaredExpression,
    body: DesugaredBlock,
    span: Span,
  },
  For
  {
    initializer: Option<Box<DesugaredStatement>>,
    condition: Option<DesugaredExpression>,
    update: Option<DesugaredExpression>,
    body: DesugaredBlock,
    span: Span,
  },
  ForEach
  {
    binding: Local,
    iterable: DesugaredExpression,
    iterable_type: Type,
    body: DesugaredBlock,
    span: Span,
  },
  Match
  {
    selector: DesugaredExpression,
    selector_type: Type,
    arms: Vec<DesugaredMatchArm>,
    span: Span,
  },
  Break
  {
    span: Span,
  },
  Continue
  {
    span: Span,
  },
  Panic
  {
    span: Span,
  },
}

#[derive(Clone, Debug, Eq, PartialEq)]
pub struct DesugaredMatchArm
{
  pub pattern: MatchPattern,
  pub body: DesugaredBlock,
  pub span: Span,
}

#[derive(Clone, Debug, Eq, PartialEq)]
pub struct DesugaredBlock
{
  pub statements: Vec<DesugaredStatement>,
  pub tail: Option<Box<DesugaredExpression>>,
  pub span: Span,
}

#[derive(Clone, Debug, Eq, PartialEq)]
pub struct DesugaredFunction
{
  pub name: String,
  pub return_type: Option<Type>,
  pub locals: Vec<Local>,
  pub body: Vec<DesugaredStatement>,
}

#[derive(Default)]
pub struct ResultDesugar
{
  locals: Vec<Local>,
  diagnostics: Vec<Diagnostic>,
  return_type: Option<Type>,
  next_binding: u32,
}

impl ResultDesugar
{
  #[must_use]
  pub fn new() -> Self
  {
    Self::default()
  }

  pub fn desugar_function(mut self, function: &Function) -> Result<DesugaredFunction, Vec<Diagnostic>>
  {
    self.locals.extend(function.locals.iter().cloned());
    self.return_type = function.return_type.clone();
    let mut body = Vec::with_capacity(function.body.len());
    for statement in &function.body
    {
      body.push(self.desugar_statement(statement));
    }
    if self.diagnostics.is_empty()
    {
      Ok(DesugaredFunction { name: function.name.clone(),
                             return_type: function.return_type.clone(),
                             locals: function.locals.clone(),
                             body })
    }
    else
    {
      Err(self.diagnostics)
    }
  }

  fn desugar_statement(&mut self, statement: &Statement) -> DesugaredStatement
  {
    match statement
    {
      Statement::Let { local,
                       initializer, } =>
      {
        let initializer = initializer.as_ref()
                                     .map(|expression| self.desugar_expression(expression));
        self.locals.push(local.clone());
        DesugaredStatement::Let { local: local.clone(),
                                  initializer }
      }
      Statement::Expr(expression) => DesugaredStatement::Expr(self.desugar_expression(expression)),
      Statement::AssignIndex { target,
                               index,
                               value,
                               element_type,
                               span, } => DesugaredStatement::AssignIndex { target: target.clone(),
                                                                            index: self.desugar_expression(index),
                                                                            value: self.desugar_expression(value),
                                                                            element_type: element_type.clone(),
                                                                            span: *span },
      Statement::AssignTupleElement { target,
                                      index,
                                      value,
                                      tuple_type,
                                      element_type,
                                      span, } =>
      {
        DesugaredStatement::AssignTupleElement { target: target.clone(),
                                                 index: *index,
                                                 value: self.desugar_expression(value),
                                                 tuple_type: tuple_type.clone(),
                                                 element_type: element_type.clone(),
                                                 span: *span }
      }
      Statement::Return { value,
                          span, } =>
      {
        let value = value.as_ref().map(|expression| self.desugar_expression(expression));
        DesugaredStatement::Return { value,
                                     span: *span }
      }
      Statement::If { condition,
                      then_block,
                      else_block,
                      span, } => DesugaredStatement::If { condition: self.desugar_expression(condition),
                                                          then_block: self.desugar_block(then_block),
                                                          else_block:
                                                            else_block.as_ref().map(|block| self.desugar_block(block)),
                                                          span: *span },
      Statement::While { condition,
                         body,
                         span, } => DesugaredStatement::While { condition: self.desugar_expression(condition),
                                                                body: self.desugar_block(body),
                                                                span: *span },
      Statement::For { initializer,
                       condition,
                       update,
                       body,
                       span, } =>
      {
        let local_count = self.locals.len();
        let initializer = initializer.as_ref()
                                     .map(|statement| Box::new(self.desugar_statement(statement)));
        let condition = condition.as_ref().map(|expression| self.desugar_expression(expression));
        let body = self.desugar_block(body);
        let update = update.as_ref().map(|expression| self.desugar_expression(expression));
        self.locals.truncate(local_count);
        DesugaredStatement::For { initializer,
                                  condition,
                                  update,
                                  body,
                                  span: *span }
      }
      Statement::ForEach { binding,
                           iterable,
                           iterable_type,
                           body,
                           span, } =>
      {
        let iterable = self.desugar_expression(iterable);
        let local_count = self.locals.len();
        self.locals.push(binding.clone());
        let body = self.desugar_block(body);
        self.locals.truncate(local_count);
        DesugaredStatement::ForEach { binding: binding.clone(),
                                      iterable,
                                      iterable_type: iterable_type.clone(),
                                      body,
                                      span: *span }
      }
      Statement::Match { selector,
                         selector_type,
                         arms,
                         span, } =>
      {
        DesugaredStatement::Match { selector: self.desugar_expression(selector),
                                    selector_type: selector_type.clone(),
                                    arms: arms.iter()
                                              .map(|arm| DesugaredMatchArm { pattern: arm.pattern.clone(),
                                                                             body: self.desugar_block(&arm.body),
                                                                             span: arm.span })
                                              .collect(),
                                    span: *span }
      }
      Statement::Break { span } => DesugaredStatement::Break { span: *span },
      Statement::Continue { span } => DesugaredStatement::Continue { span: *span },
      Statement::Panic { span } => DesugaredStatement::Panic { span: *span },
    }
  }

  fn desugar_expression(&mut self, expression: &Expression) -> DesugaredExpression
  {
    match expression
    {
      Expression::Literal { literal,
                            span, } => DesugaredExpression::Literal { literal: literal.clone(),
                                                                      span: *span },
      Expression::Local { name,
                          span, } => DesugaredExpression::Local { name: name.clone(),
                                                                  span: *span },
      Expression::Field { path } => DesugaredExpression::Field { path: path.clone() },
      Expression::Member { receiver,
                           owner,
                           name,
                           field_type,
                           span, } => DesugaredExpression::Member { receiver:
                                                                      Box::new(self.desugar_expression(receiver)),
                                                                    owner: owner.clone(),
                                                                    name: name.clone(),
                                                                    field_type: field_type.clone(),
                                                                    span: *span },
      Expression::Object { nominal_type,
                           fields,
                           span, } =>
      {
        DesugaredExpression::Object { nominal_type: nominal_type.clone(),
                                      fields: fields.iter()
                                                    .map(|field| {
                                                      DesugaredObjectField { name: field.name.clone(),
                                                                             value:
                                                                               self.desugar_expression(&field.value),
                                                                             span: field.span }
                                                    })
                                                    .collect(),
                                      span: *span }
      }
      Expression::Array { elements,
                          span, } =>
      {
        DesugaredExpression::Array { elements: elements.iter().map(|value| self.desugar_expression(value)).collect(),
                                     span: *span }
      }
      Expression::Set { elements,
                        span, } =>
      {
        DesugaredExpression::Set { elements: elements.iter().map(|value| self.desugar_expression(value)).collect(),
                                   span: *span }
      }
      Expression::Map { entries,
                        span, } =>
      {
        DesugaredExpression::Map { entries: entries.iter()
                                                   .map(|entry| {
                                                     DesugaredMapEntry { key: self.desugar_expression(&entry.key),
                                                                         value: self.desugar_expression(&entry.value),
                                                                         span: entry.span }
                                                   })
                                                   .collect(),
                                   span: *span }
      }
      Expression::Tuple { fields,
                          tuple_type,
                          span, } =>
      {
        DesugaredExpression::Tuple { fields: fields.iter()
                                                   .map(|field| {
                                                     DesugaredTupleField { name: field.name.clone(),
                                                                           value:
                                                                             self.desugar_expression(&field.value),
                                                                           span: field.span }
                                                   })
                                                   .collect(),
                                     tuple_type: tuple_type.clone(),
                                     span: *span }
      }
      Expression::TupleElement { tuple,
                                 index,
                                 element_type,
                                 span, } =>
      {
        DesugaredExpression::TupleElement { tuple: Box::new(self.desugar_expression(tuple)),
                                            index: *index,
                                            element_type: element_type.clone(),
                                            span: *span }
      }
      Expression::Index { collection,
                          index,
                          element_type,
                          span, } => DesugaredExpression::Index { collection:
                                                                    Box::new(self.desugar_expression(collection)),
                                                                  index: Box::new(self.desugar_expression(index)),
                                                                  element_type: element_type.clone(),
                                                                  span: *span },
      Expression::ArrayLength { collection,
                                span, } =>
      {
        DesugaredExpression::ArrayLength { collection: Box::new(self.desugar_expression(collection)),
                                           span: *span }
      }
      Expression::Assign { target,
                           value,
                           span, } => DesugaredExpression::Assign { target: target.clone(),
                                                                    value: Box::new(self.desugar_expression(value)),
                                                                    span: *span },
      Expression::AssignField { target,
                                value,
                                span, } => DesugaredExpression::AssignField { target: target.clone(),
                                                                              value:
                                                                                Box::new(self.desugar_expression(value)),
                                                                              span: *span },
      Expression::Update { target,
                           operator,
                           position,
                           span, } => DesugaredExpression::Update { target: target.clone(),
                                                                    operator: *operator,
                                                                    position: *position,
                                                                    span: *span },
      Expression::Binary { operator,
                           left,
                           right,
                           span, } => DesugaredExpression::Binary { operator: *operator,
                                                                    left: Box::new(self.desugar_expression(left)),
                                                                    right: Box::new(self.desugar_expression(right)),
                                                                    span: *span },
      Expression::Unary { operator,
                          operand,
                          span, } => DesugaredExpression::Unary { operator: *operator,
                                                                  operand:
                                                                    Box::new(self.desugar_expression(operand)),
                                                                  span: *span },
      Expression::ResultPropagation { value,
                                      span, } => self.desugar_result_propagation(value, *span),
      Expression::Call { function,
                         arguments,
                         parameter_types,
                         return_type,
                         span, } =>
      {
        DesugaredExpression::Call { function: function.clone(),
                                    arguments: arguments.iter().map(|value| self.desugar_expression(value)).collect(),
                                    parameter_types: parameter_types.clone(),
                                    return_type: return_type.clone(),
                                    span: *span }
      }
      Expression::If { condition,
                       then_block,
                       else_block,
                       result_type,
                       span, } => DesugaredExpression::If { condition: Box::new(self.desugar_expression(condition)),
                                                            then_block: Box::new(self.desugar_block(then_block)),
                                                            else_block: Box::new(self.desugar_block(else_block)),
                                                            result_type: result_type.clone(),
                                                            span: *span },
      Expression::Match { selector,
                          selector_type,
                          arms,
                          result_type,
                          span, } =>
      {
        DesugaredExpression::Match { selector: Box::new(self.desugar_expression(selector)),
                                     selector_type: selector_type.clone(),
                                     arms: arms.iter()
                                               .map(|arm| DesugaredMatchArm { pattern: arm.pattern.clone(),
                                                                              body: self.desugar_block(&arm.body),
                                                                              span: arm.span })
                                               .collect(),
                                     result_type: result_type.clone(),
                                     span: *span }
      }
    }
  }

  fn desugar_result_propagation(&mut self, value: &Expression, span: Span) -> DesugaredExpression
  {
    let desugared_value = self.desugar_expression(value);
    let Some((success_type, error_type)) = self.result_parts_of_expression(value, span)
    else
    {
      return desugared_value;
    };
    if !self.return_accepts_error(&error_type, span)
    {
      return desugared_value;
    }
    let index = self.next_binding;
    self.next_binding += 1;
    DesugaredExpression::ResultMatch { value: Box::new(desugared_value),
                                       success_binding: format!("__xs_try_ok_{index}"),
                                       error_binding: format!("__xs_try_error_{index}"),
                                       success_type,
                                       error_type,
                                       span }
  }

  fn return_accepts_error(&mut self, error_type: &Type, span: Span) -> bool
  {
    let Some(return_type) = &self.return_type
    else
    {
      self.diagnostics.push(Diagnostic { code: DiagnosticCode::ReturnMismatch,
                                         message: "'@' desugaring requires the enclosing function to return \
                                                   Result<_, E>"
                                                                .to_string(),
                                         span });
      return false;
    };
    let Some((_, return_error)) = result_type_parts(return_type)
    else
    {
      self.diagnostics.push(Diagnostic { code: DiagnosticCode::ReturnMismatch,
                                         message: "'@' desugaring requires the enclosing function to return \
                                                   Result<_, E>"
                                                                .to_string(),
                                         span });
      return false;
    };
    if &return_error == error_type
    {
      return true;
    }
    self.diagnostics
        .push(Diagnostic { code: DiagnosticCode::ReturnMismatch,
                           message: "'@' desugaring error type does not match function return error".to_string(),
                           span });
    false
  }

  fn result_parts_of_expression(&mut self, expression: &Expression, span: Span) -> Option<(Type, Type)>
  {
    let Some(ty) = self.expression_type(expression)
    else
    {
      self.diagnostics
          .push(Diagnostic { code: DiagnosticCode::RequiresResult,
                             message: "'@' desugaring requires a typed Result<T, E> expression".to_string(),
                             span });
      return None;
    };
    let parts = result_type_parts(&ty);
    if parts.is_none()
    {
      self.diagnostics.push(Diagnostic { code: DiagnosticCode::RequiresResult,
                                         message: "'@' desugaring requires a Result<T, E> expression".to_string(),
                                         span });
    }
    parts
  }

  fn expression_type(&mut self, expression: &Expression) -> Option<Type>
  {
    match expression
    {
      Expression::Literal { literal, .. } => literal_default_type(literal),
      Expression::Local { name,
                          span, } => self.find_local(name).map(|local| local.ty.clone()).or_else(|| {
                                                                                          self.diagnostics
                                                                 .push(Diagnostic { code:
                                                                                      DiagnosticCode::UnknownLocal,
                                                                                    message:
                                                                                      format!("unknown local '{name}'"),
                                                                                    span: *span });
                                                                                          None
                                                                                        }),
      Expression::Field { path } => Some(path.ty.clone()),
      Expression::Member { field_type, .. } => Some(field_type.as_ref().clone()),
      Expression::Object { nominal_type, .. } => Some(Type::Named(nominal_type.clone())),
      Expression::Array { elements, .. } =>
      {
        let first = self.expression_type(elements.first()?)?;
        elements.iter()
                .skip(1)
                .all(|value| self.expression_type(value).as_ref() == Some(&first))
                .then(|| Type::Array { element: Box::new(first),
                                       length: u64::try_from(elements.len()).ok() })
      }
      Expression::Set { elements, .. } =>
      {
        let first = self.expression_type(elements.first()?)?;
        elements.iter()
                .skip(1)
                .all(|value| self.expression_type(value).as_ref() == Some(&first))
                .then(|| Type::Set { element: Box::new(first) })
      }
      Expression::Map { entries, .. } =>
      {
        let first = entries.first()?;
        let key = self.expression_type(&first.key)?;
        let value = self.expression_type(&first.value)?;
        entries.iter()
               .skip(1)
               .all(|entry| {
                 self.expression_type(&entry.key).as_ref() == Some(&key) &&
                 self.expression_type(&entry.value).as_ref() == Some(&value)
               })
               .then(|| Type::Map { key: Box::new(key),
                                    value: Box::new(value) })
      }
      Expression::Tuple { tuple_type, .. } => Some(tuple_type.as_ref().clone()),
      Expression::TupleElement { element_type, .. } => Some(element_type.as_ref().clone()),
      Expression::Index { element_type, .. } => Some(element_type.as_ref().clone()),
      Expression::ArrayLength { .. } => Some(Type::Primitive(PrimitiveType::Int)),
      Expression::Assign { value, .. } => self.expression_type(value),
      Expression::AssignField { value, .. } => self.expression_type(value),
      Expression::Update { target,
                           span,
                           .. } => self.find_local(target).map(|local| local.ty.clone()).or_else(|| {
                                                                                          self.diagnostics
                                                                  .push(Diagnostic { code:
                                                                                       DiagnosticCode::UnknownLocal,
                                                                                     message:
                                                                                       format!("unknown local \
                                                                                                '{target}'"),
                                                                                     span: *span });
                                                                                          None
                                                                                        }),
      Expression::Binary { operator,
                           left,
                           right,
                           .. } => self.binary_expression_type(*operator, left, right),
      Expression::Unary { operator,
                          operand,
                          .. } => unary_expression_type(*operator, self.expression_type(operand)?),
      Expression::ResultPropagation { value,
                                      span, } => self.result_parts_of_expression(value, *span)
                                                     .map(|(success, _)| success),
      Expression::Call { return_type, .. } => Some(return_type.as_ref().clone()),
      Expression::If { result_type, .. } => Some(result_type.as_ref().clone()),
      Expression::Match { result_type, .. } => Some(result_type.as_ref().clone()),
    }
  }

  fn desugar_block(&mut self, block: &Block) -> DesugaredBlock
  {
    let local_count = self.locals.len();
    let statements = block.statements
                          .iter()
                          .map(|statement| self.desugar_statement(statement))
                          .collect();
    let tail = block.tail
                    .as_ref()
                    .map(|expression| Box::new(self.desugar_expression(expression)));
    self.locals.truncate(local_count);
    DesugaredBlock { statements,
                     tail,
                     span: block.span }
  }

  fn binary_expression_type(&mut self, operator: BinaryOperator, left: &Expression, right: &Expression)
                            -> Option<Type>
  {
    if operator == BinaryOperator::Coalesce
    {
      let right_type = self.expression_type(right)?;
      if matches!(left, Expression::Literal { literal: Literal::None,
                                              .. })
      {
        return Some(right_type);
      }
      let Type::Optional { element } = self.expression_type(left)?
      else
      {
        return None;
      };
      return (*element == right_type).then_some(*element);
    }
    let mut left_type = self.expression_type(left)?;
    let mut right_type = self.expression_type(right)?;
    if left_type != right_type
    {
      if let Expression::Literal { literal, .. } = left &&
         literal_matches_type(literal, &right_type)
      {
        left_type = right_type.clone();
      }
      else if let Expression::Literal { literal, .. } = right &&
                literal_matches_type(literal, &left_type)
      {
        right_type = left_type.clone();
      }
    }
    if left_type != right_type
    {
      return None;
    }
    let Type::Primitive(primitive) = left_type
    else
    {
      return None;
    };
    match operator
    {
      BinaryOperator::LogicalAnd | BinaryOperator::LogicalOr if primitive == PrimitiveType::Bool =>
      {
        Some(Type::Primitive(PrimitiveType::Bool))
      }
      BinaryOperator::Add |
      BinaryOperator::Sub |
      BinaryOperator::Mul |
      BinaryOperator::Div |
      BinaryOperator::Rem |
      BinaryOperator::BitAnd |
      BinaryOperator::BitOr |
      BinaryOperator::BitXor |
      BinaryOperator::ShiftLeft |
      BinaryOperator::ShiftRight
        if matches!(primitive, PrimitiveType::Long | PrimitiveType::Int) =>
      {
        Some(Type::Primitive(primitive))
      }
      BinaryOperator::Equal | BinaryOperator::NotEqual
        if matches!(primitive, PrimitiveType::Long | PrimitiveType::Int) =>
      {
        Some(Type::Primitive(PrimitiveType::Bool))
      }
      BinaryOperator::Less | BinaryOperator::LessEqual | BinaryOperator::Greater | BinaryOperator::GreaterEqual
        if primitive == PrimitiveType::Long =>
      {
        Some(Type::Primitive(PrimitiveType::Bool))
      }
      _ => None,
    }
  }

  fn find_local(&self, name: &str) -> Option<&Local>
  {
    self.locals.iter().rev().find(|local| local.name == name)
  }
}

#[cfg(test)]
mod tests
{
  use super::*;

  fn span(start: u32, end: u32) -> Span
  {
    Span::new(1, start, end)
  }

  fn local(name: &str, ty: Type) -> Local
  {
    Local { name: name.to_string(),
            ty,
            mutable: false,
            span: span(0, 0) }
  }

  fn primitive(kind: PrimitiveType) -> Type
  {
    Type::Primitive(kind)
  }

  fn named(name: &str) -> Type
  {
    Type::Named(name.to_string())
  }

  #[test]
  fn desugars_result_propagation_to_explicit_result_match()
  {
    let function = Function { name: "try_work".to_string(),
                              return_type: Some(named("Result<()>")),
                              locals: vec![local("work", named("Result<Long, Error>"))],
                              body: vec![Statement::Expr(Expression::ResultPropagation {
                                value: Box::new(Expression::Local { name: "work".to_string(),
                                                                    span: span(4, 8) }),
                                span: span(4, 9),
                              })] };

    let desugared = ResultDesugar::new().desugar_function(&function)
                                        .expect("valid Result propagation should desugar");

    let DesugaredStatement::Expr(DesugaredExpression::ResultMatch { success_binding,
                                                                    error_binding,
                                                                    success_type,
                                                                    error_type,
                                                                    .. }) = &desugared.body[0]
    else
    {
      panic!("expected explicit Result match desugar");
    };
    assert_eq!(success_binding, "__xs_try_ok_0");
    assert_eq!(error_binding, "__xs_try_error_0");
    assert_eq!(success_type, &primitive(PrimitiveType::Long));
    assert_eq!(error_type, &named("Error"));
  }

  #[test]
  fn rejects_non_result_propagation_value()
  {
    let function = Function { name: "bad".to_string(),
                              return_type: Some(named("Result<()>")),
                              locals: vec![local("value", primitive(PrimitiveType::Long))],
                              body: vec![Statement::Expr(Expression::ResultPropagation {
                                value: Box::new(Expression::Local { name: "value".to_string(),
                                                                    span: span(4, 9) }),
                                span: span(4, 10),
                              })] };

    let diagnostics = ResultDesugar::new().desugar_function(&function)
                                          .expect_err("non-Result value cannot desugar");

    assert_eq!(diagnostics[0].code, DiagnosticCode::RequiresResult);
  }

  #[test]
  fn desugars_unit_result_with_default_error()
  {
    let function = Function { name: "try_work".to_string(),
                              return_type: Some(named("Result<()>")),
                              locals: vec![local("work", named("Result<()>"))],
                              body: vec![Statement::Expr(Expression::ResultPropagation {
                                value: Box::new(Expression::Local { name: "work".to_string(),
                                                                    span: span(4, 8) }),
                                span: span(4, 9),
                              })] };

    let desugared = ResultDesugar::new().desugar_function(&function)
                                        .expect("unit Result shorthand should use Error");

    let DesugaredStatement::Expr(DesugaredExpression::ResultMatch { error_type, .. }) = &desugared.body[0]
    else
    {
      panic!("expected explicit Result match desugar");
    };
    assert_eq!(error_type, &named("Error"));
  }

  #[test]
  fn rejects_error_type_mismatch()
  {
    let function = Function { name: "bad".to_string(),
                              return_type: Some(named("Result<Bool, Other>")),
                              locals: vec![local("work", named("Result<Long, Error>"))],
                              body: vec![Statement::Expr(Expression::ResultPropagation {
                                value: Box::new(Expression::Local { name: "work".to_string(),
                                                                    span: span(4, 8) }),
                                span: span(4, 9),
                              })] };

    let diagnostics = ResultDesugar::new().desugar_function(&function)
                                          .expect_err("mismatched error type cannot desugar");

    assert_eq!(diagnostics[0].code, DiagnosticCode::ReturnMismatch);
  }
}
