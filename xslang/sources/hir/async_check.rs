/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 */

#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub struct Span
{
  pub file_id: u64,
  pub start: u32,
  pub end: u32,
}

impl Span
{
  #[must_use]
  pub const fn new(file_id: u64, start: u32, end: u32) -> Self
  {
    Self { file_id,
           start,
           end }
  }
}

#[derive(Clone, Debug, Eq, PartialEq)]
pub struct Function
{
  pub name: String,
  pub is_async: bool,
  pub body: Vec<Statement>,
  pub span: Span,
}

#[derive(Clone, Debug, Eq, PartialEq)]
pub enum Statement
{
  Expr(Expression),
  Block(Vec<Statement>),
  Function(Function),
}

#[derive(Clone, Debug, Eq, PartialEq)]
pub enum Expression
{
  Identifier
  {
    name: String, span: Span
  },
  Await
  {
    value: Box<Expression>, span: Span
  },
  Call
  {
    callee: Box<Expression>, span: Span
  },
}

impl Expression
{
  #[must_use]
  pub const fn span(&self) -> Span
  {
    match self
    {
      Self::Identifier { span, .. } | Self::Await { span, .. } | Self::Call { span, .. } => *span,
    }
  }
}

#[derive(Clone, Debug, Eq, PartialEq)]
pub enum DiagnosticCode
{
  AwaitOutsideAsync,
}

#[derive(Clone, Debug, Eq, PartialEq)]
pub struct Diagnostic
{
  pub code: DiagnosticCode,
  pub message: String,
  pub span: Span,
}

#[derive(Default)]
pub struct AsyncChecker
{
  diagnostics: Vec<Diagnostic>,
}

impl AsyncChecker
{
  #[must_use]
  pub fn new() -> Self
  {
    Self::default()
  }

  #[must_use]
  pub fn check_module(mut self, functions: &[Function]) -> Vec<Diagnostic>
  {
    for function in functions
    {
      self.check_function(function);
    }
    self.diagnostics
  }

  fn check_function(&mut self, function: &Function)
  {
    self.check_statements(&function.body, function.is_async);
  }

  fn check_statements(&mut self, statements: &[Statement], inside_async_function: bool)
  {
    for statement in statements
    {
      match statement
      {
        Statement::Expr(expression) => self.check_expression(expression, inside_async_function),
        Statement::Block(statements) => self.check_statements(statements, inside_async_function),
        Statement::Function(function) => self.check_function(function),
      }
    }
  }

  fn check_expression(&mut self, expression: &Expression, inside_async_function: bool)
  {
    match expression
    {
      Expression::Identifier { .. } =>
      {}
      Expression::Await { value,
                          span, } =>
      {
        if !inside_async_function
        {
          self.diagnostics
              .push(Diagnostic { code: DiagnosticCode::AwaitOutsideAsync,
                                 message: "await expressions are valid only inside async functions".to_string(),
                                 span: *span });
        }
        self.check_expression(value, inside_async_function);
      }
      Expression::Call { callee, .. } => self.check_expression(callee, inside_async_function),
    }
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

  fn await_call(start: u32) -> Statement
  {
    Statement::Expr(Expression::Await {
            value: Box::new(Expression::Call {
                callee: Box::new(Expression::Identifier {
                    name: "fetch".to_string(),
                    span: span(start + 6, start + 11),
                }),
                span: span(start + 6, start + 13),
            }),
            span: span(start, start + 13),
        })
  }

  #[test]
  fn accepts_await_inside_async_function()
  {
    let function = Function { name: "load".to_string(),
                              is_async: true,
                              body: vec![await_call(10)],
                              span: span(0, 40) };

    let diagnostics = AsyncChecker::new().check_module(&[function]);

    assert!(diagnostics.is_empty());
  }

  #[test]
  fn rejects_await_inside_sync_function()
  {
    let function = Function { name: "load".to_string(),
                              is_async: false,
                              body: vec![await_call(10)],
                              span: span(0, 40) };

    let diagnostics = AsyncChecker::new().check_module(&[function]);

    assert_eq!(diagnostics.len(), 1);
    assert_eq!(diagnostics[0].code, DiagnosticCode::AwaitOutsideAsync);
    assert_eq!(diagnostics[0].span, span(10, 23));
  }

  #[test]
  fn nested_function_uses_own_async_context()
  {
    let nested = Function { name: "inner".to_string(),
                            is_async: false,
                            body: vec![await_call(20)],
                            span: span(15, 45) };
    let outer = Function { name: "outer".to_string(),
                           is_async: true,
                           body: vec![Statement::Function(nested)],
                           span: span(0, 60) };

    let diagnostics = AsyncChecker::new().check_module(&[outer]);

    assert_eq!(diagnostics.len(), 1);
    assert_eq!(diagnostics[0].code, DiagnosticCode::AwaitOutsideAsync);
  }
}
