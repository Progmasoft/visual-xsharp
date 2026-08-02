/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 */

#include "xs/ast.h"
#include "xs/diagnostic.h"
#include "xs/parser.h"

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

static bool parse(const char *text, XsAst *ast, XsDiagnostics *diagnostics)
{
    XsSource source = {.path = "<test>", .text = text, .length = strlen(text)};
    XsParser parser;
    xs_ast_init(ast);
    xs_diagnostics_init(diagnostics);
    xs_parser_init(&parser, &source, diagnostics);
    return xs_parser_parse(&parser, ast);
}

static void test_top_level_declarations(void)
{
    const char *text = "namespace Advanced; import stdio, Math; "
                       "using namespace stdio; using Sum = Math::Add; "
                       "public class Box<T> { value: T; } "
                       "interface Printable { fn Print(); } "
                       "data Pair<T, U> { first: T second: U } "
                       "enum Color { Red, Green, Blue } "
                       "enum data Result<T, E> { Ok: T, Error: E } "
                       "async fn Work() -> Task<()> { return; }";
    XsAst ast;
    XsDiagnostics diagnostics;
    CHECK(parse(text, &ast, &diagnostics));
    CHECK(ast.count == 10);
    CHECK(ast.items[0].kind == XS_AST_NAMESPACE);
    CHECK(ast.items[1].kind == XS_AST_IMPORT);
    CHECK(ast.items[2].kind == XS_AST_IMPORT);
    CHECK(ast.items[3].kind == XS_AST_IMPORT);
    CHECK(ast.items[4].kind == XS_AST_CLASS);
    CHECK(ast.items[4].item.visibility == XS_VISIBILITY_PUBLIC);
    CHECK(ast.items[8].kind == XS_AST_ENUM && ast.items[8].item.is_data_enum);
    CHECK(ast.items[9].kind == XS_AST_FUNCTION && ast.items[9].item.is_async);
    xs_ast_free(&ast);
    xs_diagnostics_free(&diagnostics);
}

static void test_nested_braces(void)
{
    XsAst ast;
    XsDiagnostics diagnostics;
    CHECK(parse("fn Main() { if (true) { while (false) { break; } } }", &ast, &diagnostics));
    CHECK(ast.count == 1 && ast.items[0].kind == XS_AST_FUNCTION);
    xs_ast_free(&ast);
    xs_diagnostics_free(&diagnostics);
}

static void test_increment_and_decrement_updates(void)
{
    XsAst ast;
    XsDiagnostics diagnostics;
    CHECK(parse("fn Main() { value: Long = 0; value++; value--; ++value; --value; "
                "for (i: Long = 0; i < 3; i++) {} }",
                &ast, &diagnostics));
    CHECK(!xs_diagnostics_has_error(&diagnostics));
    xs_ast_free(&ast);
    xs_diagnostics_free(&diagnostics);
}

static void test_top_level_execution_rejected(void)
{
    XsAst ast;
    XsDiagnostics diagnostics;
    CHECK(!parse("fn Main() {} Main();", &ast, &diagnostics));
    CHECK(xs_diagnostics_has_error(&diagnostics));
    xs_ast_free(&ast);
    xs_diagnostics_free(&diagnostics);
}

static void test_class_parentheses_rejected(void)
{
    XsAst ast;
    XsDiagnostics diagnostics;
    CHECK(!parse("class User() {}", &ast, &diagnostics));
    CHECK(xs_diagnostics_has_error(&diagnostics));
    xs_ast_free(&ast);
    xs_diagnostics_free(&diagnostics);
}

static void test_macro_definition(void)
{
    XsAst ast;
    XsDiagnostics diagnostics;
    CHECK(parse("macro_rules! say { ($x:expr) -> { $x; }; } fn Main() {}", &ast, &diagnostics));
    CHECK(ast.count == 2);
    CHECK(ast.items[0].kind == XS_AST_MACRO);
    xs_ast_free(&ast);
    xs_diagnostics_free(&diagnostics);
}

static void test_macro_rule_semicolon_required(void)
{
    XsAst ast;
    XsDiagnostics diagnostics;
    CHECK(!parse("macro_rules! invalid { () -> {} }", &ast, &diagnostics));
    CHECK(xs_diagnostics_has_error(&diagnostics));
    xs_ast_free(&ast);
    xs_diagnostics_free(&diagnostics);
}

static void test_module_keyword_is_removed(void)
{
    XsAst ast;
    XsDiagnostics diagnostics;
    CHECK(!parse("fn Main() {} module Late;", &ast, &diagnostics));
    CHECK(xs_diagnostics_has_error(&diagnostics));
    xs_ast_free(&ast);
    xs_diagnostics_free(&diagnostics);
}

static void test_namespace_is_source_scoped(void)
{
    XsAst ast;
    XsDiagnostics diagnostics;
    CHECK(parse("namespace Advanced; fn Main() {}", &ast, &diagnostics));
    CHECK(!xs_diagnostics_has_error(&diagnostics));
    xs_ast_free(&ast);
    xs_diagnostics_free(&diagnostics);
}

int main(void)
{
    test_top_level_declarations();
    test_nested_braces();
    test_increment_and_decrement_updates();
    test_top_level_execution_rejected();
    test_class_parentheses_rejected();
    test_macro_definition();
    test_macro_rule_semicolon_required();
    test_module_keyword_is_removed();
    test_namespace_is_source_scoped();
    return failures == 0 ? 0 : 1;
}
