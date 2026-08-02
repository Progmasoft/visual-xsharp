/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 */

use std::fmt::Write;

use super::optimizer::{OptimizationPass, OptimizationReport};
use super::verify::{Diagnostic as VerifyDiagnostic, DiagnosticCode as VerifyDiagnosticCode};

#[cfg(test)]
mod aggregate_tests;
pub mod parser;
mod program;
#[cfg(test)]
mod storage_tests;
mod writer;

pub use parser::{XmirParseDiagnostic, parse_xmir_function};
pub use program::{XmirProgram, parse_xmir_program, program_to_xmir, program_to_xmir_with_types};
pub use writer::function_to_xmir;
/// Latest XMIR text version emitted by the canonical writer.
pub const SUPPORTED_XMIR_VERSION: u32 = 1;
#[must_use]
pub const fn is_supported_xmir_version(version: u32) -> bool
{
  matches!(version, 0 | SUPPORTED_XMIR_VERSION)
}

#[derive(Clone, Debug, Eq, PartialEq)]
pub struct XmirDocumentHeader
{
  pub version: u32,
  pub function: String,
}

#[must_use]
pub fn parse_xmir_header(text: &str) -> Option<XmirDocumentHeader>
{
  let mut lines = text.lines();
  let version = lines.next()?.strip_prefix(".xmir version ")?.parse().ok()?;
  let function = lines.next()?.strip_prefix("function ")?.to_string();
  Some(XmirDocumentHeader { version,
                            function })
}

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
    return Err(vec![XmirParseDiagnostic { line: 1,
                                          message }]);
  };
  let mut reports = Vec::new();
  let mut diagnostics = Vec::new();
  while let Some((line_index, line)) = lines.next()
  {
    let Some(pass_name) = line.strip_prefix("  pass ")
    else
    {
      diagnostics.push(XmirParseDiagnostic { line: line_index + 1,
                                             message: format!("expected optimizer pass record, found '{line}'") });
      continue;
    };
    let pass = parse_optimization_pass(pass_name, line_index + 1, &mut diagnostics);
    let Some((removed_line_index, removed_line)) = lines.next()
    else
    {
      diagnostics.push(XmirParseDiagnostic { line: line_index + 1,
                                             message: "missing removed_items record".to_string() });
      break;
    };
    let removed_items = parse_removed_items(removed_line, removed_line_index + 1, &mut diagnostics);
    if let (Some(pass), Some(removed_items)) = (pass, removed_items)
    {
      reports.push(OptimizationReport { pass,
                                        removed_items });
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
    let _ = writeln!(output,
                     "    span {}:{}..{}",
                     diagnostic.span.file_id, diagnostic.span.start, diagnostic.span.end);
    let _ = writeln!(output, "    message {}", diagnostic.message);
  }
  output
}

pub fn parse_xmir_verify_analysis(text: &str) -> Result<Vec<VerifyDiagnostic>, Vec<XmirParseDiagnostic>>
{
  let mut parser = VerifyAnalysisParser { lines: text.lines().collect(),
                                          index: 0,
                                          diagnostics: Vec::new() };
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
        parsed.push(VerifyDiagnostic { code,
                                       message,
                                       span });
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
    self.diagnostics.push(XmirParseDiagnostic { line: self.index + 1,
                                                message });
  }
}

fn parse_span(text: &str) -> Option<crate::hir::async_check::Span>
{
  let (file_id, rest) = text.split_once(':')?;
  let (start, end) = rest.split_once("..")?;
  Some(crate::hir::async_check::Span::new(file_id.parse().ok()?, start.parse().ok()?, end.parse().ok()?))
}

fn parse_optimization_pass(name: &str,
                           line: usize,
                           diagnostics: &mut Vec<XmirParseDiagnostic>)
                           -> Option<OptimizationPass>
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
      diagnostics.push(XmirParseDiagnostic { line,
                                             message: format!("unknown optimizer pass '{name}'") });
      None
    }
  }
}

fn parse_removed_items(line: &str, line_number: usize, diagnostics: &mut Vec<XmirParseDiagnostic>) -> Option<usize>
{
  let Some(value) = line.strip_prefix("    removed_items ")
  else
  {
    diagnostics.push(XmirParseDiagnostic { line: line_number,
                                           message: "expected removed_items record".to_string() });
    return None;
  };
  match value.parse()
  {
    Ok(value) => Some(value),
    Err(_) =>
    {
      diagnostics.push(XmirParseDiagnostic { line: line_number,
                                             message: format!("invalid removed_items value '{value}'") });
      None
    }
  }
}

#[cfg(test)]
mod tests
{
  use super::*;
  use crate::hir::async_check::Span;
  use crate::mir::{BasicBlock, BlockId, Function, Local, LocalId, Parameter, Statement, Terminator};

  fn span() -> Span
  {
    Span::new(1, 0, 1)
  }

  #[test]
  fn writes_function_as_structured_xmir()
  {
    let function =
      Function { name: "Main".to_string(),
                 parameters: vec![Parameter { local: LocalId(0),
                                              name: "argc".to_string(),
                                              value_type: crate::xlil::Type::I64,
                                              span: span() }],
                 return_type: crate::xlil::Type::VOID,
                 locals: vec![Local { id: LocalId(1),
                                      name: "message".to_string(),
                                      value_type: None,
                                      mutable: false,
                                      span: span() }],
                 blocks: vec![BasicBlock { id: BlockId(0),
                                           statements: vec![Statement::BorrowShared { local: LocalId(1),
                                                                                      span: span() },
                                                            Statement::EndBorrow { local: LocalId(1),
                                                                                   span: span() },],
                                           terminator: Some(Terminator::Return(None)),
                                           span: span() }] };

    let text = function_to_xmir(&function);

    assert!(text.contains(".xmir version 1\nfunction Main"));
    assert!(text.contains("returns void"));
    assert!(text.contains("parameters"));
    assert!(text.contains("parameter argc"));
    assert!(text.contains("type i64"));
    assert!(text.contains("control_flow"));
    assert!(text.contains("statement borrow shared"));
    assert!(text.contains("terminator return"));
    assert!(text.contains(".end\n.program end\n"));
    assert!(!text.contains(".func"));
    assert!(!text.contains("%0"));

    let header = parse_xmir_header(&text).expect("header should parse");
    assert_eq!(header.function, "Main");

    let parsed = parse_xmir_function(&text).expect("XMIR function should parse");
    assert_eq!(parsed.name, "Main");
    assert_eq!(parsed.parameters.len(), 1);
    assert_eq!(parsed.parameters[0].local, LocalId(0));
    assert_eq!(parsed.parameters[0].name, "argc");
    assert_eq!(parsed.parameters[0].value_type, crate::xlil::Type::I64);
    assert_eq!(parsed.locals.len(), 1);
    assert_eq!(parsed.blocks.len(), 1);
    assert_eq!(parsed.blocks[0].statements.len(), 2);
    assert_eq!(parsed.blocks[0].terminator, Some(Terminator::Return(None)));
  }

  #[test]
  fn roundtrips_goto_return_value_panic_and_unreachable()
  {
    let function =
      Function { name: "Flow".to_string(),
                 parameters: vec![],
                 return_type: crate::xlil::Type::I64,
                 locals: vec![Local { id: LocalId(0),
                                      name: "result".to_string(),
                                      value_type: Some(crate::xlil::Type::I64),
                                      mutable: true,
                                      span: span() }],
                 blocks: vec![BasicBlock { id: BlockId(0),
                                           statements: vec![Statement::ConstI64 { local: LocalId(0),
                                                                                  value: 7,
                                                                                  span: span() },
                                                            Statement::Use { local: LocalId(0),
                                                                             span: span() },
                                                            Statement::Move { local: LocalId(0),
                                                                              span: span() },
                                                            Statement::Drop { local: LocalId(0),
                                                                              span: span() }],
                                           terminator: Some(Terminator::Goto(BlockId(1))),
                                           span: span() },
                              BasicBlock { id: BlockId(1),
                                           statements: vec![Statement::BorrowMutable { local: LocalId(0),
                                                                                       span: span() }],
                                           terminator: Some(Terminator::Return(Some(LocalId(0)))),
                                           span: span() },
                              BasicBlock { id: BlockId(2),
                                           statements: vec![],
                                           terminator: Some(Terminator::Panic),
                                           span: span() },
                              BasicBlock { id: BlockId(3),
                                           statements: vec![],
                                           terminator: Some(Terminator::Unreachable),
                                           span: span() }] };

    let text = function_to_xmir(&function);
    let parsed = parse_xmir_function(&text).expect("XMIR function should parse");

    assert_eq!(parsed.name, "Flow");
    assert_eq!(parsed.return_type, crate::xlil::Type::I64);
    assert_eq!(parsed.locals[0].name, "result");
    assert_eq!(parsed.locals[0].value_type, Some(crate::xlil::Type::I64));
    assert!(parsed.locals[0].mutable);
    assert_eq!(parsed.blocks[0].terminator, Some(Terminator::Goto(BlockId(1))));
    assert_eq!(parsed.blocks[1].terminator, Some(Terminator::Return(Some(LocalId(0)))));
    assert_eq!(parsed.blocks[2].terminator, Some(Terminator::Panic));
    assert_eq!(parsed.blocks[3].terminator, Some(Terminator::Unreachable));
    assert!(matches!(parsed.blocks[0].statements[0], Statement::ConstI64 { local:
                                                                             LocalId(0),
                                                                           value: 7,
                                                                           .. }));
    assert!(matches!(parsed.blocks[0].statements[1], Statement::Use { local: LocalId(0),
                                                                      .. }));
    assert!(matches!(parsed.blocks[0].statements[2], Statement::Move { local: LocalId(0),
                                                                       .. }));
    assert!(matches!(parsed.blocks[0].statements[3], Statement::Drop { local: LocalId(0),
                                                                       .. }));
    assert!(matches!(parsed.blocks[1].statements[0], Statement::BorrowMutable { local:
                                                                                  LocalId(0),
                                                                                .. }));
  }

  #[test]
  fn roundtrips_call_statement()
  {
    let function =
      Function { name: "CallFlow".to_string(),
                 parameters: vec![],
                 return_type: crate::xlil::Type::I64,
                 locals: vec![Local { id: LocalId(0),
                                      name: "argument".to_string(),
                                      value_type: Some(crate::xlil::Type::I64),
                                      mutable: false,
                                      span: span() },
                              Local { id: LocalId(1),
                                      name: "result".to_string(),
                                      value_type: Some(crate::xlil::Type::I64),
                                      mutable: false,
                                      span: span() }],
                 blocks: vec![BasicBlock { id: BlockId(0),
                                           statements: vec![Statement::ConstI64 { local: LocalId(0),
                                                                                  value: 7,
                                                                                  span: span() },
                                                            Statement::Call { result: Some(LocalId(1)),
                                                                              function:
                                                                                "xs$App$Callee".to_string(),
                                                                              arguments: vec![LocalId(0)],
                                                                              return_type:
                                                                                crate::xlil::Type::I64,
                                                                              span: span() }],
                                           terminator: Some(Terminator::Return(Some(LocalId(1)))),
                                           span: span() }] };

    let text = function_to_xmir(&function);
    let parsed = parse_xmir_function(&text).expect("XMIR function should parse");

    assert!(matches!(&parsed.blocks[0].statements[1],
                     Statement::Call { result: Some(LocalId(1)),
                                       function,
                                       arguments,
                                       return_type,
                                       .. }
                     if function == "xs$App$Callee" &&
                        arguments == &vec![LocalId(0)] &&
                        *return_type == crate::xlil::Type::I64));
  }

  #[test]
  fn roundtrips_add_i64_statement()
  {
    let function =
      Function { name: "AddFlow".to_string(),
                 parameters: vec![],
                 return_type: crate::xlil::Type::I64,
                 locals: vec![Local { id: LocalId(0),
                                      name: "left".to_string(),
                                      value_type: Some(crate::xlil::Type::I64),
                                      mutable: false,
                                      span: span() },
                              Local { id: LocalId(1),
                                      name: "right".to_string(),
                                      value_type: Some(crate::xlil::Type::I64),
                                      mutable: false,
                                      span: span() },
                              Local { id: LocalId(2),
                                      name: "sum".to_string(),
                                      value_type: Some(crate::xlil::Type::I64),
                                      mutable: false,
                                      span: span() }],
                 blocks: vec![BasicBlock { id: BlockId(0),
                                           statements: vec![Statement::ConstI64 { local: LocalId(0),
                                                                                  value: 2,
                                                                                  span: span() },
                                                            Statement::ConstI64 { local: LocalId(1),
                                                                                  value: 3,
                                                                                  span: span() },
                                                            Statement::AddI64 { result: LocalId(2),
                                                                                left: LocalId(0),
                                                                                right: LocalId(1),
                                                                                span: span() }],
                                           terminator: Some(Terminator::Return(Some(LocalId(2)))),
                                           span: span() }] };

    let text = function_to_xmir(&function);
    let parsed = parse_xmir_function(&text).expect("XMIR function should parse");

    assert!(matches!(parsed.blocks[0].statements[2], Statement::AddI64 { result:
                                                                           LocalId(2),
                                                                         left: LocalId(0),
                                                                         right: LocalId(1),
                                                                         .. }));
  }

  #[test]
  fn roundtrips_sub_i64_statement()
  {
    let function =
      Function { name: "SubFlow".to_string(),
                 parameters: vec![],
                 return_type: crate::xlil::Type::I64,
                 locals: vec![Local { id: LocalId(0),
                                      name: "left".to_string(),
                                      value_type: Some(crate::xlil::Type::I64),
                                      mutable: false,
                                      span: span() },
                              Local { id: LocalId(1),
                                      name: "right".to_string(),
                                      value_type: Some(crate::xlil::Type::I64),
                                      mutable: false,
                                      span: span() },
                              Local { id: LocalId(2),
                                      name: "difference".to_string(),
                                      value_type: Some(crate::xlil::Type::I64),
                                      mutable: false,
                                      span: span() }],
                 blocks: vec![BasicBlock { id: BlockId(0),
                                           statements: vec![Statement::ConstI64 { local: LocalId(0),
                                                                                  value: 8,
                                                                                  span: span() },
                                                            Statement::ConstI64 { local: LocalId(1),
                                                                                  value: 3,
                                                                                  span: span() },
                                                            Statement::SubI64 { result: LocalId(2),
                                                                                left: LocalId(0),
                                                                                right: LocalId(1),
                                                                                span: span() }],
                                           terminator: Some(Terminator::Return(Some(LocalId(2)))),
                                           span: span() }] };

    let text = function_to_xmir(&function);
    let parsed = parse_xmir_function(&text).expect("XMIR function should parse");

    assert!(matches!(parsed.blocks[0].statements[2], Statement::SubI64 { result:
                                                                           LocalId(2),
                                                                         left: LocalId(0),
                                                                         right: LocalId(1),
                                                                         .. }));
  }

  #[test]
  fn roundtrips_mul_i64_statement()
  {
    let function =
      Function { name: "MulFlow".to_string(),
                 parameters: vec![],
                 return_type: crate::xlil::Type::I64,
                 locals: vec![Local { id: LocalId(0),
                                      name: "left".to_string(),
                                      value_type: Some(crate::xlil::Type::I64),
                                      mutable: false,
                                      span: span() },
                              Local { id: LocalId(1),
                                      name: "right".to_string(),
                                      value_type: Some(crate::xlil::Type::I64),
                                      mutable: false,
                                      span: span() },
                              Local { id: LocalId(2),
                                      name: "product".to_string(),
                                      value_type: Some(crate::xlil::Type::I64),
                                      mutable: false,
                                      span: span() }],
                 blocks: vec![BasicBlock { id: BlockId(0),
                                           statements: vec![Statement::ConstI64 { local: LocalId(0),
                                                                                  value: 6,
                                                                                  span: span() },
                                                            Statement::ConstI64 { local: LocalId(1),
                                                                                  value: 7,
                                                                                  span: span() },
                                                            Statement::MulI64 { result: LocalId(2),
                                                                                left: LocalId(0),
                                                                                right: LocalId(1),
                                                                                span: span() }],
                                           terminator: Some(Terminator::Return(Some(LocalId(2)))),
                                           span: span() }] };

    let text = function_to_xmir(&function);
    let parsed = parse_xmir_function(&text).expect("XMIR function should parse");

    assert!(matches!(parsed.blocks[0].statements[2], Statement::MulI64 { result:
                                                                           LocalId(2),
                                                                         left: LocalId(0),
                                                                         right: LocalId(1),
                                                                         .. }));
  }

  #[test]
  fn roundtrips_const_bool_and_eq_i64_statement()
  {
    let function =
      Function { name: "EqFlow".to_string(),
                 parameters: vec![],
                 return_type: crate::xlil::Type::BOOL,
                 locals: vec![Local { id: LocalId(0),
                                      name: "left".to_string(),
                                      value_type: Some(crate::xlil::Type::I64),
                                      mutable: false,
                                      span: span() },
                              Local { id: LocalId(1),
                                      name: "right".to_string(),
                                      value_type: Some(crate::xlil::Type::I64),
                                      mutable: false,
                                      span: span() },
                              Local { id: LocalId(2),
                                      name: "same".to_string(),
                                      value_type: Some(crate::xlil::Type::BOOL),
                                      mutable: false,
                                      span: span() }],
                 blocks: vec![BasicBlock { id: BlockId(0),
                                           statements: vec![Statement::ConstI64 { local: LocalId(0),
                                                                                  value: 7,
                                                                                  span: span() },
                                                            Statement::ConstI64 { local: LocalId(1),
                                                                                  value: 7,
                                                                                  span: span() },
                                                            Statement::EqI64 { result: LocalId(2),
                                                                               left: LocalId(0),
                                                                               right: LocalId(1),
                                                                               span: span() }],
                                           terminator: Some(Terminator::Return(Some(LocalId(2)))),
                                           span: span() }] };

    let text = function_to_xmir(&function);
    let parsed = parse_xmir_function(&text).expect("XMIR function should parse");

    assert!(matches!(parsed.blocks[0].statements[2], Statement::EqI64 { result: LocalId(2),
                                                                        left: LocalId(0),
                                                                        right: LocalId(1),
                                                                        .. }));
  }

  #[test]
  fn roundtrips_branch_if_terminator()
  {
    let function =
      Function { name: "BranchIfFlow".to_string(),
                 parameters: vec![],
                 return_type: crate::xlil::Type::VOID,
                 locals: vec![Local { id: LocalId(0),
                                      name: "condition".to_string(),
                                      value_type: Some(crate::xlil::Type::BOOL),
                                      mutable: false,
                                      span: span() }],
                 blocks: vec![BasicBlock { id: BlockId(0),
                                           statements: vec![Statement::ConstBool { local: LocalId(0),
                                                                                   value: true,
                                                                                   span: span() }],
                                           terminator: Some(Terminator::BranchIf { condition: LocalId(0),
                                                                                   then_block: BlockId(1),
                                                                                   else_block: BlockId(2) }),
                                           span: span() },
                              BasicBlock { id: BlockId(1),
                                           statements: vec![],
                                           terminator: Some(Terminator::Return(None)),
                                           span: span() },
                              BasicBlock { id: BlockId(2),
                                           statements: vec![],
                                           terminator: Some(Terminator::Return(None)),
                                           span: span() }] };

    let text = function_to_xmir(&function);
    let parsed = parse_xmir_function(&text).expect("XMIR function should parse");

    assert!(text.contains("terminator branch_if"));
    assert_eq!(parsed.blocks[0].terminator,
               Some(Terminator::BranchIf { condition: LocalId(0),
                                           then_block: BlockId(1),
                                           else_block: BlockId(2) }));
  }

  #[test]
  fn rejects_non_xmir_header()
  {
    assert_eq!(parse_xmir_header(".func Main : () -> void\n"), None);
  }

  #[test]
  fn rejects_unsupported_xmir_version()
  {
    let diagnostics = parse_xmir_function(".xmir version 2\nfunction Main\nreturns void\n").expect_err("unsupported \
                                                                                                        XMIR version \
                                                                                                        must fail");

    assert!(diagnostics[0].message.contains("unsupported XMIR version 2"));
  }

  #[test]
  fn parses_explicit_end_markers_without_indentation()
  {
    let text = ".xmir version 0\nfunction Main\nreturns void\n\nparameters\nparameter value\nlocal 0\ntype \
                i64\n.end\nlocals\nlocal 1\nname value\ntype i64\nmutability immutable\n.end\ncontrol_flow\nblock \
                0\nstatements\nstatement use\nlocal 1\nterminator return\n.end\n.program end\n";

    let parsed = parse_xmir_function(text).expect("XMIR should not be indentation based");

    assert_eq!(parsed.parameters.len(), 1);
    assert_eq!(parsed.locals.len(), 1);
    assert_eq!(parsed.blocks.len(), 1);
    assert_eq!(parsed.blocks[0].statements.len(), 1);
    assert_eq!(parsed.blocks[0].terminator, Some(Terminator::Return(None)));
  }

  #[test]
  fn rejects_invalid_xmir_function()
  {
    let diagnostics = parse_xmir_function(".xmir version 1\nfunction Main\n\ncontrol_flow\n  block nope\n    \
                                           terminator return\n").expect_err("invalid block id should fail");

    assert_eq!(diagnostics.len(), 1);
  }

  #[test]
  fn parsed_xmir_can_be_structurally_verified()
  {
    let function = Function { name: "Verified".to_string(),
                              parameters: vec![],
                              return_type: crate::xlil::Type::I64,
                              locals: vec![Local { id: LocalId(0),
                                                   name: "value".to_string(),
                                                   value_type: Some(crate::xlil::Type::I64),
                                                   mutable: false,
                                                   span: span() }],
                              blocks: vec![BasicBlock { id: BlockId(0),
                                                        statements: vec![Statement::Use { local: LocalId(0),
                                                                                          span: span() }],
                                                        terminator: Some(Terminator::Return(Some(LocalId(0)))),
                                                        span: span() }] };
    let text = function_to_xmir(&function);
    let parsed = parse_xmir_function(&text).expect("XMIR function should parse");

    assert!(crate::mir::verify::verify_function(&parsed).is_empty());
  }

  #[test]
  fn roundtrips_const_i32_statement()
  {
    let function = Function { name: "main".to_string(),
                              parameters: vec![],
                              return_type: crate::xlil::Type::I32,
                              locals: vec![Local { id: LocalId(0),
                                                   name: "exit_code".to_string(),
                                                   value_type: Some(crate::xlil::Type::I32),
                                                   mutable: false,
                                                   span: span() }],
                              blocks: vec![BasicBlock { id: BlockId(0),
                                                        statements: vec![Statement::ConstI32 { local:
                                                                                                 LocalId(0),
                                                                                               value: 0,
                                                                                               span: span() }],
                                                        terminator: Some(Terminator::Return(Some(LocalId(0)))),
                                                        span: span() }] };
    let text = function_to_xmir(&function);
    let parsed = parse_xmir_function(&text).expect("XMIR i32 const should parse");

    assert_eq!(parsed.name, function.name);
    assert_eq!(parsed.return_type, crate::xlil::Type::I32);
    assert!(matches!(parsed.blocks[0].statements[0], Statement::ConstI32 { local:
                                                                             LocalId(0),
                                                                           value: 0,
                                                                           .. }));
    assert_eq!(parsed.blocks[0].terminator, Some(Terminator::Return(Some(LocalId(0)))));
  }

  #[test]
  fn roundtrips_floating_constant_bit_patterns()
  {
    let function =
      Function { name: "float_bits".to_string(),
                 parameters: vec![],
                 return_type: crate::xlil::Type::F64,
                 locals: vec![Local { id: LocalId(0),
                                      name: "value".to_string(),
                                      value_type: Some(crate::xlil::Type::F64),
                                      mutable: false,
                                      span: span() }],
                 blocks: vec![BasicBlock { id: BlockId(0),
                                           statements: vec![Statement::ConstF64 { local: LocalId(0),
                                                                                  bits: 0x3ff8000000000000,
                                                                                  span: span() }],
                                           terminator: Some(Terminator::Return(Some(LocalId(0)))),
                                           span: span() }] };
    let text = function_to_xmir(&function);
    assert!(text.contains("bits 0x3ff8000000000000"));
    let parsed = parse_xmir_function(&text).expect("floating XMIR should parse");
    assert_eq!(parsed.return_type, crate::xlil::Type::F64);
    assert!(matches!(parsed.blocks[0].statements[0], Statement::ConstF64 { bits:
                                                                             0x3ff8000000000000,
                                                                           .. }));
  }

  #[test]
  fn roundtrips_i32_instruction_family()
  {
    let function =
      Function { name: "I32Flow".to_string(),
                 parameters: vec![],
                 return_type: crate::xlil::Type::VOID,
                 locals: vec![Local { id: LocalId(0),
                                      name: "left".to_string(),
                                      value_type: Some(crate::xlil::Type::I32),
                                      mutable: false,
                                      span: span() },
                              Local { id: LocalId(1),
                                      name: "right".to_string(),
                                      value_type: Some(crate::xlil::Type::I32),
                                      mutable: false,
                                      span: span() },
                              Local { id: LocalId(2),
                                      name: "sum".to_string(),
                                      value_type: Some(crate::xlil::Type::I32),
                                      mutable: false,
                                      span: span() },
                              Local { id: LocalId(3),
                                      name: "same".to_string(),
                                      value_type: Some(crate::xlil::Type::BOOL),
                                      mutable: false,
                                      span: span() }],
                 blocks: vec![BasicBlock { id: BlockId(0),
                                           statements: vec![Statement::AddI32 { result: LocalId(2),
                                                                                left: LocalId(0),
                                                                                right: LocalId(1),
                                                                                span: span() },
                                                            Statement::EqI32 { result: LocalId(3),
                                                                               left: LocalId(0),
                                                                               right: LocalId(1),
                                                                               span: span() },
                                                            Statement::LtI32 { result: LocalId(3),
                                                                               left: LocalId(0),
                                                                               right: LocalId(1),
                                                                               span: span() },
                                                            Statement::LeI32 { result: LocalId(3),
                                                                               left: LocalId(0),
                                                                               right: LocalId(1),
                                                                               span: span() },
                                                            Statement::GtI32 { result: LocalId(3),
                                                                               left: LocalId(0),
                                                                               right: LocalId(1),
                                                                               span: span() },
                                                            Statement::GeI32 { result: LocalId(3),
                                                                               left: LocalId(0),
                                                                               right: LocalId(1),
                                                                               span: span() }],
                                           terminator: Some(Terminator::Return(None)),
                                           span: span() }] };

    let text = function_to_xmir(&function);
    let parsed = parse_xmir_function(&text).expect("XMIR i32 instructions should parse");

    assert!(text.contains("statement add.i32"));
    assert!(matches!(parsed.blocks[0].statements[0], Statement::AddI32 { result:
                                                                           LocalId(2),
                                                                         left: LocalId(0),
                                                                         right: LocalId(1),
                                                                         .. }));
    assert!(matches!(parsed.blocks[0].statements[1], Statement::EqI32 { result: LocalId(3),
                                                                        .. }));
    assert!(matches!(parsed.blocks[0].statements[2], Statement::LtI32 { result: LocalId(3),
                                                                        .. }));
    assert!(matches!(parsed.blocks[0].statements[3], Statement::LeI32 { result: LocalId(3),
                                                                        .. }));
    assert!(matches!(parsed.blocks[0].statements[4], Statement::GtI32 { result: LocalId(3),
                                                                        .. }));
    assert!(matches!(parsed.blocks[0].statements[5], Statement::GeI32 { result: LocalId(3),
                                                                        .. }));
  }

  #[test]
  fn roundtrips_optimizer_analysis()
  {
    let reports = vec![OptimizationReport { pass: OptimizationPass::RemoveUnreachableBlocks,
                                            removed_items: 2 },
                       OptimizationReport { pass: OptimizationPass::RemoveRedundantEndBorrow,
                                            removed_items: 1 },
                       OptimizationReport { pass: OptimizationPass::FoldConstBoolBranch,
                                            removed_items: 1 }];

    let text = optimizer_analysis_to_xmir(&reports);
    let parsed = parse_xmir_optimizer_analysis(&text).expect("optimizer analysis should parse");

    assert_eq!(parsed, reports);
    assert!(text.contains("analysis optimizer"));
    assert!(text.contains("fold_const_bool_branch"));
    assert!(!text.contains(".pass"));
  }

  #[test]
  fn rejects_invalid_optimizer_analysis()
  {
    let diagnostics = parse_xmir_optimizer_analysis("analysis optimizer\n  pass nope\n    removed_items many\n")
      .expect_err("invalid optimizer analysis must fail");

    assert_eq!(diagnostics.len(), 2);
  }

  #[test]
  fn roundtrips_verify_analysis()
  {
    let diagnostics = vec![VerifyDiagnostic { code: VerifyDiagnosticCode::UnknownLocal,
                                              message: "local id 7 is not declared".to_string(),
                                              span: span() },
                           VerifyDiagnostic { code: VerifyDiagnosticCode::MissingTerminator,
                                              message: "MIR block is missing a terminator".to_string(),
                                              span: Span::new(1, 4, 8) }];

    let text = verify_analysis_to_xmir(&diagnostics);
    let parsed = parse_xmir_verify_analysis(&text).expect("verify analysis should parse");

    assert_eq!(parsed, diagnostics);
    assert!(text.contains("analysis verify"));
    assert!(!text.contains(".diagnostic"));
  }

  #[test]
  fn rejects_invalid_verify_analysis()
  {
    let diagnostics = parse_xmir_verify_analysis("analysis verify\n  diagnostic nope\n    span bad\n    message \
                                                  broken\n").expect_err("invalid verify analysis must fail");

    assert_eq!(diagnostics.len(), 2);
  }
}
