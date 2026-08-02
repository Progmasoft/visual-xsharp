/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 */

use std::fmt::Write;

use crate::mir::Function;
use crate::xlil::{AggregateType, ArrayType, Type, type_from_name, type_text};

use super::parser::{XmirParseDiagnostic, parse_xmir_function};
use super::{SUPPORTED_XMIR_VERSION, function_to_xmir, is_supported_xmir_version};

#[derive(Clone, Debug, Eq, PartialEq)]
pub struct XmirProgram
{
    pub name: String,
    pub aggregate_types: Vec<AggregateType>,
    pub array_types: Vec<ArrayType>,
    pub functions: Vec<Function>,
}

#[must_use]
pub fn program_to_xmir(name: &str, functions: &[Function]) -> String
{
    program_to_xmir_with_types(name, &[], &[], functions)
}

#[must_use]
pub fn program_to_xmir_with_types(
    name: &str,
    aggregate_types: &[AggregateType],
    array_types: &[ArrayType],
    functions: &[Function],
) -> String
{
    let mut output = format!(".xmir version {SUPPORTED_XMIR_VERSION}\nprogram {name}\n");
    write_types(&mut output, aggregate_types, array_types);
    for function in functions
    {
        let document = function_to_xmir(function);
        let body = document
            .strip_prefix(&format!(".xmir version {SUPPORTED_XMIR_VERSION}\n"))
            .and_then(|text| text.strip_suffix(".program end\n"))
            .expect("canonical XMIR function markers");
        output.push('\n');
        output.push_str(body);
        output.push_str(".function end\n");
    }
    output.push_str(".program end\n");
    output
}

pub fn parse_xmir_program(text: &str) -> Result<XmirProgram, Vec<XmirParseDiagnostic>>
{
    let lines = text.lines().collect::<Vec<_>>();
    let mut diagnostics = Vec::new();
    validate_header(&lines, &mut diagnostics);
    let name = program_name(&lines, &mut diagnostics);
    let mut aggregate_types = Vec::new();
    let mut array_types = Vec::new();
    let mut functions = Vec::new();
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
        if line == "types"
        {
            parse_types(
                &lines,
                &mut index,
                &mut aggregate_types,
                &mut array_types,
                &mut diagnostics,
            );
            continue;
        }
        if !line.starts_with("function ")
        {
            diagnostics.push(diagnostic(
                index + 1,
                format!("unexpected XMIR program record '{line}'"),
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
            diagnostics.push(diagnostic(start + 1, "unterminated XMIR function record".to_string()));
            break;
        }
        let document = function_document(&lines[start..index]);
        match parse_xmir_function(&document)
        {
            Ok(function) => functions.push(function),
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
        diagnostics.push(diagnostic(lines.len().max(1), "missing XMIR program end".to_string()));
    }
    if index < lines.len() && lines[index..].iter().any(|line| !line.trim().is_empty())
    {
        diagnostics.push(diagnostic(index + 1, "records follow XMIR program end".to_string()));
    }
    if diagnostics.is_empty()
    {
        Ok(XmirProgram {
            name,
            aggregate_types,
            array_types,
            functions,
        })
    }
    else
    {
        Err(diagnostics)
    }
}

fn write_types(output: &mut String, aggregates: &[AggregateType], arrays: &[ArrayType])
{
    if aggregates.is_empty() && arrays.is_empty()
    {
        return;
    }
    output.push_str("\ntypes\n");
    for aggregate in aggregates
    {
        let _ = writeln!(output, "  aggregate {} {}", aggregate.id, aggregate.name);
        for field in &aggregate.fields
        {
            let _ = writeln!(output, "    field {}", type_text(*field));
        }
        output.push_str("  .end\n");
    }
    for array in arrays
    {
        let _ = writeln!(output, "  array {}", array.id);
        let _ = writeln!(output, "    element {}", type_text(array.element_type));
        match array.length
        {
            Some(length) =>
            {
                let _ = writeln!(output, "    length {length}");
            }
            None => output.push_str("    length dynamic\n"),
        }
        output.push_str("  .end\n");
    }
    output.push_str(".end\n");
}

fn parse_types(
    lines: &[&str],
    index: &mut usize,
    aggregates: &mut Vec<AggregateType>,
    arrays: &mut Vec<ArrayType>,
    diagnostics: &mut Vec<XmirParseDiagnostic>,
)
{
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
            return;
        }
        if let Some(header) = line.strip_prefix("aggregate ")
        {
            parse_aggregate(lines, index, header, aggregates, diagnostics);
            continue;
        }
        if let Some(id) = line.strip_prefix("array ")
        {
            parse_array(lines, index, id, arrays, diagnostics);
            continue;
        }
        diagnostics.push(diagnostic(*index + 1, format!("unexpected XMIR type record '{line}'")));
        *index += 1;
    }
    diagnostics.push(diagnostic(
        lines.len().max(1),
        "unterminated XMIR types section".to_string(),
    ));
}

fn parse_aggregate(
    lines: &[&str],
    index: &mut usize,
    header: &str,
    aggregates: &mut Vec<AggregateType>,
    diagnostics: &mut Vec<XmirParseDiagnostic>,
)
{
    let line_number = *index + 1;
    let Some((id, name)) = header.split_once(' ')
    else
    {
        diagnostics.push(diagnostic(line_number, "expected aggregate id and name".to_string()));
        *index += 1;
        return;
    };
    let id = parse_id(id, "aggregate", line_number, diagnostics);
    *index += 1;
    let mut fields = Vec::new();
    while *index < lines.len() && lines[*index].trim() != ".end"
    {
        let line = lines[*index].trim();
        if let Some(field) = line.strip_prefix("field ")
        {
            if let Some(field) = parse_type(field)
            {
                fields.push(field);
            }
            else
            {
                diagnostics.push(diagnostic(
                    *index + 1,
                    format!("unknown aggregate field type '{field}'"),
                ));
            }
        }
        else if !line.is_empty()
        {
            diagnostics.push(diagnostic(*index + 1, "expected aggregate field record".to_string()));
        }
        *index += 1;
    }
    if *index == lines.len()
    {
        diagnostics.push(diagnostic(
            line_number,
            "unterminated aggregate type record".to_string(),
        ));
    }
    else
    {
        *index += 1;
    }
    if let Some(id) = id
    {
        aggregates.push(AggregateType {
            id,
            name: name.to_string(),
            fields,
        });
    }
}

fn parse_array(
    lines: &[&str],
    index: &mut usize,
    id: &str,
    arrays: &mut Vec<ArrayType>,
    diagnostics: &mut Vec<XmirParseDiagnostic>,
)
{
    let line_number = *index + 1;
    let id = parse_id(id, "array", line_number, diagnostics);
    *index += 1;
    let mut element_type = None;
    let mut length = None;
    let mut length_seen = false;
    while *index < lines.len() && lines[*index].trim() != ".end"
    {
        let line = lines[*index].trim();
        if let Some(element) = line.strip_prefix("element ")
        {
            element_type = parse_type(element);
            if element_type.is_none()
            {
                diagnostics.push(diagnostic(
                    *index + 1,
                    format!("unknown array element type '{element}'"),
                ));
            }
        }
        else if let Some(value) = line.strip_prefix("length ")
        {
            length_seen = true;
            if value == "dynamic"
            {
                length = None;
            }
            else if let Ok(value) = value.parse()
            {
                length = Some(value);
            }
            else
            {
                diagnostics.push(diagnostic(*index + 1, format!("invalid array length '{value}'")));
            }
        }
        else if !line.is_empty()
        {
            diagnostics.push(diagnostic(
                *index + 1,
                "expected array element or length record".to_string(),
            ));
        }
        *index += 1;
    }
    if *index == lines.len()
    {
        diagnostics.push(diagnostic(line_number, "unterminated array type record".to_string()));
    }
    else
    {
        *index += 1;
    }
    match (id, element_type, length_seen)
    {
        (Some(id), Some(element_type), true) => arrays.push(ArrayType {
            id,
            element_type,
            length,
        }),
        _ => diagnostics.push(diagnostic(line_number, "incomplete array type record".to_string())),
    }
}

fn parse_id(text: &str, kind: &str, line: usize, diagnostics: &mut Vec<XmirParseDiagnostic>) -> Option<u32>
{
    match text.parse()
    {
        Ok(id) => Some(id),
        Err(_) =>
        {
            diagnostics.push(diagnostic(line, format!("invalid {kind} type id '{text}'")));
            None
        }
    }
}

fn parse_type(text: &str) -> Option<Type>
{
    text.strip_prefix("%t")
        .and_then(|id| id.parse().ok())
        .map(Type::aggregate)
        .or_else(|| text.strip_prefix("%a").and_then(|id| id.parse().ok()).map(Type::array))
        .or_else(|| type_from_name(text))
}

fn function_document(lines: &[&str]) -> String
{
    let mut document = format!(".xmir version {SUPPORTED_XMIR_VERSION}\n");
    for line in lines
    {
        let _ = writeln!(document, "{line}");
    }
    document.push_str(".program end\n");
    document
}

fn validate_header(lines: &[&str], diagnostics: &mut Vec<XmirParseDiagnostic>)
{
    let version = lines
        .first()
        .map(|line| line.trim())
        .and_then(|line| line.strip_prefix(".xmir version "))
        .and_then(|version| version.parse::<u32>().ok());
    if !version.is_some_and(is_supported_xmir_version)
    {
        diagnostics.push(diagnostic(
            1,
            format!("expected a supported XMIR version (0 or {SUPPORTED_XMIR_VERSION})"),
        ));
    }
}

fn program_name(lines: &[&str], diagnostics: &mut Vec<XmirParseDiagnostic>) -> String
{
    if let Some(name) = lines
        .get(1)
        .and_then(|line| line.trim().strip_prefix("program "))
        .filter(|name| !name.is_empty())
    {
        return name.to_string();
    }
    diagnostics.push(diagnostic(2, "expected XMIR program name".to_string()));
    String::new()
}

fn diagnostic(line: usize, message: String) -> XmirParseDiagnostic
{
    XmirParseDiagnostic {
        line,
        message,
    }
}

#[cfg(test)]
mod tests
{
    use super::*;
    use crate::mir::{BasicBlock, BlockId, Terminator};
    use crate::xlil::Type;

    #[test]
    fn roundtrips_multiple_xmir_functions_in_one_program()
    {
        let function = |name: &str| Function {
            name: name.to_string(),
            parameters: Vec::new(),
            return_type: Type::VOID,
            locals: Vec::new(),
            blocks: vec![BasicBlock {
                id: BlockId(0),
                statements: Vec::new(),
                terminator: Some(Terminator::Return(None)),
                span: crate::hir::Span::new(0, 0, 1),
            }],
        };
        let functions = [function("first"), function("second")];
        let aggregates = [AggregateType {
            id: 0,
            name: "tuple.0".to_string(),
            fields: vec![Type::I32, Type::I32],
        }];
        let arrays = [ArrayType {
            id: 0,
            element_type: Type::aggregate(0),
            length: Some(2),
        }];
        let text = program_to_xmir_with_types("root", &aggregates, &arrays, &functions);
        let parsed = parse_xmir_program(&text).expect("program should parse");
        assert_eq!(parsed.name, "root");
        assert_eq!(parsed.aggregate_types, aggregates);
        assert_eq!(parsed.array_types, arrays);
        assert_eq!(
            program_to_xmir_with_types(
                &parsed.name,
                &parsed.aggregate_types,
                &parsed.array_types,
                &parsed.functions
            ),
            text
        );
        assert!(parse_xmir_program(&text.replace("length 2", "length invalid")).is_err());
        assert!(parse_xmir_program(text.trim_end_matches(".program end\n")).is_err());
    }
}
