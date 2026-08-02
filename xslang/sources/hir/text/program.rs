/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 */

use std::fmt::Write;

use crate::hir::declarations::NominalType;
use crate::hir::type_check::Function;

use super::declaration::{parse_declarations, write_declarations};
use super::parser::{XhirParseDiagnostic, parse_xhir_function};
use super::{SUPPORTED_XHIR_VERSION, function_to_xhir_with_parameters, is_supported_xhir_version};

#[derive(Clone, Debug, Eq, PartialEq)]
pub struct XhirProgram
{
    pub name: String,
    pub nominal_types: Vec<NominalType>,
    pub functions: Vec<Function>,
    pub parameter_counts: Vec<usize>,
}

#[must_use]
pub fn program_to_xhir(name: &str, functions: &[Function]) -> String
{
    program_to_xhir_with_parameters(name, functions, &vec![0; functions.len()])
}

#[must_use]
pub fn program_to_xhir_with_parameters(name: &str, functions: &[Function], parameter_counts: &[usize]) -> String
{
    program_to_xhir_with_declarations(name, &[], functions, parameter_counts)
}

#[must_use]
pub fn program_to_xhir_with_declarations(
    name: &str,
    nominal_types: &[NominalType],
    functions: &[Function],
    parameter_counts: &[usize],
) -> String
{
    assert_eq!(
        functions.len(),
        parameter_counts.len(),
        "every XHIR function needs a parameter count"
    );
    let mut output = format!(".xhir version {SUPPORTED_XHIR_VERSION}\nprogram {name}\n");
    write_declarations(&mut output, nominal_types);
    for (function, parameter_count) in functions.iter().zip(parameter_counts)
    {
        let document = function_to_xhir_with_parameters(function, *parameter_count);
        let body = document
            .strip_prefix(&format!(".xhir version {SUPPORTED_XHIR_VERSION}\n"))
            .and_then(|text| text.strip_suffix(".program end\n"))
            .expect("canonical XHIR function markers");
        output.push('\n');
        output.push_str(body);
        output.push_str(".function end\n");
    }
    output.push_str(".program end\n");
    output
}

pub fn parse_xhir_program(text: &str) -> Result<XhirProgram, Vec<XhirParseDiagnostic>>
{
    let lines = text.lines().collect::<Vec<_>>();
    let mut diagnostics = Vec::new();
    validate_header(&lines, &mut diagnostics);
    let name = program_name(&lines, &mut diagnostics);
    let mut nominal_types = Vec::new();
    let mut functions = Vec::new();
    let mut parameter_counts = Vec::new();
    let mut index = 2;
    let mut ended = false;
    while index < lines.len()
    {
        let line = lines[index].trim();
        if line.is_empty()
        {
            index += 1;
            continue;
        }
        if line == ".program end"
        {
            ended = true;
            index += 1;
            break;
        }
        if line == "declarations"
        {
            nominal_types.extend(parse_declarations(&lines, &mut index, &mut diagnostics));
            continue;
        }
        if !line.starts_with("function ")
        {
            diagnostics.push(diagnostic(
                index + 1,
                format!("unexpected XHIR program record '{line}'"),
            ));
            index += 1;
            continue;
        }
        let start = index;
        while index < lines.len() && lines[index].trim() != ".function end"
        {
            index += 1;
        }
        if index == lines.len()
        {
            diagnostics.push(diagnostic(start + 1, "unterminated XHIR function record".to_string()));
            break;
        }
        let function_lines = &lines[start..index];
        let parameter_count = parameter_count(function_lines);
        let document = function_document(function_lines);
        match parse_xhir_function(&document)
        {
            Ok(function) =>
            {
                functions.push(function);
                parameter_counts.push(parameter_count);
            }
            Err(errors) => diagnostics.extend(
                errors
                    .into_iter()
                    .map(|error| diagnostic(start + error.line, error.message)),
            ),
        }
        index += 1;
    }
    if !ended
    {
        diagnostics.push(diagnostic(lines.len().max(1), "missing XHIR program end".to_string()));
    }
    if index < lines.len() && lines[index..].iter().any(|line| !line.trim().is_empty())
    {
        diagnostics.push(diagnostic(index + 1, "records follow XHIR program end".to_string()));
    }
    if diagnostics.is_empty()
    {
        Ok(XhirProgram {
            name,
            nominal_types,
            functions,
            parameter_counts,
        })
    }
    else
    {
        Err(diagnostics)
    }
}

fn function_document(lines: &[&str]) -> String
{
    let mut document = format!(".xhir version {SUPPORTED_XHIR_VERSION}\n");
    let mut parameters = false;
    for line in lines
    {
        let trimmed = line.trim();
        if trimmed == "parameters"
        {
            parameters = true;
            let _ = writeln!(document, "  locals");
        }
        else if parameters && trimmed == ".end"
        {
            parameters = false;
            let _ = writeln!(document, "  .end");
        }
        else if parameters && trimmed.starts_with("parameter ")
        {
            let _ = writeln!(document, "    {}", trimmed.replacen("parameter ", "local ", 1));
        }
        else
        {
            let _ = writeln!(document, "{line}");
        }
    }
    document.push_str(".program end\n");
    document
}

fn parameter_count(lines: &[&str]) -> usize
{
    lines
        .iter()
        .skip_while(|line| line.trim() != "parameters")
        .skip(1)
        .take_while(|line| line.trim() != ".end")
        .filter(|line| line.trim().starts_with("parameter "))
        .count()
}

fn validate_header(lines: &[&str], diagnostics: &mut Vec<XhirParseDiagnostic>)
{
    let version = lines
        .first()
        .map(|line| line.trim())
        .and_then(|line| line.strip_prefix(".xhir version "))
        .and_then(|version| version.parse::<u32>().ok());
    if !version.is_some_and(is_supported_xhir_version)
    {
        diagnostics.push(diagnostic(
            1,
            format!("expected a supported XHIR version (0 or {SUPPORTED_XHIR_VERSION})"),
        ));
    }
}

fn program_name(lines: &[&str], diagnostics: &mut Vec<XhirParseDiagnostic>) -> String
{
    if let Some(name) = lines
        .get(1)
        .and_then(|line| line.trim().strip_prefix("program "))
        .filter(|name| !name.is_empty())
    {
        return name.to_string();
    }
    diagnostics.push(diagnostic(2, "expected XHIR program name".to_string()));
    String::new()
}

fn diagnostic(line: usize, message: String) -> XhirParseDiagnostic
{
    XhirParseDiagnostic {
        line,
        message,
    }
}

#[cfg(test)]
mod tests
{
    use super::*;
    use crate::compiler_core::SourceSpan;
    use crate::hir::declarations::{Base, EnumVariant, Field, NominalKind, TypeRef, Visibility};
    use crate::hir::{Function, Local, PrimitiveType, Span, Statement, Type};

    #[test]
    fn roundtrips_multiple_xhir_functions_in_one_program()
    {
        let functions = [
            Function {
                name: "first".to_string(),
                return_type: None,
                locals: Vec::new(),
                body: vec![Statement::Return {
                    value: None,
                    span: Span::new(0, 0, 1),
                }],
            },
            Function {
                name: "second".to_string(),
                return_type: Some(Type::Primitive(PrimitiveType::Long)),
                locals: vec![Local {
                    name: "value".to_string(),
                    ty: Type::Primitive(PrimitiveType::Long),
                    mutable: false,
                    span: Span::new(0, 0, 1),
                }],
                body: Vec::new(),
            },
        ];
        let source_span = SourceSpan {
            file_id: 0,
            start_offset: 0,
            end_offset: 0,
            start_line: 1,
            start_column: 0,
            end_line: 1,
            end_column: 0,
        };
        let nominal_types = [
            NominalType {
                name: "Marker".to_string(),
                kind: NominalKind::Interface,
                bases: Vec::new(),
                fields: Vec::new(),
                variants: Vec::new(),
                span: source_span.clone(),
            },
            NominalType {
                name: "Named".to_string(),
                kind: NominalKind::Data,
                bases: Vec::new(),
                fields: vec![Field {
                    name: "label".to_string(),
                    ty: TypeRef::Primitive(PrimitiveType::Long),
                    mutable: true,
                    span: source_span.clone(),
                }],
                variants: Vec::new(),
                span: source_span.clone(),
            },
            NominalType {
                name: "Point".to_string(),
                kind: NominalKind::Data,
                bases: vec![Base {
                    ty: TypeRef::Named("Named".to_string()),
                    visibility: Visibility::Internal,
                    is_virtual: false,
                    span: source_span.clone(),
                }],
                fields: vec![Field {
                    name: "x".to_string(),
                    ty: TypeRef::Primitive(PrimitiveType::Long),
                    mutable: true,
                    span: source_span.clone(),
                }],
                variants: Vec::new(),
                span: source_span.clone(),
            },
            NominalType {
                name: "Color".to_string(),
                kind: NominalKind::Enum,
                bases: Vec::new(),
                fields: Vec::new(),
                variants: vec![
                    EnumVariant {
                        name: "Red".to_string(),
                        payload: None,
                        tag: 0,
                        span: source_span.clone(),
                    },
                    EnumVariant {
                        name: "Green".to_string(),
                        payload: None,
                        tag: 1,
                        span: source_span.clone(),
                    },
                ],
                span: source_span.clone(),
            },
            NominalType {
                name: "Value".to_string(),
                kind: NominalKind::EnumData,
                bases: Vec::new(),
                fields: Vec::new(),
                variants: vec![EnumVariant {
                    name: "Number".to_string(),
                    payload: Some(TypeRef::Primitive(PrimitiveType::Long)),
                    tag: 0,
                    span: source_span.clone(),
                }],
                span: source_span,
            },
        ];
        let text = program_to_xhir_with_declarations("root", &nominal_types, &functions, &[0, 1]);
        let parsed = parse_xhir_program(&text).expect("program should parse");
        assert_eq!(parsed.name, "root");
        assert_eq!(parsed.nominal_types.len(), 5);
        assert_eq!(parsed.nominal_types[0].kind, NominalKind::Interface);
        assert_eq!(parsed.nominal_types[2].name, "Point");
        assert_eq!(parsed.nominal_types[2].kind, NominalKind::Data);
        assert_eq!(parsed.nominal_types[2].bases[0].ty, TypeRef::Named("Named".to_string()));
        assert_eq!(parsed.nominal_types[2].bases[0].visibility, Visibility::Internal);
        assert!(!parsed.nominal_types[2].bases[0].is_virtual);
        assert_eq!(parsed.nominal_types[2].fields[0].name, "x");
        assert_eq!(
            parsed.nominal_types[2].fields[0].ty,
            TypeRef::Primitive(PrimitiveType::Long)
        );
        assert!(parsed.nominal_types[2].fields[0].mutable);
        assert_eq!(parsed.nominal_types[3].kind, NominalKind::Enum);
        assert_eq!(parsed.nominal_types[3].variants[1].name, "Green");
        assert_eq!(parsed.nominal_types[3].variants[1].tag, 1);
        assert_eq!(parsed.nominal_types[4].kind, NominalKind::EnumData);
        assert_eq!(
            parsed.nominal_types[4].variants[0].payload,
            Some(TypeRef::Primitive(PrimitiveType::Long))
        );
        assert_eq!(parsed.parameter_counts, vec![0, 1]);
        assert_eq!(
            program_to_xhir_with_declarations(
                &parsed.name,
                &parsed.nominal_types,
                &parsed.functions,
                &parsed.parameter_counts
            ),
            text
        );
        assert!(parse_xhir_program(text.trim_end_matches(".program end\n")).is_err());
    }
}
