/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 */

#include "xs/hir/inheritance.h"
#include "xs/hir/type_resolution.h"
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

static bool validate(const char *text)
{
    XsSource source = {.path = "Inheritance.vxs", .module_name = "App", .text = text, .length = strlen(text)};
    XsDiagnostics diagnostics;
    XsSyntaxTree tree;
    XsHirSymbolTable symbols;
    XsHirImportScope import;
    xs_diagnostics_init(&diagnostics);
    xs_hir_symbol_table_init(&symbols);
    xs_hir_import_scope_init(&import);
    bool success = xs_syntax_parse(&source, 201, &diagnostics, &tree);
    if(success)
        success = xs_hir_collect_symbols(&tree, &symbols, &diagnostics);
    if(success)
        success = xs_hir_resolve_imports(&tree, &symbols, &import, &diagnostics);
    if(success)
        success = xs_hir_resolve_types(&tree, &symbols, &import, &diagnostics);
    if(success)
        success = xs_hir_validate_inheritance(&tree, &symbols, &import, &diagnostics);
    xs_hir_import_scope_free(&import);
    xs_hir_symbol_table_free(&symbols);
    xs_syntax_tree_free(&tree);
    xs_diagnostics_free(&diagnostics);
    return success;
}

static void test_multiple_and_virtual_inheritance(void)
{
    const char *text = "class Root { virtual fn tick() {} }\n"
                       "class Left : public virtual Root {}\n"
                       "class Right : public virtual Root {}\n"
                       "interface Printable { fn print(); }\n"
                       "class Leaf : public Left, public Right, public Printable { override fn tick() {} }\n";
    CHECK(validate(text));
}

static void test_each_nominal_category_supports_multiple_same_kind_bases(void)
{
    const char *text = "interface Named { fn name(); } interface Tagged { fn tag(); }\n"
                       "interface Combined : Named, Tagged { fn value(); }\n"
                       "data Positioned { x: Long } data Identified { id: Long }\n"
                       "data Record : Positioned, Identified { value: Long }\n"
                       "enum data Numbered { Number: Long } enum data Textual { Text: Str }\n"
                       "enum data Event : Numbered, Textual { Ready: Long }\n";
    CHECK(validate(text));
}

static void test_standard_enum_data_family_inheritance(void)
{
    CHECK(validate("enum data Option : Optional { Some: Str }\n"
                   "enum data MyResult : Result { Success: Int }\n"));
    CHECK(!validate("class Invalid : Result {}\n"));
    CHECK(!validate("data Invalid : Optional {}\n"));
}

static void test_one_override_can_cover_multiple_base_slots(void)
{
    const char *text = "class Left { virtual fn run(value: Long) {} }\n"
                       "class Right { virtual fn run(value: Long) {} }\n"
                       "class Both : public Left, public Right { virtual override fn run(value: Long) {} }\n";
    CHECK(validate(text));
}

static void test_invalid_inheritance_graphs(void)
{
    const char *invalid[] = {
        "class Base {} class Derived : Base, Base {}\n",
        "class Left : Right {} class Right : Left {}\n",
        "sealed class Base {} class Derived : public Base {}\n",
        "class Base {} interface Bad : Base { fn run(); }\n",
        "data Value {} class Bad : Value {}\n",
        "interface Named { fn name(); } data Bad : Named {}\n",
        "data Value {} interface Bad : Value { fn run(); }\n",
        "data Value {} enum data Bad : Value { Ready: Long }\n",
        "enum data Value { Ready: Long } data Bad : Value {}\n",
        "enum Plain : Value { Ready }\n",
        "class Base { fn run() {} } class Derived : Base { override fn run() {} }\n",
        "class Base { virtual fn run() {} } class Mid : Base { sealed override fn run() {} } "
        "class Leaf : Mid { override fn run() {} }\n",
    };
    for(size_t index = 0; index < sizeof(invalid) / sizeof(invalid[0]); ++index)
        CHECK(!validate(invalid[index]));
}

int main(void)
{
    test_multiple_and_virtual_inheritance();
    test_each_nominal_category_supports_multiple_same_kind_bases();
    test_standard_enum_data_family_inheritance();
    test_one_override_can_cover_multiple_base_slots();
    test_invalid_inheritance_graphs();
    return failures == 0 ? 0 : 1;
}
