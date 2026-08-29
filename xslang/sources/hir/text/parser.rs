/*
 * SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
 * SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.0
 */

use crate::hir::MatchArm;
use crate::hir::symbols::{Module, Symbol, SymbolKind, Visibility};
use crate::hir::type_check::{
    BinaryOperator, Block, Expression, FieldPath, Function, Literal, Local, ObjectField, Statement, Type,
    UnaryOperator, UpdateOperator, UpdatePosition,
};

use super::{SUPPORTED_XHIR_VERSION, is_supported_xhir_version};
mod collection;
mod enum_data;
mod for_each;
mod helpers;
mod literal;
mod match_expression;
mod nominal;
mod tuple;
pub(super) mod type_parser;
mod unary;

use helpers::{parse_import_line, span};
use type_parser::{parse_local_record, parse_type_text, split_type_list};
#[derive(Clone, Debug, Eq, PartialEq)]
pub struct XhirParseDiagnostic
{
    pub line: usize,
    pub message: String,
}

pub fn parse_xhir_module_symbols(text: &str) -> Result<Module, Vec<XhirParseDiagnostic>>
{
    let mut parser = Parser {
        lines: text.lines().collect(),
        index: 0,
        diagnostics: Vec::new(),
    };
    let module = parser.module_symbols();
    if parser.diagnostics.is_empty()
    {
        Ok(module)
    }
    else
    {
        Err(parser.diagnostics)
    }
}

pub fn parse_xhir_function(text: &str) -> Result<Function, Vec<XhirParseDiagnostic>>
{
    let mut parser = Parser {
        lines: text.lines().collect(),
        index: 0,
        diagnostics: Vec::new(),
    };
    let function = parser.function();
    if parser.diagnostics.is_empty()
    {
        Ok(function)
    }
    else
    {
        Err(parser.diagnostics)
    }
}

struct Parser<'a>
{
    lines: Vec<&'a str>,
    index: usize,
    diagnostics: Vec<XhirParseDiagnostic>,
}

impl Parser<'_>
{
    fn module_symbols(&mut self) -> Module
    {
        self.xhir_version();
        let name = self.module_name();
        let mut module = Module {
            name,
            imports: Vec::new(),
            symbols: Vec::new(),
        };
        while let Some(line) = self.next_non_empty()
        {
            match line.as_str()
            {
                "import" => self.import(&mut module),
                "declarations" => self.declarations(&mut module),
                ".program end" => break,
                other =>
                {
                    self.report(format!("unexpected XHIR section '{other}'"));
                    self.index += 1;
                }
            }
        }
        module
    }

    fn function(&mut self) -> Function
    {
        self.xhir_version();
        let name = self.function_name();
        let mut function = Function {
            name,
            return_type: None,
            locals: Vec::new(),
            body: Vec::new(),
        };
        while let Some(line) = self.next_non_empty()
        {
            match line.as_str()
            {
                "signature" => self.signature(&mut function),
                "locals" => self.locals(&mut function),
                "body" => self.body(&mut function),
                ".program end" => break,
                other =>
                {
                    self.report(format!("unexpected XHIR function section '{other}'"));
                    self.index += 1;
                }
            }
        }
        function
    }

    fn module_name(&mut self) -> String
    {
        let Some(line) = self.current()
        else
        {
            self.report("missing module declaration".to_string());
            return String::new();
        };
        let Some(name) = line.strip_prefix("module ")
        else
        {
            self.report("expected module declaration".to_string());
            return String::new();
        };
        self.index += 1;
        name.to_string()
    }

    fn function_name(&mut self) -> String
    {
        let Some(line) = self.current()
        else
        {
            self.report("missing function declaration".to_string());
            return String::new();
        };
        let Some(name) = line.strip_prefix("function ")
        else
        {
            self.report("expected function declaration".to_string());
            return String::new();
        };
        self.index += 1;
        name.to_string()
    }

    fn signature(&mut self, function: &mut Function)
    {
        self.index += 1;
        let Some(line) = self.current()
        else
        {
            self.report("missing function return type".to_string());
            return;
        };
        let Some(name) = line.strip_prefix("returns ")
        else
        {
            self.report("expected function return type".to_string());
            return;
        };
        self.index += 1;
        function.return_type = if name == "void"
        {
            None
        }
        else
        {
            self.parse_type(name)
        };
        if self.current().as_deref() == Some(".end")
        {
            self.index += 1;
        }
    }

    fn locals(&mut self, function: &mut Function)
    {
        self.index += 1;
        while let Some(line) = self.current()
        {
            if line == ".end"
            {
                self.index += 1;
                break;
            }
            let Some(rest) = line.strip_prefix("local ")
            else
            {
                break;
            };
            match parse_local_record(rest)
            {
                Some(local) => function.locals.push(local),
                None => self.report(format!("invalid local record '{line}'")),
            }
            self.index += 1;
        }
    }

    fn body(&mut self, function: &mut Function)
    {
        self.index += 1;
        while let Some(line) = self.current()
        {
            if line == ".end"
            {
                self.index += 1;
                break;
            }
            let Some(statement) = self.statement()
            else
            {
                break;
            };
            function.body.push(statement);
        }
    }

    fn statement(&mut self) -> Option<Statement>
    {
        match self.current()?.as_str()
        {
            line if line.starts_with("let ") => Some(self.let_statement()),
            "expression" => Some(self.expression_statement()),
            line if line.starts_with("assign_index ") => Some(self.assign_index_statement()),
            line if line.starts_with("assign_tuple_element ") => Some(self.assign_tuple_element_statement()),
            "return" => Some(self.return_statement()),
            "if" => Some(self.if_statement()),
            "while" => Some(self.while_statement()),
            "for" => Some(self.for_statement()),
            line if line.starts_with("for_each ") => Some(self.for_each_statement()),
            line if line.starts_with("match ") => Some(self.match_statement()),
            "break" => Some(self.break_statement()),
            "continue" => Some(self.continue_statement()),
            "panic" => Some(self.panic_statement()),
            _ => None,
        }
    }

    fn let_statement(&mut self) -> Statement
    {
        let name = self
            .current()
            .and_then(|line| line.strip_prefix("let ").map(ToString::to_string))
            .unwrap_or_default();
        self.index += 1;
        let ty = self.local_type();
        let mutable = self.local_mutability();
        let initializer = if self.current().as_deref() == Some("initializer")
        {
            self.index += 1;
            self.expression()
        }
        else
        {
            None
        };
        Statement::Let {
            local: Local {
                name,
                ty,
                mutable,
                span: span(),
            },
            initializer,
        }
    }

    fn expression_statement(&mut self) -> Statement
    {
        self.index += 1;
        Statement::Expr(self.expression().unwrap_or(Expression::Literal {
            literal: Literal::None,
            span: span(),
        }))
    }

    fn return_statement(&mut self) -> Statement
    {
        self.index += 1;
        let value = self.expression();
        Statement::Return {
            value,
            span: span(),
        }
    }

    fn panic_statement(&mut self) -> Statement
    {
        self.index += 1;
        Statement::Panic {
            span: span(),
        }
    }

    fn break_statement(&mut self) -> Statement
    {
        self.index += 1;
        Statement::Break {
            span: span(),
        }
    }

    fn continue_statement(&mut self) -> Statement
    {
        self.index += 1;
        Statement::Continue {
            span: span(),
        }
    }

    fn if_statement(&mut self) -> Statement
    {
        self.index += 1;
        self.consume_expression_field("condition");
        let condition = self.expression().unwrap_or(Expression::Literal {
            literal: Literal::None,
            span: span(),
        });
        let then_block = self.named_block("then");
        let else_block = if self.current().as_deref() == Some("else")
        {
            Some(self.named_block("else"))
        }
        else
        {
            None
        };
        Statement::If {
            condition,
            then_block,
            else_block,
            span: span(),
        }
    }

    fn while_statement(&mut self) -> Statement
    {
        self.index += 1;
        self.consume_expression_field("condition");
        let condition = self.expression().unwrap_or(Expression::Literal {
            literal: Literal::None,
            span: span(),
        });
        let body = self.named_block("body");
        Statement::While {
            condition,
            body,
            span: span(),
        }
    }

    fn for_statement(&mut self) -> Statement
    {
        self.index += 1;
        let initializer = if self.current().as_deref() == Some("initializer")
        {
            self.index += 1;
            self.statement().map(Box::new)
        }
        else
        {
            None
        };
        let condition = if self.current().as_deref() == Some("condition")
        {
            self.index += 1;
            self.expression()
        }
        else
        {
            None
        };
        let update = if self.current().as_deref() == Some("update")
        {
            self.index += 1;
            self.expression()
        }
        else
        {
            None
        };
        let body = self.named_block("body");
        Statement::For {
            initializer,
            condition,
            update,
            body,
            span: span(),
        }
    }

    fn match_statement(&mut self) -> Statement
    {
        let line = self.current().unwrap_or_default();
        let selector_type = line
            .strip_prefix("match ")
            .and_then(|name| self.parse_type(name))
            .unwrap_or(Type::Named(String::new()));
        self.index += 1;
        self.consume_expression_field("selector");
        let selector = self.expression().unwrap_or(Expression::Literal {
            literal: Literal::None,
            span: span(),
        });
        let mut arms = Vec::new();
        while let Some(line) = self.current()
        {
            if line == ".end"
            {
                self.index += 1;
                break;
            }
            let Some(pattern) = self.match_pattern(&line)
            else
            {
                self.report(format!("invalid match arm record '{line}'"));
                self.index += 1;
                continue;
            };
            self.index += 1;
            arms.push(MatchArm {
                pattern,
                body: self.named_block("body"),
                span: span(),
            });
        }
        Statement::Match {
            selector,
            selector_type,
            arms,
            span: span(),
        }
    }

    fn named_block(&mut self, name: &str) -> Block
    {
        if self.current().as_deref() == Some(name)
        {
            self.index += 1;
        }
        else
        {
            self.report(format!("expected {name} block"));
        }
        let mut statements = Vec::new();
        let mut tail = None;
        while let Some(line) = self.current()
        {
            if line == ".end"
            {
                self.index += 1;
                break;
            }
            if line == "tail"
            {
                self.index += 1;
                tail = self.expression().map(Box::new);
                continue;
            }
            let Some(statement) = self.statement()
            else
            {
                self.report(format!("unexpected record '{line}' in {name} block"));
                self.index += 1;
                continue;
            };
            statements.push(statement);
        }
        Block {
            statements,
            tail,
            span: span(),
        }
    }
}
include!("parser/expression_and_declarations.rs");
