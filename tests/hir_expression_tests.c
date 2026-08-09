/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 */

#include "xs/diagnostic.h"
#include "xs/hir/expression_check.h"
#include "xs/hir/symbol_table.h"
#include "xs/hir/type_resolution.h"
#include "xs/macro.h"
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

static bool add_file(const char *text, uint64_t file_id, XsSyntaxTree *tree, XsHirSymbolTable *symbols,
                     XsDiagnostics *diagnostics)
{
    XsSource source = {.path = "HirExpressions.vxs", .module_name = "App", .text = text, .length = strlen(text)};
    if(!xs_syntax_parse(&source, file_id, diagnostics, tree))
        return false;
    return xs_hir_collect_symbols(tree, symbols, diagnostics);
}

static bool check_single_source_expressions(const char *text)
{
    XsSyntaxTree tree;
    XsHirSymbolTable symbols;
    XsHirImportScope import;
    XsDiagnostics diagnostics;
    xs_diagnostics_init(&diagnostics);
    xs_hir_symbol_table_init(&symbols);
    xs_hir_import_scope_init(&import);
    bool success = add_file(text, 72, &tree, &symbols, &diagnostics);
    if(success)
        success = xs_hir_resolve_imports(&tree, &symbols, &import, &diagnostics);
    if(success)
        success = xs_hir_resolve_types(&tree, &symbols, &import, &diagnostics);
    if(success)
        success = xs_hir_check_expression_types(&tree, &diagnostics);
    xs_hir_import_scope_free(&import);
    xs_hir_symbol_table_free(&symbols);
    xs_syntax_tree_free(&tree);
    xs_diagnostics_free(&diagnostics);
    return success;
}

static bool check_macro_expression_error(const char *main, uint64_t file_id)
{
    XsSource source = {.path = "MacroExpressionTypes.vxs", .module_name = "App", .text = main, .length = strlen(main)};
    XsSyntaxTree tree;
    XsHirSymbolTable symbols;
    XsHirImportScope import;
    XsMacroStatementExpansionSet statements;
    XsDiagnostics diagnostics;
    xs_diagnostics_init(&diagnostics);
    xs_hir_symbol_table_init(&symbols);
    xs_hir_import_scope_init(&import);
    bool success = xs_syntax_parse(&source, file_id, &diagnostics, &tree);
    if(success)
        success = xs_macro_validate(&tree, &diagnostics);
    if(success)
        success = xs_macro_expand_statements(&tree, &diagnostics, &statements);
    if(success)
        success = xs_hir_collect_symbols(&tree, &symbols, &diagnostics);
    if(success)
        success = xs_hir_resolve_imports(&tree, &symbols, &import, &diagnostics);
    if(success)
        success = xs_hir_resolve_types_expanded(&tree, &statements, &symbols, &import, &diagnostics);
    if(success)
        success = !xs_hir_check_expression_types_with_macros(&tree, nullptr, &statements, &diagnostics) &&
                  xs_diagnostics_has_error(&diagnostics);
    xs_macro_statement_expansion_set_free(&statements);
    xs_hir_import_scope_free(&import);
    xs_hir_symbol_table_free(&symbols);
    xs_syntax_tree_free(&tree);
    xs_diagnostics_free(&diagnostics);
    return success;
}

static void test_literal_initializer_expression_types(void)
{
    const char *valid = ""
                        "fn Main() {\n"
                        "  a: Int = 1;\n"
                        "  b: Bool = true;\n"
                        "  numeric_false: Bool = 0;\n"
                        "  numeric_true: Bool = 2;\n"
                        "  c: &Str = \"hello\";\n"
                        "  d: Char = 'x';\n"
                        "  e: Float = 1.0;\n"
                        "}\n";
    CHECK(check_single_source_expressions(valid));
    CHECK(!check_single_source_expressions("fn Main() { value: Int = \"bad\"; }\n"));
    CHECK(check_single_source_expressions("fn Main() { value: Bool = 1; inferred := 1; }\n"));
    CHECK(!check_single_source_expressions("fn Main() { value: Char = 1; }\n"));
    CHECK(!check_single_source_expressions("fn Main() { value: Float = true; }\n"));
}

static void test_control_flow_initializer_expression_types(void)
{
    const char *valid_if = ""
                           "fn Main() {\n"
                           "  value: Int = if (true) { 1 } else { 2 };\n"
                           "}\n";
    const char *valid_match = ""
                              "fn Main() {\n"
                              "  value: &Str = match (1) { 0 -> { \"zero\" }, else -> { \"many\" }, };\n"
                              "}\n";
    CHECK(check_single_source_expressions(valid_if));
    CHECK(check_single_source_expressions(valid_match));
    CHECK(!check_single_source_expressions("fn Main() { value: Int = if (true) { 1; } else { 2 }; }\n"));
    CHECK(!check_single_source_expressions("fn Main() { value: Int = if (true) { \"bad\" } else { 1 }; }\n"));
    CHECK(!check_single_source_expressions(
        "fn Main() { value: Bool = match (1) { 0 -> { true }, else -> { \"bad\" }, }; }\n"));
}

static void test_optional_expression_types(void)
{
    const char *valid = ""
                        "fn Read() -> Optional<Str> {\n"
                        "  value: Optional<Str> = None;\n"
                        "  value = std::optional::Some(\"xs\");\n"
                        "  other: std::optional::Optional<Str> = std::optional::None;\n"
                        "  return Some(\"ok\");\n"
                        "}\n";
    CHECK(check_single_source_expressions(valid));
    CHECK(check_single_source_expressions(
        "fn Read(flag: Bool) -> Optional<Str> { return if (flag) { Some(\"ok\") } else { None }; }\n"));
    CHECK(!check_single_source_expressions("fn Main() { value: Int = None; }\n"));
    CHECK(!check_single_source_expressions("fn Main() { value: Int = std::optional::None; }\n"));
    CHECK(!check_single_source_expressions("fn Main() { value: Int = Some(1); }\n"));
    CHECK(check_single_source_expressions("fn Main() { value: Optional<Int> = 1; }\n"));
    CHECK(check_single_source_expressions("fn Main() { value: Int? = nil; }\n"));
    CHECK(!check_single_source_expressions("fn Main() { value: Optional<Int> = Some(); }\n"));
    CHECK(check_single_source_expressions("fn Read() -> Optional<Int> { return 1; }\n"));
}

static void test_owned_string_is_not_optional_or_implicitly_borrowed(void)
{
    const char *valid = ""
                        "fn Keep(value: String) -> String { return value; }\n"
                        "fn Borrow(value: &Str) -> &Str { return value; }\n";
    CHECK(check_single_source_expressions(valid));
    CHECK(!check_single_source_expressions("fn Main() { bad: String = \"Leitwolf\"; }\n"));
    CHECK(!check_single_source_expressions("fn Main() { bad: Str = \"Leitwolf\"; }\n"));
    CHECK(!check_single_source_expressions("fn Main() { bad: String = 1; }\n"));
    CHECK(!check_single_source_expressions("fn Main() { bad: String = nil; }\n"));
    CHECK(!check_single_source_expressions("fn Bad() -> String { return 1; }\n"));
}

static void test_binding_reassignment_errors(void)
{
    const char *valid = ""
                        "fn Main() {\n"
                        "  value: Int = 1;\n"
                        "  value = 2;\n"
                        "}\n";
    CHECK(check_single_source_expressions(valid));
    CHECK(!check_single_source_expressions("fn Main() { val value: Int = 1; value = 2; }\n"));
    CHECK(!check_single_source_expressions("fn Main() { const value: Int = 1; value = 2; }\n"));
}

static void test_constant_initializer_errors(void)
{
    CHECK(check_single_source_expressions("fn Main() { const value: Int = 1 + 2; }\n"));
    CHECK(check_single_source_expressions("fn Main() { static value: &Str = \"xs\"; }\n"));
    CHECK(check_single_source_expressions("fn RuntimeValue() -> Int { return 1; }\nfn Main() { const "
                                          "value: Int = RuntimeValue(); }\n"));
    CHECK(!check_single_source_expressions("fn Main() { const value: Int; }\n"));
    CHECK(!check_single_source_expressions("fn Main() { static value: Int; }\n"));
    CHECK(!check_single_source_expressions("fn RuntimeValue() -> Int { return 1; }\nfn Main() { static "
                                           "value: Int = RuntimeValue(); }\n"));
}

static void test_assignment_literal_expression_types(void)
{
    const char *valid = ""
                        "fn Main() {\n"
                        "  value: Int = 1;\n"
                        "  value = 2;\n"
                        "  flag: Bool = false;\n"
                        "  flag = true;\n"
                        "}\n";
    CHECK(check_single_source_expressions(valid));
    CHECK(!check_single_source_expressions("fn Main() { value: Int = 1; value = \"bad\"; }\n"));
    CHECK(check_single_source_expressions("fn Main() { value: Bool = false; value = 1; }\n"));
    CHECK(!check_single_source_expressions("fn Main() { value: &Str = \"ok\"; value = 'x'; }\n"));
}

static void test_control_flow_assignment_expression_types(void)
{
    const char *valid = ""
                        "fn Main() {\n"
                        "  value: Int = 0;\n"
                        "  value = if (true) { 1 } else { 2 };\n"
                        "}\n";
    CHECK(check_single_source_expressions(valid));
    CHECK(
        !check_single_source_expressions("fn Main() { value: Int = 0; value = if (true) { 1 } else { \"bad\"; }; }\n"));
}

static void test_return_literal_expression_types(void)
{
    CHECK(check_single_source_expressions("fn Count() -> Int { return 1; }\n"));
    CHECK(check_single_source_expressions("fn Flag() -> Bool { return true; }\n"));
    CHECK(check_single_source_expressions("fn Name() -> &Str { return \"xs\"; }\n"));
    CHECK(!check_single_source_expressions("fn Count() -> Int { return \"bad\"; }\n"));
    CHECK(check_single_source_expressions("fn Flag() -> Bool { return 1; }\n"));
    CHECK(!check_single_source_expressions("fn Name() -> &Str { return 'x'; }\n"));
}

static void test_control_flow_return_expression_types(void)
{
    CHECK(check_single_source_expressions("fn Count() -> Int { return if (true) { 1 } else { 2 }; }\n"));
    CHECK(check_single_source_expressions(
        "fn Name() -> &Str { return match (1) { 0 -> { \"zero\" }, else -> { \"many\" }, }; }\n"));
    CHECK(!check_single_source_expressions("fn Count() -> Int { return if (true) { 1 } else { \"bad\" }; }\n"));
    CHECK(
        check_single_source_expressions("fn Flag() -> Bool { return match (1) { 0 -> { true }, else -> { 1 }, }; }\n"));
    CHECK(!check_single_source_expressions(
        "fn Flag() -> Bool { return match (1) { 0 -> { true }, else -> { \"bad\" }, }; }\n"));
}

static void test_result_propagation_requires_result_return(void)
{
    const char *valid = ""
                        "fn DoWork() -> Result<Int, Error> { return Ok(1); }\n"
                        "fn DoMore() -> std::result::Result<Int, Error> { return Ok(2); }\n"
                        "fn Main() -> Result<Int, Error> {\n"
                        "  DoWork()@;\n"
                        "  DoMore()@;\n"
                        "  return Ok(1);\n"
                        "}\n";
    CHECK(check_single_source_expressions(valid));
    CHECK(check_single_source_expressions("async fn Main() -> Task<Result<Int, Error>> { Work()@; return Ok(1); }\n"));
    CHECK(!check_single_source_expressions("async fn Main() -> Task<Int> { Work()@; return Ok(1); }\n"));
    CHECK(check_single_source_expressions("fn Main() -> Result<Int, Error> { DoWork()@; return Ok(1); }\n"));
    CHECK(!check_single_source_expressions("fn Count() -> Int { return 1; }\n"
                                           "fn Main() -> Result<Int, Error> { Count()@; return "
                                           "Ok(1); }\n"));
    CHECK(!check_single_source_expressions("fn DoWork() -> Result<Int, Error> { return "
                                           "Ok(1); }\nfn Main() { DoWork()@; }\n"));
    CHECK(check_single_source_expressions("fn Plain() { return; }\n"));
    CHECK(!check_single_source_expressions("fn Main() -> Int { return Ok(1); }\n"));
    CHECK(!check_single_source_expressions("fn Main() -> () { Error(Error { message: \"x\" }); }\n"));
}

static void test_macro_literal_initializer_expression_errors(void)
{
    const char *main = ""
                       "macro_rules! bad { () -> { value: Int = \"bad\"; }; }\n"
                       "fn Main() { bad!(); }\n";
    CHECK(check_macro_expression_error(main, 96));
}

static void test_macro_binding_reassignment_errors(void)
{
    const char *main = ""
                       "macro_rules! bad { () -> { value = 2; }; }\n"
                       "fn Main() { val value: Int = 1; bad!(); }\n";
    CHECK(check_macro_expression_error(main, 97));
}

static void test_macro_static_runtime_initializer_errors(void)
{
    const char *main = ""
                       "fn RuntimeValue() -> Int { return 1; }\n"
                       "macro_rules! bad { () -> { static value: Int = RuntimeValue(); }; }\n"
                       "fn Main() { bad!(); }\n";
    CHECK(check_macro_expression_error(main, 100));
}

static void test_macro_assignment_literal_expression_errors(void)
{
    const char *main = ""
                       "macro_rules! bad { () -> { value = \"bad\"; }; }\n"
                       "fn Main() { value: Int = 1; bad!(); }\n";
    CHECK(check_macro_expression_error(main, 98));
}

static void test_macro_return_literal_expression_errors(void)
{
    const char *main = ""
                       "macro_rules! bad { () -> { return \"bad\"; }; }\n"
                       "fn Main() -> Int { bad!(); }\n";
    CHECK(check_macro_expression_error(main, 99));
}

static void test_op_referential_transparency_rules(void)
{
    CHECK(check_single_source_expressions("op Add(left: Int, right: Int) -> Int { return left + right; }\n"));
    CHECK(!check_single_source_expressions("fn Read() -> Int { return 1; }\nop Bad() -> Int { return Read(); }\n"));
    CHECK(!check_single_source_expressions("op Bad() -> Int { value: Int = 1; value = 2; return value; }\n"));
    CHECK(!check_single_source_expressions("op bad() -> User { return new User(); }\n"));
    CHECK(!check_single_source_expressions("macro_rules! bad { () -> { 1 }; }\nop Bad() -> Int { bad!(); }\n"));
}

static void test_property_accessor_expression_rules(void)
{
    const char *valid = ""
                        "class User {\n"
                        "  private _age: Int;\n"
                        "  age: Int { getter { return self._age; } setter { self._age = value; } }\n"
                        "}\n";
    CHECK(check_single_source_expressions(valid));
    CHECK(!check_single_source_expressions("class User { age: Int { getter; getter; } }\n"));
    CHECK(!check_single_source_expressions("class User { age: Int { getter { return \"bad\"; } } }\n"));
    CHECK(!check_single_source_expressions(
        "class User { age: Int { setter { copy: Int = value; copy = \"bad\"; } } }\n"));
    CHECK(!check_single_source_expressions("class User { age: Int { getter { return self.age; } } }\n"));
    CHECK(!check_single_source_expressions("class User { age: Int { setter { self.age = value; } } }\n"));
}

int main(void)
{
    test_literal_initializer_expression_types();
    test_control_flow_initializer_expression_types();
    test_optional_expression_types();
    test_owned_string_is_not_optional_or_implicitly_borrowed();
    test_binding_reassignment_errors();
    test_constant_initializer_errors();
    test_assignment_literal_expression_types();
    test_control_flow_assignment_expression_types();
    test_return_literal_expression_types();
    test_control_flow_return_expression_types();
    test_result_propagation_requires_result_return();
    test_macro_literal_initializer_expression_errors();
    test_macro_binding_reassignment_errors();
    test_macro_static_runtime_initializer_errors();
    test_macro_assignment_literal_expression_errors();
    test_macro_return_literal_expression_errors();
    test_op_referential_transparency_rules();
    test_property_accessor_expression_rules();
    return failures == 0 ? 0 : 1;
}
