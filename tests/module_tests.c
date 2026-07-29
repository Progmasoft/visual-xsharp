/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 */

#include "xs/diagnostic.h"
#include "xs/hir/symbol_table.h"
#include "xs/syntax_parser.h"

#include <stdio.h>
#include <string.h>

static int failures;

#define CHECK(condition)                                                                                               \
  do                                                                                                                   \
  {                                                                                                                    \
    if(!(condition))                                                                                                   \
    {                                                                                                                  \
      fprintf(stderr, "%s:%d: check failed: %s\n", __FILE__, __LINE__, #condition);                                    \
      ++failures;                                                                                                      \
    }                                                                                                                  \
  } while(0)

static bool add_source(const char *path, const char *module_name, const char *text, uint64_t file_id,
                       XsSyntaxTree *tree, XsHirSymbolTable *symbols, XsDiagnostics *diagnostics)
{
  XsSource source = {.path = path, .module_name = module_name, .text = text, .length = strlen(text)};
  return xs_syntax_parse(&source, file_id, diagnostics, tree) &&
         xs_hir_collect_symbols(tree, symbols, diagnostics);
}

static void test_qualified_public_module_import(void)
{
  const char *library = "public fn add(left: Int, right: Int) -> Int { return left + right; }\n";
  const char *application = "import Math::Advanced;\nfn main() -> Int { return Math::Advanced::add(2, 5); }\n";
  XsSyntaxTree library_tree;
  XsSyntaxTree application_tree;
  XsHirSymbolTable symbols;
  XsHirImportScope imports;
  XsDiagnostics diagnostics;
  xs_hir_symbol_table_init(&symbols);
  xs_hir_import_scope_init(&imports);
  xs_diagnostics_init(&diagnostics);
  CHECK(add_source("Modules/Math/add.xs", "Math::Advanced", library, 1, &library_tree, &symbols, &diagnostics));
  CHECK(add_source("Sources/main.xs", nullptr, application, 2, &application_tree, &symbols, &diagnostics));
  CHECK(xs_hir_symbol_table_find(&symbols, "Math.Advanced.add") != nullptr);
  CHECK(xs_hir_resolve_imports(&application_tree, &symbols, &imports, &diagnostics));
  CHECK(xs_hir_import_scope_has_module(&imports, "Math.Advanced"));
  CHECK(xs_hir_validate_name_uses(&application_tree, &symbols, &imports, &diagnostics));
  xs_hir_import_scope_free(&imports);
  xs_hir_symbol_table_free(&symbols);
  xs_syntax_tree_free(&application_tree);
  xs_syntax_tree_free(&library_tree);
  xs_diagnostics_free(&diagnostics);
}

static void test_module_names_are_case_sensitive(void)
{
  const char *library = "public fn add(left: Int, right: Int) -> Int { return left + right; }\n";
  const char *application = "import math;\nfn main() -> Int { return math::add(2, 5); }\n";
  XsSyntaxTree library_tree;
  XsSyntaxTree application_tree;
  XsHirSymbolTable symbols;
  XsHirImportScope imports;
  XsDiagnostics diagnostics;
  xs_hir_symbol_table_init(&symbols);
  xs_hir_import_scope_init(&imports);
  xs_diagnostics_init(&diagnostics);
  CHECK(add_source("Modules/Math/add.xs", "Math", library, 10, &library_tree, &symbols, &diagnostics));
  CHECK(add_source("Sources/main.xs", "App", application, 11, &application_tree, &symbols, &diagnostics));
  CHECK(!xs_hir_resolve_imports(&application_tree, &symbols, &imports, &diagnostics));
  CHECK(xs_diagnostics_has_error(&diagnostics));
  xs_hir_import_scope_free(&imports);
  xs_hir_symbol_table_free(&symbols);
  xs_syntax_tree_free(&application_tree);
  xs_syntax_tree_free(&library_tree);
  xs_diagnostics_free(&diagnostics);
}

static void test_default_visibility_is_module_internal(void)
{
  const char *library = "fn hidden() -> Int { return 7; }\n";
  const char *same_module = "fn use_hidden() -> Int { return Math::hidden(); }\n";
  const char *other_module = "import Math;\nfn main() -> Int { return Math::hidden(); }\n";
  XsSyntaxTree library_tree;
  XsSyntaxTree same_tree;
  XsSyntaxTree other_tree;
  XsHirSymbolTable symbols;
  XsHirImportScope same_imports;
  XsHirImportScope other_imports;
  XsDiagnostics diagnostics;
  xs_hir_symbol_table_init(&symbols);
  xs_hir_import_scope_init(&same_imports);
  xs_hir_import_scope_init(&other_imports);
  xs_diagnostics_init(&diagnostics);
  CHECK(add_source("Modules/Math/hidden.xs", "Math", library, 20, &library_tree, &symbols, &diagnostics));
  CHECK(add_source("Modules/Math/use_hidden.xs", "Math", same_module, 21, &same_tree, &symbols, &diagnostics));
  CHECK(add_source("Sources/main.xs", "App", other_module, 22, &other_tree, &symbols, &diagnostics));
  const XsHirSymbol *hidden = xs_hir_symbol_table_find(&symbols, "Math.hidden");
  CHECK(hidden != nullptr);
  CHECK(hidden != nullptr && hidden->visibility == XS_SYNTAX_VISIBILITY_INTERNAL);
  CHECK(xs_hir_resolve_imports(&same_tree, &symbols, &same_imports, &diagnostics));
  CHECK(xs_hir_validate_name_uses(&same_tree, &symbols, &same_imports, &diagnostics));
  CHECK(xs_hir_resolve_imports(&other_tree, &symbols, &other_imports, &diagnostics));
  CHECK(!xs_hir_validate_name_uses(&other_tree, &symbols, &other_imports, &diagnostics));
  xs_hir_import_scope_free(&other_imports);
  xs_hir_import_scope_free(&same_imports);
  xs_hir_symbol_table_free(&symbols);
  xs_syntax_tree_free(&other_tree);
  xs_syntax_tree_free(&same_tree);
  xs_syntax_tree_free(&library_tree);
  xs_diagnostics_free(&diagnostics);
}

static void test_panic_namespace_cannot_be_opened(void)
{
  const char *text = "using namespace panic;\nfn main() { panic!(); }\n";
  XsSyntaxTree tree;
  XsHirSymbolTable symbols;
  XsHirImportScope imports;
  XsDiagnostics diagnostics;
  xs_hir_symbol_table_init(&symbols);
  xs_hir_import_scope_init(&imports);
  xs_diagnostics_init(&diagnostics);
  CHECK(add_source("Sources/main.xs", "App", text, 30, &tree, &symbols, &diagnostics));
  CHECK(!xs_hir_resolve_imports(&tree, &symbols, &imports, &diagnostics));
  CHECK(xs_diagnostics_has_error(&diagnostics));
  xs_hir_import_scope_free(&imports);
  xs_hir_symbol_table_free(&symbols);
  xs_syntax_tree_free(&tree);
  xs_diagnostics_free(&diagnostics);
}

static void test_block_namespaces_restore_parent_scope(void)
{
  const char *text = "namespace detail;\n"
                     "namespace first { public fn one() {} }\n"
                     "namespace second { namespace nested { public fn two() {} } }\n"
                     "public fn root() {}\n";
  XsSyntaxTree tree;
  XsHirSymbolTable symbols;
  XsDiagnostics diagnostics;
  xs_hir_symbol_table_init(&symbols);
  xs_diagnostics_init(&diagnostics);
  CHECK(add_source("Modules/Math/namespaces.xs", "Math", text, 40, &tree, &symbols, &diagnostics));
  CHECK(xs_hir_symbol_table_find(&symbols, "Math.detail.first.one") != nullptr);
  CHECK(xs_hir_symbol_table_find(&symbols, "Math.detail.second.nested.two") != nullptr);
  CHECK(xs_hir_symbol_table_find(&symbols, "Math.detail.root") != nullptr);
  xs_hir_symbol_table_free(&symbols);
  xs_syntax_tree_free(&tree);
  xs_diagnostics_free(&diagnostics);
}

int main(void)
{
  test_qualified_public_module_import();
  test_module_names_are_case_sensitive();
  test_default_visibility_is_module_internal();
  test_panic_namespace_cannot_be_opened();
  test_block_namespaces_restore_parent_scope();
  return failures == 0 ? 0 : 1;
}
