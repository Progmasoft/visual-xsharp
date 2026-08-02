/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 */

use crate::hir::{MatchArm, async_check::Span};

#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub enum PrimitiveType
{
  Bool,
  Byte,
  SByte,
  Char,
  Short,
  Long,
  Int,
  Integer,
  UShort,
  ULong,
  UInt,
  UInteger,
  SFloat,
  LFloat,
  Float,
  Double,
  Str,
  String,
}

#[derive(Clone, Debug, Eq, PartialEq)]
pub enum Type
{
  Unit,
  Primitive(PrimitiveType),
  Named(String),
  Optional
  {
    element: Box<Type>,
  },
  Result
  {
    success: Box<Type>,
    error: Box<Type>,
  },
  Reference
  {
    referent: Box<Type>,
    mutable: bool,
  },
  Array
  {
    element: Box<Type>,
    length: Option<u64>,
  },
  Set
  {
    element: Box<Type>,
  },
  Map
  {
    key: Box<Type>,
    value: Box<Type>,
  },
  Tuple
  {
    fields: Vec<TupleFieldType>,
  },
}

#[derive(Clone, Debug, Eq, PartialEq)]
pub struct TupleFieldType
{
  pub name: Option<String>,
  pub ty: Type,
}

#[derive(Clone, Debug, Eq, PartialEq)]
pub struct Local
{
  pub name: String,
  pub ty: Type,
  pub mutable: bool,
  pub span: Span,
}

#[derive(Clone, Debug, Eq, PartialEq)]
pub struct FieldPath
{
  pub root: String,
  pub fields: Vec<String>,
  pub ty: Type,
  pub mutable: bool,
  pub span: Span,
}

#[derive(Clone, Debug, Eq, PartialEq)]
pub struct ObjectField
{
  pub name: String,
  pub value: Expression,
  pub span: Span,
}

#[derive(Clone, Debug, Eq, PartialEq)]
pub struct MapEntry
{
  pub key: Expression,
  pub value: Expression,
  pub span: Span,
}

#[derive(Clone, Debug, Eq, PartialEq)]
pub struct TupleFieldValue
{
  pub name: Option<String>,
  pub value: Expression,
  pub span: Span,
}

#[derive(Clone, Debug, Eq, PartialEq)]
pub enum Literal
{
  Bool(bool),
  Integer(String),
  Float(String),
  Char(u32),
  String(String),
  None,
  EnumVariant
  {
    enum_type: String,
    variant: String,
    tag: u32,
  },
}

#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub enum BinaryOperator
{
  Add,
  Sub,
  Mul,
  Div,
  Rem,
  BitAnd,
  BitOr,
  BitXor,
  LogicalAnd,
  LogicalOr,
  Coalesce,
  ShiftLeft,
  ShiftRight,
  Equal,
  NotEqual,
  Less,
  LessEqual,
  Greater,
  GreaterEqual,
}

#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub enum UnaryOperator
{
  LogicalNot,
  Positive,
  Negative,
}

#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub enum UpdateOperator
{
  Increment,
  Decrement,
}

#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub enum UpdatePosition
{
  Prefix,
  Postfix,
}

#[derive(Clone, Debug, Eq, PartialEq)]
pub enum Expression
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
    receiver: Box<Expression>,
    owner: String,
    name: String,
    field_type: Box<Type>,
    span: Span,
  },
  Object
  {
    nominal_type: String,
    fields: Vec<ObjectField>,
    span: Span,
  },
  EnumData
  {
    enum_type: String,
    owner: String,
    variant: String,
    tag: u32,
    payload: Option<Box<Expression>>,
    payload_type: Option<Box<Type>>,
    span: Span,
  },
  Array
  {
    elements: Vec<Expression>, span: Span
  },
  Set
  {
    elements: Vec<Expression>, span: Span
  },
  Map
  {
    entries: Vec<MapEntry>, span: Span
  },
  Tuple
  {
    fields: Vec<TupleFieldValue>,
    tuple_type: Box<Type>,
    span: Span,
  },
  TupleElement
  {
    tuple: Box<Expression>,
    index: u32,
    element_type: Box<Type>,
    span: Span,
  },
  Index
  {
    collection: Box<Expression>,
    index: Box<Expression>,
    element_type: Box<Type>,
    span: Span,
  },
  ArrayLength
  {
    collection: Box<Expression>, span: Span
  },
  Assign
  {
    target: String,
    value: Box<Expression>,
    span: Span,
  },
  AssignField
  {
    target: FieldPath,
    value: Box<Expression>,
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
    left: Box<Expression>,
    right: Box<Expression>,
    span: Span,
  },
  Unary
  {
    operator: UnaryOperator,
    operand: Box<Expression>,
    span: Span,
  },
  OptionalUnwrap
  {
    value: Box<Expression>,
    element_type: Box<Type>,
    span: Span,
  },
  OptionalCoalesceAssign
  {
    target: String,
    value: Box<Expression>,
    optional_type: Box<Type>,
    span: Span,
  },
  OptionalMember
  {
    receiver: Box<Expression>,
    owner: String,
    name: String,
    field_type: Box<Type>,
    result_type: Box<Type>,
    span: Span,
  },
  Call
  {
    function: String,
    arguments: Vec<Expression>,
    parameter_types: Vec<Type>,
    return_type: Box<Type>,
    span: Span,
  },
  If
  {
    condition: Box<Expression>,
    then_block: Box<Block>,
    else_block: Box<Block>,
    result_type: Box<Type>,
    span: Span,
  },
  Match
  {
    selector: Box<Expression>,
    selector_type: Box<Type>,
    arms: Vec<MatchArm>,
    result_type: Box<Type>,
    span: Span,
  },
  ResultPropagation
  {
    value: Box<Expression>, span: Span
  },
}

#[derive(Clone, Debug, Eq, PartialEq)]
pub enum Statement
{
  Let
  {
    local: Local,
    initializer: Option<Expression>,
  },
  Expr(Expression),
  AssignIndex
  {
    target: String,
    index: Expression,
    value: Expression,
    element_type: Type,
    span: Span,
  },
  AssignTupleElement
  {
    target: String,
    index: u32,
    value: Expression,
    tuple_type: Type,
    element_type: Type,
    span: Span,
  },
  Return
  {
    value: Option<Expression>,
    span: Span,
  },
  If
  {
    condition: Expression,
    then_block: Block,
    else_block: Option<Block>,
    span: Span,
  },
  While
  {
    condition: Expression,
    body: Block,
    span: Span,
  },
  For
  {
    initializer: Option<Box<Statement>>,
    condition: Option<Expression>,
    update: Option<Expression>,
    body: Block,
    span: Span,
  },
  ForEach
  {
    binding: Local,
    iterable: Expression,
    iterable_type: Type,
    body: Block,
    span: Span,
  },
  Match
  {
    selector: Expression,
    selector_type: Type,
    arms: Vec<MatchArm>,
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
pub struct Block
{
  pub statements: Vec<Statement>,
  pub tail: Option<Box<Expression>>,
  pub span: Span,
}

#[derive(Clone, Debug, Eq, PartialEq)]
pub struct Function
{
  pub name: String,
  pub return_type: Option<Type>,
  pub locals: Vec<Local>,
  pub body: Vec<Statement>,
}
