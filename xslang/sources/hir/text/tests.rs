/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 */

use super::*;
use crate::hir::async_check::Span;
use crate::hir::symbols::{Symbol, SymbolKind};
use crate::hir::type_check::{Diagnostic, DiagnosticCode, Local};

fn span() -> Span
{
  Span::new(1, 0, 1)
}

#[test]
fn writes_symbol_module_as_structured_xhir()
{
  let module = Module { name: "App".to_string(),
                        imports: vec![Import::Selected { module: "std.io".to_string(),
                                                         name: "println".to_string(),
                                                         alias: None,
                                                         span: span() }],
                        symbols: vec![Symbol { name: "Main".to_string(),
                                               kind: SymbolKind::Function,
                                               visibility: Visibility::Public,
                                               span: span() }] };

  let text = module_symbols_to_xhir(&module);

  assert!(text.contains(".xhir version 1\nmodule App"));
  assert!(text.contains("import println from std.io"));
  assert!(text.contains("symbol Main\n    kind function\n    visibility public"));
  assert!(text.contains(".end\n.program end\n"));
  assert!(!text.contains(".func"));
  assert!(!text.contains("%0"));

  let header = parse_xhir_header(&text).expect("header should parse");
  assert_eq!(header.kind, XhirDocumentKind::Module);
  assert_eq!(header.name, "App");

  let parsed = parse_xhir_module_symbols(&text).expect("module symbols should parse");
  assert_eq!(parsed.name, "App");
  assert_eq!(parsed.imports.len(), 1);
  assert_eq!(parsed.symbols.len(), 1);
  assert_eq!(parsed.symbols[0].kind, SymbolKind::Function);
}

#[test]
fn writes_checked_function_as_structured_xhir()
{
  let function = Function { name: "Main".to_string(),
                            return_type: Some(Type::Primitive(PrimitiveType::Int)),
                            locals: vec![Local { name: "answer".to_string(),
                                                 ty: Type::Primitive(PrimitiveType::Int),
                                                 mutable: false,
                                                 span: span() }],
                            body: vec![Statement::Return { value: Some(Expression::Literal { literal:
                                                                                    Literal::Integer("42".to_string()),
                                                                                  span: span() }),
                                                span: span() }] };

  let text = function_to_xhir(&function);

  assert!(text.contains("function Main"));
  assert!(text.contains("returns Int"));
  assert!(text.contains("local answer: Int immutable"));
  assert!(text.contains("literal integer 42"));
  assert!(text.contains(".end\n.program end\n"));
  assert!(!text.contains("bb0"));

  let header = parse_xhir_header(&text).expect("header should parse");
  assert_eq!(header.kind, XhirDocumentKind::Function);
  assert_eq!(header.name, "Main");

  let parsed = parse_xhir_function(&text).expect("function should parse");
  assert_eq!(parsed.name, "Main");
  assert_eq!(parsed.return_type, Some(Type::Primitive(PrimitiveType::Int)));
  assert_eq!(parsed.locals.len(), 1);
  assert_eq!(parsed.body.len(), 1);
}

#[test]
fn roundtrips_binary_expression()
{
  let function = Function { name: "Compare".to_string(),
                            return_type: Some(Type::Primitive(PrimitiveType::Bool)),
                            locals: vec![Local { name: "left".to_string(),
                                                 ty: Type::Primitive(PrimitiveType::Long),
                                                 mutable: false,
                                                 span: span() },
                                         Local { name: "right".to_string(),
                                                 ty: Type::Primitive(PrimitiveType::Long),
                                                 mutable: false,
                                                 span: span() }],
                            body: vec![Statement::Return { value:
                                                                Some(Expression::Binary {
                                                                  operator: BinaryOperator::LessEqual,
                                                                  left: Box::new(Expression::Local {
                                                                    name: "left".to_string(),
                                                                    span: span(),
                                                                  }),
                                                                  right: Box::new(Expression::Local {
                                                                    name: "right".to_string(),
                                                                    span: span(),
                                                                  }),
                                                                  span: span(),
                                                                }),
                                                              span: span() }] };

  let text = function_to_xhir(&function);
  let parsed = parse_xhir_function(&text).expect("binary XHIR should parse");

  assert!(text.contains("binary le"));
  assert!(matches!(&parsed.body[0], Statement::Return { value:
                                                          Some(Expression::Binary { operator:
                                                                                      BinaryOperator::LessEqual,
                                                                                    .. }),
                                                        .. }));
}

#[test]
fn roundtrips_result_propagation_expression()
{
  let function = Function { name: "TryWork".to_string(),
                            return_type: None,
                            locals: vec![Local { name: "work".to_string(),
                                                 ty: Type::Primitive(PrimitiveType::Int),
                                                 mutable: false,
                                                 span: span() }],
                            body: vec![Statement::Expr(Expression::ResultPropagation {
                                value: Box::new(Expression::Local { name: "work".to_string(),
                                                                    span: span() }),
                                span: span(),
                              })] };

  let text = function_to_xhir(&function);
  let parsed = parse_xhir_function(&text).expect("propagation XHIR should parse");

  assert!(text.contains("propagate"));
  assert!(matches!(&parsed.body[0], Statement::Expr(Expression::ResultPropagation { .. })));
}

#[test]
fn writes_desugared_result_match_as_structured_xhir()
{
  let function = DesugaredFunction { name: "TryWork".to_string(),
                                     return_type: Some(Type::Named("Result<()>".to_string())),
                                     locals: vec![Local { name: "work".to_string(),
                                                          ty: Type::Named("Result<Long, Error>".to_string()),
                                                          mutable: false,
                                                          span: span() }],
                                     body: vec![DesugaredStatement::Expr(DesugaredExpression::ResultMatch {
        value: Box::new(DesugaredExpression::Local { name: "work".to_string(),
                                                     span: span() }),
        success_binding: "__xs_try_ok_0".to_string(),
        error_binding: "__xs_try_error_0".to_string(),
        success_type: Type::Primitive(PrimitiveType::Long),
        error_type: Type::Named("Error".to_string()),
        span: span(),
      })] };

  let text = desugared_function_to_xhir(&function);

  assert!(text.contains("function TryWork"));
  assert!(text.contains("result_match"));
  assert!(text.contains("ok __xs_try_ok_0: Long"));
  assert!(text.contains("error __xs_try_error_0: Error"));
  assert!(text.contains("value\n          local work"));
  assert!(!text.contains("propagate"));
}

#[test]
fn roundtrips_panic_statement()
{
  let function = Function { name: "Stop".to_string(),
                            return_type: None,
                            locals: vec![],
                            body: vec![Statement::Panic { span: span() }] };

  let text = function_to_xhir(&function);
  let parsed = parse_xhir_function(&text).expect("panic XHIR should parse");

  assert!(text.contains("panic"));
  assert!(matches!(parsed.body[0], Statement::Panic { .. }));
}

#[test]
fn rejects_non_xhir_header()
{
  assert_eq!(parse_xhir_header(".func Main : () -> void\n"), None);
}

#[test]
fn rejects_unsupported_xhir_version()
{
  let diagnostics =
    parse_xhir_function(".xhir version 2\nfunction Main\n  signature\n    returns void\n").expect_err("unsupported \
                                                                                                       XHIR version \
                                                                                                       must fail");

  assert!(diagnostics[0].message.contains("unsupported XHIR version 2"));
}

#[test]
fn parses_explicit_end_markers_without_indentation()
{
  let text = ".xhir version 0\nfunction Main\nsignature\nreturns Int\n.end\nlocals\nlocal answer: Int \
              immutable\n.end\nbody\nreturn\nliteral integer 42\n.end\n.program end\n";

  let parsed = parse_xhir_function(text).expect("XHIR should not be indentation based");

  assert_eq!(parsed.name, "Main");
  assert_eq!(parsed.return_type, Some(Type::Primitive(PrimitiveType::Int)));
  assert_eq!(parsed.locals.len(), 1);
  assert_eq!(parsed.body.len(), 1);
}

#[test]
fn rejects_invalid_xhir_module_symbols()
{
  let diagnostics = parse_xhir_module_symbols(".xhir version 1\nmodule App\n\ndeclarations\n  symbol Main\n    kind \
                                               nope\n").expect_err("invalid symbol kind should fail");

  assert_eq!(diagnostics.len(), 2);
}

#[test]
fn rejects_invalid_xhir_function()
{
  let diagnostics =
    parse_xhir_function(".xhir version 1\nfunction Main\n  signature\n    returns ?\n").expect_err("unknown return \
                                                                                                    type should fail");

  assert_eq!(diagnostics.len(), 1);
}

#[test]
fn roundtrips_typecheck_analysis()
{
  let diagnostics = vec![Diagnostic { code: DiagnosticCode::LiteralTypeMismatch,
                                      message: "literal is not assignable".to_string(),
                                      span: span() },
                         Diagnostic { code: DiagnosticCode::UnknownLocal,
                                      message: "unknown local 'value'".to_string(),
                                      span: Span::new(1, 4, 8) }];

  let text = typecheck_analysis_to_xhir(&diagnostics);
  let parsed = parse_xhir_typecheck_analysis(&text).expect("typecheck analysis should parse");

  assert_eq!(parsed, diagnostics);
  assert!(text.contains("analysis typecheck"));
  assert!(!text.contains(".diagnostic"));
}

#[test]
fn rejects_invalid_typecheck_analysis()
{
  let diagnostics = parse_xhir_typecheck_analysis("analysis typecheck\n  diagnostic nope\n    span bad\n    message \
                                                   broken\n").expect_err("invalid analysis must fail");

  assert_eq!(diagnostics.len(), 2);
}
