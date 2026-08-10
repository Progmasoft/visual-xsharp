/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 */

#include "Visual/C23/diagnostic.hh"
#include "Visual/XSharp/hir/symbol_table.h"
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

static bool parse_symbols(const char *text, XsSyntaxTree *tree, XsHirSymbolTable *symbols, XsDiagnostics *diagnostics)
{
    XsSource source = {.path = "Members.vxs", .module_name = "App", .text = text, .length = strlen(text)};
    xs_diagnostics_init(diagnostics);
    xs_hir_symbol_table_init(symbols);
    if(!xs_syntax_parse(&source, 101, diagnostics, tree))
        return false;
    return xs_hir_collect_symbols(tree, symbols, diagnostics);
}

static void test_class_member_symbols(void)
{
    const char *text = ""
                       "class User {\n"
                       "  name: Str;\n"
                       "  fn get_name() -> Str { return name; }\n"
                       "}\n";
    XsSyntaxTree tree;
    XsHirSymbolTable symbols;
    XsHirMemberSymbolTable members;
    XsDiagnostics diagnostics;
    CHECK(parse_symbols(text, &tree, &symbols, &diagnostics));
    xs_hir_member_symbol_table_init(&members);
    const XsHirSymbol *user = xs_hir_symbol_table_find(&symbols, "App.User");
    CHECK(user != nullptr);
    CHECK(xs_hir_collect_member_symbols(user, nullptr, &members, &diagnostics));
    CHECK(members.count == 2);
    const XsHirMemberSymbol *name = xs_hir_member_symbol_table_find(&members, "App.User", "name");
    const XsHirMemberSymbol *method = xs_hir_member_symbol_table_find(&members, "App.User", "get_name");
    CHECK(name != nullptr && name->kind == XS_HIR_MEMBER_FIELD);
    CHECK(name != nullptr && name->visibility == XS_SYNTAX_VISIBILITY_INTERNAL);
    CHECK(method != nullptr && method->kind == XS_HIR_MEMBER_METHOD);
    CHECK(method != nullptr && method->visibility == XS_SYNTAX_VISIBILITY_INTERNAL);
    CHECK(method != nullptr && strcmp(method->qualified_name, "App.User.get_name") == 0);
    xs_hir_member_symbol_table_free(&members);
    xs_hir_symbol_table_free(&symbols);
    xs_syntax_tree_free(&tree);
    xs_diagnostics_free(&diagnostics);
}

static void test_csharp_style_public_property_members(void)
{
    const char *text = ""
                       "public class Person {\n"
                       "  public Name: Str { getter; setter; }\n"
                       "  public Age: Int { getter; setter; }\n"
                       "}\n";
    XsSyntaxTree tree;
    XsHirSymbolTable symbols;
    XsHirMemberSymbolTable members;
    XsDiagnostics diagnostics;
    CHECK(parse_symbols(text, &tree, &symbols, &diagnostics));
    xs_hir_member_symbol_table_init(&members);
    const XsHirSymbol *person = xs_hir_symbol_table_find(&symbols, "App.Person");
    CHECK(person != nullptr && person->visibility == XS_SYNTAX_VISIBILITY_PUBLIC);
    CHECK(xs_hir_collect_member_symbols(person, nullptr, &members, &diagnostics));
    const XsHirMemberSymbol *name = xs_hir_member_symbol_table_find(&members, "App.Person", "Name");
    const XsHirMemberSymbol *age = xs_hir_member_symbol_table_find(&members, "App.Person", "Age");
    CHECK(name != nullptr && name->kind == XS_HIR_MEMBER_FIELD);
    CHECK(age != nullptr && age->kind == XS_HIR_MEMBER_FIELD);
    CHECK(name != nullptr && name->visibility == XS_SYNTAX_VISIBILITY_PUBLIC);
    CHECK(age != nullptr && age->visibility == XS_SYNTAX_VISIBILITY_PUBLIC);
    xs_hir_member_symbol_table_free(&members);
    xs_hir_symbol_table_free(&symbols);
    xs_syntax_tree_free(&tree);
    xs_diagnostics_free(&diagnostics);
}

static void test_data_member_symbols_and_method_resolution(void)
{
    const char *text = ""
                       "data User {\n"
                       "  name: Str;\n"
                       "  User(name: Str) {}\n"
                       "  User(name: Str, age: Int) {}\n"
                       "  fn get_name() -> Str { return name; }\n"
                       "}\n"
                       "fn main() { user: User = new User(); user.get_name(); }\n";
    XsSyntaxTree tree;
    XsHirSymbolTable symbols;
    XsHirMemberSymbolTable members;
    XsHirImportScope import;
    XsDiagnostics diagnostics;
    CHECK(parse_symbols(text, &tree, &symbols, &diagnostics));
    xs_hir_member_symbol_table_init(&members);
    const XsHirSymbol *user = xs_hir_symbol_table_find(&symbols, "App.User");
    CHECK(user != nullptr);
    CHECK(xs_hir_collect_member_symbols(user, nullptr, &members, &diagnostics));
    CHECK(xs_hir_member_symbol_table_find(&members, "App.User", "name") != nullptr);
    const XsHirMemberSymbol *constructor = xs_hir_member_symbol_table_find(&members, "App.User", "User");
    CHECK(constructor != nullptr && constructor->kind == XS_HIR_MEMBER_CONSTRUCTOR);
    const XsHirMemberSymbol *method = xs_hir_member_symbol_table_find(&members, "App.User", "get_name");
    CHECK(method != nullptr && method->kind == XS_HIR_MEMBER_METHOD);
    xs_hir_import_scope_init(&import);
    CHECK(xs_hir_resolve_imports(&tree, &symbols, &import, &diagnostics));
    CHECK(xs_hir_validate_name_uses_with_macros(&tree, nullptr, nullptr, &symbols, &import, &diagnostics));
    xs_hir_import_scope_free(&import);
    xs_hir_member_symbol_table_free(&members);
    xs_hir_symbol_table_free(&symbols);
    xs_syntax_tree_free(&tree);
    xs_diagnostics_free(&diagnostics);
}

static void test_method_merge_uses_last_member_symbol(void)
{
    const char *text = ""
                       "class User {\n"
                       "  incomplete fn Merge(value: Int);\n"
                       "  incomplete fn Merge(value: Str);\n"
                       "}\n";
    XsSyntaxTree tree;
    XsHirSymbolTable symbols;
    XsHirMemberSymbolTable members;
    XsDiagnostics diagnostics;
    CHECK(parse_symbols(text, &tree, &symbols, &diagnostics));
    xs_hir_member_symbol_table_init(&members);
    const XsHirSymbol *user = xs_hir_symbol_table_find(&symbols, "App.User");
    CHECK(xs_hir_collect_member_symbols(user, nullptr, &members, &diagnostics));
    CHECK(members.count == 2);
    const XsHirMemberSymbol *merged = xs_hir_member_symbol_table_find(&members, "App.User", "Merge");
    CHECK(merged != nullptr && merged->kind == XS_HIR_MEMBER_METHOD);
    CHECK(merged != nullptr && merged->span.start_offset > 60);
    xs_hir_member_symbol_table_free(&members);
    xs_hir_symbol_table_free(&symbols);
    xs_syntax_tree_free(&tree);
    xs_diagnostics_free(&diagnostics);
}

static void test_duplicate_field_member_errors(void)
{
    const char *text = ""
                       "class User {\n"
                       "  value: Int;\n"
                       "  value: Str;\n"
                       "}\n";
    XsSyntaxTree tree;
    XsHirSymbolTable symbols;
    XsHirMemberSymbolTable members;
    XsDiagnostics diagnostics;
    CHECK(parse_symbols(text, &tree, &symbols, &diagnostics));
    xs_hir_member_symbol_table_init(&members);
    const XsHirSymbol *user = xs_hir_symbol_table_find(&symbols, "App.User");
    CHECK(!xs_hir_collect_member_symbols(user, nullptr, &members, &diagnostics));
    CHECK(xs_diagnostics_has_error(&diagnostics));
    xs_hir_member_symbol_table_free(&members);
    xs_hir_symbol_table_free(&symbols);
    xs_syntax_tree_free(&tree);
    xs_diagnostics_free(&diagnostics);
}

static void test_macro_generated_field_like_member_symbol(void)
{
    const char *text = ""
                       "class User {\n"
                       "  macro_rules! make { () -> { value: Int; }; }\n"
                       "  make!();\n"
                       "}\n";
    XsSource source = {.path = "MacroMembers.vxs", .module_name = "App", .text = text, .length = strlen(text)};
    XsSyntaxTree tree;
    XsHirSymbolTable symbols;
    XsHirMemberSymbolTable members;
    XsMacroDeclarationExpansionSet declarations;
    XsDiagnostics diagnostics;
    xs_diagnostics_init(&diagnostics);
    xs_hir_symbol_table_init(&symbols);
    xs_hir_member_symbol_table_init(&members);
    CHECK(xs_syntax_parse(&source, 102, &diagnostics, &tree));
    CHECK(xs_macro_validate(&tree, &diagnostics));
    CHECK(xs_macro_expand_declarations(&tree, &diagnostics, &declarations));
    CHECK(xs_hir_collect_symbols_expanded(&tree, &declarations, &symbols, &diagnostics));
    const XsHirSymbol *user = xs_hir_symbol_table_find(&symbols, "App.User");
    CHECK(xs_hir_collect_member_symbols(user, &declarations, &members, &diagnostics));
    const XsHirMemberSymbol *value = xs_hir_member_symbol_table_find(&members, "App.User", "value");
    CHECK(value != nullptr && value->kind == XS_HIR_MEMBER_FIELD);
    xs_macro_declaration_expansion_set_free(&declarations);
    xs_hir_member_symbol_table_free(&members);
    xs_hir_symbol_table_free(&symbols);
    xs_syntax_tree_free(&tree);
    xs_diagnostics_free(&diagnostics);
}

static void test_macro_generated_member_conflicts_with_field(void)
{
    const char *text = ""
                       "class User {\n"
                       "  value: Int;\n"
                       "  macro_rules! make { () -> { value: Str; }; }\n"
                       "  make!();\n"
                       "}\n";
    XsSource source = {.path = "MacroFieldConflict.vxs", .module_name = "App", .text = text, .length = strlen(text)};
    XsSyntaxTree tree;
    XsHirSymbolTable symbols;
    XsHirMemberSymbolTable members;
    XsMacroDeclarationExpansionSet declarations;
    XsDiagnostics diagnostics;
    xs_diagnostics_init(&diagnostics);
    xs_hir_symbol_table_init(&symbols);
    xs_hir_member_symbol_table_init(&members);
    CHECK(xs_syntax_parse(&source, 104, &diagnostics, &tree));
    CHECK(xs_macro_validate(&tree, &diagnostics));
    CHECK(xs_macro_expand_declarations(&tree, &diagnostics, &declarations));
    CHECK(xs_hir_collect_symbols_expanded(&tree, &declarations, &symbols, &diagnostics));
    const XsHirSymbol *user = xs_hir_symbol_table_find(&symbols, "App.User");
    CHECK(!xs_hir_collect_member_symbols(user, &declarations, &members, &diagnostics));
    CHECK(xs_diagnostics_has_error(&diagnostics));
    xs_macro_declaration_expansion_set_free(&declarations);
    xs_hir_member_symbol_table_free(&members);
    xs_hir_symbol_table_free(&symbols);
    xs_syntax_tree_free(&tree);
    xs_diagnostics_free(&diagnostics);
}

static void test_macro_generated_member_conflicts_with_method(void)
{
    const char *text = ""
                       "class User {\n"
                       "  incomplete fn value();\n"
                       "  macro_rules! make { () -> { value: Int; }; }\n"
                       "  make!();\n"
                       "}\n";
    XsSource source = {
        .path = "MacroMethodFieldConflict.vxs", .module_name = "App", .text = text, .length = strlen(text)};
    XsSyntaxTree tree;
    XsHirSymbolTable symbols;
    XsHirMemberSymbolTable members;
    XsMacroDeclarationExpansionSet declarations;
    XsDiagnostics diagnostics;
    xs_diagnostics_init(&diagnostics);
    xs_hir_symbol_table_init(&symbols);
    xs_hir_member_symbol_table_init(&members);
    CHECK(xs_syntax_parse(&source, 105, &diagnostics, &tree));
    CHECK(xs_macro_validate(&tree, &diagnostics));
    CHECK(xs_macro_expand_declarations(&tree, &diagnostics, &declarations));
    CHECK(xs_hir_collect_symbols_expanded(&tree, &declarations, &symbols, &diagnostics));
    const XsHirSymbol *user = xs_hir_symbol_table_find(&symbols, "App.User");
    CHECK(!xs_hir_collect_member_symbols(user, &declarations, &members, &diagnostics));
    CHECK(xs_diagnostics_has_error(&diagnostics));
    xs_macro_declaration_expansion_set_free(&declarations);
    xs_hir_member_symbol_table_free(&members);
    xs_hir_symbol_table_free(&symbols);
    xs_syntax_tree_free(&tree);
    xs_diagnostics_free(&diagnostics);
}

static void test_explicit_local_method_call_resolution(void)
{
    const char *text = ""
                       "class User { incomplete fn get_name(); }\n"
                       "fn main() { user: User = new User(); user.get_name(); }\n";
    XsSyntaxTree tree;
    XsHirSymbolTable symbols;
    XsHirImportScope import;
    XsDiagnostics diagnostics;
    CHECK(parse_symbols(text, &tree, &symbols, &diagnostics));
    xs_hir_import_scope_init(&import);
    CHECK(xs_hir_resolve_imports(&tree, &symbols, &import, &diagnostics));
    CHECK(xs_hir_validate_name_uses_with_macros(&tree, nullptr, nullptr, &symbols, &import, &diagnostics));
    xs_hir_import_scope_free(&import);
    xs_hir_symbol_table_free(&symbols);
    xs_syntax_tree_free(&tree);
    xs_diagnostics_free(&diagnostics);
}

static void test_missing_explicit_local_method_call_errors(void)
{
    const char *text = ""
                       "class User { incomplete fn get_name(); }\n"
                       "fn main() { user: User = new User(); user.missing(); }\n";
    XsSyntaxTree tree;
    XsHirSymbolTable symbols;
    XsHirImportScope import;
    XsDiagnostics diagnostics;
    CHECK(parse_symbols(text, &tree, &symbols, &diagnostics));
    xs_hir_import_scope_init(&import);
    CHECK(xs_hir_resolve_imports(&tree, &symbols, &import, &diagnostics));
    CHECK(!xs_hir_validate_name_uses_with_macros(&tree, nullptr, nullptr, &symbols, &import, &diagnostics));
    CHECK(xs_diagnostics_has_error(&diagnostics));
    xs_hir_import_scope_free(&import);
    xs_hir_symbol_table_free(&symbols);
    xs_syntax_tree_free(&tree);
    xs_diagnostics_free(&diagnostics);
}

static void test_macro_generated_method_call_resolution(void)
{
    const char *text = ""
                       "class User {\n"
                       "  macro_rules! make { () -> { incomplete fn generated(); }; }\n"
                       "  make!();\n"
                       "}\n"
                       "fn main() { user: User = new User(); user.generated(); }\n";
    XsSource source = {.path = "MacroMethodCall.vxs", .module_name = "App", .text = text, .length = strlen(text)};
    XsSyntaxTree tree;
    XsHirSymbolTable symbols;
    XsHirImportScope import;
    XsMacroDeclarationExpansionSet declarations;
    XsDiagnostics diagnostics;
    xs_diagnostics_init(&diagnostics);
    xs_hir_symbol_table_init(&symbols);
    xs_hir_import_scope_init(&import);
    CHECK(xs_syntax_parse(&source, 103, &diagnostics, &tree));
    CHECK(xs_macro_validate(&tree, &diagnostics));
    CHECK(xs_macro_expand_declarations(&tree, &diagnostics, &declarations));
    CHECK(xs_hir_collect_symbols_expanded(&tree, &declarations, &symbols, &diagnostics));
    CHECK(xs_hir_resolve_imports(&tree, &symbols, &import, &diagnostics));
    CHECK(xs_hir_validate_name_uses_with_macros(&tree, &declarations, nullptr, &symbols, &import, &diagnostics));
    xs_macro_declaration_expansion_set_free(&declarations);
    xs_hir_import_scope_free(&import);
    xs_hir_symbol_table_free(&symbols);
    xs_syntax_tree_free(&tree);
    xs_diagnostics_free(&diagnostics);
}

int main(void)
{
    test_class_member_symbols();
    test_csharp_style_public_property_members();
    test_data_member_symbols_and_method_resolution();
    test_method_merge_uses_last_member_symbol();
    test_duplicate_field_member_errors();
    test_macro_generated_field_like_member_symbol();
    test_macro_generated_member_conflicts_with_field();
    test_macro_generated_member_conflicts_with_method();
    test_explicit_local_method_call_resolution();
    test_missing_explicit_local_method_call_errors();
    test_macro_generated_method_call_resolution();
    return failures == 0 ? 0 : 1;
}
