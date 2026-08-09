/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 */

#include "xs/diagnostic.h"
#include "xs/hir/cffi.h"
#include "xs/hir/jit.h"
#include "xs/hir/symbol_table.h"
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

static bool parse_and_collect(const char *text, XsSyntaxTree *tree, XsHirSymbolTable *symbols,
                              XsDiagnostics *diagnostics)
{
    XsSource source = {.path = "Symbols.vxs", .module_name = "App", .text = text, .length = strlen(text)};
    xs_diagnostics_init(diagnostics);
    xs_hir_symbol_table_init(symbols);
    if(!xs_syntax_parse(&source, 21, diagnostics, tree))
        return false;
    return xs_hir_collect_symbols(tree, symbols, diagnostics);
}

static void free_all(XsSyntaxTree *tree, XsHirSymbolTable *symbols, XsDiagnostics *diagnostics)
{
    xs_hir_symbol_table_free(symbols);
    xs_syntax_tree_free(tree);
    xs_diagnostics_free(diagnostics);
}

static void test_module_namespace_symbols(void)
{
    const char *text = ""
                       "namespace Core {\n"
                       "  public fn Main() {}\n"
                       "  internal class Service {}\n"
                       "}\n"
                       "namespace Util {\n"
                       "  data Result { value: Int; }\n"
                       "}\n";
    XsSyntaxTree tree;
    XsHirSymbolTable symbols;
    XsDiagnostics diagnostics;
    CHECK(parse_and_collect(text, &tree, &symbols, &diagnostics));
    CHECK(symbols.count == 3);
    const XsHirSymbol *main = xs_hir_symbol_table_find(&symbols, "App.Core.Main");
    CHECK(main != nullptr);
    CHECK(main != nullptr && main->kind == XS_HIR_SYMBOL_FUNCTION);
    CHECK(main != nullptr && main->visibility == XS_SYNTAX_VISIBILITY_PUBLIC);
    const XsHirSymbol *service = xs_hir_symbol_table_find(&symbols, "App.Core.Service");
    const XsHirSymbol *result = xs_hir_symbol_table_find(&symbols, "App.Util.Result");
    CHECK(service != nullptr && service->visibility == XS_SYNTAX_VISIBILITY_INTERNAL);
    CHECK(result != nullptr && result->visibility == XS_SYNTAX_VISIBILITY_INTERNAL);
    free_all(&tree, &symbols, &diagnostics);
}

static void test_duplicate_names_in_namespace(void)
{
    const char *text = ""
                       "namespace Core;\n"
                       "fn Twice() {}\n"
                       "class Twice {}\n";
    XsSyntaxTree tree;
    XsHirSymbolTable symbols;
    XsDiagnostics diagnostics;
    CHECK(!parse_and_collect(text, &tree, &symbols, &diagnostics));
    CHECK(xs_diagnostics_has_error(&diagnostics));
    free_all(&tree, &symbols, &diagnostics);
}

static void test_extern_block_symbols(void)
{
    const char *text = ""
                       "#[repr(C)]\n"
                       "extern \"C\" {\n"
                       "  fn puts(text: std::cffi::CStr) -> Int;\n"
                       "  static errno: Int;\n"
                       "}\n";
    XsSyntaxTree tree;
    XsHirSymbolTable symbols;
    XsDiagnostics diagnostics;
    CHECK(parse_and_collect(text, &tree, &symbols, &diagnostics));
    const XsHirSymbol *puts = xs_hir_symbol_table_find(&symbols, "App.puts");
    const XsHirSymbol *errno_symbol = xs_hir_symbol_table_find(&symbols, "App.errno");
    CHECK(puts != nullptr && puts->kind == XS_HIR_SYMBOL_FUNCTION);
    CHECK(puts != nullptr && (puts->syntax->flags & XS_SYNTAX_FLAG_EXTERN) != 0);
    CHECK(errno_symbol != nullptr && errno_symbol->kind == XS_HIR_SYMBOL_EXTERN_GLOBAL);
    CHECK(errno_symbol != nullptr && (errno_symbol->syntax->flags & XS_SYNTAX_FLAG_EXTERN) != 0);
    free_all(&tree, &symbols, &diagnostics);
}

static void test_extern_block_duplicate_symbol_errors(void)
{
    const char *text = ""
                       "fn puts() {}\n"
                       "extern \"C\" { fn puts() -> Int; }\n";
    XsSyntaxTree tree;
    XsHirSymbolTable symbols;
    XsDiagnostics diagnostics;
    CHECK(!parse_and_collect(text, &tree, &symbols, &diagnostics));
    CHECK(xs_diagnostics_has_error(&diagnostics));
    free_all(&tree, &symbols, &diagnostics);
}

static bool parse_and_validate_cffi(const char *text, XsSyntaxTree *tree, XsDiagnostics *diagnostics)
{
    XsSource source = {.path = "std.cffi.vxs", .module_name = "App", .text = text, .length = strlen(text)};
    xs_diagnostics_init(diagnostics);
    if(!xs_syntax_parse(&source, 27, diagnostics, tree))
        return false;
    CHECK(xs_syntax_find_first(tree->root, XS_SYNTAX_DECL_EXTERN_BLOCK) != nullptr);
    return xs_hir_validate_cffi(tree, diagnostics);
}

static bool node_text_is(XsText text, const char *expected)
{
    size_t length = strlen(expected);
    return text.length == length && memcmp(text.data, expected, length) == 0;
}

static void test_cffi_validation_accepts_repr_c_extern_block(void)
{
    const char *text = ""
                       "#[LinkLibrary(\"c\")]\n"
                       "#[Header(\"stdio.h\")]\n"
                       "#[repr(C)]\n"
                       "extern \"C\" {\n"
                       "  #[LinkName(\"puts\")]\n"
                       "  #[NoUnwind]\n"
                       "  fn puts(text: std::cffi::CStr) -> Int;\n"
                       "  #[ThreadLocal]\n"
                       "  #[LinkName(\"errno\")]\n"
                       "  static errno: Int;\n"
                       "}\n";
    XsSyntaxTree tree;
    XsDiagnostics diagnostics;
    CHECK(parse_and_validate_cffi(text, &tree, &diagnostics));
    const XsSyntaxNode *block = xs_syntax_find_first(tree.root, XS_SYNTAX_DECL_EXTERN_BLOCK);
    const XsSyntaxNode *function = xs_syntax_find_first(tree.root, XS_SYNTAX_DECL_FUNCTION);
    const XsSyntaxNode *global = xs_syntax_find_first(tree.root, XS_SYNTAX_DECL_VARIABLE);
    XsHirCffiExternBlock block_metadata;
    XsHirCffiFunction function_metadata;
    XsHirCffiStatic static_metadata;
    CHECK(xs_hir_cffi_read_extern_block(block, &block_metadata));
    CHECK(xs_hir_cffi_read_function(function, &function_metadata));
    CHECK(xs_hir_cffi_read_static(global, &static_metadata));
    CHECK(node_text_is(block_metadata.abi, "\"C\""));
    CHECK(node_text_is(block_metadata.link_library, "\"c\""));
    CHECK(node_text_is(block_metadata.header, "\"stdio.h\""));
    CHECK(node_text_is(function_metadata.link_name, "\"puts\""));
    CHECK(function_metadata.no_unwind);
    CHECK(node_text_is(static_metadata.link_name, "\"errno\""));
    CHECK(static_metadata.thread_local_storage);
    xs_syntax_tree_free(&tree);
    xs_diagnostics_free(&diagnostics);
}

static void test_cffi_validation_rejects_missing_repr_c(void)
{
    const char *text = ""
                       "extern \"C\" { fn puts(text: std::cffi::CStr) -> Int; }\n";
    XsSyntaxTree tree;
    XsDiagnostics diagnostics;
    CHECK(!parse_and_validate_cffi(text, &tree, &diagnostics));
    CHECK(xs_diagnostics_has_error(&diagnostics));
    xs_syntax_tree_free(&tree);
    xs_diagnostics_free(&diagnostics);
}

static void test_cffi_validation_rejects_unsupported_abi(void)
{
    const char *text = ""
                       "#[repr(C)]\n"
                       "extern \"stdcall\" { fn puts(text: std::cffi::CStr) -> Int; }\n";
    XsSyntaxTree tree;
    XsDiagnostics diagnostics;
    CHECK(!parse_and_validate_cffi(text, &tree, &diagnostics));
    CHECK(xs_diagnostics_has_error(&diagnostics));
    xs_syntax_tree_free(&tree);
    xs_diagnostics_free(&diagnostics);
}

static void test_cffi_validation_rejects_non_c_repr(void)
{
    const char *text = ""
                       "#[repr(Rust)]\n"
                       "extern \"C\" { fn puts(text: std::cffi::CStr) -> Int; }\n";
    XsSyntaxTree tree;
    XsDiagnostics diagnostics;
    CHECK(!parse_and_validate_cffi(text, &tree, &diagnostics));
    CHECK(xs_diagnostics_has_error(&diagnostics));
    xs_syntax_tree_free(&tree);
    xs_diagnostics_free(&diagnostics);
}

static void test_cffi_validation_rejects_invalid_link_name(void)
{
    const char *text = ""
                       "#[repr(C)]\n"
                       "extern \"C\" { #[LinkName(puts)] fn puts(text: std::cffi::CStr) -> Int; }\n";
    XsSyntaxTree tree;
    XsDiagnostics diagnostics;
    CHECK(!parse_and_validate_cffi(text, &tree, &diagnostics));
    CHECK(xs_diagnostics_has_error(&diagnostics));
    xs_syntax_tree_free(&tree);
    xs_diagnostics_free(&diagnostics);
}

static void test_cffi_validation_rejects_thread_local_function(void)
{
    const char *text = ""
                       "#[repr(C)]\n"
                       "extern \"C\" { #[ThreadLocal] fn puts(text: std::cffi::CStr) -> Int; }\n";
    XsSyntaxTree tree;
    XsDiagnostics diagnostics;
    CHECK(!parse_and_validate_cffi(text, &tree, &diagnostics));
    CHECK(xs_diagnostics_has_error(&diagnostics));
    xs_syntax_tree_free(&tree);
    xs_diagnostics_free(&diagnostics);
}

static void test_cffi_validation_rejects_function_attribute_on_static(void)
{
    const char *text = ""
                       "#[repr(C)]\n"
                       "extern \"C\" { #[NoUnwind] static errno: Int; }\n";
    XsSyntaxTree tree;
    XsDiagnostics diagnostics;
    CHECK(!parse_and_validate_cffi(text, &tree, &diagnostics));
    CHECK(xs_diagnostics_has_error(&diagnostics));
    xs_syntax_tree_free(&tree);
    xs_diagnostics_free(&diagnostics);
}

static void test_cffi_validation_rejects_block_attribute_on_function(void)
{
    const char *text = ""
                       "#[repr(C)]\n"
                       "extern \"C\" { #[LinkLibrary(\"c\")] fn puts(text: std::cffi::CStr) -> Int; }\n";
    XsSyntaxTree tree;
    XsDiagnostics diagnostics;
    CHECK(!parse_and_validate_cffi(text, &tree, &diagnostics));
    CHECK(xs_diagnostics_has_error(&diagnostics));
    xs_syntax_tree_free(&tree);
    xs_diagnostics_free(&diagnostics);
}

static void test_same_name_in_different_namespace(void)
{
    const char *text = ""
                       "namespace First { fn Value() {} }\n"
                       "namespace Second { fn Value() {} }\n";
    XsSyntaxTree tree;
    XsHirSymbolTable symbols;
    XsDiagnostics diagnostics;
    CHECK(parse_and_collect(text, &tree, &symbols, &diagnostics));
    CHECK(symbols.count == 2);
    CHECK(xs_hir_symbol_table_find(&symbols, "App.First.Value") != nullptr);
    CHECK(xs_hir_symbol_table_find(&symbols, "App.Second.Value") != nullptr);
    free_all(&tree, &symbols, &diagnostics);
}

static bool add_file_symbols_in_module(const char *text, const char *module_name, uint64_t file_id, XsSyntaxTree *tree,
                                       XsHirSymbolTable *symbols, XsDiagnostics *diagnostics)
{
    XsSource source = {.path = "File.vxs", .module_name = module_name, .text = text, .length = strlen(text)};
    if(!xs_syntax_parse(&source, file_id, diagnostics, tree))
        return false;
    return xs_hir_collect_symbols(tree, symbols, diagnostics);
}

static bool add_file_symbols(const char *text, uint64_t file_id, XsSyntaxTree *tree, XsHirSymbolTable *symbols,
                             XsDiagnostics *diagnostics)
{
    return add_file_symbols_in_module(text, "App", file_id, tree, symbols, diagnostics);
}

static bool check_single_source_names(const char *text)
{
    XsSyntaxTree tree;
    XsHirSymbolTable symbols;
    XsHirImportScope import;
    XsDiagnostics diagnostics;
    xs_diagnostics_init(&diagnostics);
    xs_hir_symbol_table_init(&symbols);
    xs_hir_import_scope_init(&import);
    bool success = add_file_symbols(text, 30, &tree, &symbols, &diagnostics);
    if(success)
        success = xs_hir_resolve_imports(&tree, &symbols, &import, &diagnostics);
    if(success)
        success = xs_hir_validate_name_uses(&tree, &symbols, &import, &diagnostics);
    xs_hir_import_scope_free(&import);
    xs_hir_symbol_table_free(&symbols);
    xs_syntax_tree_free(&tree);
    xs_diagnostics_free(&diagnostics);
    return success;
}

static void test_import_resolution(void)
{
    const char *library = ""
                          "public fn Add() {}\n"
                          "public extern \"C\" { public static errno: Int; }\n"
                          "private fn Hidden() {}\n";
    const char *main = ""
                       "import Math;\n"
                       "using Sum = Math::Add;\n"
                       "using Math::errno;\n"
                       "fn Main() {}\n";
    XsSyntaxTree library_tree;
    XsSyntaxTree main_tree;
    XsHirSymbolTable symbols;
    XsHirImportScope import;
    XsDiagnostics diagnostics;
    xs_diagnostics_init(&diagnostics);
    xs_hir_symbol_table_init(&symbols);
    xs_hir_import_scope_init(&import);
    CHECK(add_file_symbols_in_module(library, "Math", 31, &library_tree, &symbols, &diagnostics));
    CHECK(add_file_symbols(main, 32, &main_tree, &symbols, &diagnostics));
    CHECK(xs_hir_resolve_imports(&main_tree, &symbols, &import, &diagnostics));
    CHECK(xs_hir_import_scope_has_module(&import, "panic"));
    CHECK(xs_hir_import_scope_has_module(&import, "optional"));
    CHECK(xs_hir_import_scope_has_module(&import, "std.optional"));
    CHECK(xs_hir_import_scope_has_module(&import, "std.optional.Optional"));
    CHECK(xs_hir_import_scope_has_module(&import, "result"));
    CHECK(xs_hir_import_scope_has_module(&import, "std.result"));
    CHECK(xs_hir_import_scope_has_module(&import, "std.result.Result"));
    CHECK(xs_hir_import_scope_has_module(&import, "Math"));
    CHECK(xs_hir_import_scope_find(&import, "Math.Add") == nullptr);
    CHECK(xs_hir_import_scope_find(&import, "Math.errno") == nullptr);
    CHECK(xs_hir_import_scope_find(&import, "Math.Hidden") == nullptr);
    const XsHirImportBinding *sum = xs_hir_import_scope_find(&import, "Sum");
    CHECK(sum != nullptr);
    CHECK(sum != nullptr && strcmp(sum->symbol->qualified_name, "Math.Add") == 0);
    const XsHirImportBinding *errno_import = xs_hir_import_scope_find(&import, "errno");
    CHECK(errno_import != nullptr);
    CHECK(errno_import != nullptr && errno_import->symbol->kind == XS_HIR_SYMBOL_EXTERN_GLOBAL);
    xs_hir_import_scope_free(&import);
    xs_hir_symbol_table_free(&symbols);
    xs_syntax_tree_free(&main_tree);
    xs_syntax_tree_free(&library_tree);
    xs_diagnostics_free(&diagnostics);
}

static void test_public_namespace_exports_explicit_public_symbols(void)
{
    const char *library = ""
                          "public namespace Advanced;\n"
                          "public fn Add() {}\n"
                          "private fn Hidden() {}\n";
    const char *main = ""
                       "import Math::Advanced;\n"
                       "using Math::Advanced::Add;\n"
                       "fn Main() { Math::Advanced::Add(); Add(); }\n";
    XsSyntaxTree library_tree;
    XsSyntaxTree main_tree;
    XsHirSymbolTable symbols;
    XsHirImportScope import;
    XsDiagnostics diagnostics;
    xs_diagnostics_init(&diagnostics);
    xs_hir_symbol_table_init(&symbols);
    xs_hir_import_scope_init(&import);
    CHECK(add_file_symbols_in_module(library, "Math", 35, &library_tree, &symbols, &diagnostics));
    const XsHirSymbol *add = xs_hir_symbol_table_find(&symbols, "Math.Advanced.Add");
    const XsHirSymbol *hidden = xs_hir_symbol_table_find(&symbols, "Math.Advanced.Hidden");
    CHECK(add != nullptr && add->visibility == XS_SYNTAX_VISIBILITY_PUBLIC);
    CHECK(hidden != nullptr && hidden->visibility == XS_SYNTAX_VISIBILITY_PRIVATE);
    CHECK(add_file_symbols(main, 36, &main_tree, &symbols, &diagnostics));
    CHECK(xs_hir_resolve_imports(&main_tree, &symbols, &import, &diagnostics));
    CHECK(xs_hir_import_scope_has_module(&import, "Math.Advanced"));
    CHECK(xs_hir_import_scope_find(&import, "Math.Advanced.Add") == nullptr);
    CHECK(xs_hir_import_scope_find(&import, "Math.Advanced.Hidden") == nullptr);
    CHECK(xs_hir_validate_name_uses(&main_tree, &symbols, &import, &diagnostics));
    xs_hir_import_scope_free(&import);
    xs_hir_symbol_table_free(&symbols);
    xs_syntax_tree_free(&main_tree);
    xs_syntax_tree_free(&library_tree);
    xs_diagnostics_free(&diagnostics);
}

static void test_import_errors(void)
{
    const char *library = ""
                          "private fn Hidden() {}\n";
    const char *main = ""
                       "using Math::Hidden;\n";
    XsSyntaxTree library_tree;
    XsSyntaxTree main_tree;
    XsHirSymbolTable symbols;
    XsHirImportScope import;
    XsDiagnostics diagnostics;
    xs_diagnostics_init(&diagnostics);
    xs_hir_symbol_table_init(&symbols);
    xs_hir_import_scope_init(&import);
    CHECK(add_file_symbols_in_module(library, "Math", 41, &library_tree, &symbols, &diagnostics));
    CHECK(add_file_symbols(main, 42, &main_tree, &symbols, &diagnostics));
    CHECK(!xs_hir_resolve_imports(&main_tree, &symbols, &import, &diagnostics));
    CHECK(xs_diagnostics_has_error(&diagnostics));
    xs_hir_import_scope_free(&import);
    xs_hir_symbol_table_free(&symbols);
    xs_syntax_tree_free(&main_tree);
    xs_syntax_tree_free(&library_tree);
    xs_diagnostics_free(&diagnostics);
}

static void test_name_use_resolution(void)
{
    const char *library = ""
                          "public fn Add(a: Int, b: Int) -> Int { return a + b; }\n";
    const char *main = ""
                       "import Math::Advanced;\n"
                       "using Sum = Math::Advanced::Add;\n"
                       "fn Main() {\n"
                       "  first: Int = Math::Advanced::Add(1, 2);\n"
                       "  second: Int = Sum(3, 4);\n"
                       "}\n";
    XsSyntaxTree library_tree;
    XsSyntaxTree main_tree;
    XsHirSymbolTable symbols;
    XsHirImportScope import;
    XsDiagnostics diagnostics;
    xs_diagnostics_init(&diagnostics);
    xs_hir_symbol_table_init(&symbols);
    xs_hir_import_scope_init(&import);
    CHECK(add_file_symbols_in_module(library, "Math::Advanced", 51, &library_tree, &symbols, &diagnostics));
    CHECK(add_file_symbols(main, 52, &main_tree, &symbols, &diagnostics));
    CHECK(xs_hir_resolve_imports(&main_tree, &symbols, &import, &diagnostics));
    CHECK(xs_hir_validate_name_uses(&main_tree, &symbols, &import, &diagnostics));
    xs_hir_import_scope_free(&import);
    xs_hir_symbol_table_free(&symbols);
    xs_syntax_tree_free(&main_tree);
    xs_syntax_tree_free(&library_tree);
    xs_diagnostics_free(&diagnostics);
}

static void test_standard_library_name_resolution(void)
{
    const char *valid = ""
                        "import fs, process, thread, net, stdio;\n"
                        "fn Main() -> Result<()> {\n"
                        "  content: Optional<Str> = Some(std::fs::read_to_str(\"input.txt\"));\n"
                        "  std::fs::create_dir(\"out\");\n"
                        "  std::process::run(\"tool\");\n"
                        "  std::thread::yield();\n"
                        "  std::net::listen(\"127.0.0.1:9000\");\n"
                        "  std::stdout();\n"
                        "  return Ok();\n"
                        "}\n";
    CHECK(check_single_source_names(valid));
    CHECK(!check_single_source_names("fn Main() { std::fs::read_to_str(\"input.txt\"); }\n"));
    CHECK(!check_single_source_names("import fs;\nfn Main() { std::fs::invented(); }\n"));
}

static void test_associated_items_and_pattern_bindings(void)
{
    const char *valid = ""
                        "class Parser { static fn parse(value: Str) -> Int { return 1; } }\n"
                        "enum data Command { Add: Str, Done: Int, }\n"
                        "fn Main() {\n"
                        "  value: Int = Parser::parse(\"7\");\n"
                        "  add: Command = Command::Add(\"task\");\n"
                        "  done: Command = Command::Done(value);\n"
                        "  for (line: Str in lines) { line.length(); }\n"
                        "  (sender, receiver): (Int, Int) = pair;\n"
                        "  receiver.consume();\n"
                        "}\n";
    CHECK(check_single_source_names(valid));
    CHECK(!check_single_source_names("class Parser { fn parse() {} }\nfn Main() { Parser::parse(); }\n"));
    CHECK(!check_single_source_names("enum data Command { Add: Str, }\nfn Main() { Command::Missing(); }\n"));
}

static void test_name_use_errors(void)
{
    const char *main = ""
                       "fn Main() { Missing.Call(); }\n";
    XsSyntaxTree tree;
    XsHirSymbolTable symbols;
    XsHirImportScope import;
    XsDiagnostics diagnostics;
    xs_diagnostics_init(&diagnostics);
    xs_hir_symbol_table_init(&symbols);
    xs_hir_import_scope_init(&import);
    CHECK(add_file_symbols(main, 61, &tree, &symbols, &diagnostics));
    CHECK(xs_hir_resolve_imports(&tree, &symbols, &import, &diagnostics));
    CHECK(!xs_hir_validate_name_uses(&tree, &symbols, &import, &diagnostics));
    CHECK(xs_diagnostics_has_error(&diagnostics));
    xs_hir_import_scope_free(&import);
    xs_hir_symbol_table_free(&symbols);
    xs_syntax_tree_free(&tree);
    xs_diagnostics_free(&diagnostics);
}

static void test_qualified_external_name_requires_import(void)
{
    const char *library = ""
                          "public fn Add() {}\n";
    const char *main = ""
                       "fn Main() { Math::Add(); }\n";
    XsSyntaxTree library_tree;
    XsSyntaxTree main_tree;
    XsHirSymbolTable symbols;
    XsHirImportScope import;
    XsDiagnostics diagnostics;
    xs_diagnostics_init(&diagnostics);
    xs_hir_symbol_table_init(&symbols);
    xs_hir_import_scope_init(&import);
    CHECK(add_file_symbols_in_module(library, "Math", 41, &library_tree, &symbols, &diagnostics));
    CHECK(add_file_symbols(main, 42, &main_tree, &symbols, &diagnostics));
    CHECK(xs_hir_resolve_imports(&main_tree, &symbols, &import, &diagnostics));
    CHECK(!xs_hir_validate_name_uses(&main_tree, &symbols, &import, &diagnostics));
    CHECK(xs_diagnostics_has_error(&diagnostics));
    xs_hir_import_scope_free(&import);
    xs_hir_symbol_table_free(&symbols);
    xs_syntax_tree_free(&main_tree);
    xs_syntax_tree_free(&library_tree);
    xs_diagnostics_free(&diagnostics);
}

static void test_expanded_macro_name_use_errors(void)
{
    const char *main = ""
                       "macro_rules! bad { () -> { Missing.Call() }; }\n"
                       "fn Main() { bad!(); }\n";
    XsSource source = {.path = "MacroNameUse.vxs", .module_name = "App", .text = main, .length = strlen(main)};
    XsSyntaxTree tree;
    XsHirSymbolTable symbols;
    XsHirImportScope import;
    XsMacroExpansionReport report;
    XsMacroStatementExpansionSet statements;
    XsDiagnostics diagnostics;
    xs_diagnostics_init(&diagnostics);
    xs_hir_symbol_table_init(&symbols);
    xs_hir_import_scope_init(&import);
    CHECK(xs_syntax_parse(&source, 62, &diagnostics, &tree));
    CHECK(xs_macro_validate(&tree, &diagnostics));
    CHECK(xs_macro_prepare_expansion(&tree, &diagnostics, &report));
    CHECK(xs_macro_expand_statements(&tree, &diagnostics, &statements));
    CHECK(statements.count == 1);
    CHECK(xs_hir_collect_symbols(&tree, &symbols, &diagnostics));
    CHECK(xs_hir_resolve_imports(&tree, &symbols, &import, &diagnostics));
    CHECK(xs_hir_validate_name_uses(&tree, &symbols, &import, &diagnostics));
    CHECK(!xs_hir_validate_name_uses_expanded(&tree, &statements, &symbols, &import, &diagnostics));
    CHECK(xs_diagnostics_has_error(&diagnostics));
    xs_macro_statement_expansion_set_free(&statements);
    xs_hir_import_scope_free(&import);
    xs_hir_symbol_table_free(&symbols);
    xs_syntax_tree_free(&tree);
    xs_diagnostics_free(&diagnostics);
}

static void test_statement_fragment_macro_name_use_errors(void)
{
    const char *main = ""
                       "macro_rules! pass { ($body:stmt) -> { $body }; }\n"
                       "fn Main() { pass!(Missing.Call();); }\n";
    XsSource source = {.path = "MacroStatementNameUse.vxs", .module_name = "App", .text = main, .length = strlen(main)};
    XsSyntaxTree tree;
    XsHirSymbolTable symbols;
    XsHirImportScope import;
    XsMacroExpansionReport report;
    XsMacroStatementExpansionSet statements;
    XsDiagnostics diagnostics;
    xs_diagnostics_init(&diagnostics);
    xs_hir_symbol_table_init(&symbols);
    xs_hir_import_scope_init(&import);
    CHECK(xs_syntax_parse(&source, 63, &diagnostics, &tree));
    CHECK(xs_macro_validate(&tree, &diagnostics));
    CHECK(xs_macro_prepare_expansion(&tree, &diagnostics, &report));
    CHECK(report.calls_expandable == 1);
    CHECK(xs_macro_expand_statements(&tree, &diagnostics, &statements));
    CHECK(statements.count == 1);
    CHECK(xs_hir_collect_symbols(&tree, &symbols, &diagnostics));
    CHECK(xs_hir_resolve_imports(&tree, &symbols, &import, &diagnostics));
    CHECK(xs_hir_validate_name_uses(&tree, &symbols, &import, &diagnostics));
    CHECK(!xs_hir_validate_name_uses_expanded(&tree, &statements, &symbols, &import, &diagnostics));
    CHECK(xs_diagnostics_has_error(&diagnostics));
    xs_macro_statement_expansion_set_free(&statements);
    xs_hir_import_scope_free(&import);
    xs_hir_symbol_table_free(&symbols);
    xs_syntax_tree_free(&tree);
    xs_diagnostics_free(&diagnostics);
}

static void test_block_fragment_macro_name_use_errors(void)
{
    const char *main = ""
                       "macro_rules! pass { ($body:block) -> { $body }; }\n"
                       "fn Main() { pass!({ Missing.Call(); }); }\n";
    XsSource source = {.path = "MacroBlockNameUse.vxs", .module_name = "App", .text = main, .length = strlen(main)};
    XsSyntaxTree tree;
    XsHirSymbolTable symbols;
    XsHirImportScope import;
    XsMacroExpansionReport report;
    XsMacroStatementExpansionSet statements;
    XsDiagnostics diagnostics;
    xs_diagnostics_init(&diagnostics);
    xs_hir_symbol_table_init(&symbols);
    xs_hir_import_scope_init(&import);
    CHECK(xs_syntax_parse(&source, 64, &diagnostics, &tree));
    CHECK(xs_macro_validate(&tree, &diagnostics));
    CHECK(xs_macro_prepare_expansion(&tree, &diagnostics, &report));
    CHECK(report.calls_expandable == 1);
    CHECK(xs_macro_expand_statements(&tree, &diagnostics, &statements));
    CHECK(statements.count == 1);
    CHECK(xs_hir_collect_symbols(&tree, &symbols, &diagnostics));
    CHECK(xs_hir_resolve_imports(&tree, &symbols, &import, &diagnostics));
    CHECK(xs_hir_validate_name_uses(&tree, &symbols, &import, &diagnostics));
    CHECK(!xs_hir_validate_name_uses_expanded(&tree, &statements, &symbols, &import, &diagnostics));
    CHECK(xs_diagnostics_has_error(&diagnostics));
    xs_macro_statement_expansion_set_free(&statements);
    xs_hir_import_scope_free(&import);
    xs_hir_symbol_table_free(&symbols);
    xs_syntax_tree_free(&tree);
    xs_diagnostics_free(&diagnostics);
}

static void test_path_fragment_macro_name_use_errors(void)
{
    const char *main = ""
                       "macro_rules! call { ($target:path) -> { $target(); }; }\n"
                       "fn Main() { call!(Missing.Call); }\n";
    XsSource source = {.path = "MacroPathNameUse.vxs", .module_name = "App", .text = main, .length = strlen(main)};
    XsSyntaxTree tree;
    XsHirSymbolTable symbols;
    XsHirImportScope import;
    XsMacroExpansionReport report;
    XsMacroStatementExpansionSet statements;
    XsDiagnostics diagnostics;
    xs_diagnostics_init(&diagnostics);
    xs_hir_symbol_table_init(&symbols);
    xs_hir_import_scope_init(&import);
    CHECK(xs_syntax_parse(&source, 65, &diagnostics, &tree));
    CHECK(xs_macro_validate(&tree, &diagnostics));
    CHECK(xs_macro_prepare_expansion(&tree, &diagnostics, &report));
    CHECK(report.calls_expandable == 1);
    CHECK(xs_macro_expand_statements(&tree, &diagnostics, &statements));
    CHECK(statements.count == 1);
    CHECK(xs_hir_collect_symbols(&tree, &symbols, &diagnostics));
    CHECK(xs_hir_resolve_imports(&tree, &symbols, &import, &diagnostics));
    CHECK(xs_hir_validate_name_uses(&tree, &symbols, &import, &diagnostics));
    CHECK(!xs_hir_validate_name_uses_expanded(&tree, &statements, &symbols, &import, &diagnostics));
    CHECK(xs_diagnostics_has_error(&diagnostics));
    xs_macro_statement_expansion_set_free(&statements);
    xs_hir_import_scope_free(&import);
    xs_hir_symbol_table_free(&symbols);
    xs_syntax_tree_free(&tree);
    xs_diagnostics_free(&diagnostics);
}

static void test_private_qualified_name_visibility(void)
{
    const char *library = ""
                          "private fn Hidden() {}\n";
    const char *main = ""
                       "fn Main() { Math::Hidden(); }\n";
    XsSyntaxTree library_tree;
    XsSyntaxTree main_tree;
    XsHirSymbolTable symbols;
    XsHirImportScope import;
    XsDiagnostics diagnostics;
    xs_diagnostics_init(&diagnostics);
    xs_hir_symbol_table_init(&symbols);
    xs_hir_import_scope_init(&import);
    CHECK(add_file_symbols_in_module(library, "Math", 71, &library_tree, &symbols, &diagnostics));
    CHECK(add_file_symbols(main, 72, &main_tree, &symbols, &diagnostics));
    CHECK(xs_hir_resolve_imports(&main_tree, &symbols, &import, &diagnostics));
    CHECK(!xs_hir_validate_name_uses(&main_tree, &symbols, &import, &diagnostics));
    CHECK(xs_diagnostics_has_error(&diagnostics));
    xs_hir_import_scope_free(&import);
    xs_hir_symbol_table_free(&symbols);
    xs_syntax_tree_free(&main_tree);
    xs_syntax_tree_free(&library_tree);
    xs_diagnostics_free(&diagnostics);
}

static void test_private_same_namespace_name_visibility(void)
{
    const char *text = ""
                       "private fn Hidden() {}\n"
                       "fn Main() { Math::Hidden(); }\n";
    XsSyntaxTree tree;
    XsHirSymbolTable symbols;
    XsHirImportScope import;
    XsDiagnostics diagnostics;
    xs_diagnostics_init(&diagnostics);
    xs_hir_symbol_table_init(&symbols);
    xs_hir_import_scope_init(&import);
    CHECK(add_file_symbols_in_module(text, "Math", 73, &tree, &symbols, &diagnostics));
    CHECK(xs_hir_resolve_imports(&tree, &symbols, &import, &diagnostics));
    CHECK(xs_hir_validate_name_uses(&tree, &symbols, &import, &diagnostics));
    xs_hir_import_scope_free(&import);
    xs_hir_symbol_table_free(&symbols);
    xs_syntax_tree_free(&tree);
    xs_diagnostics_free(&diagnostics);
}

static void test_private_same_namespace_different_file_name_visibility(void)
{
    const char *library = ""
                          "private fn Hidden() {}\n";
    const char *main = ""
                       "fn Main() { Math::Hidden(); }\n";
    XsSyntaxTree library_tree;
    XsSyntaxTree main_tree;
    XsHirSymbolTable symbols;
    XsHirImportScope import;
    XsDiagnostics diagnostics;
    xs_diagnostics_init(&diagnostics);
    xs_hir_symbol_table_init(&symbols);
    xs_hir_import_scope_init(&import);
    CHECK(add_file_symbols_in_module(library, "Math", 74, &library_tree, &symbols, &diagnostics));
    CHECK(add_file_symbols_in_module(main, "Math", 75, &main_tree, &symbols, &diagnostics));
    CHECK(xs_hir_resolve_imports(&main_tree, &symbols, &import, &diagnostics));
    CHECK(!xs_hir_validate_name_uses(&main_tree, &symbols, &import, &diagnostics));
    CHECK(xs_diagnostics_has_error(&diagnostics));
    xs_hir_import_scope_free(&import);
    xs_hir_symbol_table_free(&symbols);
    xs_syntax_tree_free(&main_tree);
    xs_syntax_tree_free(&library_tree);
    xs_diagnostics_free(&diagnostics);
}

static void test_internal_same_module_different_file_visibility(void)
{
    const char *library = ""
                          "internal fn Add(left: Long, right: Long) -> Long { return left + right; }\n";
    const char *main = ""
                       "fn Main() -> Long { return Add(3, 4); }\n";
    XsSyntaxTree library_tree;
    XsSyntaxTree main_tree;
    XsHirSymbolTable symbols;
    XsHirImportScope import;
    XsDiagnostics diagnostics;
    xs_diagnostics_init(&diagnostics);
    xs_hir_symbol_table_init(&symbols);
    xs_hir_import_scope_init(&import);
    CHECK(add_file_symbols_in_module(library, "Math", 76, &library_tree, &symbols, &diagnostics));
    CHECK(add_file_symbols_in_module(main, "Math", 77, &main_tree, &symbols, &diagnostics));
    CHECK(xs_hir_resolve_imports(&main_tree, &symbols, &import, &diagnostics));
    CHECK(xs_hir_validate_name_uses(&main_tree, &symbols, &import, &diagnostics));
    xs_hir_import_scope_free(&import);
    xs_hir_symbol_table_free(&symbols);
    xs_syntax_tree_free(&main_tree);
    xs_syntax_tree_free(&library_tree);
    xs_diagnostics_free(&diagnostics);
}

int main(void)
{
    test_module_namespace_symbols();
    test_duplicate_names_in_namespace();
    test_extern_block_symbols();
    test_extern_block_duplicate_symbol_errors();
    test_cffi_validation_accepts_repr_c_extern_block();
    test_cffi_validation_rejects_missing_repr_c();
    test_cffi_validation_rejects_unsupported_abi();
    test_cffi_validation_rejects_non_c_repr();
    test_cffi_validation_rejects_invalid_link_name();
    test_cffi_validation_rejects_thread_local_function();
    test_cffi_validation_rejects_function_attribute_on_static();
    test_cffi_validation_rejects_block_attribute_on_function();
    test_same_name_in_different_namespace();
    test_import_resolution();
    test_public_namespace_exports_explicit_public_symbols();
    test_import_errors();
    test_name_use_resolution();
    test_standard_library_name_resolution();
    test_associated_items_and_pattern_bindings();
    test_name_use_errors();
    test_qualified_external_name_requires_import();
    test_expanded_macro_name_use_errors();
    test_statement_fragment_macro_name_use_errors();
    test_block_fragment_macro_name_use_errors();
    test_path_fragment_macro_name_use_errors();
    test_private_qualified_name_visibility();
    test_private_same_namespace_name_visibility();
    test_private_same_namespace_different_file_name_visibility();
    test_internal_same_module_different_file_visibility();
    return failures == 0 ? 0 : 1;
}
