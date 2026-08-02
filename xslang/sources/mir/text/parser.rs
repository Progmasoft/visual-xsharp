/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 */

use crate::hir::async_check::Span;
use crate::mir::{BasicBlock, BlockId, Function, Local, LocalId, Parameter, Statement, Terminator};
use crate::xlil::{I64BinaryOperation, I64ComparisonOperation, Type, type_from_name};

use super::{SUPPORTED_XMIR_VERSION, is_supported_xmir_version};

mod aggregate;
mod float;
mod i32;
mod i64;
mod integer_operation;
mod scalar;
mod string;

#[derive(Clone, Debug, Eq, PartialEq)]
pub struct XmirParseDiagnostic
{
    pub line: usize,
    pub message: String,
}

pub fn parse_xmir_function(text: &str) -> Result<Function, Vec<XmirParseDiagnostic>>
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
    diagnostics: Vec<XmirParseDiagnostic>,
}

fn type_from_text(text: &str) -> Option<Type>
{
    text.strip_prefix("%t")
        .and_then(|id| id.parse().ok())
        .map(Type::aggregate)
        .or_else(|| text.strip_prefix("%a").and_then(|id| id.parse().ok()).map(Type::array))
        .or_else(|| type_from_name(text))
}

impl Parser<'_>
{
    fn function(&mut self) -> Function
    {
        self.xmir_version();
        let name = self.function_name();
        let return_type = self.return_type();
        let mut function = Function {
            name,
            parameters: Vec::new(),
            return_type,
            locals: Vec::new(),
            blocks: Vec::new(),
        };
        while let Some(line) = self.next_non_empty()
        {
            match line.as_str()
            {
                "parameters" => self.parameters(&mut function),
                "locals" => self.locals(&mut function),
                "control_flow" => self.control_flow(&mut function),
                ".program end" => break,
                other =>
                {
                    self.report(format!("unexpected XMIR section '{other}'"));
                    self.index += 1;
                }
            }
        }
        function
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

    fn return_type(&mut self) -> Type
    {
        let Some(line) = self.current()
        else
        {
            return Type::VOID;
        };
        let Some(type_name) = line.strip_prefix("returns ")
        else
        {
            return Type::VOID;
        };
        self.index += 1;
        let return_type = type_from_text(type_name);
        if return_type.is_none()
        {
            self.report(format!("unknown return type '{type_name}'"));
        }
        return_type.unwrap_or(Type::VOID)
    }

    fn parameters(&mut self, function: &mut Function)
    {
        self.index += 1;
        while let Some(line) = self.current()
        {
            if line.is_empty()
            {
                self.index += 1;
                continue;
            }
            if line == ".end"
            {
                self.index += 1;
                break;
            }
            let Some(name) = line.strip_prefix("parameter ")
            else
            {
                break;
            };
            self.index += 1;
            let local = self.required_parameter_local();
            let value_type = self.required_parameter_type();
            function.parameters.push(Parameter {
                local,
                name: name.to_string(),
                value_type,
                span: span(),
            });
        }
    }

    fn required_parameter_local(&mut self) -> LocalId
    {
        let Some(line) = self.current()
        else
        {
            self.report("missing parameter local".to_string());
            return LocalId(u32::MAX);
        };
        self.index += 1;
        let Some(local) = line.strip_prefix("local ").and_then(|local| local.parse::<u32>().ok())
        else
        {
            self.report("expected parameter local".to_string());
            return LocalId(u32::MAX);
        };
        LocalId(local)
    }

    fn required_parameter_type(&mut self) -> Type
    {
        let Some(line) = self.current()
        else
        {
            self.report("missing parameter type".to_string());
            return Type::VOID;
        };
        self.index += 1;
        let Some(type_name) = line.strip_prefix("type ")
        else
        {
            self.report("expected parameter type".to_string());
            return Type::VOID;
        };
        let value_type = type_from_text(type_name);
        if value_type.is_none()
        {
            self.report(format!("unknown parameter type '{type_name}'"));
        }
        value_type.unwrap_or(Type::VOID)
    }

    fn locals(&mut self, function: &mut Function)
    {
        self.index += 1;
        while let Some(line) = self.current()
        {
            if line.is_empty()
            {
                self.index += 1;
                continue;
            }
            if line == ".end"
            {
                self.index += 1;
                break;
            }
            let Some(id_text) = line.strip_prefix("local ")
            else
            {
                break;
            };
            let id = self.local_id(id_text);
            self.index += 1;
            let name = self.local_name();
            let value_type = self.local_type();
            let mutable = self.local_mutability();
            function.locals.push(Local {
                id,
                name,
                value_type,
                mutable,
                span: span(),
            });
        }
    }

    fn control_flow(&mut self, function: &mut Function)
    {
        self.index += 1;
        while let Some(line) = self.current()
        {
            if line.is_empty()
            {
                self.index += 1;
                continue;
            }
            if line == ".end"
            {
                self.index += 1;
                break;
            }
            let Some(id_text) = line.strip_prefix("block ")
            else
            {
                break;
            };
            let id = self.block_id(id_text);
            self.index += 1;
            function.blocks.push(self.block(id));
        }
    }

    fn block(&mut self, id: BlockId) -> BasicBlock
    {
        let mut block = BasicBlock {
            id,
            statements: Vec::new(),
            terminator: None,
            span: span(),
        };
        if self.current().as_deref() == Some("statements")
        {
            self.index += 1;
            self.statements(&mut block);
        }
        block.terminator = self.terminator();
        block
    }

    fn statements(&mut self, block: &mut BasicBlock)
    {
        while let Some(line) = self.current()
        {
            let Some(kind) = line.strip_prefix("statement ")
            else
            {
                break;
            };
            self.index += 1;
            match kind
            {
                "const.i64" => block.statements.push(self.const_i64_statement()),
                "const.i32" => block.statements.push(self.const_i32_statement()),
                "const.u16" => block.statements.push(self.const_u16_statement()),
                kind if scalar::is_integer_constant(kind) =>
                {
                    block.statements.push(self.integer_constant_statement(kind))
                }
                kind if integer_operation::is_integer_operation(kind) =>
                {
                    block.statements.push(self.integer_operation_statement(kind));
                }
                "const.f32" => block.statements.push(self.const_f32_statement()),
                "const.f64" => block.statements.push(self.const_f64_statement()),
                "const.str" => block.statements.push(self.const_str_statement()),
                "eq.str" | "ne.str" => block.statements.push(self.str_comparison_statement(kind)),
                kind if float::is_float_instruction(kind) => block.statements.push(self.float_statement(kind)),
                "const.bool" => block.statements.push(self.const_bool_statement()),
                "store.local" => block.statements.push(self.store_local_statement()),
                "load.local" => block.statements.push(self.load_local_statement()),
                "aggregate" => block.statements.push(aggregate::statement(self)),
                "extract" => block.statements.push(aggregate::extract_statement(self)),
                "array.get" => block.statements.push(aggregate::array_get_statement(self)),
                "array.set" => block.statements.push(aggregate::array_set_statement(self)),
                "array.length" => block.statements.push(aggregate::array_length_statement(self)),
                "add.i64" => block.statements.push(self.add_i64_statement()),
                "sub.i64" => block.statements.push(self.sub_i64_statement()),
                "mul.i64" => block.statements.push(self.mul_i64_statement()),
                "eq.i64" => block.statements.push(self.eq_i64_statement()),
                "div.i64" | "rem.i64" | "and.i64" | "or.i64" | "xor.i64" | "shl.i64" | "shr.i64" | "lt.i64" |
                "le.i64" | "gt.i64" | "ge.i64" => block.statements.push(self.extended_i64_statement(kind)),
                "add.i32" => block.statements.push(self.i32_statement("add.i32")),
                "sub.i32" => block.statements.push(self.i32_statement("sub.i32")),
                "mul.i32" => block.statements.push(self.i32_statement("mul.i32")),
                "div.i32" | "rem.i32" | "and.i32" | "or.i32" | "xor.i32" | "shl.i32" | "shr.i32" =>
                {
                    block.statements.push(self.i32_statement(kind));
                }
                "eq.i32" => block.statements.push(self.i32_statement("eq.i32")),
                "lt.i32" => block.statements.push(self.i32_statement("lt.i32")),
                "le.i32" => block.statements.push(self.i32_statement("le.i32")),
                "gt.i32" => block.statements.push(self.i32_statement("gt.i32")),
                "ge.i32" => block.statements.push(self.i32_statement("ge.i32")),
                "not.bool" => block.statements.push(self.not_bool_statement()),
                "call" => block.statements.push(self.call_statement()),
                "use" | "move" | "borrow shared" | "borrow mutable" | "borrow end" | "drop" =>
                {
                    self.local_statement(block, kind);
                }
                _ => self.report(format!("unknown statement kind '{kind}'")),
            }
        }
    }

    fn local_statement(&mut self, block: &mut BasicBlock, kind: &str)
    {
        let local = self.statement_local();
        match kind
        {
            "use" => block.statements.push(Statement::Use {
                local,
                span: span(),
            }),
            "move" => block.statements.push(Statement::Move {
                local,
                span: span(),
            }),
            "borrow shared" => block.statements.push(Statement::BorrowShared {
                local,
                span: span(),
            }),
            "borrow mutable" => block.statements.push(Statement::BorrowMutable {
                local,
                span: span(),
            }),
            "borrow end" => block.statements.push(Statement::EndBorrow {
                local,
                span: span(),
            }),
            "drop" => block.statements.push(Statement::Drop {
                local,
                span: span(),
            }),
            _ =>
            {}
        }
    }

    fn terminator(&mut self) -> Option<Terminator>
    {
        let Some(line) = self.current()
        else
        {
            self.report("missing block terminator".to_string());
            return None;
        };
        let Some(kind) = line.strip_prefix("terminator ")
        else
        {
            self.report("expected block terminator".to_string());
            return None;
        };
        self.index += 1;
        match kind
        {
            "missing" => None,
            "return" => Some(self.return_terminator()),
            "goto" => Some(Terminator::Goto(self.goto_target())),
            "branch_if" => Some(self.branch_if_terminator()),
            "panic" => Some(Terminator::Panic),
            "unreachable" => Some(Terminator::Unreachable),
            _ =>
            {
                self.report(format!("unknown terminator '{kind}'"));
                None
            }
        }
    }

    fn return_terminator(&mut self) -> Terminator
    {
        if let Some(line) = self.current() &&
            let Some(local) = line.strip_prefix("value local ")
        {
            self.index += 1;
            return Terminator::Return(Some(self.local_id(local)));
        }
        Terminator::Return(None)
    }

    fn goto_target(&mut self) -> BlockId
    {
        let Some(line) = self.current()
        else
        {
            self.report("missing goto target".to_string());
            return BlockId(0);
        };
        self.index += 1;
        let Some(target) = line.strip_prefix("target block ")
        else
        {
            self.report("expected goto target".to_string());
            return BlockId(0);
        };
        self.block_id(target)
    }

    fn branch_if_terminator(&mut self) -> Terminator
    {
        let condition = self.branch_if_condition();
        let then_block = self.branch_if_block("then");
        let else_block = self.branch_if_block("else");
        Terminator::BranchIf {
            condition,
            then_block,
            else_block,
        }
    }

    fn branch_if_condition(&mut self) -> LocalId
    {
        let Some(line) = self.current()
        else
        {
            self.report("missing branch_if condition".to_string());
            return LocalId(0);
        };
        self.index += 1;
        let Some(local) = line.strip_prefix("condition local ")
        else
        {
            self.report("expected branch_if condition".to_string());
            return LocalId(0);
        };
        self.local_id(local)
    }

    fn branch_if_block(&mut self, field: &str) -> BlockId
    {
        let Some(line) = self.current()
        else
        {
            self.report(format!("missing branch_if {field} block"));
            return BlockId(0);
        };
        self.index += 1;
        let expected = format!("{field} block ");
        let Some(block) = line.strip_prefix(&expected)
        else
        {
            self.report(format!("expected branch_if {field} block"));
            return BlockId(0);
        };
        self.block_id(block)
    }

    fn local_name(&mut self) -> String
    {
        let Some(line) = self.current()
        else
        {
            self.report("missing local name".to_string());
            return String::new();
        };
        self.index += 1;
        match line.strip_prefix("name ")
        {
            Some(name) => name.to_string(),
            None =>
            {
                self.report("expected local name".to_string());
                String::new()
            }
        }
    }

    fn local_mutability(&mut self) -> bool
    {
        let Some(line) = self.current()
        else
        {
            self.report("missing local mutability".to_string());
            return false;
        };
        self.index += 1;
        match line.strip_prefix("mutability ")
        {
            Some("mutable") => true,
            Some("immutable") => false,
            Some(value) =>
            {
                self.report(format!("unknown mutability '{value}'"));
                false
            }
            None =>
            {
                self.report("expected local mutability".to_string());
                false
            }
        }
    }

    fn local_type(&mut self) -> Option<Type>
    {
        let line = self.current()?;
        let type_name = line.strip_prefix("type ")?;
        self.index += 1;
        let value_type = type_from_text(type_name);
        if value_type.is_none()
        {
            self.report(format!("unknown local type '{type_name}'"));
        }
        value_type
    }

    fn statement_local(&mut self) -> LocalId
    {
        let Some(line) = self.current()
        else
        {
            self.report("missing statement local".to_string());
            return LocalId(0);
        };
        self.index += 1;
        let Some(local) = line.strip_prefix("local ")
        else
        {
            self.report("expected statement local".to_string());
            return LocalId(0);
        };
        self.local_id(local)
    }
}
include!("parser/statements.rs");
