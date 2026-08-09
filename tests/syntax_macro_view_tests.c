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

static bool text_is(XsText text, const char *value)
{
    return text.length == strlen(value) && memcmp(text.data, value, text.length) == 0;
}

static const XsSyntaxNode *first_identifier(const XsSyntaxNode *node)
{
    return xs_syntax_find_first(node, XS_SYNTAX_IDENTIFIER);
}

static size_t count_kind(const XsSyntaxNode *node, XsSyntaxKind kind)
{
    if(node == nullptr)
        return 0;
    size_t count = node->kind == kind ? 1 : 0;
    for(size_t i = 0; i < node->child_count; ++i)
        count += count_kind(node->children[i], kind);
    return count;
}

static bool has_identifier(const XsSyntaxNode *node, const char *value)
{
    if(node == nullptr)
        return false;
    if(node->kind == XS_SYNTAX_IDENTIFIER && text_is(node->text, value))
        return true;
    for(size_t i = 0; i < node->child_count; ++i)
    {
        if(has_identifier(node->children[i], value))
            return true;
    }
    return false;
}

static void test_top_level_declaration_view_expands_macro_calls(void)
{
    const char *text = ""
                       "macro_rules! make {\n"
                       "  (first) -> { incomplete fn First(); };\n"
                       "  (second) -> { incomplete fn Second(); };\n"
                       "}\n"
                       "make!(first);\n"
                       "data Tail { value: Int }\n";
    XsSource source = {.path = "ExpandedView.vxs", .module_name = "App", .text = text, .length = strlen(text)};
    XsDiagnostics diagnostics;
    XsSyntaxTree tree;
    XsMacroDeclarationExpansionSet declarations;
    XsMacroExpandedDeclarationSet expanded;
    xs_diagnostics_init(&diagnostics);
    CHECK(xs_syntax_parse(&source, 91, &diagnostics, &tree));
    CHECK(xs_macro_validate(&tree, &diagnostics));
    CHECK(xs_macro_expand_declarations(&tree, &diagnostics, &declarations));
    CHECK(declarations.count == 1);
    CHECK(xs_macro_expand_top_level_declarations(&tree, &declarations, &diagnostics, &expanded));
    CHECK(expanded.count == 3);
    CHECK(expanded.count < 2 || expanded.items[1].from_macro_expansion);
    CHECK(expanded.count < 2 || expanded.items[1].declaration->kind == XS_SYNTAX_DECL_FUNCTION);
    CHECK(expanded.count < 3 || expanded.items[2].declaration->kind == XS_SYNTAX_DECL_DATA);
    const XsSyntaxNode *first = expanded.count < 2 ? nullptr : first_identifier(expanded.items[1].declaration);
    CHECK(first != nullptr && text_is(first->text, "First"));
    xs_macro_expanded_declaration_set_free(&expanded);
    xs_macro_declaration_expansion_set_free(&declarations);
    xs_syntax_tree_free(&tree);
    xs_diagnostics_free(&diagnostics);
}

static void test_child_declaration_view_expands_member_macro_calls(void)
{
    const char *text = ""
                       "class User {\n"
                       "  macro_rules! make { () -> { incomplete fn Generated(); }; }\n"
                       "  make!();\n"
                       "}\n";
    XsSource source = {.path = "ExpandedMemberView.vxs", .module_name = "App", .text = text, .length = strlen(text)};
    XsDiagnostics diagnostics;
    XsSyntaxTree tree;
    XsMacroDeclarationExpansionSet declarations;
    XsMacroExpandedDeclarationSet expanded;
    xs_diagnostics_init(&diagnostics);
    CHECK(xs_syntax_parse(&source, 92, &diagnostics, &tree));
    CHECK(xs_macro_validate(&tree, &diagnostics));
    CHECK(xs_macro_expand_declarations(&tree, &diagnostics, &declarations));
    CHECK(declarations.count == 1);
    const XsSyntaxNode *class_node = xs_syntax_find_first(tree.root, XS_SYNTAX_DECL_CLASS);
    CHECK(class_node != nullptr);
    CHECK(xs_macro_expand_child_declarations(class_node, &declarations, &diagnostics, &expanded));
    const XsSyntaxNode *generated = nullptr;
    for(size_t i = 0; i < expanded.count; ++i)
    {
        if(expanded.items[i].from_macro_expansion && expanded.items[i].declaration->kind == XS_SYNTAX_DECL_FUNCTION)
            generated = expanded.items[i].declaration;
    }
    CHECK(generated != nullptr);
    const XsSyntaxNode *name = first_identifier(generated);
    CHECK(name != nullptr && text_is(name->text, "Generated"));
    xs_macro_expanded_declaration_set_free(&expanded);
    xs_macro_declaration_expansion_set_free(&declarations);
    xs_syntax_tree_free(&tree);
    xs_diagnostics_free(&diagnostics);
}

static void test_child_declaration_view_expands_field_like_macro_calls(void)
{
    const char *text = ""
                       "class User {\n"
                       "  macro_rules! make { () -> { value: Int; }; }\n"
                       "  make!();\n"
                       "}\n";
    XsSource source = {
        .path = "ExpandedFieldLikeMemberView.vxs", .module_name = "App", .text = text, .length = strlen(text)};
    XsDiagnostics diagnostics;
    XsSyntaxTree tree;
    XsMacroDeclarationExpansionSet declarations;
    XsMacroExpandedDeclarationSet expanded;
    xs_diagnostics_init(&diagnostics);
    CHECK(xs_syntax_parse(&source, 93, &diagnostics, &tree));
    CHECK(xs_macro_validate(&tree, &diagnostics));
    CHECK(xs_macro_expand_declarations(&tree, &diagnostics, &declarations));
    CHECK(declarations.count == 1);
    const XsSyntaxNode *class_node = xs_syntax_find_first(tree.root, XS_SYNTAX_DECL_CLASS);
    CHECK(class_node != nullptr);
    CHECK(xs_macro_expand_child_declarations(class_node, &declarations, &diagnostics, &expanded));
    const XsSyntaxNode *field_like = nullptr;
    for(size_t i = 0; i < expanded.count; ++i)
    {
        if(expanded.items[i].from_macro_expansion && expanded.items[i].declaration->kind == XS_SYNTAX_DECL_VARIABLE)
            field_like = expanded.items[i].declaration;
    }
    CHECK(field_like != nullptr);
    const XsSyntaxNode *name = first_identifier(field_like);
    CHECK(name != nullptr && text_is(name->text, "value"));
    xs_macro_expanded_declaration_set_free(&expanded);
    xs_macro_declaration_expansion_set_free(&declarations);
    xs_syntax_tree_free(&tree);
    xs_diagnostics_free(&diagnostics);
}

static void test_child_statement_view_expands_macro_calls(void)
{
    const char *text = ""
                       "macro_rules! emit { () -> { Known(); }; }\n"
                       "fn Known() {}\n"
                       "fn Main() {\n"
                       "  Before();\n"
                       "  emit!();\n"
                       "  After();\n"
                       "}\n";
    XsSource source = {.path = "ExpandedStatementView.vxs", .module_name = "App", .text = text, .length = strlen(text)};
    XsDiagnostics diagnostics;
    XsSyntaxTree tree;
    XsMacroStatementExpansionSet statements;
    XsMacroExpandedStatementSet expanded;
    xs_diagnostics_init(&diagnostics);
    CHECK(xs_syntax_parse(&source, 94, &diagnostics, &tree));
    CHECK(xs_macro_validate(&tree, &diagnostics));
    CHECK(xs_macro_expand_statements(&tree, &diagnostics, &statements));
    CHECK(statements.count == 1);
    const XsSyntaxNode *function = nullptr;
    for(size_t i = 0; tree.root != nullptr && i < tree.root->child_count; ++i)
    {
        const XsSyntaxNode *child = tree.root->children[i];
        const XsSyntaxNode *name = child->kind == XS_SYNTAX_DECL_FUNCTION ? first_identifier(child) : nullptr;
        if(name != nullptr && text_is(name->text, "Main"))
            function = child;
    }
    const XsSyntaxNode *block = xs_syntax_find_first(function, XS_SYNTAX_STMT_BLOCK);
    CHECK(block != nullptr);
    CHECK(xs_macro_expand_child_statements(block, &statements, &diagnostics, &expanded));
    CHECK(expanded.count == 3);
    CHECK(expanded.count < 2 || expanded.items[1].from_macro_expansion);
    CHECK(expanded.count < 1 || expanded.items[0].statement->kind == XS_SYNTAX_STMT_EXPRESSION);
    CHECK(expanded.count < 2 || expanded.items[1].statement->kind == XS_SYNTAX_STMT_EXPRESSION);
    CHECK(expanded.count < 3 || expanded.items[2].statement->kind == XS_SYNTAX_STMT_EXPRESSION);
    const XsSyntaxNode *replacement = expanded.count < 2 ? nullptr : first_identifier(expanded.items[1].statement);
    CHECK(replacement != nullptr && text_is(replacement->text, "Known"));
    xs_macro_expanded_statement_set_free(&expanded);
    xs_macro_statement_expansion_set_free(&statements);
    xs_syntax_tree_free(&tree);
    xs_diagnostics_free(&diagnostics);
}

static void test_materialized_expanded_tree_replaces_macro_calls(void)
{
    const char *text = ""
                       "macro_rules! make { () -> { incomplete fn Generated(); }; }\n"
                       "macro_rules! emit { () -> { Generated(); }; }\n"
                       "make!();\n"
                       "fn Main() { emit!(); }\n";
    XsSource source = {.path = "MaterializedMacroTree.vxs", .module_name = "App", .text = text, .length = strlen(text)};
    XsDiagnostics diagnostics;
    XsSyntaxTree tree;
    XsSyntaxTree expanded_tree;
    XsMacroDeclarationExpansionSet declarations;
    XsMacroStatementExpansionSet statements;
    xs_diagnostics_init(&diagnostics);
    CHECK(xs_syntax_parse(&source, 95, &diagnostics, &tree));
    CHECK(xs_macro_validate(&tree, &diagnostics));
    CHECK(xs_macro_expand_declarations(&tree, &diagnostics, &declarations));
    CHECK(xs_macro_expand_statements(&tree, &diagnostics, &statements));
    CHECK(xs_macro_materialize_expanded_tree(&tree, &declarations, &statements, &diagnostics, &expanded_tree));
    CHECK(count_kind(expanded_tree.root, XS_SYNTAX_DECL_MACRO_CALL) == 0);
    CHECK(count_kind(expanded_tree.root, XS_SYNTAX_STMT_MACRO_CALL) == 0);
    CHECK(count_kind(expanded_tree.root, XS_SYNTAX_DECL_FUNCTION) == 2);
    CHECK(has_identifier(expanded_tree.root, "Generated"));
    xs_syntax_tree_free(&expanded_tree);
    xs_macro_statement_expansion_set_free(&statements);
    xs_macro_declaration_expansion_set_free(&declarations);
    xs_syntax_tree_free(&tree);
    xs_diagnostics_free(&diagnostics);
}

int main(void)
{
    test_top_level_declaration_view_expands_macro_calls();
    test_child_declaration_view_expands_member_macro_calls();
    test_child_declaration_view_expands_field_like_macro_calls();
    test_child_statement_view_expands_macro_calls();
    test_materialized_expanded_tree_replaces_macro_calls();
    return failures == 0 ? 0 : 1;
}
