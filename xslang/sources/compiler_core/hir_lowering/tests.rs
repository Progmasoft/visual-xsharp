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
fn preserves_enum_data_identity_and_variant_payloads()
{
  let mut nodes = vec![syntax(FILE, "", None, vec![1]),
                       syntax(DECL_ENUM, "enum data Value", Some(0), vec![2, 3, 6]),
                       syntax(IDENTIFIER, "Value", Some(1), vec![]),
                       syntax(ENUM_VARIANT, "Number: Long", Some(1), vec![4, 5]),
                       syntax(IDENTIFIER, "Number", Some(3), vec![]),
                       syntax(TYPE_NAMED, "Long", Some(3), vec![7]),
                       syntax(ENUM_VARIANT, "Empty", Some(1), vec![8]),
                       syntax(PATH, "Long", Some(5), vec![9]),
                       syntax(IDENTIFIER, "Empty", Some(6), vec![]),
                       syntax(IDENTIFIER, "Long", Some(7), vec![])];
  nodes[1].flags = DATA_ENUM;
  let module = lower_declarations(&SyntaxTree { root: 0,
                                                nodes }).expect("enum data declaration");
  let declaration = &module.nominal_types[0];
  assert_eq!(declaration.kind, declarations::NominalKind::EnumData);
  assert_eq!(declaration.variants[0].name, "Number");
  assert_eq!(declaration.variants[0].payload,
             Some(declarations::TypeRef::Primitive(PrimitiveType::Long)));
  assert_eq!(declaration.variants[1].payload, None);
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
                                  enum_data: crate::hir::enum_data::EnumDataRegistry::default(),
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

fn point_nominal() -> declarations::NominalType
{
  declarations::NominalType { name: "Point".to_string(),
                              kind: declarations::NominalKind::Data,
                              bases: vec![],
                              fields: vec![declarations::Field { name: "x".to_string(),
                                                                 ty:
                                                                   declarations::TypeRef::Primitive(PrimitiveType::Long),
                                                                 mutable: true,
                                                                 span: SourceSpan { file_id: 1,
                                                                                    start_offset: 0,
                                                                                    end_offset: 1,
                                                                                    start_line: 1,
                                                                                    start_column: 1,
                                                                                    end_line: 1,
                                                                                    end_column: 2 } },
                                           declarations::Field { name: "visible".to_string(),
                                                                 ty:
                                                                   declarations::TypeRef::Primitive(PrimitiveType::Bool),
                                                                 mutable: false,
                                                                 span: SourceSpan { file_id: 1,
                                                                                    start_offset: 0,
                                                                                    end_offset: 1,
                                                                                    start_line: 1,
                                                                                    start_column: 1,
                                                                                    end_line: 1,
                                                                                    end_column: 2 } },],
                              variants: vec![],
                              span: SourceSpan { file_id: 1,
                                                 start_offset: 0,
                                                 end_offset: 1,
                                                 start_line: 1,
                                                 start_column: 1,
                                                 end_line: 1,
                                                 end_column: 2 } }
}

fn optional_member_tree(member: &str) -> SyntaxTree
{
  SyntaxTree { root: 0,
               nodes: vec![syntax(EXPR_OPTIONAL_MEMBER_ACCESS, &format!("point?.{member}"), None, vec![1,
                                                                                                       4]),
                           syntax(EXPR_IDENTIFIER, "point", Some(0), vec![2]),
                           syntax(PATH, "point", Some(1), vec![3]),
                           syntax(IDENTIFIER, "point", Some(2), vec![]),
                           syntax(IDENTIFIER, member, Some(0), vec![]),] }
}

fn point_context() -> LoweringContext
{
  let point = point_nominal();
  LoweringContext { calls: HashMap::new(),
                    generic_calls: HashMap::new(),
                    constructors: HashMap::new(),
                    methods: HashMap::new(),
                    nominal_types: HashMap::from([(point.name.clone(), point)]),
                    enum_data: crate::hir::enum_data::EnumDataRegistry::default(),
                    type_substitutions: HashMap::new() }
}

#[test]
fn resolves_optional_member_type_from_the_wrapped_nominal()
{
  let tree = optional_member_tree("x");
  let context = point_context();
  let locals =
    HashMap::from([("point".to_string(), Type::Optional { element: Box::new(Type::Named("Point".to_string())) })]);
  assert_eq!(nominal::optional_member_type(&tree, &tree.nodes[0], &context, &locals),
             Some(Type::Optional { element: Box::new(Type::Primitive(PrimitiveType::Long)) }));

  let visible = optional_member_tree("visible");
  assert_eq!(nominal::optional_member_type(&visible, &visible.nodes[0], &context, &locals),
             Some(Type::Optional { element: Box::new(Type::Primitive(PrimitiveType::Bool)) }));
}

#[test]
fn lowers_optional_member_to_explicit_typed_hir()
{
  let tree = optional_member_tree("x");
  let context = point_context();
  let receiver_type = Type::Optional { element: Box::new(Type::Named("Point".to_string())) };
  let result_type = Type::Optional { element: Box::new(Type::Primitive(PrimitiveType::Long)) };
  let locals = HashMap::from([("point".to_string(), receiver_type)]);
  let lowered =
    lower_expression(&tree, &tree.nodes[0], &context, &locals, Some(&result_type)).expect("valid optional member \
                                                                                           should lower");
  assert!(matches!(lowered,
                   Expression::OptionalMember {
                     receiver,
                     owner,
                     name,
                     field_type,
                     result_type: lowered_result,
                     ..
                   } if matches!(receiver.as_ref(), Expression::Local { name, .. } if name == "point") &&
                        owner == "Point" && name == "x" &&
                        field_type.as_ref() == &Type::Primitive(PrimitiveType::Long) &&
                        lowered_result.as_ref() == &result_type));
}

#[test]
fn optional_member_rejects_non_optional_and_unknown_receivers()
{
  let tree = optional_member_tree("x");
  let context = point_context();
  let plain = HashMap::from([("point".to_string(), Type::Named("Point".to_string()))]);
  assert_eq!(nominal::optional_member_type(&tree, &tree.nodes[0], &context, &plain),
             None);

  let optional_long =
    HashMap::from([("point".to_string(), Type::Optional { element: Box::new(Type::Primitive(PrimitiveType::Long)) })]);
  assert_eq!(nominal::optional_member_type(&tree, &tree.nodes[0], &context, &optional_long),
             None);

  let unknown =
    HashMap::from([("point".to_string(), Type::Optional { element: Box::new(Type::Named("Unknown".to_string())) })]);
  assert_eq!(nominal::optional_member_type(&tree, &tree.nodes[0], &context, &unknown),
             None);
}

#[test]
fn optional_member_rejects_unknown_fields_and_wrong_context_types()
{
  let tree = optional_member_tree("missing");
  let context = point_context();
  let locals =
    HashMap::from([("point".to_string(), Type::Optional { element: Box::new(Type::Named("Point".to_string())) })]);
  assert_eq!(nominal::optional_member_type(&tree, &tree.nodes[0], &context, &locals),
             None);

  let valid = optional_member_tree("x");
  let wrong_expected = Type::Optional { element: Box::new(Type::Primitive(PrimitiveType::Bool)) };
  assert!(lower_expression(&valid, &valid.nodes[0], &context, &locals, Some(&wrong_expected)).is_none());
}

#[test]
fn lowers_postfix_optional_unwrap_from_syntax_packet()
{
  let tree = SyntaxTree { root: 0,
                          nodes: vec![syntax(EXPR_OPTIONAL_FORGIVING, "value!", None, vec![1]),
                                      syntax(EXPR_IDENTIFIER, "value", Some(0), vec![2]),
                                      syntax(PATH, "value", Some(1), vec![3]),
                                      syntax(IDENTIFIER, "value", Some(2), vec![]),] };
  let context = point_context();
  let locals =
    HashMap::from([("value".to_string(), Type::Optional { element: Box::new(Type::Primitive(PrimitiveType::Long)) })]);
  assert!(matches!(lower_expression(&tree,
                                    &tree.nodes[0],
                                    &context,
                                    &locals,
                                    Some(&Type::Primitive(PrimitiveType::Long))),
                   Some(Expression::OptionalUnwrap {
                     value,
                     element_type,
                     ..
                   }) if matches!(value.as_ref(), Expression::Local { name, .. } if name == "value") &&
                        element_type.as_ref() == &Type::Primitive(PrimitiveType::Long)));
  assert!(lower_expression(&tree,
                           &tree.nodes[0],
                           &context,
                           &locals,
                           Some(&Type::Primitive(PrimitiveType::Bool))).is_none());
}

#[test]
fn lowers_optional_coalescing_assignment_with_implicit_some()
{
  let mut assignment = syntax(EXPR_ASSIGNMENT, "value ??= 7", None, vec![1, 4, 5]);
  assignment.token_kind = TOKEN_QUESTION_QUESTION_ASSIGN;
  let mut literal = syntax(EXPR_LITERAL, "7", Some(0), vec![]);
  literal.token_kind = TOKEN_INTEGER;
  let tree = SyntaxTree { root: 0,
                          nodes: vec![assignment,
                                      syntax(EXPR_IDENTIFIER, "value", Some(0), vec![2]),
                                      syntax(PATH, "value", Some(1), vec![3]),
                                      syntax(IDENTIFIER, "value", Some(2), vec![]),
                                      syntax(IDENTIFIER, "??=", Some(0), vec![]),
                                      literal,] };
  let context = point_context();
  let optional_long = Type::Optional { element: Box::new(Type::Primitive(PrimitiveType::Long)) };
  let locals = HashMap::from([("value".to_string(), optional_long.clone())]);
  assert!(matches!(lower_expression(&tree, &tree.nodes[0], &context, &locals, Some(&optional_long)),
                   Some(Expression::OptionalCoalesceAssign {
                     target,
                     value,
                     optional_type,
                     ..
                   }) if target == "value" &&
                        matches!(value.as_ref(), Expression::Call { function, .. } if function == "Some") &&
                        optional_type.as_ref() == &optional_long));
}

fn result_long_type() -> Type
{
  Type::Result { success: Box::new(Type::Primitive(PrimitiveType::Long)),
                 error: Box::new(Type::Primitive(PrimitiveType::Long)) }
}

fn empty_context() -> LoweringContext
{
  LoweringContext { calls: HashMap::new(),
                    generic_calls: HashMap::new(),
                    constructors: HashMap::new(),
                    methods: HashMap::new(),
                    nominal_types: HashMap::new(),
                    enum_data: crate::hir::enum_data::EnumDataRegistry::default(),
                    type_substitutions: HashMap::new() }
}

fn result_type_tree(base_name: &str, arguments: &[&str]) -> SyntaxTree
{
  let mut nodes = vec![syntax(TYPE_GENERIC,
                              &format!("{base_name}<{}>", arguments.join(", ")),
                              None,
                              vec![])];
  let base_type = nodes.len();
  nodes.push(syntax(TYPE_NAMED, base_name, Some(0), vec![base_type + 1]));
  nodes.push(syntax(PATH, base_name, Some(base_type), vec![base_type + 2]));
  nodes.push(syntax(IDENTIFIER, base_name, Some(base_type + 1), vec![]));
  nodes[0].children.push(base_type);
  for argument in arguments
  {
    let ty = nodes.len();
    nodes.push(syntax(if *argument == "()"
                      {
                        TYPE_UNIT
                      }
                      else
                      {
                        TYPE_NAMED
                      },
                      argument,
                      Some(0),
                      vec![]));
    if *argument != "()"
    {
      let path = nodes.len();
      nodes.push(syntax(PATH, argument, Some(ty), vec![path + 1]));
      nodes.push(syntax(IDENTIFIER, argument, Some(path), vec![]));
      nodes[ty].children.push(path);
    }
    nodes[0].children.push(ty);
  }
  SyntaxTree { root: 0,
               nodes }
}

#[test]
fn lowers_result_generic_to_a_structured_type()
{
  let tree = result_type_tree("Result", &["Long", "Bool"]);
  assert_eq!(lower_type(&tree, &tree.nodes[0]),
             declarations::TypeRef::Result { success:
                                               Box::new(declarations::TypeRef::Primitive(PrimitiveType::Long)),
                                             error: Box::new(declarations::TypeRef::Primitive(PrimitiveType::Bool)) });
  assert_eq!(checked_type(&lower_type(&tree, &tree.nodes[0])),
             Some(Type::Result { success: Box::new(Type::Primitive(PrimitiveType::Long)),
                                 error: Box::new(Type::Primitive(PrimitiveType::Bool)) }));
}

#[test]
fn lowers_canonical_result_path_and_unit_shorthand()
{
  let canonical = result_type_tree("std::result::Result", &["Long", "Long"]);
  assert_eq!(lower_type(&canonical, &canonical.nodes[0]),
             declarations::TypeRef::Result { success:
                                               Box::new(declarations::TypeRef::Primitive(PrimitiveType::Long)),
                                             error: Box::new(declarations::TypeRef::Primitive(PrimitiveType::Long)) });

  let shorthand = result_type_tree("Result", &["()"]);
  assert_eq!(lower_type(&shorthand, &shorthand.nodes[0]),
             declarations::TypeRef::Result { success: Box::new(declarations::TypeRef::Unit),
                                             error: Box::new(declarations::TypeRef::Named("Error".to_string())) });
}

fn result_constructor_tree(name: &str, literal_text: &str, token_kind: u32) -> SyntaxTree
{
  let mut literal = syntax(EXPR_LITERAL, literal_text, Some(0), vec![]);
  literal.token_kind = token_kind;
  SyntaxTree { root: 0,
               nodes: vec![syntax(EXPR_CALL, &format!("{name}({literal_text})"), None, vec![1, 4]),
                           syntax(EXPR_IDENTIFIER, name, Some(0), vec![2]),
                           syntax(PATH, name, Some(1), vec![3]),
                           syntax(IDENTIFIER, name, Some(2), vec![]),
                           literal,] }
}

#[test]
fn lowers_ok_and_error_calls_against_result_context()
{
  let expected = result_long_type();
  for name in ["Ok", "Error"]
  {
    let tree = result_constructor_tree(name, "7", TOKEN_INTEGER);
    let lowered = lower_expression(&tree,
                                   &tree.nodes[0],
                                   &empty_context(),
                                   &HashMap::new(),
                                   Some(&expected)).expect("Result constructor should lower");
    assert!(matches!(lowered,
                     Expression::Call {
                       function,
                       arguments,
                       parameter_types,
                       return_type,
                       ..
                     } if function == name &&
                          arguments.len() == 1 &&
                          parameter_types == vec![Type::Primitive(PrimitiveType::Long)] &&
                          return_type.as_ref() == &expected));
  }
}

#[test]
fn result_constructor_rejects_wrong_payload_and_non_result_context()
{
  let wrong_payload = result_constructor_tree("Ok", "true", 0);
  let lowered = lower_expression(&wrong_payload,
                                 &wrong_payload.nodes[0],
                                 &empty_context(),
                                 &HashMap::new(),
                                 Some(&result_long_type())).expect("syntax lowering preserves the mismatched payload \
                                                                    for type diagnostics");
  let function = crate::hir::type_check::Function { name: "invalid".to_string(),
                                                    return_type: Some(result_long_type()),
                                                    locals: vec![],
                                                    body: vec![Statement::Return { value: Some(lowered),
                                                                                   span: Span::new(1, 0, 1) }] };
  assert!(!crate::hir::type_check::TypeChecker::new().check_function(&function)
                                                     .is_empty());

  let valid_payload = result_constructor_tree("Ok", "7", TOKEN_INTEGER);
  assert!(lower_expression(&valid_payload,
                           &valid_payload.nodes[0],
                           &empty_context(),
                           &HashMap::new(),
                           Some(&Type::Primitive(PrimitiveType::Long))).is_none());
}

fn propagation_tree() -> SyntaxTree
{
  SyntaxTree { root: 0,
               nodes: vec![syntax(EXPR_RESULT_PROPAGATION, "work@", None, vec![1]),
                           syntax(EXPR_IDENTIFIER, "work", Some(0), vec![2]),
                           syntax(PATH, "work", Some(1), vec![3]),
                           syntax(IDENTIFIER, "work", Some(2), vec![]),] }
}

#[test]
fn lowers_postfix_result_propagation_to_typed_hir()
{
  let tree = propagation_tree();
  let locals = HashMap::from([("work".to_string(), result_long_type())]);
  let lowered = lower_expression(&tree,
                                 &tree.nodes[0],
                                 &empty_context(),
                                 &locals,
                                 Some(&Type::Primitive(PrimitiveType::Long))).expect("Result propagation should lower");
  assert!(matches!(lowered,
                   Expression::ResultPropagation {
                     value,
                     ..
                   } if matches!(value.as_ref(), Expression::Local { name, .. } if name == "work")));
  assert_eq!(expression_type(&tree, &tree.nodes[0], &empty_context(), &locals),
             Some(Type::Primitive(PrimitiveType::Long)));
}

#[test]
fn result_propagation_rejects_non_result_and_wrong_success_context()
{
  let tree = propagation_tree();
  let plain = HashMap::from([("work".to_string(), Type::Primitive(PrimitiveType::Long))]);
  assert!(lower_expression(&tree,
                           &tree.nodes[0],
                           &empty_context(),
                           &plain,
                           Some(&Type::Primitive(PrimitiveType::Long))).is_none());

  let result = HashMap::from([("work".to_string(), result_long_type())]);
  assert!(lower_expression(&tree,
                           &tree.nodes[0],
                           &empty_context(),
                           &result,
                           Some(&Type::Primitive(PrimitiveType::Bool))).is_none());
}
