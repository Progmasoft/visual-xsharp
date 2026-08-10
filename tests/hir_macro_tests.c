/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 */

#include "Visual/C23/diagnostic.hh"
#include "Visual/XSharp/hir/symbol_table.h"
#include "Visual/XSharp/hir/type_resolution.h"
#include "Visual/XSharp/macro.h"
#include "Visual/XSharp/syntax_parser.h"

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

static void test_declaration_macro_symbols(void)
{
    const char *text = ""
                       "macro_rules! make { () -> { incomplete fn Generated(); }; }\n"
                       "make!();\n";
    XsSource source = {.path = "MacroSymbols.vxs", .module_name = "App", .text = text, .length = strlen(text)};
    XsDiagnostics diagnostics;
    XsSyntaxTree tree;
    XsMacroExpansionReport report;
    XsMacroDeclarationExpansionSet declarations;
    XsHirSymbolTable symbols;
    xs_diagnostics_init(&diagnostics);
    xs_hir_symbol_table_init(&symbols);
    CHECK(xs_syntax_parse(&source, 81, &diagnostics, &tree));
    CHECK(xs_macro_validate(&tree, &diagnostics));
    CHECK(xs_macro_prepare_expansion(&tree, &diagnostics, &report));
    CHECK(report.calls_seen == 1);
    CHECK(report.calls_expandable == 1);
    CHECK(xs_macro_expand_declarations(&tree, &diagnostics, &declarations));
    CHECK(declarations.count == 1);
    CHECK(xs_hir_collect_symbols_expanded(&tree, &declarations, &symbols, &diagnostics));
    const XsHirSymbol *generated = xs_hir_symbol_table_find(&symbols, "App.Generated");
    CHECK(generated != nullptr);
    CHECK(generated == nullptr || generated->kind == XS_HIR_SYMBOL_FUNCTION);
    CHECK(generated == nullptr || generated->span.file_id == 81);
    xs_hir_symbol_table_free(&symbols);
    xs_macro_declaration_expansion_set_free(&declarations);
    xs_syntax_tree_free(&tree);
    xs_diagnostics_free(&diagnostics);
}

static void test_declaration_macro_duplicate_symbols(void)
{
    const char *text = ""
                       "fn Generated() {}\n"
                       "macro_rules! make { () -> { incomplete fn Generated(); }; }\n"
                       "make!();\n";
    XsSource source = {.path = "MacroDuplicateSymbols.vxs", .module_name = "App", .text = text, .length = strlen(text)};
    XsDiagnostics diagnostics;
    XsSyntaxTree tree;
    XsMacroDeclarationExpansionSet declarations;
    XsHirSymbolTable symbols;
    xs_diagnostics_init(&diagnostics);
    xs_hir_symbol_table_init(&symbols);
    CHECK(xs_syntax_parse(&source, 82, &diagnostics, &tree));
    CHECK(xs_macro_validate(&tree, &diagnostics));
    CHECK(xs_macro_expand_declarations(&tree, &diagnostics, &declarations));
    CHECK(!xs_hir_collect_symbols_expanded(&tree, &declarations, &symbols, &diagnostics));
    CHECK(xs_diagnostics_has_error(&diagnostics));
    xs_hir_symbol_table_free(&symbols);
    xs_macro_declaration_expansion_set_free(&declarations);
    xs_syntax_tree_free(&tree);
    xs_diagnostics_free(&diagnostics);
}

static void test_generated_function_name_use(void)
{
    const char *text = ""
                       "fn Main() { Generated(); }\n"
                       "macro_rules! make { () -> { incomplete fn Generated(); }; }\n"
                       "make!();\n";
    XsSource source = {.path = "MacroGeneratedNameUse.vxs", .module_name = "App", .text = text, .length = strlen(text)};
    XsDiagnostics diagnostics;
    XsSyntaxTree tree;
    XsMacroStatementExpansionSet statements;
    XsMacroDeclarationExpansionSet declarations;
    XsHirSymbolTable symbols;
    XsHirImportScope import;
    xs_diagnostics_init(&diagnostics);
    xs_hir_symbol_table_init(&symbols);
    xs_hir_import_scope_init(&import);
    CHECK(xs_syntax_parse(&source, 83, &diagnostics, &tree));
    CHECK(xs_macro_validate(&tree, &diagnostics));
    CHECK(xs_macro_expand_statements(&tree, &diagnostics, &statements));
    CHECK(xs_macro_expand_declarations(&tree, &diagnostics, &declarations));
    CHECK(xs_hir_collect_symbols_expanded(&tree, &declarations, &symbols, &diagnostics));
    CHECK(xs_hir_resolve_imports(&tree, &symbols, &import, &diagnostics));
    CHECK(xs_hir_validate_name_uses_expanded(&tree, &statements, &symbols, &import, &diagnostics));
    xs_hir_import_scope_free(&import);
    xs_hir_symbol_table_free(&symbols);
    xs_macro_declaration_expansion_set_free(&declarations);
    xs_macro_statement_expansion_set_free(&statements);
    xs_syntax_tree_free(&tree);
    xs_diagnostics_free(&diagnostics);
}

static void test_generated_declaration_type_errors(void)
{
    const char *text = ""
                       "macro_rules! make { () -> { incomplete fn Broken(value: Missing); }; }\n"
                       "make!();\n";
    XsSource source = {
        .path = "MacroGeneratedTypeError.vxs", .module_name = "App", .text = text, .length = strlen(text)};
    XsDiagnostics diagnostics;
    XsSyntaxTree tree;
    XsMacroStatementExpansionSet statements;
    XsMacroDeclarationExpansionSet declarations;
    XsHirSymbolTable symbols;
    XsHirImportScope import;
    xs_diagnostics_init(&diagnostics);
    xs_hir_symbol_table_init(&symbols);
    xs_hir_import_scope_init(&import);
    CHECK(xs_syntax_parse(&source, 84, &diagnostics, &tree));
    CHECK(xs_macro_validate(&tree, &diagnostics));
    CHECK(xs_macro_expand_statements(&tree, &diagnostics, &statements));
    CHECK(xs_macro_expand_declarations(&tree, &diagnostics, &declarations));
    CHECK(xs_hir_collect_symbols_expanded(&tree, &declarations, &symbols, &diagnostics));
    CHECK(xs_hir_resolve_imports(&tree, &symbols, &import, &diagnostics));
    CHECK(!xs_hir_resolve_types_expanded(&tree, &statements, &symbols, &import, &diagnostics));
    CHECK(xs_diagnostics_has_error(&diagnostics));
    xs_hir_import_scope_free(&import);
    xs_hir_symbol_table_free(&symbols);
    xs_macro_declaration_expansion_set_free(&declarations);
    xs_macro_statement_expansion_set_free(&statements);
    xs_syntax_tree_free(&tree);
    xs_diagnostics_free(&diagnostics);
}

static void test_item_fragment_declaration_symbol(void)
{
    const char *text = ""
                       "macro_rules! forward { ($item:item) -> { $item }; }\n"
                       "forward!(incomplete fn Generated(););\n";
    XsSource source = {.path = "MacroItemFragment.vxs", .module_name = "App", .text = text, .length = strlen(text)};
    XsDiagnostics diagnostics;
    XsSyntaxTree tree;
    XsMacroExpansionReport report;
    XsMacroDeclarationExpansionSet declarations;
    XsHirSymbolTable symbols;
    xs_diagnostics_init(&diagnostics);
    xs_hir_symbol_table_init(&symbols);
    CHECK(xs_syntax_parse(&source, 85, &diagnostics, &tree));
    CHECK(xs_macro_validate(&tree, &diagnostics));
    CHECK(xs_macro_prepare_expansion(&tree, &diagnostics, &report));
    CHECK(report.calls_seen == 1);
    CHECK(report.calls_expandable == 1);
    CHECK(report.substitutions_planned == 1);
    CHECK(xs_macro_expand_declarations(&tree, &diagnostics, &declarations));
    CHECK(declarations.count == 1);
    CHECK(declarations.count < 1 || declarations.items[0].declaration_count == 1);
    CHECK(xs_hir_collect_symbols_expanded(&tree, &declarations, &symbols, &diagnostics));
    const XsHirSymbol *generated = xs_hir_symbol_table_find(&symbols, "App.Generated");
    CHECK(generated != nullptr);
    CHECK(generated == nullptr || generated->kind == XS_HIR_SYMBOL_FUNCTION);
    xs_hir_symbol_table_free(&symbols);
    xs_macro_declaration_expansion_set_free(&declarations);
    xs_syntax_tree_free(&tree);
    xs_diagnostics_free(&diagnostics);
}

static void test_item_fragment_empty_call_errors(void)
{
    const char *text = ""
                       "macro_rules! forward { ($item:item) -> { $item }; }\n"
                       "forward!();\n";
    XsSource source = {.path = "MacroItemFragmentEmpty.vxs", .module_name = "App", .text = text, .length = strlen(text)};
    XsDiagnostics diagnostics;
    XsSyntaxTree tree;
    xs_diagnostics_init(&diagnostics);
    CHECK(xs_syntax_parse(&source, 86, &diagnostics, &tree));
    CHECK(!xs_macro_validate(&tree, &diagnostics));
    CHECK(xs_diagnostics_has_error(&diagnostics));
    xs_syntax_tree_free(&tree);
    xs_diagnostics_free(&diagnostics);
}

static void test_multiple_matching_declaration_rules(void)
{
    const char *text = ""
                       "macro_rules! make {\n"
                       "  () -> { incomplete fn First(); };\n"
                       "  () -> { incomplete fn Second(); };\n"
                       "}\n"
                       "make!();\n";
    XsSource source = {.path = "MacroMultipleRules.vxs", .module_name = "App", .text = text, .length = strlen(text)};
    XsDiagnostics diagnostics;
    XsSyntaxTree tree;
    xs_diagnostics_init(&diagnostics);
    CHECK(xs_syntax_parse(&source, 87, &diagnostics, &tree));
    CHECK(!xs_macro_validate(&tree, &diagnostics));
    CHECK(xs_diagnostics_has_error(&diagnostics));
    xs_syntax_tree_free(&tree);
    xs_diagnostics_free(&diagnostics);
}

static void test_multiple_matching_statement_rules_name_errors(void)
{
    const char *text = ""
                       "incomplete fn Known();\n"
                       "macro_rules! both {\n"
                       "  () -> { Known(); };\n"
                       "  () -> { Missing(); };\n"
                       "}\n"
                       "fn Main() { both!(); }\n";
    XsSource source = {
        .path = "MacroMultipleStatementNames.vxs", .module_name = "App", .text = text, .length = strlen(text)};
    XsDiagnostics diagnostics;
    XsSyntaxTree tree;
    xs_diagnostics_init(&diagnostics);
    CHECK(xs_syntax_parse(&source, 88, &diagnostics, &tree));
    CHECK(!xs_macro_validate(&tree, &diagnostics));
    CHECK(xs_diagnostics_has_error(&diagnostics));
    xs_syntax_tree_free(&tree);
    xs_diagnostics_free(&diagnostics);
}

static void test_multiple_matching_statement_rules_type_errors(void)
{
    const char *text = ""
                       "macro_rules! both {\n"
                       "  () -> { value: Int = None; };\n"
                       "  () -> { broken: Missing = None; };\n"
                       "}\n"
                       "fn Main() { both!(); }\n";
    XsSource source = {
        .path = "MacroMultipleStatementTypes.vxs", .module_name = "App", .text = text, .length = strlen(text)};
    XsDiagnostics diagnostics;
    XsSyntaxTree tree;
    xs_diagnostics_init(&diagnostics);
    CHECK(xs_syntax_parse(&source, 89, &diagnostics, &tree));
    CHECK(!xs_macro_validate(&tree, &diagnostics));
    CHECK(xs_diagnostics_has_error(&diagnostics));
    xs_syntax_tree_free(&tree);
    xs_diagnostics_free(&diagnostics);
}

static void test_generated_class_member_type_errors(void)
{
    const char *text = ""
                       "class User {\n"
                       "  macro_rules! make { () -> { incomplete fn Broken(value: Missing); }; }\n"
                       "  make!();\n"
                       "}\n";
    XsSource source = {
        .path = "MacroGeneratedMemberTypeError.vxs", .module_name = "App", .text = text, .length = strlen(text)};
    XsDiagnostics diagnostics;
    XsSyntaxTree tree;
    XsMacroStatementExpansionSet statements;
    XsMacroDeclarationExpansionSet declarations;
    XsHirSymbolTable symbols;
    XsHirImportScope import;
    xs_diagnostics_init(&diagnostics);
    xs_hir_symbol_table_init(&symbols);
    xs_hir_import_scope_init(&import);
    CHECK(xs_syntax_parse(&source, 90, &diagnostics, &tree));
    CHECK(xs_macro_validate(&tree, &diagnostics));
    CHECK(xs_macro_expand_statements(&tree, &diagnostics, &statements));
    CHECK(xs_macro_expand_declarations(&tree, &diagnostics, &declarations));
    CHECK(declarations.count == 1);
    CHECK(xs_hir_collect_symbols_expanded(&tree, &declarations, &symbols, &diagnostics));
    CHECK(xs_hir_resolve_imports(&tree, &symbols, &import, &diagnostics));
    CHECK(!xs_hir_resolve_types_with_macros(&tree, &declarations, &statements, &symbols, &import, &diagnostics));
    CHECK(xs_diagnostics_has_error(&diagnostics));
    xs_hir_import_scope_free(&import);
    xs_hir_symbol_table_free(&symbols);
    xs_macro_declaration_expansion_set_free(&declarations);
    xs_macro_statement_expansion_set_free(&statements);
    xs_syntax_tree_free(&tree);
    xs_diagnostics_free(&diagnostics);
}

static void test_generated_class_field_like_type_errors(void)
{
    const char *text = ""
                       "class User {\n"
                       "  macro_rules! make { () -> { value: Missing; }; }\n"
                       "  make!();\n"
                       "}\n";
    XsSource source = {
        .path = "MacroGeneratedFieldLikeTypeError.vxs", .module_name = "App", .text = text, .length = strlen(text)};
    XsDiagnostics diagnostics;
    XsSyntaxTree tree;
    XsMacroStatementExpansionSet statements;
    XsMacroDeclarationExpansionSet declarations;
    XsHirSymbolTable symbols;
    XsHirImportScope import;
    xs_diagnostics_init(&diagnostics);
    xs_hir_symbol_table_init(&symbols);
    xs_hir_import_scope_init(&import);
    CHECK(xs_syntax_parse(&source, 91, &diagnostics, &tree));
    CHECK(xs_macro_validate(&tree, &diagnostics));
    CHECK(xs_macro_expand_statements(&tree, &diagnostics, &statements));
    CHECK(xs_macro_expand_declarations(&tree, &diagnostics, &declarations));
    CHECK(declarations.count == 1);
    CHECK(xs_hir_collect_symbols_expanded(&tree, &declarations, &symbols, &diagnostics));
    CHECK(xs_hir_resolve_imports(&tree, &symbols, &import, &diagnostics));
    CHECK(!xs_hir_resolve_types_with_macros(&tree, &declarations, &statements, &symbols, &import, &diagnostics));
    CHECK(xs_diagnostics_has_error(&diagnostics));
    xs_hir_import_scope_free(&import);
    xs_hir_symbol_table_free(&symbols);
    xs_macro_declaration_expansion_set_free(&declarations);
    xs_macro_statement_expansion_set_free(&statements);
    xs_syntax_tree_free(&tree);
    xs_diagnostics_free(&diagnostics);
}

int main(void)
{
    test_declaration_macro_symbols();
    test_declaration_macro_duplicate_symbols();
    test_generated_function_name_use();
    test_generated_declaration_type_errors();
    test_item_fragment_declaration_symbol();
    test_item_fragment_empty_call_errors();
    test_multiple_matching_declaration_rules();
    test_multiple_matching_statement_rules_name_errors();
    test_multiple_matching_statement_rules_type_errors();
    test_generated_class_member_type_errors();
    test_generated_class_field_like_type_errors();
    return failures == 0 ? 0 : 1;
}
