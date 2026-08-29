/*
 * SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
 * SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.0
 */

use std::fmt::Write;

use super::optimizer::{OptimizationPass, OptimizationReport};
use super::verify::{Diagnostic as VerifyDiagnostic, DiagnosticCode as VerifyDiagnosticCode};

#[cfg(test)]
mod aggregate_tests;
mod header;
pub mod parser;
mod program;
#[cfg(test)]
mod storage_tests;
mod writer;

pub use header::{SUPPORTED_XMIR_VERSION, XmirDocumentHeader, is_supported_xmir_version, parse_xmir_header};
pub use parser::{XmirParseDiagnostic, parse_xmir_function};
pub use program::{XmirProgram, parse_xmir_program, program_to_xmir, program_to_xmir_with_types};
pub use writer::function_to_xmir;

#[must_use]
pub fn optimizer_analysis_to_xmir(reports: &[OptimizationReport]) -> String
{
    let mut output = String::new();
    let _ = writeln!(output, "analysis optimizer");
    for report in reports
    {
        let _ = writeln!(output, "  pass {}", optimization_pass_name(report.pass));
        let _ = writeln!(output, "    removed_items {}", report.removed_items);
    }
    output
}

pub fn parse_xmir_optimizer_analysis(text: &str) -> Result<Vec<OptimizationReport>, Vec<XmirParseDiagnostic>>
{
    let mut lines = text.lines().enumerate();
    let Some((_, "analysis optimizer")) = lines.next()
    else
    {
        let message = "expected optimizer analysis section".to_string();
        return Err(vec![XmirParseDiagnostic {
            line: 1,
            message,
        }]);
    };
    let mut reports = Vec::new();
    let mut diagnostics = Vec::new();
    while let Some((line_index, line)) = lines.next()
    {
        let Some(pass_name) = line.strip_prefix("  pass ")
        else
        {
            diagnostics.push(XmirParseDiagnostic {
                line: line_index + 1,
                message: format!("expected optimizer pass record, found '{line}'"),
            });
            continue;
        };
        let pass = parse_optimization_pass(pass_name, line_index + 1, &mut diagnostics);
        let Some((removed_line_index, removed_line)) = lines.next()
        else
        {
            diagnostics.push(XmirParseDiagnostic {
                line: line_index + 1,
                message: "missing removed_items record".to_string(),
            });
            break;
        };
        let removed_items = parse_removed_items(removed_line, removed_line_index + 1, &mut diagnostics);
        if let (Some(pass), Some(removed_items)) = (pass, removed_items)
        {
            reports.push(OptimizationReport {
                pass,
                removed_items,
            });
        }
    }
    if diagnostics.is_empty()
    {
        Ok(reports)
    }
    else
    {
        Err(diagnostics)
    }
}

#[must_use]
pub fn verify_analysis_to_xmir(diagnostics: &[VerifyDiagnostic]) -> String
{
    let mut output = String::new();
    let _ = writeln!(output, "analysis verify");
    for diagnostic in diagnostics
    {
        let _ = writeln!(output, "  diagnostic {}", verify_diagnostic_code_name(&diagnostic.code));
        let _ = writeln!(
            output,
            "    span {}:{}..{}",
            diagnostic.span.file_id, diagnostic.span.start, diagnostic.span.end
        );
        let _ = writeln!(output, "    message {}", diagnostic.message);
    }
    output
}

pub fn parse_xmir_verify_analysis(text: &str) -> Result<Vec<VerifyDiagnostic>, Vec<XmirParseDiagnostic>>
{
    let mut parser = VerifyAnalysisParser {
        lines: text.lines().collect(),
        index: 0,
        diagnostics: Vec::new(),
    };
    let diagnostics = parser.parse();
    if parser.diagnostics.is_empty()
    {
        Ok(diagnostics)
    }
    else
    {
        Err(parser.diagnostics)
    }
}

fn verify_diagnostic_code_name(code: &VerifyDiagnosticCode) -> &'static str
{
    match code
    {
        VerifyDiagnosticCode::EmptyParameterName => "empty_parameter_name",
        VerifyDiagnosticCode::DuplicateParameter => "duplicate_parameter",
        VerifyDiagnosticCode::DuplicateLocal => "duplicate_local",
        VerifyDiagnosticCode::DuplicateBlock => "duplicate_block",
        VerifyDiagnosticCode::MissingTerminator => "missing_terminator",
        VerifyDiagnosticCode::UnknownLocal => "unknown_local",
        VerifyDiagnosticCode::UnknownBlock => "unknown_block",
        VerifyDiagnosticCode::MissingLocalType => "missing_local_type",
        VerifyDiagnosticCode::LocalTypeMismatch => "local_type_mismatch",
        VerifyDiagnosticCode::ReturnTypeMismatch => "return_type_mismatch",
    }
}

fn parse_verify_diagnostic_code(name: &str) -> Option<VerifyDiagnosticCode>
{
    match name
    {
        "empty_parameter_name" => Some(VerifyDiagnosticCode::EmptyParameterName),
        "duplicate_parameter" => Some(VerifyDiagnosticCode::DuplicateParameter),
        "duplicate_local" => Some(VerifyDiagnosticCode::DuplicateLocal),
        "duplicate_block" => Some(VerifyDiagnosticCode::DuplicateBlock),
        "missing_terminator" => Some(VerifyDiagnosticCode::MissingTerminator),
        "unknown_local" => Some(VerifyDiagnosticCode::UnknownLocal),
        "unknown_block" => Some(VerifyDiagnosticCode::UnknownBlock),
        "missing_local_type" => Some(VerifyDiagnosticCode::MissingLocalType),
        "local_type_mismatch" => Some(VerifyDiagnosticCode::LocalTypeMismatch),
        "return_type_mismatch" => Some(VerifyDiagnosticCode::ReturnTypeMismatch),
        _ => None,
    }
}

const fn optimization_pass_name(pass: OptimizationPass) -> &'static str
{
    match pass
    {
        OptimizationPass::RemoveUnreachableBlocks => "remove_unreachable_blocks",
        OptimizationPass::RemoveRedundantEndBorrow => "remove_redundant_end_borrow",
        OptimizationPass::FoldConstI64Add => "fold_const_i64_add",
        OptimizationPass::FoldConstI64Sub => "fold_const_i64_sub",
        OptimizationPass::FoldConstI64Mul => "fold_const_i64_mul",
        OptimizationPass::FoldConstI64Eq => "fold_const_i64_eq",
        OptimizationPass::FoldConstI64Binary => "fold_const_i64_binary",
        OptimizationPass::FoldConstI64Comparison => "fold_const_i64_comparison",
        OptimizationPass::FoldConstI32Binary => "fold_const_i32_binary",
        OptimizationPass::FoldConstIntegerBinary => "fold_const_integer_binary",
        OptimizationPass::FoldConstBoolNot => "fold_const_bool_not",
        OptimizationPass::FoldConstBoolBranch => "fold_const_bool_branch",
        OptimizationPass::SimplifyBoolBranch => "simplify_bool_branch",
        OptimizationPass::CollapseSinglePredecessorGoto => "collapse_single_predecessor_goto",
        OptimizationPass::DeadPureStatement => "dead_pure_statement",
        OptimizationPass::RemoveUnusedLocal => "remove_unused_local",
        OptimizationPass::SimplifySameTargetBranch => "simplify_same_target_branch",
        OptimizationPass::CanonicalizeBlockOrder => "canonicalize_block_order",
    }
}

struct VerifyAnalysisParser<'a>
{
    lines: Vec<&'a str>,
    index: usize,
    diagnostics: Vec<XmirParseDiagnostic>,
}

impl VerifyAnalysisParser<'_>
{
    fn parse(&mut self) -> Vec<VerifyDiagnostic>
    {
        if self.current().as_deref() != Some("analysis verify")
        {
            self.report("expected verify analysis section".to_string());
            return Vec::new();
        }
        self.index += 1;
        let mut parsed = Vec::new();
        while let Some(line) = self.current()
        {
            let Some(code_name) = line.strip_prefix("  diagnostic ").map(ToString::to_string)
            else
            {
                self.report(format!("expected verifier diagnostic record, found '{line}'"));
                self.index += 1;
                continue;
            };
            let code = self.diagnostic_code(&code_name);
            self.index += 1;
            let span = self.span();
            let message = self.message();
            if let (Some(code), Some(span), Some(message)) = (code, span, message)
            {
                parsed.push(VerifyDiagnostic {
                    code,
                    message,
                    span,
                });
            }
        }
        parsed
    }

    fn diagnostic_code(&mut self, name: &str) -> Option<VerifyDiagnosticCode>
    {
        let code = parse_verify_diagnostic_code(name);
        if code.is_none()
        {
            self.report(format!("unknown verifier diagnostic code '{name}'"));
        }
        code
    }

    fn span(&mut self) -> Option<crate::hir::async_check::Span>
    {
        let Some(line) = self.current()
        else
        {
            self.report("missing diagnostic span".to_string());
            return None;
        };
        self.index += 1;
        let Some(text) = line.strip_prefix("    span ").map(ToString::to_string)
        else
        {
            self.report("expected diagnostic span".to_string());
            return None;
        };
        parse_span(&text).or_else(|| {
            self.report(format!("invalid diagnostic span '{text}'"));
            None
        })
    }

    fn message(&mut self) -> Option<String>
    {
        let Some(line) = self.current()
        else
        {
            self.report("missing diagnostic message".to_string());
            return None;
        };
        self.index += 1;
        let Some(message) = line.strip_prefix("    message ").map(ToString::to_string)
        else
        {
            self.report("expected diagnostic message".to_string());
            return None;
        };
        Some(message)
    }

    fn current(&self) -> Option<String>
    {
        self.lines.get(self.index).map(|line| (*line).to_string())
    }

    fn report(&mut self, message: String)
    {
        self.diagnostics.push(XmirParseDiagnostic {
            line: self.index + 1,
            message,
        });
    }
}

fn parse_span(text: &str) -> Option<crate::hir::async_check::Span>
{
    let (file_id, rest) = text.split_once(':')?;
    let (start, end) = rest.split_once("..")?;
    Some(crate::hir::async_check::Span::new(
        file_id.parse().ok()?,
        start.parse().ok()?,
        end.parse().ok()?,
    ))
}

fn parse_optimization_pass(
    name: &str,
    line: usize,
    diagnostics: &mut Vec<XmirParseDiagnostic>,
) -> Option<OptimizationPass>
{
    match name
    {
        "remove_unreachable_blocks" => Some(OptimizationPass::RemoveUnreachableBlocks),
        "remove_redundant_end_borrow" => Some(OptimizationPass::RemoveRedundantEndBorrow),
        "fold_const_i64_add" => Some(OptimizationPass::FoldConstI64Add),
        "fold_const_i64_sub" => Some(OptimizationPass::FoldConstI64Sub),
        "fold_const_i64_mul" => Some(OptimizationPass::FoldConstI64Mul),
        "fold_const_i64_eq" => Some(OptimizationPass::FoldConstI64Eq),
        "fold_const_i64_binary" => Some(OptimizationPass::FoldConstI64Binary),
        "fold_const_i64_comparison" => Some(OptimizationPass::FoldConstI64Comparison),
        "fold_const_i32_binary" => Some(OptimizationPass::FoldConstI32Binary),
        "fold_const_integer_binary" => Some(OptimizationPass::FoldConstIntegerBinary),
        "fold_const_bool_not" => Some(OptimizationPass::FoldConstBoolNot),
        "fold_const_bool_branch" => Some(OptimizationPass::FoldConstBoolBranch),
        "simplify_bool_branch" => Some(OptimizationPass::SimplifyBoolBranch),
        "collapse_single_predecessor_goto" => Some(OptimizationPass::CollapseSinglePredecessorGoto),
        _ =>
        {
            diagnostics.push(XmirParseDiagnostic {
                line,
                message: format!("unknown optimizer pass '{name}'"),
            });
            None
        }
    }
}

fn parse_removed_items(line: &str, line_number: usize, diagnostics: &mut Vec<XmirParseDiagnostic>) -> Option<usize>
{
    let Some(value) = line.strip_prefix("    removed_items ")
    else
    {
        diagnostics.push(XmirParseDiagnostic {
            line: line_number,
            message: "expected removed_items record".to_string(),
        });
        return None;
    };
    match value.parse()
    {
        Ok(value) => Some(value),
        Err(_) =>
        {
            diagnostics.push(XmirParseDiagnostic {
                line: line_number,
                message: format!("invalid removed_items value '{value}'"),
            });
            None
        }
    }
}

include!("text/roundtrip_tests.rs");
