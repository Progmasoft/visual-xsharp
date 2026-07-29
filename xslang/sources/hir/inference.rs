/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 */

use super::async_check::Span;
use super::type_check::{
  BinaryOperator, Expression, Literal, Local, PrimitiveType, Type, UnaryOperator, result_type_parts,
};

#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub enum BindingOperator
{
  Assign,
  InferAssign,
}

#[derive(Clone, Debug, Eq, PartialEq)]
pub struct Binding
{
  pub name: String,
  pub annotation: Option<Type>,
  pub initializer: Option<Expression>,
  pub mutable: bool,
  pub operator: BindingOperator,
  pub span: Span,
}

#[derive(Clone, Debug, Eq, PartialEq)]
pub enum DiagnosticCode
{
  MissingTypeAnnotation,
  InferenceBindingHasAnnotation,
  MissingInferenceInitializer,
  CannotInferType,
  OptionalTypeRequiresAnnotation,
}

#[derive(Clone, Debug, Eq, PartialEq)]
pub struct Diagnostic
{
  pub code: DiagnosticCode,
  pub message: String,
  pub span: Span,
}

pub fn resolve_binding(binding: &Binding, locals: &[Local]) -> Result<Local, Diagnostic>
{
  let ty = match binding.operator
  {
    BindingOperator::Assign => binding.annotation.clone().ok_or_else(|| {
                                                            diagnostic(DiagnosticCode::MissingTypeAnnotation,
                                                                       "a declaration using '=' requires an explicit \
                                                                        type annotation",
                                                                       binding.span)
                                                          })?,
    BindingOperator::InferAssign =>
    {
      if binding.annotation.is_some()
      {
        return Err(diagnostic(DiagnosticCode::InferenceBindingHasAnnotation,
                              "a declaration using ':=' must not contain a type annotation",
                              binding.span));
      }
      let initializer = binding.initializer.as_ref().ok_or_else(|| {
                                                       diagnostic(DiagnosticCode::MissingInferenceInitializer,
                                                                  "a declaration using ':=' requires an initializer",
                                                                  binding.span)
                                                     })?;
      let inferred = infer_expression_type(initializer, locals).ok_or_else(|| {
                                                                 diagnostic(DiagnosticCode::CannotInferType,
                                                                            "the initializer does not provide a type \
                                                                             for this ':=' declaration",
                                                                            binding.span)
                                                               })?;
      if is_optional_type(&inferred)
      {
        return Err(diagnostic(DiagnosticCode::OptionalTypeRequiresAnnotation,
                              "Optional<T> must be written explicitly and cannot be inferred with ':='",
                              binding.span));
      }
      inferred
    }
  };

  Ok(Local { name: binding.name.clone(),
             ty,
             mutable: binding.mutable,
             span: binding.span })
}

#[must_use]
pub fn infer_expression_type(expression: &Expression, locals: &[Local]) -> Option<Type>
{
  match expression
  {
    Expression::Literal { literal, .. } => infer_literal_type(literal),
    Expression::Local { name, .. } => locals.iter()
                                            .rev()
                                            .find(|local| local.name == *name)
                                            .map(|local| local.ty.clone()),
    Expression::Field { path } => Some(path.ty.clone()),
    Expression::Member { field_type, .. } => Some(field_type.as_ref().clone()),
    Expression::Object { nominal_type, .. } => Some(Type::Named(nominal_type.clone())),
    Expression::Array { elements, .. } =>
    {
      let element = homogeneous_expression_type(elements, locals)?;
      Some(Type::Array { element: Box::new(element),
                         length: None })
    }
    Expression::Set { elements, .. } =>
    {
      Some(Type::Set { element: Box::new(homogeneous_expression_type(elements, locals)?) })
    }
    Expression::Map { entries, .. } =>
    {
      let keys = entries.iter().map(|entry| &entry.key).collect::<Vec<_>>();
      let values = entries.iter().map(|entry| &entry.value).collect::<Vec<_>>();
      Some(Type::Map { key: Box::new(homogeneous_expression_refs_type(&keys, locals)?),
                       value: Box::new(homogeneous_expression_refs_type(&values, locals)?) })
    }
    Expression::Tuple { tuple_type, .. } => Some(tuple_type.as_ref().clone()),
    Expression::TupleElement { element_type, .. } => Some(element_type.as_ref().clone()),
    Expression::Index { element_type, .. } => Some(element_type.as_ref().clone()),
    Expression::ArrayLength { .. } => Some(Type::Primitive(PrimitiveType::Int)),
    Expression::Assign { value, .. } => infer_expression_type(value, locals),
    Expression::AssignField { value, .. } => infer_expression_type(value, locals),
    Expression::Update { target, .. } => locals.iter()
                                               .rev()
                                               .find(|local| local.name == *target)
                                               .map(|local| local.ty.clone()),
    Expression::Binary { operator,
                         left,
                         right,
                         .. } => infer_binary_expression_type(*operator, left, right, locals),
    Expression::Unary { operator,
                        operand,
                        .. } => infer_unary_expression_type(*operator, operand, locals),
    Expression::ResultPropagation { value, .. } =>
    {
      let result_type = infer_expression_type(value, locals)?;
      result_type_parts(&result_type).map(|(success_type, _)| success_type)
    }
    Expression::Call { return_type, .. } => Some(return_type.as_ref().clone()),
    Expression::If { result_type, .. } => Some(result_type.as_ref().clone()),
    Expression::Match { result_type, .. } => Some(result_type.as_ref().clone()),
  }
}

fn homogeneous_expression_type(expressions: &[Expression], locals: &[Local]) -> Option<Type>
{
  let references = expressions.iter().collect::<Vec<_>>();
  homogeneous_expression_refs_type(&references, locals)
}

fn homogeneous_expression_refs_type(expressions: &[&Expression], locals: &[Local]) -> Option<Type>
{
  let first = infer_expression_type(expressions.first()?, locals)?;
  expressions.iter()
             .skip(1)
             .all(|value| infer_expression_type(value, locals).as_ref() == Some(&first))
             .then_some(first)
}

fn infer_unary_expression_type(operator: UnaryOperator, operand: &Expression, locals: &[Local]) -> Option<Type>
{
  let Type::Primitive(primitive) = infer_expression_type(operand, locals)?
  else
  {
    return None;
  };
  match operator
  {
    UnaryOperator::Positive | UnaryOperator::Negative
      if matches!(primitive, PrimitiveType::Long | PrimitiveType::Int) =>
    {
      Some(Type::Primitive(primitive))
    }
    UnaryOperator::LogicalNot if primitive == PrimitiveType::Bool => Some(Type::Primitive(PrimitiveType::Bool)),
    _ => None,
  }
}

fn infer_binary_expression_type(operator: BinaryOperator,
                                left: &Expression,
                                right: &Expression,
                                locals: &[Local])
                                -> Option<Type>
{
  let left_type = infer_expression_type(left, locals)?;
  let right_type = infer_expression_type(right, locals)?;
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
    BinaryOperator::Add | BinaryOperator::Sub | BinaryOperator::Mul | BinaryOperator::Div | BinaryOperator::Rem
      if matches!(primitive,
                  PrimitiveType::SFloat | PrimitiveType::LFloat | PrimitiveType::Float | PrimitiveType::Double) =>
    {
      Some(Type::Primitive(primitive))
    }
    BinaryOperator::Equal | BinaryOperator::NotEqual
      if matches!(primitive,
                  PrimitiveType::Long |
                  PrimitiveType::Int |
                  PrimitiveType::SFloat |
                  PrimitiveType::LFloat |
                  PrimitiveType::Float |
                  PrimitiveType::Double) =>
    {
      Some(Type::Primitive(PrimitiveType::Bool))
    }
    BinaryOperator::Less | BinaryOperator::LessEqual | BinaryOperator::Greater | BinaryOperator::GreaterEqual
      if matches!(primitive,
                  PrimitiveType::Long |
                  PrimitiveType::SFloat |
                  PrimitiveType::LFloat |
                  PrimitiveType::Float |
                  PrimitiveType::Double) =>
    {
      Some(Type::Primitive(PrimitiveType::Bool))
    }
    _ => None,
  }
}

fn infer_literal_type(literal: &Literal) -> Option<Type>
{
  if matches!(literal, Literal::String(_))
  {
    return Some(Type::Reference { referent: Box::new(Type::Primitive(PrimitiveType::Str)),
                                  mutable: false });
  }
  let primitive = match literal
  {
    Literal::Bool(_) => PrimitiveType::Bool,
    Literal::Integer(_) => PrimitiveType::Int,
    Literal::Float(_) => PrimitiveType::Float,
    Literal::Char(_) => PrimitiveType::Char,
    Literal::String(_) => unreachable!("string literals are handled as &Str"),
    Literal::None => return None,
    Literal::EnumVariant { enum_type, .. } => return Some(Type::Named(enum_type.clone())),
  };
  Some(Type::Primitive(primitive))
}

fn is_optional_type(ty: &Type) -> bool
{
  match ty
  {
    Type::Optional { .. } => true,
    Type::Named(name) => name == "Optional" || name.starts_with("Optional<"),
    _ => false,
  }
}

fn diagnostic(code: DiagnosticCode, message: impl Into<String>, span: Span) -> Diagnostic
{
  Diagnostic { code,
               message: message.into(),
               span }
}

#[cfg(test)]
mod tests
{
  use super::*;

  fn span() -> Span
  {
    Span::new(1, 0, 1)
  }

  fn literal(literal: Literal) -> Expression
  {
    Expression::Literal { literal,
                          span: span() }
  }

  fn inferred(initializer: Literal) -> Binding
  {
    Binding { name: "value".to_string(),
              annotation: None,
              initializer: Some(literal(initializer)),
              mutable: true,
              operator: BindingOperator::InferAssign,
              span: span() }
  }

  #[test]
  fn infers_documented_literal_types()
  {
    assert_eq!(resolve_binding(&inferred(Literal::Integer("26".to_string())), &[]).unwrap()
                                                                                  .ty,
               Type::Primitive(PrimitiveType::Int));
    assert_eq!(resolve_binding(&inferred(Literal::Float("3.14".to_string())), &[]).unwrap()
                                                                                  .ty,
               Type::Primitive(PrimitiveType::Float));
    assert_eq!(resolve_binding(&inferred(Literal::Char(u32::from(b'A'))), &[]).unwrap()
                                                                              .ty,
               Type::Primitive(PrimitiveType::Char));
    assert_eq!(resolve_binding(&inferred(Literal::String("A".to_string())), &[]).unwrap()
                                                                                .ty,
               Type::Reference { referent: Box::new(Type::Primitive(PrimitiveType::Str)),
                                 mutable: false });
  }

  #[test]
  fn infers_builtin_array_type_without_desugaring()
  {
    let expression = Expression::Array { elements: vec![literal(Literal::Integer("1".to_string())),
                                                        literal(Literal::Integer("2".to_string()))],
                                         span: span() };
    assert_eq!(infer_expression_type(&expression, &[]),
               Some(Type::Array { element: Box::new(Type::Primitive(PrimitiveType::Int)),
                                  length: None }));
  }

  #[test]
  fn infer_assign_requires_an_initializer()
  {
    let binding = Binding { initializer: None,
                            ..inferred(Literal::Bool(true)) };
    assert_eq!(resolve_binding(&binding, &[]).unwrap_err().code,
               DiagnosticCode::MissingInferenceInitializer);
  }

  #[test]
  fn none_requires_an_explicit_optional_annotation()
  {
    assert_eq!(resolve_binding(&inferred(Literal::None), &[]).unwrap_err().code,
               DiagnosticCode::CannotInferType);
  }

  #[test]
  fn assign_requires_an_explicit_annotation()
  {
    let binding = Binding { operator: BindingOperator::Assign,
                            ..inferred(Literal::Integer("1".to_string())) };
    assert_eq!(resolve_binding(&binding, &[]).unwrap_err().code,
               DiagnosticCode::MissingTypeAnnotation);
  }

  #[test]
  fn optional_local_cannot_be_inferred()
  {
    let optional = Local { name: "source".to_string(),
                           ty: Type::Named("Optional<Int>".to_string()),
                           mutable: false,
                           span: span() };
    let binding = Binding { initializer: Some(Expression::Local { name: "source".to_string(),
                                                                  span: span() }),
                            ..inferred(Literal::Bool(true)) };
    assert_eq!(resolve_binding(&binding, &[optional]).unwrap_err().code,
               DiagnosticCode::OptionalTypeRequiresAnnotation);
  }

  #[test]
  fn explicit_optional_annotation_does_not_infer()
  {
    let binding = Binding { name: "maybe".to_string(),
                            annotation: Some(Type::Named("Optional<Int>".to_string())),
                            initializer: Some(literal(Literal::None)),
                            mutable: true,
                            operator: BindingOperator::Assign,
                            span: span() };
    assert_eq!(resolve_binding(&binding, &[]).unwrap().ty,
               Type::Named("Optional<Int>".to_string()));
  }

  #[test]
  fn infers_result_propagation_success_type()
  {
    let result = Local { name: "source".to_string(),
                         ty: Type::Named("Result<Long, Error>".to_string()),
                         mutable: false,
                         span: span() };
    let binding = Binding { name: "value".to_string(),
                            annotation: None,
                            initializer:
                              Some(Expression::ResultPropagation { value:
                                                                     Box::new(Expression::Local { name:
                                                                                                    "source".to_string(),
                                                                                                  span: span() }),
                                                                   span: span() }),
                            mutable: false,
                            operator: BindingOperator::InferAssign,
                            span: span() };

    assert_eq!(resolve_binding(&binding, &[result]).unwrap().ty,
               Type::Primitive(PrimitiveType::Long));
  }
}
