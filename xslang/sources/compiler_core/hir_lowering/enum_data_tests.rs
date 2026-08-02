/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 */

use super::*;
use crate::compiler_core::SourceSpan;

struct TreeBuilder
{
    nodes: Vec<SyntaxNode>,
}

impl TreeBuilder
{
    fn new() -> Self
    {
        Self {
            nodes: vec![node(FILE, "", None)],
        }
    }

    fn add(&mut self, parent: usize, kind: u32, text: &str) -> usize
    {
        let index = self.nodes.len();
        self.nodes.push(node(kind, text, Some(parent)));
        self.nodes[parent].children.push(index);
        index
    }

    fn named_type(&mut self, parent: usize, name: &str, flags: u32) -> usize
    {
        let ty = self.add(parent, TYPE_NAMED, name);
        self.nodes[ty].flags = flags;
        let path = self.add(ty, PATH, name);
        self.add(path, IDENTIFIER, name);
        ty
    }

    fn expression_identifier(&mut self, parent: usize, name: &str) -> usize
    {
        let expression = self.add(parent, EXPR_IDENTIFIER, name);
        let path = self.add(expression, PATH, name);
        for segment in name.split("::")
        {
            self.add(path, IDENTIFIER, segment);
        }
        expression
    }

    fn enum_data(&mut self, name: &str, variants: &[(&str, Option<&str>)])
    {
        let declaration = self.add(0, DECL_ENUM, name);
        self.nodes[declaration].flags = DATA_ENUM;
        self.add(declaration, IDENTIFIER, name);
        for (variant_name, payload) in variants
        {
            let variant = self.add(declaration, ENUM_VARIANT, variant_name);
            self.add(variant, IDENTIFIER, variant_name);
            if let Some(payload) = payload
            {
                self.named_type(variant, payload, 0);
            }
        }
    }

    fn returning_constructor(
        &mut self,
        function_name: &str,
        enum_type: &str,
        variant: &str,
        parameter_type: Option<&str>,
    )
    {
        let function = self.add(0, DECL_FUNCTION, function_name);
        self.add(function, IDENTIFIER, function_name);
        if let Some(parameter_type) = parameter_type
        {
            let parameter = self.add(function, PARAMETER, "value");
            self.add(parameter, IDENTIFIER, "value");
            self.named_type(parameter, parameter_type, 0);
        }
        self.named_type(function, enum_type, RETURN_TYPE);
        let block = self.add(function, STMT_BLOCK, "body");
        let statement = self.add(block, STMT_RETURN, "return");
        if parameter_type.is_some()
        {
            let call = self.add(statement, EXPR_CALL, "constructor");
            self.expression_identifier(call, &format!("{enum_type}::{variant}"));
            self.expression_identifier(call, "value");
        }
        else
        {
            self.expression_identifier(statement, &format!("{enum_type}::{variant}"));
        }
    }

    fn finish(self) -> SyntaxTree
    {
        SyntaxTree {
            root: 0,
            nodes: self.nodes,
        }
    }
}

fn node(kind: u32, text: &str, parent: Option<usize>) -> SyntaxNode
{
    SyntaxNode {
        kind,
        token_kind: 0,
        visibility: 0,
        flags: 0,
        parent,
        children: Vec::new(),
        text: text.to_string(),
        span: SourceSpan {
            file_id: 1,
            start_offset: 0,
            end_offset: text.len() as u64,
            start_line: 1,
            start_column: 1,
            end_line: 1,
            end_column: text.len() as u64 + 1,
        },
    }
}

#[test]
fn lowers_exact_int_payload_overload()
{
    let mut tree = TreeBuilder::new();
    tree.enum_data("Value", &[("Number", Some("Int")), ("Number", Some("Long"))]);
    tree.returning_constructor("from_int", "Value", "Number", Some("Int"));
    let module = lower_program(&[tree.finish()]).expect("valid enum-data program");
    let body = module.functions[0]
        .body
        .as_ref()
        .unwrap_or_else(|| panic!("lowered body: {:?}", module.functions[0]));
    let Statement::Return {
        value:
            Some(Expression::EnumData {
                enum_type,
                owner,
                variant,
                tag,
                payload_type: Some(payload_type),
                payload: Some(payload),
                ..
            }),
        ..
    } = &body[0]
    else
    {
        panic!("typed enum-data constructor should become a canonical HIR value");
    };
    assert_eq!(enum_type, "Value");
    assert_eq!(owner, "Value");
    assert_eq!(variant, "Number");
    assert_eq!(*tag, 0);
    assert_eq!(payload_type.as_ref(), &Type::Primitive(PrimitiveType::Int));
    assert!(matches!(payload.as_ref(), Expression::Local { name, .. } if name == "value"));
}

#[test]
fn selects_long_payload_independently_from_int_overload()
{
    let mut tree = TreeBuilder::new();
    tree.enum_data("Value", &[("Number", Some("Int")), ("Number", Some("Long"))]);
    tree.returning_constructor("from_long", "Value", "Number", Some("Long"));
    let module = lower_program(&[tree.finish()]).expect("valid enum-data program");
    let body = module.functions[0]
        .body
        .as_ref()
        .unwrap_or_else(|| panic!("lowered body: {:?}", module.functions[0]));
    let Statement::Return {
        value:
            Some(Expression::EnumData {
                tag,
                payload_type: Some(payload_type),
                ..
            }),
        ..
    } = &body[0]
    else
    {
        panic!("Long overload should be retained");
    };
    assert_eq!(*tag, 1);
    assert_eq!(payload_type.as_ref(), &Type::Primitive(PrimitiveType::Long));
}

#[test]
fn lowers_payload_free_variant_without_call_syntax()
{
    let mut tree = TreeBuilder::new();
    tree.enum_data("Token", &[("Text", Some("String")), ("End", None)]);
    tree.returning_constructor("finish", "Token", "End", None);
    let module = lower_program(&[tree.finish()]).expect("valid enum-data program");
    let body = module.functions[0].body.as_ref().expect("lowered body");
    assert!(matches!(&body[0],
                   Statement::Return { value:
                                         Some(Expression::EnumData { variant,
                                                                     tag: 1,
                                                                     payload: None,
                                                                     .. }),
                                       .. } if variant == "End"));
}

#[test]
fn rejects_invalid_enum_data_hierarchy_before_body_lowering()
{
    let mut tree = TreeBuilder::new();
    tree.enum_data("Invalid", &[("Empty", None)]);
    let error = lower_program(&[tree.finish()]).expect_err("missing typed variant must fail");
    assert!(matches!(error, LoweringError::InvalidEnumData(message) if message.contains("typed variant")));
}
