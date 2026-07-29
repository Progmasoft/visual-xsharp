/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 */

use std::fmt::Write;

use crate::compiler_core::SourceSpan;
use crate::hir::declarations::{
  Base, EnumVariant, Field, NominalKind, NominalType, TupleFieldRef, TypeRef, Visibility,
};
use crate::hir::type_check::Type;

use super::names::type_name;
use super::parser::{XhirParseDiagnostic, type_parser::parse_type_text};

pub(super) fn write_declarations(output: &mut String, declarations: &[NominalType])
{
  if declarations.is_empty()
  {
    return;
  }
  output.push_str("\ndeclarations\n");
  for declaration in declarations
  {
    let kind = match declaration.kind
    {
      NominalKind::Class => "class",
      NominalKind::Interface => "interface",
      NominalKind::Data => "data",
      NominalKind::Enum => "enum",
      NominalKind::EnumData => "enum data",
    };
    let _ = writeln!(output, "  {kind} {}", declaration.name);
    for base in &declaration.bases
    {
      let ty = type_ref_text(&base.ty);
      let visibility = visibility_text(base.visibility);
      let virtual_suffix = if base.is_virtual
      {
        " virtual"
      }
      else
      {
        ""
      };
      let _ = writeln!(output, "    base {ty} visibility {visibility}{virtual_suffix}");
    }
    for field in &declaration.fields
    {
      let ty = type_ref_text(&field.ty);
      let mutability = if field.mutable
      {
        "mutable"
      }
      else
      {
        "immutable"
      };
      let _ = writeln!(output, "    field {}: {ty} {mutability}", field.name);
    }
    for variant in &declaration.variants
    {
      if let Some(payload) = &variant.payload
      {
        let _ = writeln!(output, "    variant {}: {}", variant.name, type_ref_text(payload));
      }
      else
      {
        let _ = writeln!(output, "    variant {}", variant.name);
      }
    }
    output.push_str("  .end\n");
  }
  output.push_str(".end\n");
}

pub(super) fn parse_declarations(lines: &[&str],
                                 index: &mut usize,
                                 diagnostics: &mut Vec<XhirParseDiagnostic>)
                                 -> Vec<NominalType>
{
  let mut declarations = Vec::new();
  *index += 1;
  while *index < lines.len()
  {
    let line = lines[*index].trim();
    if line.is_empty()
    {
      *index += 1;
      continue;
    }
    if line == ".end"
    {
      *index += 1;
      return declarations;
    }
    let (kind, name) = if let Some(name) = line.strip_prefix("data ")
    {
      (NominalKind::Data, name)
    }
    else if let Some(name) = line.strip_prefix("enum data ")
    {
      (NominalKind::EnumData, name)
    }
    else if let Some(name) = line.strip_prefix("class ")
    {
      (NominalKind::Class, name)
    }
    else if let Some(name) = line.strip_prefix("interface ")
    {
      (NominalKind::Interface, name)
    }
    else if let Some(name) = line.strip_prefix("enum ")
    {
      (NominalKind::Enum, name)
    }
    else
    {
      diagnostics.push(error(*index + 1, format!("unexpected XHIR declaration '{line}'")));
      *index += 1;
      continue;
    };
    let start_line = *index + 1;
    *index += 1;
    let (bases, fields, variants) = parse_members(lines, index, diagnostics);
    declarations.push(NominalType { name: name.to_string(),
                                    kind,
                                    bases,
                                    fields,
                                    variants,
                                    span: source_span(start_line) });
  }
  diagnostics.push(error(lines.len().max(1), "unterminated XHIR declarations section".to_string()));
  declarations
}

fn parse_members(lines: &[&str],
                 index: &mut usize,
                 diagnostics: &mut Vec<XhirParseDiagnostic>)
                 -> (Vec<Base>, Vec<Field>, Vec<EnumVariant>)
{
  let mut bases = Vec::new();
  let mut fields = Vec::new();
  let mut variants = Vec::new();
  while *index < lines.len()
  {
    let line_number = *index + 1;
    let line = lines[*index].trim();
    if line == ".end"
    {
      *index += 1;
      return (bases, fields, variants);
    }
    *index += 1;
    if let Some(base) = line.strip_prefix("base ")
    {
      match parse_base(base, line_number)
      {
        Some(base) => bases.push(base),
        None => diagnostics.push(error(line_number, "invalid nominal base record".to_string())),
      }
      continue;
    }
    if let Some(record) = line.strip_prefix("variant ")
    {
      let (name, payload) = match record.split_once(": ")
      {
        Some((name, ty)) => match parse_type_text(ty).and_then(checked_to_ref)
        {
          Some(ty) => (name, Some(ty)),
          None =>
          {
            diagnostics.push(error(line_number, "invalid enum data payload type".to_string()));
            continue;
          }
        },
        None => (record, None),
      };
      if name.is_empty()
      {
        diagnostics.push(error(line_number, "enum variant name cannot be empty".to_string()));
      }
      else if let Ok(tag) = u32::try_from(variants.len())
      {
        variants.push(EnumVariant { name: name.to_string(),
                                    payload,
                                    tag,
                                    span: source_span(line_number) });
      }
      else
      {
        diagnostics.push(error(line_number, "enum has too many variants".to_string()));
      }
      continue;
    }
    let (record, mutable) = if let Some(record) = line.strip_suffix(" mutable")
    {
      (record, true)
    }
    else if let Some(record) = line.strip_suffix(" immutable")
    {
      (record, false)
    }
    else
    {
      diagnostics.push(error(line_number, "nominal field requires mutability".to_string()));
      continue;
    };
    let Some((name, ty)) = record.strip_prefix("field ").and_then(|record| record.split_once(": "))
    else
    {
      diagnostics.push(error(line_number, "expected nominal field record".to_string()));
      continue;
    };
    let Some(ty) = parse_type_text(ty).and_then(checked_to_ref)
    else
    {
      diagnostics.push(error(line_number, format!("unknown nominal field type '{ty}'")));
      continue;
    };
    fields.push(Field { name: name.to_string(),
                        ty,
                        mutable,
                        span: source_span(line_number) });
  }
  diagnostics.push(error(lines.len().max(1), "unterminated nominal declaration".to_string()));
  (bases, fields, variants)
}

fn parse_base(text: &str, line: usize) -> Option<Base>
{
  let (text, is_virtual) = text.strip_suffix(" virtual").map_or((text, false), |text| (text, true));
  let (ty, visibility) = text.rsplit_once(" visibility ")?;
  let ty = parse_type_text(ty).and_then(checked_to_ref)?;
  let visibility = match visibility
  {
    "public" => Visibility::Public,
    "private" => Visibility::Private,
    "protected" => Visibility::Protected,
    "internal" => Visibility::Internal,
    _ => return None,
  };
  Some(Base { ty,
              visibility,
              is_virtual,
              span: source_span(line) })
}

fn visibility_text(visibility: Visibility) -> &'static str
{
  match visibility
  {
    Visibility::Public => "public",
    Visibility::Private => "private",
    Visibility::Protected => "protected",
    Visibility::Internal => "internal",
  }
}

fn type_ref_text(ty: &TypeRef) -> String
{
  crate::hir::declarations::type_ref_to_checked(ty).map(|ty| type_name(&ty))
                                                   .unwrap_or_else(|| "<unsupported>".to_string())
}

fn checked_to_ref(ty: Type) -> Option<TypeRef>
{
  Some(match ty
  {
    Type::Unit => TypeRef::Unit,
    Type::Primitive(value) => TypeRef::Primitive(value),
    Type::Named(value) => TypeRef::Named(value),
    Type::Optional { element } => TypeRef::Optional { element: Box::new(checked_to_ref(*element)?) },
    Type::Result { success,
                   error, } => TypeRef::Result { success: Box::new(checked_to_ref(*success)?),
                                                 error: Box::new(checked_to_ref(*error)?) },
    Type::Reference { referent,
                      mutable, } => TypeRef::Reference { referent: Box::new(checked_to_ref(*referent)?),
                                                         mutable },
    Type::Array { element,
                  length, } => TypeRef::Array { element: Box::new(checked_to_ref(*element)?),
                                                length },
    Type::Map { key,
                value, } => TypeRef::Map { key: Box::new(checked_to_ref(*key)?),
                                           value: Box::new(checked_to_ref(*value)?) },
    Type::Tuple { fields } => TypeRef::Tuple { fields: fields.into_iter()
                                                             .map(|field| {
                                                               Some(TupleFieldRef { name: field.name,
                                                                                    ty: checked_to_ref(field.ty)? })
                                                             })
                                                             .collect::<Option<Vec<_>>>()? },
    Type::Set { .. } => return None,
  })
}

const fn source_span(line: usize) -> SourceSpan
{
  SourceSpan { file_id: 0,
               start_offset: 0,
               end_offset: 0,
               start_line: line as u64,
               start_column: 0,
               end_line: line as u64,
               end_column: 0 }
}

fn error(line: usize, message: String) -> XhirParseDiagnostic
{
  XhirParseDiagnostic { line,
                        message }
}
