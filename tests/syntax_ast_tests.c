/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 */

#include "xs/diagnostic.h"
#include "xs/macro.h"
#include "xs/syntax_ast.h"
#include "xs/syntax_parser.h"

#include <stdio.h>
#include <string.h>

static int failures;

#define CHECK(condition)                                                                                               \
    do                                                                                                                 \
    {                                                                                                                  \
        if(!(condition))                                                                                               \
        {                                                                                                              \
            fprintf(stderr, "%s:%d: check failed: %s\n", __FILE__, __LINE__, #condition);                              \
            ++failures;                                                                                                \
        }                                                                                                              \
    } while(0)

static size_t count_kind(const XsSyntaxNode *node, XsSyntaxKind kind)
{
    if(node == nullptr)
        return 0;
    size_t count = node->kind == kind ? 1 : 0;
    for(size_t index = 0; index < node->child_count; ++index)
        count += count_kind(node->children[index], kind);
    return count;
}

static const XsSyntaxNode *find_kind_with_flag(const XsSyntaxNode *node, XsSyntaxKind kind, uint32_t flag)
{
    if(node == nullptr)
        return nullptr;
    if(node->kind == kind && (node->flags & flag) != 0)
        return node;
    for(size_t index = 0; index < node->child_count; ++index)
    {
        const XsSyntaxNode *found = find_kind_with_flag(node->children[index], kind, flag);
        if(found != nullptr)
            return found;
    }
    return nullptr;
}

static void test_function_tree(void)
{
    const char *text = "fn Add(a: Int, b: Int) -> Int {\n    result: Int = a + b;\n    return result;\n}\n";
    XsSource source = {.path = "Add.xs", .text = text, .length = strlen(text)};
    XsDiagnostics diagnostics;
    XsSyntaxTree tree;
    xs_diagnostics_init(&diagnostics);
    CHECK(xs_syntax_parse(&source, 42, &diagnostics, &tree));
    CHECK(tree.root != nullptr && tree.root->kind == XS_SYNTAX_FILE);
    if(tree.root == nullptr)
    {
        xs_syntax_tree_free(&tree);
        xs_diagnostics_free(&diagnostics);
        return;
    }
    CHECK(tree.root->child_count == 1);
    const XsSyntaxNode *function = xs_syntax_find_first(tree.root, XS_SYNTAX_DECL_FUNCTION);
    CHECK(function != nullptr);
    if(function == nullptr)
    {
        xs_syntax_tree_free(&tree);
        xs_diagnostics_free(&diagnostics);
        return;
    }
    CHECK(function->span.file_id == 42);
    CHECK(function->span.start_line == 1);
    CHECK(xs_syntax_find_first(function, XS_SYNTAX_PARAMETER) != nullptr);
    CHECK(xs_syntax_find_first(function, XS_SYNTAX_TYPE_NAMED) != nullptr);
    CHECK(xs_syntax_find_first(function, XS_SYNTAX_STMT_VARIABLE) != nullptr);
    CHECK(xs_syntax_find_first(function, XS_SYNTAX_EXPR_BINARY) != nullptr);
    CHECK(xs_syntax_find_first(function, XS_SYNTAX_STMT_RETURN) != nullptr);
    xs_syntax_tree_free(&tree);
    xs_diagnostics_free(&diagnostics);
}

static void test_import_and_macro(void)
{
    const char *text = "import Math::Advanced;\n"
                       "macro_rules! identity { ($value:expr) -> { $value }; }\n"
                       "fn Main() { identity!(42); }\n";
    XsSource source = {.path = "Main.xs", .text = text, .length = strlen(text)};
    XsDiagnostics diagnostics;
    XsSyntaxTree tree;
    xs_diagnostics_init(&diagnostics);
    CHECK(xs_syntax_parse(&source, 7, &diagnostics, &tree));
    CHECK(xs_syntax_find_first(tree.root, XS_SYNTAX_DECL_MODULE) == nullptr);
    CHECK(xs_syntax_find_first(tree.root, XS_SYNTAX_DECL_IMPORT) != nullptr);
    CHECK(xs_syntax_find_first(tree.root, XS_SYNTAX_DECL_MACRO) != nullptr);
    CHECK(xs_syntax_find_first(tree.root, XS_SYNTAX_MACRO_MATCHER_FRAGMENT) != nullptr);
    CHECK(xs_syntax_find_first(tree.root, XS_SYNTAX_MACRO_EXPANSION_VARIABLE) != nullptr);
    CHECK(xs_syntax_find_first(tree.root, XS_SYNTAX_STMT_MACRO_CALL) != nullptr);
    CHECK(xs_macro_validate(&tree, &diagnostics));
    xs_syntax_tree_free(&tree);
    xs_diagnostics_free(&diagnostics);
}

static void test_macro_call_declaration_structure(void)
{
    const char *text = "macro_rules! create_item { () -> {}; }\n"
                       "create_item!();\n"
                       "class Host { create_item!(); }\n";
    XsSource source = {.path = "MacroDeclarationCall.xs", .text = text, .length = strlen(text)};
    XsDiagnostics diagnostics;
    XsSyntaxTree tree;
    xs_diagnostics_init(&diagnostics);
    CHECK(xs_syntax_parse(&source, 8, &diagnostics, &tree));
    CHECK(count_kind(tree.root, XS_SYNTAX_DECL_MACRO_CALL) == 2);
    const XsSyntaxNode *call = xs_syntax_find_first(tree.root, XS_SYNTAX_DECL_MACRO_CALL);
    CHECK(call != nullptr);
    CHECK(call == nullptr || xs_syntax_find_first(call, XS_SYNTAX_EXPR_MACRO_CALL) != nullptr);
    CHECK(xs_macro_validate(&tree, &diagnostics));
    xs_syntax_tree_free(&tree);
    xs_diagnostics_free(&diagnostics);
}

static void test_control_flow_structure(void)
{
    const char *text = "fn Flow(value: Int) {\n"
                       "  for (i: Int = 0; i < 3; i = i + 1) { continue; }\n"
                       "  while (value > 0) { break; }\n"
                       "  do { continue; } while (false);\n"
                       "  match (value) { 0 -> { return; }, else -> { return; }, }\n"
                       "}\n";
    XsSource source = {.path = "Flow.xs", .text = text, .length = strlen(text)};
    XsDiagnostics diagnostics;
    XsSyntaxTree tree;
    xs_diagnostics_init(&diagnostics);
    CHECK(xs_syntax_parse(&source, 9, &diagnostics, &tree));
    const XsSyntaxNode *classic_for = xs_syntax_find_first(tree.root, XS_SYNTAX_STMT_FOR);
    CHECK(classic_for != nullptr);
    CHECK(classic_for == nullptr ||
          (classic_for->flags &
           (XS_SYNTAX_FLAG_FOR_INITIALIZER | XS_SYNTAX_FLAG_FOR_CONDITION | XS_SYNTAX_FLAG_FOR_UPDATE)) ==
              (XS_SYNTAX_FLAG_FOR_INITIALIZER | XS_SYNTAX_FLAG_FOR_CONDITION | XS_SYNTAX_FLAG_FOR_UPDATE));
    CHECK(xs_syntax_find_first(tree.root, XS_SYNTAX_STMT_WHILE) != nullptr);
    const XsSyntaxNode *post_test = find_kind_with_flag(tree.root, XS_SYNTAX_STMT_WHILE, XS_SYNTAX_FLAG_POST_TEST_LOOP);
    CHECK(post_test != nullptr);
    CHECK(post_test == nullptr || post_test->child_count == 2);
    CHECK(xs_syntax_find_first(tree.root, XS_SYNTAX_STMT_MATCH) != nullptr);
    xs_syntax_tree_free(&tree);
    xs_diagnostics_free(&diagnostics);
}

static void test_control_flow_expression_structure(void)
{
    const char *text = "fn Choose(value: Int) -> Int {\n"
                       "  selected: Int = if (value > 0) { 1 } else { 0 };\n"
                       "  return match (selected) { 0 -> { 10 }, else -> { 20 }, };\n"
                       "}\n";
    XsSource source = {.path = "ControlFlowExpressions.xs", .text = text, .length = strlen(text)};
    XsDiagnostics diagnostics;
    XsSyntaxTree tree;
    xs_diagnostics_init(&diagnostics);
    CHECK(xs_syntax_parse(&source, 28, &diagnostics, &tree));
    CHECK(xs_syntax_find_first(tree.root, XS_SYNTAX_EXPR_IF) != nullptr);
    CHECK(xs_syntax_find_first(tree.root, XS_SYNTAX_EXPR_MATCH) != nullptr);
    xs_syntax_tree_free(&tree);
    xs_diagnostics_free(&diagnostics);
}

static void test_if_expression_requires_else(void)
{
    const char *text = "fn Choose(value: Int) -> Int {\n"
                       "  return if (value > 0) { 1 };\n"
                       "}\n";
    XsSource source = {.path = "InvalidIfExpression.xs", .text = text, .length = strlen(text)};
    XsDiagnostics diagnostics;
    XsSyntaxTree tree;
    xs_diagnostics_init(&diagnostics);
    CHECK(!xs_syntax_parse(&source, 29, &diagnostics, &tree));
    CHECK(xs_diagnostics_has_error(&diagnostics));
    xs_syntax_tree_free(&tree);
    xs_diagnostics_free(&diagnostics);
}

static void test_function_expression_structure(void)
{
    const char *text = "fn spawn_tasks() {\n"
                       "  std::thread::spawn(move fn() { 42 });\n"
                       "  mapper: Mapper = fn(value) { value + 1 };\n"
                       "}\n";
    XsSource source = {.path = "ThreadClosure.xs", .text = text, .length = strlen(text)};
    XsDiagnostics diagnostics;
    XsSyntaxTree tree;
    xs_diagnostics_init(&diagnostics);
    CHECK(xs_syntax_parse(&source, 17, &diagnostics, &tree));
    const XsSyntaxNode *function_expression = xs_syntax_find_first(tree.root, XS_SYNTAX_EXPR_FUNCTION);
    CHECK(function_expression != nullptr);
    CHECK(function_expression == nullptr || (function_expression->flags & XS_SYNTAX_FLAG_MOVE_CAPTURE) != 0);
    CHECK(count_kind(tree.root, XS_SYNTAX_EXPR_FUNCTION) == 2);
    CHECK(xs_syntax_find_first(tree.root, XS_SYNTAX_PARAMETER) != nullptr);
    CHECK(xs_syntax_find_first(tree.root, XS_SYNTAX_TYPE_NAMED) != nullptr);
    xs_syntax_tree_free(&tree);
    xs_diagnostics_free(&diagnostics);
}

static void test_function_expression_rejects_written_types(void)
{
    const char *text = "fn main() { mapper: Mapper = fn(value: Int) -> Int { value }; }\n";
    XsSource source = {.path = "TypedLambda.xs", .text = text, .length = strlen(text)};
    XsDiagnostics diagnostics;
    XsSyntaxTree tree;
    xs_diagnostics_init(&diagnostics);
    CHECK(!xs_syntax_parse(&source, 31, &diagnostics, &tree));
    CHECK(xs_diagnostics_has_error(&diagnostics));
    xs_syntax_tree_free(&tree);
    xs_diagnostics_free(&diagnostics);
}

static void test_new_expression_structure(void)
{
    const char *text = "fn main() {\n"
                       "  user: User = new User();\n"
                       "  client: std::http::Client = new std::http::Client();\n"
                       "  factory: Factory<Str> = new Factory<Str>();\n"
                       "}\n";
    XsSource source = {.path = "NewExpression.xs", .text = text, .length = strlen(text)};
    XsDiagnostics diagnostics;
    XsSyntaxTree tree;
    xs_diagnostics_init(&diagnostics);
    CHECK(xs_syntax_parse(&source, 18, &diagnostics, &tree));
    CHECK(count_kind(tree.root, XS_SYNTAX_EXPR_NEW) == 3);
    const XsSyntaxNode *created = xs_syntax_find_first(tree.root, XS_SYNTAX_EXPR_NEW);
    CHECK(created != nullptr);
    CHECK(created == nullptr || created->child_count == 1);
    CHECK(created == nullptr || created->children[0]->kind == XS_SYNTAX_TYPE_NAMED);
    CHECK(xs_syntax_find_first(tree.root, XS_SYNTAX_STMT_VARIABLE) != nullptr);
    xs_syntax_tree_free(&tree);
    xs_diagnostics_free(&diagnostics);
}

static void test_associated_new_constructor_is_rejected(void)
{
    const char *text = "fn main() { value := Type::new(); }\n";
    XsSource source = {.path = "AssociatedNew.xs", .text = text, .length = strlen(text)};
    XsDiagnostics diagnostics;
    XsSyntaxTree tree;
    xs_diagnostics_init(&diagnostics);
    CHECK(!xs_syntax_parse(&source, 19, &diagnostics, &tree));
    CHECK(xs_diagnostics_has_error(&diagnostics));
    xs_syntax_tree_free(&tree);
    xs_diagnostics_free(&diagnostics);
}

static void test_else_discard_statement_structure(void)
{
    const char *text = "fn main() { else: a = new(); if (true) {} else: b = new(); }\n";
    XsSource source = {.path = "Discard.xs", .text = text, .length = strlen(text)};
    XsDiagnostics diagnostics;
    XsSyntaxTree tree;
    xs_diagnostics_init(&diagnostics);
    CHECK(xs_syntax_parse(&source, 30, &diagnostics, &tree));
    const XsSyntaxNode *discard = xs_syntax_find_first(tree.root, XS_SYNTAX_STMT_DISCARD);
    CHECK(discard != nullptr);
    CHECK(discard == nullptr || discard->child_count == 1);
    CHECK(discard == nullptr || discard->children[0]->kind == XS_SYNTAX_EXPR_ASSIGNMENT);
    CHECK(count_kind(tree.root, XS_SYNTAX_STMT_DISCARD) == 2);
    CHECK(count_kind(tree.root, XS_SYNTAX_EXPR_NEW) == 2);
    xs_syntax_tree_free(&tree);
    xs_diagnostics_free(&diagnostics);
}

static void test_tail_expression_semicolon_split(void)
{
    const char *text = "fn Value() -> Int { 42 }\n"
                       "fn Discard() { 42; }\n";
    XsSource source = {.path = "TailExpression.xs", .text = text, .length = strlen(text)};
    XsDiagnostics diagnostics;
    XsSyntaxTree tree;
    xs_diagnostics_init(&diagnostics);
    CHECK(xs_syntax_parse(&source, 66, &diagnostics, &tree));
    CHECK(count_kind(tree.root, XS_SYNTAX_STMT_EXPRESSION) == 2);
    const XsSyntaxNode *first = xs_syntax_find_first(tree.root, XS_SYNTAX_STMT_EXPRESSION);
    CHECK(first != nullptr);
    CHECK(first == nullptr || (first->flags & XS_SYNTAX_FLAG_DISCARDED) == 0);
    const XsSyntaxNode *second = nullptr;
    if(first != nullptr)
    {
        const XsSyntaxNode *root = tree.root;
        for(size_t index = 0; index < root->child_count; ++index)
        {
            const XsSyntaxNode *candidate = xs_syntax_find_first(root->children[index], XS_SYNTAX_STMT_EXPRESSION);
            if(candidate != nullptr && candidate != first)
                second = candidate;
        }
    }
    CHECK(second != nullptr);
    CHECK(second == nullptr || (second->flags & XS_SYNTAX_FLAG_DISCARDED) != 0);
    xs_syntax_tree_free(&tree);
    xs_diagnostics_free(&diagnostics);
}

static void test_top_level_execution_stays_invalid(void)
{
    const char *text = "Run();\n";
    XsSource source = {.path = "TopLevelExecution.xs", .text = text, .length = strlen(text)};
    XsDiagnostics diagnostics;
    XsSyntaxTree tree;
    xs_diagnostics_init(&diagnostics);
    CHECK(!xs_syntax_parse(&source, 20, &diagnostics, &tree));
    CHECK(xs_diagnostics_has_error(&diagnostics));
    xs_syntax_tree_free(&tree);
    xs_diagnostics_free(&diagnostics);
}

static void test_data_literal_and_member_access_structure(void)
{
    const char *text = "fn Update(user: User) {\n"
                       "  next: User = { name: \"Alpha\" };\n"
                       "  name: Str = user.name;\n"
                       "}\n";
    XsSource source = {.path = "DataAccess.xs", .text = text, .length = strlen(text)};
    XsDiagnostics diagnostics;
    XsSyntaxTree tree;
    xs_diagnostics_init(&diagnostics);
    CHECK(xs_syntax_parse(&source, 21, &diagnostics, &tree));
    CHECK(count_kind(tree.root, XS_SYNTAX_EXPR_OBJECT_LITERAL) == 1);
    CHECK(count_kind(tree.root, XS_SYNTAX_OBJECT_FIELD) == 1);
    CHECK(xs_syntax_find_first(tree.root, XS_SYNTAX_EXPR_MEMBER_ACCESS) != nullptr);
    xs_syntax_tree_free(&tree);
    xs_diagnostics_free(&diagnostics);
}

static void test_function_type_structure(void)
{
    const char *text = "fn main() {\n"
                       "  mapper: fn(Int, Str) -> Bool = fn(value, name) { true };\n"
                       "  done: fn() -> () = fn() { return; };\n"
                       "}\n";
    XsSource source = {.path = "FunctionTypes.xs", .text = text, .length = strlen(text)};
    XsDiagnostics diagnostics;
    XsSyntaxTree tree;
    xs_diagnostics_init(&diagnostics);
    CHECK(xs_syntax_parse(&source, 22, &diagnostics, &tree));
    CHECK(count_kind(tree.root, XS_SYNTAX_TYPE_FUNCTION) == 2);
    CHECK(xs_syntax_find_first(tree.root, XS_SYNTAX_TYPE_UNIT) != nullptr);
    CHECK(count_kind(tree.root, XS_SYNTAX_EXPR_FUNCTION) == 2);
    xs_syntax_tree_free(&tree);
    xs_diagnostics_free(&diagnostics);
}

static void test_character_literal_structure(void)
{
    const char *text = "fn Main(value: Char) {\n"
                       "  letter: Char = 'A';\n"
                       "  newline: Char = '\\n';\n"
                       "  match (value) { 'A' -> { return; }, else -> { return; }, }\n"
                       "}\n";
    XsSource source = {.path = "CharacterLiteral.xs", .text = text, .length = strlen(text)};
    XsDiagnostics diagnostics;
    XsSyntaxTree tree;
    xs_diagnostics_init(&diagnostics);
    CHECK(xs_syntax_parse(&source, 24, &diagnostics, &tree));
    CHECK(count_kind(tree.root, XS_SYNTAX_EXPR_LITERAL) >= 2);
    CHECK(xs_syntax_find_first(tree.root, XS_SYNTAX_PATTERN_LITERAL) != nullptr);
    xs_syntax_tree_free(&tree);
    xs_diagnostics_free(&diagnostics);
}

static void test_optional_operator_structure(void)
{
    const char *text = "fn Main(user: Optional<User>, fallback: Str) {\n"
                       "  name: Optional<Str> = user?.Name;\n"
                       "  direct: Str = user.Name;\n"
                       "  name = std::optional::None;\n"
                       "  display: Str = name ?? fallback;\n"
                       "  name ?"
                       "?= Some(\"guest\");\n"
                       "  forced: Str = name!;\n"
                       "}\n";
    XsSource source = {.path = "OptionalOperators.xs", .text = text, .length = strlen(text)};
    XsDiagnostics diagnostics;
    XsSyntaxTree tree;
    xs_diagnostics_init(&diagnostics);
    CHECK(xs_syntax_parse(&source, 27, &diagnostics, &tree));
    CHECK(count_kind(tree.root, XS_SYNTAX_EXPR_OPTIONAL_MEMBER_ACCESS) == 1);
    CHECK(count_kind(tree.root, XS_SYNTAX_EXPR_OPTIONAL_FORGIVING) == 1);
    CHECK(count_kind(tree.root, XS_SYNTAX_EXPR_BINARY) >= 1);
    CHECK(count_kind(tree.root, XS_SYNTAX_EXPR_ASSIGNMENT) >= 1);
    CHECK(count_kind(tree.root, XS_SYNTAX_EXPR_MEMBER_ACCESS) >= 1);
    xs_syntax_tree_free(&tree);
    xs_diagnostics_free(&diagnostics);
}

static void test_postfix_binds_before_prefix(void)
{
    const char *text = "fn Main(value: Value) { if (!value.is_empty) { return; } }\n";
    XsSource source = {.path = "PrefixPostfix.xs", .text = text, .length = strlen(text)};
    XsDiagnostics diagnostics;
    XsSyntaxTree tree;
    xs_diagnostics_init(&diagnostics);
    CHECK(xs_syntax_parse(&source, 64, &diagnostics, &tree));
    const XsSyntaxNode *unary = xs_syntax_find_first(tree.root, XS_SYNTAX_EXPR_UNARY);
    CHECK(unary != nullptr && unary->child_count == 1);
    CHECK(unary == nullptr || unary->child_count != 1 || unary->children[0]->kind == XS_SYNTAX_EXPR_MEMBER_ACCESS);
    xs_syntax_tree_free(&tree);
    xs_diagnostics_free(&diagnostics);
}

static void test_control_parentheses_are_optional(void)
{
    const char *text = "fn main(flag: Bool, items: [Int]) {\n"
                       "  if flag { while flag { break; } } else if !flag { return; }\n"
                       "  do { continue; } while flag;\n"
                       "  for item in items { item; }\n"
                       "  for index: Int = 0; index < 2; index++ { index; }\n"
                       "  match flag { true -> return;, else -> return;, }\n"
                       "}\n"
                       "fn choose(flag: Bool) -> Int { return if flag { 1 } else { 2 }; }\n"
                       "fn inspect(flag: Bool) -> Int { return match flag { true -> { 1 }, else -> { 0 }, }; }\n";
    XsSource source = {.path = "OptionalControlParentheses.xs", .text = text, .length = strlen(text)};
    XsDiagnostics diagnostics;
    XsSyntaxTree tree;
    xs_diagnostics_init(&diagnostics);
    CHECK(xs_syntax_parse(&source, 64, &diagnostics, &tree));
    CHECK(xs_syntax_find_first(tree.root, XS_SYNTAX_STMT_IF) != nullptr);
    CHECK(xs_syntax_find_first(tree.root, XS_SYNTAX_STMT_FOR_EACH) != nullptr);
    CHECK(xs_syntax_find_first(tree.root, XS_SYNTAX_EXPR_IF) != nullptr);
    CHECK(xs_syntax_find_first(tree.root, XS_SYNTAX_EXPR_MATCH) != nullptr);
    xs_syntax_tree_free(&tree);
    xs_diagnostics_free(&diagnostics);
}

static void test_positional_and_named_tuple_structure(void)
{
    const char *text = "fn pair() -> (Str, Int) { return (\"Leitwolf\", 29); }\n"
                       "fn person() -> (name: Str, age: Int) {\n"
                       "  value := (name: \"Leitwolf\", age: 29);\n"
                       "  value.age;\n"
                       "  return value;\n"
                       "}\n"
                       "fn singleton() -> (name: Str) { return (name: \"Leitwolf\"); }\n"
                       "fn first(value: (Int, Int)) -> Int { return value.0; }\n";
    XsSource source = {.path = "Tuples.xs", .text = text, .length = strlen(text)};
    XsDiagnostics diagnostics;
    XsSyntaxTree tree;
    xs_diagnostics_init(&diagnostics);
    CHECK(xs_syntax_parse(&source, 65, &diagnostics, &tree));
    CHECK(count_kind(tree.root, XS_SYNTAX_TYPE_TUPLE) == 4);
    CHECK(count_kind(tree.root, XS_SYNTAX_EXPR_TUPLE) == 3);
    CHECK(count_kind(tree.root, XS_SYNTAX_TUPLE_FIELD) == 6);
    const XsSyntaxNode *member = xs_syntax_find_first(tree.root, XS_SYNTAX_EXPR_MEMBER_ACCESS);
    CHECK(member != nullptr);
    xs_syntax_tree_free(&tree);
    xs_diagnostics_free(&diagnostics);
}

static void test_chained_positional_tuple_projection(void)
{
    const char *text = "fn nested(value: ((Long, Bool), Long)) -> Long { return value.0.0 + value.1; }\n";
    XsSource source = {.path = "NestedTuple.xs", .text = text, .length = strlen(text)};
    XsDiagnostics diagnostics;
    XsSyntaxTree tree;
    xs_diagnostics_init(&diagnostics);
    CHECK(xs_syntax_parse(&source, 67, &diagnostics, &tree));
    CHECK(count_kind(tree.root, XS_SYNTAX_EXPR_MEMBER_ACCESS) == 3);
    xs_syntax_tree_free(&tree);
    xs_diagnostics_free(&diagnostics);
}

static void test_tuple_forms_cannot_be_mixed(void)
{
    const char *text = "fn invalid(value: (left: Int, Int)) { pair := (left: 1, 2); }\n";
    XsSource source = {.path = "MixedTuple.xs", .text = text, .length = strlen(text)};
    XsDiagnostics diagnostics;
    XsSyntaxTree tree;
    xs_diagnostics_init(&diagnostics);
    CHECK(!xs_syntax_parse(&source, 66, &diagnostics, &tree));
    CHECK(xs_diagnostics_has_error(&diagnostics));
    xs_syntax_tree_free(&tree);
    xs_diagnostics_free(&diagnostics);
}

static void test_expression_turbofish_structure(void)
{
    const char *text = "fn Main() {\n"
                       "  value: Int = Factory::<Int>();\n"
                       "  name: Optional<Str> = std::optional::Some::<Str>(\"xs\");\n"
                       "}\n";
    XsSource source = {.path = "Turbofish.xs", .text = text, .length = strlen(text)};
    XsDiagnostics diagnostics;
    XsSyntaxTree tree;
    xs_diagnostics_init(&diagnostics);
    CHECK(xs_syntax_parse(&source, 60, &diagnostics, &tree));
    CHECK(count_kind(tree.root, XS_SYNTAX_EXPR_CALL) == 2);
    CHECK(count_kind(tree.root, XS_SYNTAX_EXPR_GENERIC_QUALIFIER) == 2);
    xs_syntax_tree_free(&tree);
    xs_diagnostics_free(&diagnostics);
}

static void test_removed_exception_syntax_is_rejected(void)
{
    const char *text = "fn Main() throws Error { try { throw Error(); } catch (error: Error) {} }\n";
    XsSource source = {.path = "RemovedExceptions.xs", .text = text, .length = strlen(text)};
    XsDiagnostics diagnostics;
    XsSyntaxTree tree;
    xs_diagnostics_init(&diagnostics);
    CHECK(!xs_syntax_parse(&source, 31, &diagnostics, &tree));
    CHECK(xs_diagnostics_has_error(&diagnostics));
    xs_syntax_tree_free(&tree);
    xs_diagnostics_free(&diagnostics);
}

static void test_result_propagation_structure(void)
{
    const char *text = "fn Main() {\n"
                       "  DoWork()@;\n"
                       "  file.read_to_string(&mut content)@;\n"
                       "}\n";
    XsSource source = {.path = "ResultPropagation.xs", .text = text, .length = strlen(text)};
    XsDiagnostics diagnostics;
    XsSyntaxTree tree;
    xs_diagnostics_init(&diagnostics);
    CHECK(xs_syntax_parse(&source, 28, &diagnostics, &tree));
    CHECK(count_kind(tree.root, XS_SYNTAX_EXPR_RESULT_PROPAGATION) == 2);
    CHECK(count_kind(tree.root, XS_SYNTAX_EXPR_CALL) >= 1);
    CHECK(count_kind(tree.root, XS_SYNTAX_EXPR_METHOD_CALL) >= 1);
    xs_syntax_tree_free(&tree);
    xs_diagnostics_free(&diagnostics);
}

static void test_lifetime_type_structure(void)
{
    const char *text =
        "fn Print(first: &'a User, second: &'b mut User, shared: &'static Str, inferred: &'else User) {\n"
        "  return;\n"
        "}\n";
    XsSource source = {.path = "Lifetimes.xs", .text = text, .length = strlen(text)};
    XsDiagnostics diagnostics;
    XsSyntaxTree tree;
    xs_diagnostics_init(&diagnostics);
    CHECK(xs_syntax_parse(&source, 25, &diagnostics, &tree));
    CHECK(count_kind(tree.root, XS_SYNTAX_LIFETIME) == 4);
    CHECK(xs_syntax_find_first(tree.root, XS_SYNTAX_TYPE_REFERENCE) != nullptr);
    CHECK(xs_syntax_find_first(tree.root, XS_SYNTAX_TYPE_MUTABLE_REFERENCE) != nullptr);
    xs_syntax_tree_free(&tree);
    xs_diagnostics_free(&diagnostics);
}

static void test_underscore_lifetime_is_rejected(void)
{
    const char *text = "fn Print(inferred: &'_ User) {}\n";
    XsSource source = {.path = "BadLifetime.xs", .text = text, .length = strlen(text)};
    XsDiagnostics diagnostics;
    XsSyntaxTree tree;
    xs_diagnostics_init(&diagnostics);
    CHECK(!xs_syntax_parse(&source, 61, &diagnostics, &tree));
    CHECK(xs_diagnostics_has_error(&diagnostics));
    xs_syntax_tree_free(&tree);
    xs_diagnostics_free(&diagnostics);
}

static void test_else_type_placeholder_structure(void)
{
    const char *text = "class Box<T> { value: T; }\n"
                       "fn Use(value: Box<else>) {}\n";
    XsSource source = {.path = "ElseTypePlaceholder.xs", .text = text, .length = strlen(text)};
    XsDiagnostics diagnostics;
    XsSyntaxTree tree;
    xs_diagnostics_init(&diagnostics);
    CHECK(xs_syntax_parse(&source, 62, &diagnostics, &tree));
    CHECK(count_kind(tree.root, XS_SYNTAX_TYPE_GENERIC) == 1);
    xs_syntax_tree_free(&tree);
    xs_diagnostics_free(&diagnostics);
}

static void test_nested_generic_type_closers(void)
{
    const char *text = "interface Parser<T> { fn Parse(value: T); }\n"
                       "fn UseParser<T: Parser<Box<Int>> >(value: T, items: List<Box<Int>>) {}\n";
    XsSource source = {.path = "NestedGenerics.xs", .text = text, .length = strlen(text)};
    XsDiagnostics diagnostics;
    XsSyntaxTree tree;
    xs_diagnostics_init(&diagnostics);
    CHECK(xs_syntax_parse(&source, 26, &diagnostics, &tree));
    CHECK(count_kind(tree.root, XS_SYNTAX_TYPE_GENERIC) == 4);
    xs_syntax_tree_free(&tree);
    xs_diagnostics_free(&diagnostics);
}

static void test_attribute_structure(void)
{
    const char *text = "#![NoMain]\n"
                       "#[Deprecated(\"use NewMain\")]\n"
                       "#[Derive(std::attrs::Ord, std::attrs::Partial)]\n"
                       "fn Main() {}\n";
    XsSource source = {.path = "Attributes.xs", .text = text, .length = strlen(text)};
    XsDiagnostics diagnostics;
    XsSyntaxTree tree;
    xs_diagnostics_init(&diagnostics);
    CHECK(xs_syntax_parse(&source, 49, &diagnostics, &tree));
    CHECK(count_kind(tree.root, XS_SYNTAX_ATTRIBUTE) == 3);
    CHECK(count_kind(tree.root, XS_SYNTAX_ATTRIBUTE_LIST) == 1);
    const XsSyntaxNode *attribute = xs_syntax_find_first(tree.root, XS_SYNTAX_ATTRIBUTE);
    CHECK(attribute != nullptr);
    CHECK(attribute == nullptr || (attribute->flags & XS_SYNTAX_FLAG_INNER_ATTRIBUTE) != 0);
    const XsSyntaxNode *function = xs_syntax_find_first(tree.root, XS_SYNTAX_DECL_FUNCTION);
    CHECK(function != nullptr);
    CHECK(function == nullptr || xs_syntax_find_first(function, XS_SYNTAX_ATTRIBUTE_LIST) != nullptr);
    xs_syntax_tree_free(&tree);
    xs_diagnostics_free(&diagnostics);
}

static void test_complete_program_expression_shapes(void)
{
    const char *text = "fn Build(items: Items, pair: (Int, Int)) {\n"
                       "  map: [Str: Int] = [];\n"
                       "  factory := Factory::<Str>();\n"
                       "  entry := Entry { name: \"Alpha\", value: 7 };\n"
                       "  for (item: Entry in items) { item.run(); }\n"
                       "  (left, right): (Int, Int) = pair;\n"
                       "  task := move async fn(value) { value };\n"
                       "  loop { break; }\n"
                       "}\n";
    XsSource source = {.path = "CompleteShapes.xs", .text = text, .length = strlen(text)};
    XsDiagnostics diagnostics;
    XsSyntaxTree tree;
    xs_diagnostics_init(&diagnostics);
    CHECK(xs_syntax_parse(&source, 63, &diagnostics, &tree));
    CHECK(count_kind(tree.root, XS_SYNTAX_EXPR_GENERIC_QUALIFIER) == 1);
    CHECK(count_kind(tree.root, XS_SYNTAX_EXPR_TYPED_OBJECT_LITERAL) == 1);
    CHECK(count_kind(tree.root, XS_SYNTAX_PATTERN_TYPED) == 2);
    CHECK(count_kind(tree.root, XS_SYNTAX_DECL_PATTERN_VARIABLE) == 1);
    CHECK(count_kind(tree.root, XS_SYNTAX_STMT_LOOP) == 1);
    const XsSyntaxNode *function = find_kind_with_flag(tree.root, XS_SYNTAX_EXPR_FUNCTION, XS_SYNTAX_FLAG_ASYNC);
    CHECK(function != nullptr);
    CHECK(function == nullptr || (function->flags & XS_SYNTAX_FLAG_MOVE_CAPTURE) != 0);
    xs_syntax_tree_free(&tree);
    xs_diagnostics_free(&diagnostics);
}

int main(void)
{
    test_function_tree();
    test_import_and_macro();
    test_macro_call_declaration_structure();
    test_control_flow_structure();
    test_control_flow_expression_structure();
    test_if_expression_requires_else();
    test_function_expression_structure();
    test_function_expression_rejects_written_types();
    test_new_expression_structure();
    test_associated_new_constructor_is_rejected();
    test_else_discard_statement_structure();
    test_tail_expression_semicolon_split();
    test_top_level_execution_stays_invalid();
    test_data_literal_and_member_access_structure();
    test_function_type_structure();
    test_character_literal_structure();
    test_optional_operator_structure();
    test_postfix_binds_before_prefix();
    test_control_parentheses_are_optional();
    test_positional_and_named_tuple_structure();
    test_chained_positional_tuple_projection();
    test_tuple_forms_cannot_be_mixed();
    test_expression_turbofish_structure();
    test_removed_exception_syntax_is_rejected();
    test_result_propagation_structure();
    test_lifetime_type_structure();
    test_underscore_lifetime_is_rejected();
    test_else_type_placeholder_structure();
    test_nested_generic_type_closers();
    test_attribute_structure();
    test_complete_program_expression_shapes();
    return failures == 0 ? 0 : 1;
}
