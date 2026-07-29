/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 */

use std::collections::{HashMap, HashSet};
use std::fmt;

use crate::compiler_core::SourceSpan;

use super::type_check::{self, PrimitiveType};

#[derive(Clone, Debug, Eq, PartialEq)]
pub enum TypeRef
{
  Unit,
  Primitive(PrimitiveType),
  Named(String),
  Optional
  {
    element: Box<TypeRef>,
  },
  Result
  {
    success: Box<TypeRef>,
    error: Box<TypeRef>,
  },
  Reference
  {
    referent: Box<TypeRef>,
    mutable: bool,
  },
  Array
  {
    element: Box<TypeRef>,
    length: Option<u64>,
  },
  Map
  {
    key: Box<TypeRef>,
    value: Box<TypeRef>,
  },
  Tuple
  {
    fields: Vec<TupleFieldRef>,
  },
}

#[derive(Clone, Debug, Eq, PartialEq)]
pub struct TupleFieldRef
{
  pub name: Option<String>,
  pub ty: TypeRef,
}

#[must_use]
pub fn type_ref_to_checked(value: &TypeRef) -> Option<type_check::Type>
{
  Some(match value
  {
    TypeRef::Unit => type_check::Type::Unit,
    TypeRef::Primitive(value) => type_check::Type::Primitive(*value),
    TypeRef::Named(value) => type_check::Type::Named(value.clone()),
    TypeRef::Optional { element } => type_check::Type::Optional { element: Box::new(type_ref_to_checked(element)?) },
    TypeRef::Result { success,
                      error, } => type_check::Type::Result { success: Box::new(type_ref_to_checked(success)?),
                                                             error: Box::new(type_ref_to_checked(error)?) },
    TypeRef::Reference { referent,
                         mutable, } => type_check::Type::Reference { referent:
                                                                       Box::new(type_ref_to_checked(referent)?),
                                                                     mutable: *mutable },
    TypeRef::Array { element,
                     length, } => type_check::Type::Array { element: Box::new(type_ref_to_checked(element)?),
                                                            length: *length },
    TypeRef::Map { key,
                   value, } => type_check::Type::Map { key: Box::new(type_ref_to_checked(key)?),
                                                       value: Box::new(type_ref_to_checked(value)?) },
    TypeRef::Tuple { fields } =>
    {
      type_check::Type::Tuple { fields: fields.iter()
                                              .map(|field| {
                                                Some(type_check::TupleFieldType { name: field.name.clone(),
                                                                                  ty:
                                                                                    type_ref_to_checked(&field.ty)? })
                                              })
                                              .collect::<Option<Vec<_>>>()? }
    }
  })
}

#[derive(Clone, Debug, Eq, PartialEq)]
pub struct Parameter
{
  pub name: String,
  pub ty: TypeRef,
  pub span: SourceSpan,
}

#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub enum NominalKind
{
  Class,
  Interface,
  Data,
  Enum,
  EnumData,
}

#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub enum Visibility
{
  Public,
  Private,
  Protected,
  Internal,
}

#[derive(Clone, Debug, Eq, PartialEq)]
pub struct Base
{
  pub ty: TypeRef,
  pub visibility: Visibility,
  pub is_virtual: bool,
  pub span: SourceSpan,
}

#[derive(Clone, Debug, Eq, PartialEq)]
pub struct Field
{
  pub name: String,
  pub ty: TypeRef,
  pub mutable: bool,
  pub span: SourceSpan,
}

#[derive(Clone, Debug, Eq, PartialEq)]
pub struct EnumVariant
{
  pub name: String,
  pub payload: Option<TypeRef>,
  pub tag: u32,
  pub span: SourceSpan,
}

#[derive(Clone, Debug, Eq, PartialEq)]
pub struct NominalType
{
  pub name: String,
  pub kind: NominalKind,
  pub bases: Vec<Base>,
  pub fields: Vec<Field>,
  pub variants: Vec<EnumVariant>,
  pub span: SourceSpan,
}

#[derive(Clone, Debug, Eq, PartialEq)]
pub enum LayoutError
{
  UnknownBase(String),
  InvalidBaseType,
  CategoryMismatch(String),
  Cycle(String),
  DuplicateField(String),
}

impl fmt::Display for LayoutError
{
  fn fmt(&self, formatter: &mut fmt::Formatter<'_>) -> fmt::Result
  {
    match self
    {
      Self::UnknownBase(name) => write!(formatter, "references unknown base '{name}'"),
      Self::InvalidBaseType => formatter.write_str("has a non-nominal base"),
      Self::CategoryMismatch(name) => write!(formatter, "has base '{name}' from a different nominal category"),
      Self::Cycle(name) => write!(formatter, "contains an inheritance cycle through '{name}'"),
      Self::DuplicateField(name) => write!(formatter, "has ambiguous inherited field '{name}'"),
    }
  }
}

pub(crate) fn validate_layouts(declarations: &[NominalType]) -> Vec<(String, LayoutError)>
{
  let definitions = declarations.iter()
                                .cloned()
                                .map(|declaration| (declaration.name.clone(), declaration))
                                .collect::<HashMap<_, _>>();
  declarations.iter()
              .filter(|declaration| declaration.kind == NominalKind::Data)
              .filter_map(|declaration| {
                resolved_fields(declaration, &definitions).err()
                                                          .map(|error| (declaration.name.clone(), error))
              })
              .collect()
}

pub(crate) fn resolved_fields(definition: &NominalType,
                              definitions: &HashMap<String, NominalType>)
                              -> Result<Vec<Field>, LayoutError>
{
  let mut visiting = HashSet::new();
  let mut names = HashSet::new();
  let mut fields = Vec::new();
  collect_fields(definition, definitions, &mut visiting, &mut names, &mut fields)?;
  Ok(fields)
}

fn collect_fields(definition: &NominalType,
                  definitions: &HashMap<String, NominalType>,
                  visiting: &mut HashSet<String>,
                  names: &mut HashSet<String>,
                  fields: &mut Vec<Field>)
                  -> Result<(), LayoutError>
{
  if !visiting.insert(definition.name.clone())
  {
    return Err(LayoutError::Cycle(definition.name.clone()));
  }
  for base in &definition.bases
  {
    let TypeRef::Named(name) = &base.ty
    else
    {
      return Err(LayoutError::InvalidBaseType);
    };
    let base = definitions.get(name)
                          .ok_or_else(|| LayoutError::UnknownBase(name.clone()))?;
    if base.kind != definition.kind
    {
      return Err(LayoutError::CategoryMismatch(name.clone()));
    }
    collect_fields(base, definitions, visiting, names, fields)?;
  }
  for field in &definition.fields
  {
    if !names.insert(field.name.clone())
    {
      return Err(LayoutError::DuplicateField(field.name.clone()));
    }
    fields.push(field.clone());
  }
  visiting.remove(&definition.name);
  Ok(())
}

#[derive(Clone, Debug, Eq, PartialEq)]
pub struct Function
{
  pub name: String,
  pub parameters: Vec<Parameter>,
  pub return_type: TypeRef,
  pub flags: u32,
  pub span: SourceSpan,
  pub body_present: bool,
  pub body: Option<Vec<type_check::Statement>>,
}

impl Function
{
  #[must_use]
  pub fn as_type_checked_input(&self) -> Option<type_check::Function>
  {
    let body = self.body.clone()?;
    let return_type = if self.return_type == TypeRef::Unit
    {
      None
    }
    else
    {
      Some(type_ref_to_checked(&self.return_type)?)
    };
    let locals = self.parameters
                     .iter()
                     .map(|parameter| {
                       Some(type_check::Local { name: parameter.name.clone(),
                                                               ty: type_ref_to_checked(&parameter.ty)?,
                                                               mutable: false,
                                                               span: crate::hir::async_check::Span::new(
                                                                 parameter.span.file_id,
                                                                 u32::try_from(parameter.span.start_offset).ok()?,
                                                                 u32::try_from(parameter.span.end_offset).ok()?), })
                     })
                     .collect::<Option<Vec<_>>>()?;
    Some(type_check::Function { name: self.name.clone(),
                                return_type,
                                locals,
                                body })
  }
}

#[derive(Clone, Debug, Eq, PartialEq)]
pub struct Module
{
  pub name: Option<String>,
  pub nominal_types: Vec<NominalType>,
  pub functions: Vec<Function>,
}

#[cfg(test)]
mod tests
{
  use super::*;

  fn span() -> SourceSpan
  {
    SourceSpan { file_id: 0,
                 start_offset: 0,
                 end_offset: 1,
                 start_line: 1,
                 start_column: 0,
                 end_line: 1,
                 end_column: 1 }
  }

  fn field(name: &str) -> Field
  {
    Field { name: name.to_string(),
            ty: TypeRef::Primitive(PrimitiveType::Long),
            mutable: true,
            span: span() }
  }

  fn data(name: &str, bases: &[&str], fields: &[&str]) -> NominalType
  {
    NominalType { name: name.to_string(),
                  kind: NominalKind::Data,
                  bases: bases.iter()
                              .map(|base| Base { ty: TypeRef::Named((*base).to_string()),
                                                 visibility: Visibility::Internal,
                                                 is_virtual: false,
                                                 span: span() })
                              .collect(),
                  fields: fields.iter().map(|name| field(name)).collect(),
                  variants: Vec::new(),
                  span: span() }
  }

  #[test]
  fn resolves_transitive_multiple_data_bases_before_derived_fields()
  {
    let declarations = [data("Named", &[], &["label"]),
                        data("Tagged", &[], &["tag"]),
                        data("Point", &["Named", "Tagged"], &["x", "y"]),
                        data("ColoredPoint", &["Point"], &["color"])];
    let definitions = declarations.iter()
                                  .cloned()
                                  .map(|value| (value.name.clone(), value))
                                  .collect();
    let fields = resolved_fields(&declarations[3], &definitions).expect("valid data layout");
    assert_eq!(fields.iter().map(|field| field.name.as_str()).collect::<Vec<_>>(),
               ["label", "tag", "x", "y", "color"]);
  }

  #[test]
  fn rejects_ambiguous_inherited_data_field()
  {
    let declarations = [data("Left", &[], &["value"]),
                        data("Right", &[], &["value"]),
                        data("Combined", &["Left", "Right"], &[])];
    let diagnostics = validate_layouts(&declarations);
    assert_eq!(diagnostics, vec![("Combined".to_string(),
                                  LayoutError::DuplicateField("value".to_string()))]);
  }
}
