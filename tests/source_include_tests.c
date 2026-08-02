/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 */

#include "xs/diagnostic.h"
#include "xs/source_include.h"
#include "xs/syntax_parser.h"

#include <stdio.h>
#include <stdlib.h>
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

static bool write_text_file(const char *path, const char *text)
{
    FILE *file = fopen(path, "wb");
    if(file == nullptr)
        return false;
    size_t length = strlen(text);
    bool success = fwrite(text, 1, length, file) == length;
    return fclose(file) == 0 && success;
}

static void test_source_include_macro(void)
{
    const char *leaf_path = "xs_include_leaf.xs";
    const char *main_path = "xs_include_main.xs";
    CHECK(write_text_file(leaf_path, "incomplete fn Included();\n"));
    const char *main = "include!(\"xs_include_leaf.xs\");\n"
                       "incomplete fn Main();\n";
    CHECK(write_text_file(main_path, main));
    XsSource source = {.path = main_path, .text = main, .length = strlen(main)};
    XsDiagnostics diagnostics;
    XsIncludedSource expanded;
    XsSyntaxTree tree;
    xs_diagnostics_init(&diagnostics);
    CHECK(xs_syntax_parse(&source, 35, &diagnostics, &tree));
    CHECK(xs_source_expand_include_macros(&tree, &diagnostics, &expanded));
    CHECK(expanded.text != nullptr && strstr(expanded.text, "Included") != nullptr);
    xs_syntax_tree_free(&tree);
    XsSource expanded_source = {.path = main_path, .text = expanded.text, .length = expanded.length};
    CHECK(xs_syntax_parse(&expanded_source, 35, &diagnostics, &tree));
    xs_syntax_tree_free(&tree);
    xs_included_source_free(&expanded);
    xs_diagnostics_free(&diagnostics);
    remove(leaf_path);
    remove(main_path);
}

static void test_source_include_rejects_nonlocal_path(void)
{
    const char *text = "include!(\"/tmp/Nope.xs\");\n";
    XsSource source = {.path = "RejectInclude.xs", .text = text, .length = strlen(text)};
    XsDiagnostics diagnostics;
    XsIncludedSource expanded;
    XsSyntaxTree tree;
    xs_diagnostics_init(&diagnostics);
    CHECK(xs_syntax_parse(&source, 36, &diagnostics, &tree));
    CHECK(!xs_source_expand_include_macros(&tree, &diagnostics, &expanded));
    CHECK(xs_diagnostics_has_error(&diagnostics));
    xs_syntax_tree_free(&tree);
    xs_diagnostics_free(&diagnostics);
}

static void test_source_include_ignores_comments_and_strings(void)
{
    const char *text = "// include!(\"missing.xs\");\n"
                       "incomplete fn Main();\n"
                       "fn Text() { value: Str = \"include!(\\\"missing.xs\\\")\"; }\n";
    XsSource source = {.path = "IgnoredInclude.xs", .text = text, .length = strlen(text)};
    XsDiagnostics diagnostics;
    XsIncludedSource expanded;
    XsSyntaxTree tree;
    xs_diagnostics_init(&diagnostics);
    CHECK(xs_syntax_parse(&source, 37, &diagnostics, &tree));
    CHECK(xs_source_expand_include_macros(&tree, &diagnostics, &expanded));
    CHECK(expanded.length == strlen(text));
    xs_syntax_tree_free(&tree);
    xs_included_source_free(&expanded);
    xs_diagnostics_free(&diagnostics);
}

int main(void)
{
    test_source_include_macro();
    test_source_include_rejects_nonlocal_path();
    test_source_include_ignores_comments_and_strings();
    return failures == 0 ? 0 : 1;
}
