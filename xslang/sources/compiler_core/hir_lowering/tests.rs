/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 */

use super::*;
use crate::compiler_core::SourceSpan;

fn syntax(kind: u32, text: &str, parent: Option<usize>, children: Vec<usize>) -> SyntaxNode
{
  SyntaxNode { kind,
               token_kind: 0,
               visibility: 0,
               flags: 0,
               parent,
               children,
               text: text.to_owned(),
               span: SourceSpan { file_id: 1,
                                  start_offset: 0,
                                  end_offset: text.len() as u64,
                                  start_line: 1,
                                  start_column: 1,
                                  end_line: 1,
                                  end_column: text.len() as u64 + 1 } }
}

#[test]
fn lowers_module_and_function_signature()
{
  let mut nodes = vec![syntax(FILE, "", None, vec![1, 4]),
                       syntax(DECL_MODULE, "module app;", Some(0), vec![2]),
                       syntax(PATH, "app", Some(1), vec![3]),
                       syntax(IDENTIFIER, "app", Some(2), vec![]),
                       syntax(DECL_FUNCTION, "fn main(value: Long) -> Long", Some(0), vec![5, 6, 9]),
                       syntax(IDENTIFIER, "main", Some(4), vec![]),
                       syntax(PARAMETER, "value: Long", Some(4), vec![7, 8]),
                       syntax(IDENTIFIER, "value", Some(6), vec![]),
                       syntax(TYPE_NAMED, "Long", Some(6), vec![10]),
                       syntax(TYPE_NAMED, "Long", Some(4), vec![11])];
  nodes.push(syntax(PATH, "Long", Some(8), vec![12]));
  nodes.push(syntax(PATH, "Long", Some(9), vec![13]));
  nodes.push(syntax(IDENTIFIER, "Long", Some(10), vec![]));
  nodes.push(syntax(IDENTIFIER, "Long", Some(11), vec![]));
  nodes[9].flags = RETURN_TYPE;
  let module = lower_declarations(&SyntaxTree { root: 0,
                                                nodes }).expect("signature module");
  assert_eq!(module.name.as_deref(), Some("app"));
  assert_eq!(module.functions[0].name, "main");
  assert_eq!(module.functions[0].parameters[0].ty,
             declarations::TypeRef::Primitive(PrimitiveType::Long));
  assert_eq!(module.functions[0].return_type,
             declarations::TypeRef::Primitive(PrimitiveType::Long));
}

#[test]
fn canonicalizes_explicit_string_without_changing_literal_inference()
{
  let mut nodes = vec![syntax(FILE, "", None, vec![1]),
                       syntax(DECL_FUNCTION, "fn keep(value: String) -> String", Some(0), vec![2, 3,
                                                                                               6]),
                       syntax(IDENTIFIER, "keep", Some(1), vec![]),
                       syntax(PARAMETER, "value: String", Some(1), vec![4, 5]),
                       syntax(IDENTIFIER, "value", Some(3), vec![]),
                       syntax(TYPE_NAMED, "String", Some(3), vec![7]),
                       syntax(TYPE_NAMED, "String", Some(1), vec![8])];
  nodes.push(syntax(PATH, "String", Some(5), vec![9]));
  nodes.push(syntax(PATH, "String", Some(6), vec![10]));
  nodes.push(syntax(IDENTIFIER, "String", Some(7), vec![]));
  nodes.push(syntax(IDENTIFIER, "String", Some(8), vec![]));
  nodes[6].flags = RETURN_TYPE;
  let module = lower_declarations(&SyntaxTree { root: 0,
                                                nodes }).expect("owned String signature");
  let owned = declarations::TypeRef::Primitive(PrimitiveType::String);
  assert_eq!(module.functions[0].parameters[0].ty, owned);
  assert_eq!(module.functions[0].return_type, owned);
  let inferred = crate::hir::inference::resolve_binding(
    &crate::hir::inference::Binding {
      name: "inferred".to_string(),
      annotation: None,
      initializer: Some(Expression::Literal { literal: Literal::String("Leitwolf".to_string()),
                                               span: Span::new(1, 0, 10) }),
      mutable: true,
      operator: crate::hir::inference::BindingOperator::InferAssign,
      span: Span::new(1, 0, 10),
    },
    &[],
  ).expect("string literal inference");
  assert_eq!(inferred.ty, Type::Reference { referent:
                                              Box::new(Type::Primitive(PrimitiveType::Str)),
                                            mutable: false });
}

#[test]
fn preserves_borrowed_str_and_rejects_implicit_owned_string_construction()
{
  let mut nodes = vec![syntax(FILE, "", None, vec![1]),
                       syntax(DECL_FUNCTION, "fn names()", Some(0), vec![2, 3]),
                       syntax(IDENTIFIER, "names", Some(1), vec![]),
                       syntax(STMT_BLOCK, "{ ... }", Some(1), vec![4, 8]),
                       syntax(STMT_VARIABLE, "borrowed := \"Leitwolf\";", Some(3), vec![5]),
                       syntax(DECL_VARIABLE, "borrowed := \"Leitwolf\"", Some(4), vec![6, 7]),
                       syntax(IDENTIFIER, "borrowed", Some(5), vec![]),
                       syntax(EXPR_LITERAL, "\"Leitwolf\"", Some(5), vec![]),
                       syntax(STMT_VARIABLE, "boxed: String = \"Luna\";", Some(3), vec![9]),
                       syntax(DECL_VARIABLE, "boxed: String = \"Luna\"", Some(8), vec![10, 11, 14]),
                       syntax(IDENTIFIER, "boxed", Some(9), vec![]),
                       syntax(TYPE_NAMED, "String", Some(9), vec![12]),
                       syntax(PATH, "String", Some(11), vec![13]),
                       syntax(IDENTIFIER, "String", Some(12), vec![]),
                       syntax(EXPR_LITERAL, "\"Luna\"", Some(9), vec![])];
  nodes[5].flags = INFERRED_TYPE;
  nodes[7].token_kind = TOKEN_STRING;
  nodes[14].token_kind = TOKEN_STRING;
  let module = lower_declarations(&SyntaxTree { root: 0,
                                                nodes }).expect("string local module");
  let body = module.functions[0].body.as_ref().expect("lowered string locals");
  let Statement::Let { local: borrowed,
                       initializer:
                         Some(Expression::Literal { literal: Literal::String(_),
                                                    .. }), } = &body[0]
  else
  {
    panic!("inferred string literal should stay a borrowed Str");
  };
  assert_eq!(borrowed.ty, Type::Reference { referent:
                                              Box::new(Type::Primitive(PrimitiveType::Str)),
                                            mutable: false });
  let Statement::Let { local: owned,
                       initializer:
                         Some(Expression::Literal { literal: Literal::String(_),
                                                    .. }), } = &body[1]
  else
  {
    panic!("String must stay an owned type without Optional/Some desugaring");
  };
  assert_eq!(owned.ty, Type::Primitive(PrimitiveType::String));
  let checked = module.functions[0].as_type_checked_input()
                                   .expect("checked string body");
  let diagnostics = crate::hir::type_check::TypeChecker::new().check_function(&checked);
  assert_eq!(diagnostics.len(), 1);
  assert_eq!(diagnostics[0].code,
             crate::hir::type_check::DiagnosticCode::LiteralTypeMismatch);
  let xhir = crate::hir::text::function_to_xhir(&checked);
  assert!(xhir.contains("type &Str"));
  assert!(xhir.contains("type String"));
  let parsed = crate::hir::text::parse_xhir_function(&xhir).expect("canonical String XHIR");
  let Statement::Let { local: parsed_owned,
                       initializer:
                         Some(Expression::Literal { literal: Literal::String(_),
                                                    .. }), } = &parsed.body[1]
  else
  {
    panic!("canonical XHIR should retain owned String without implicit conversion");
  };
  assert_eq!(parsed_owned.ty, Type::Primitive(PrimitiveType::String));
}

#[test]
fn lowers_long_return_body_for_hir_to_mir()
{
  let mut nodes = vec![syntax(FILE, "", None, vec![1]),
                       syntax(DECL_FUNCTION, "fn main() -> Long { return 7; }", Some(0), vec![2, 3, 6]),
                       syntax(IDENTIFIER, "main", Some(1), vec![]),
                       syntax(TYPE_NAMED, "Long", Some(1), vec![4]),
                       syntax(PATH, "Long", Some(3), vec![5]),
                       syntax(IDENTIFIER, "Long", Some(4), vec![]),
                       syntax(STMT_BLOCK, "{ return 7; }", Some(1), vec![7]),
                       syntax(STMT_RETURN, "return 7;", Some(6), vec![8]),
                       syntax(EXPR_LITERAL, "7", Some(7), vec![])];
  nodes[3].flags = RETURN_TYPE;
  nodes[8].token_kind = TOKEN_INTEGER;
  let module = lower_declarations(&SyntaxTree { root: 0,
                                                nodes }).expect("body module");
  let hir = module.functions[0].as_type_checked_input().expect("HIR body");
  assert!(crate::hir::type_check::TypeChecker::new().check_function(&hir)
                                                    .is_empty());
  let mir = crate::hir::mir_lowering::HirToMirLowerer::new().lower_function(&hir)
                                                            .expect("MIR body");
  assert!(matches!(mir.blocks[0].statements[0],
                   crate::mir::Statement::ConstI32 { value: 7,
                                                     .. }));
}

#[test]
fn resolves_function_body_calls_across_program_trees()
{
  let mut helper_nodes = vec![syntax(FILE, "", None, vec![1]),
                              syntax(DECL_FUNCTION, "fn answer() -> Long", Some(0), vec![2, 3, 6]),
                              syntax(IDENTIFIER, "answer", Some(1), vec![]),
                              syntax(TYPE_NAMED, "Long", Some(1), vec![4]),
                              syntax(PATH, "Long", Some(3), vec![5]),
                              syntax(IDENTIFIER, "Long", Some(4), vec![]),
                              syntax(STMT_BLOCK, "{ return 7; }", Some(1), vec![7]),
                              syntax(STMT_RETURN, "return 7;", Some(6), vec![8]),
                              syntax(EXPR_LITERAL, "7", Some(7), vec![])];
  helper_nodes[3].flags = RETURN_TYPE;
  helper_nodes[8].token_kind = TOKEN_INTEGER;
  let mut main_nodes = vec![syntax(FILE, "", None, vec![1]),
                            syntax(DECL_FUNCTION, "fn main() -> Long", Some(0), vec![2, 3, 6]),
                            syntax(IDENTIFIER, "main", Some(1), vec![]),
                            syntax(TYPE_NAMED, "Long", Some(1), vec![4]),
                            syntax(PATH, "Long", Some(3), vec![5]),
                            syntax(IDENTIFIER, "Long", Some(4), vec![]),
                            syntax(STMT_BLOCK, "{ return answer(); }", Some(1), vec![7]),
                            syntax(STMT_RETURN, "return answer();", Some(6), vec![8]),
                            syntax(EXPR_CALL, "answer()", Some(7), vec![9]),
                            syntax(EXPR_IDENTIFIER, "answer", Some(8), vec![10]),
                            syntax(PATH, "answer", Some(9), vec![11]),
                            syntax(IDENTIFIER, "answer", Some(10), vec![])];
  main_nodes[3].flags = RETURN_TYPE;
  let module = lower_program(&[SyntaxTree { root: 0,
                                            nodes: main_nodes },
                               SyntaxTree { root: 0,
                                            nodes: helper_nodes }]).expect("multi-tree program");
  assert_eq!(module.functions.len(), 2);
  let main = module.functions[0].as_type_checked_input().expect("main HIR body");
  let Statement::Return { value: Some(Expression::Call { function, .. }),
                          .. } = &main.body[0]
  else
  {
    panic!("main should retain its cross-file direct call");
  };
  assert_eq!(function, "answer");
}

#[test]
fn preserves_canonical_builtin_collection_types()
{
  let array = SyntaxTree { root: 0,
                           nodes: vec![syntax(TYPE_ARRAY, "[Int]", None, vec![1]),
                                       syntax(TYPE_NAMED, "Int", Some(0), vec![2]),
                                       syntax(PATH, "Int", Some(1), vec![3]),
                                       syntax(IDENTIFIER, "Int", Some(2), vec![])] };
  let fixed = SyntaxTree { root: 0,
                           nodes: vec![syntax(TYPE_FIXED_ARRAY, "[Long; 4]", None, vec![1, 4]),
                                       syntax(TYPE_NAMED, "Long", Some(0), vec![2]),
                                       syntax(PATH, "Long", Some(1), vec![3]),
                                       syntax(IDENTIFIER, "Long", Some(2), vec![]),
                                       syntax(EXPR_LITERAL, "4", Some(0), vec![])] };
  let map = SyntaxTree { root: 0,
                         nodes: vec![syntax(TYPE_MAP, "[String: Optional<Int>]", None, vec![1, 4]),
                                     syntax(TYPE_NAMED, "String", Some(0), vec![2]),
                                     syntax(PATH, "String", Some(1), vec![3]),
                                     syntax(IDENTIFIER, "String", Some(2), vec![]),
                                     syntax(TYPE_NAMED, "Optional<Int>", Some(0), vec![5]),
                                     syntax(PATH, "Optional<Int>", Some(4), vec![6]),
                                     syntax(IDENTIFIER, "Optional<Int>", Some(5), vec![])] };

  assert_eq!(lower_type(&array, &array.nodes[0]),
             declarations::TypeRef::Array { element: Box::new(declarations::TypeRef::Primitive(PrimitiveType::Int)),
                                            length: None });
  assert_eq!(lower_type(&fixed, &fixed.nodes[0]),
             declarations::TypeRef::Array { element:
                                              Box::new(declarations::TypeRef::Primitive(PrimitiveType::Long)),
                                            length: Some(4) });
  assert_eq!(lower_type(&map, &map.nodes[0]),
             declarations::TypeRef::Map { key: Box::new(declarations::TypeRef::Primitive(PrimitiveType::String)),
                                          value:
                                            Box::new(declarations::TypeRef::Named("Optional<Int>".to_string())) });
}

#[test]
fn resolves_fixed_array_members_to_canonical_hir()
{
  let tree = SyntaxTree { root: 0,
                          nodes: vec![syntax(EXPR_MEMBER_ACCESS, "values.count", None, vec![1, 4]),
                                      syntax(EXPR_IDENTIFIER, "values", Some(0), vec![2]),
                                      syntax(PATH, "values", Some(1), vec![3]),
                                      syntax(IDENTIFIER, "values", Some(2), vec![]),
                                      syntax(IDENTIFIER, "count", Some(0), vec![])] };
  let context = LoweringContext { calls: HashMap::new(),
                                  generic_calls: HashMap::new(),
                                  constructors: HashMap::new(),
                                  methods: HashMap::new(),
                                  nominal_types: HashMap::new(),
                                  type_substitutions: HashMap::new() };
  let array_type = Type::Array { element: Box::new(Type::Primitive(PrimitiveType::Long)),
                                 length: Some(3) };
  let locals = HashMap::from([("values".to_string(), array_type)]);

  assert_eq!(collection::array_member_type(&tree, &tree.nodes[0], &context, &locals),
             Some(Type::Primitive(PrimitiveType::Int)));
  assert!(matches!(collection::lower_array_member(&tree,
                                                   &tree.nodes[0],
                                                   &context,
                                                   &locals,
                                                   Span::new(1, 0, 12)),
                   Some(Expression::Literal { literal: Literal::Integer(ref value), .. }) if value == "3"));

  let mut first_tree = tree;
  first_tree.nodes[4].text = "first".to_string();
  assert!(matches!(collection::lower_array_member(&first_tree,
                                                   &first_tree.nodes[0],
                                                   &context,
                                                   &locals,
                                                   Span::new(1, 0, 12)),
                   Some(Expression::Index { element_type, .. })
                     if element_type.as_ref() == &Type::Primitive(PrimitiveType::Long)));
  first_tree.nodes[4].text = "isEmpty".to_string();
  assert_eq!(collection::array_member_type(&first_tree, &first_tree.nodes[0], &context, &locals),
             None);
}
