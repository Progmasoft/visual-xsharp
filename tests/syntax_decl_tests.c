/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 */

#include "xs/diagnostic.h"
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

static size_t count_kind_with_flag(const XsSyntaxNode *node, XsSyntaxKind kind, uint32_t flag)
{
    if(node == nullptr)
        return 0;
    size_t count = node->kind == kind && (node->flags & flag) != 0;
    for(size_t index = 0; index < node->child_count; ++index)
        count += count_kind_with_flag(node->children[index], kind, flag);
    return count;
}

static void test_top_level_variable_declaration_structure(void)
{
    const char *text = "data User { name: Str }\n"
                       "user: User = { name: \"Alpha\" };\n"
                       "public static Pi: Float = 3.141592653589793;\n";
    XsSource source = {.path = "TopLevelData.vxs", .text = text, .length = strlen(text)};
    XsDiagnostics diagnostics;
    XsSyntaxTree tree;
    xs_diagnostics_init(&diagnostics);
    CHECK(xs_syntax_parse(&source, 19, &diagnostics, &tree));
    CHECK(count_kind(tree.root, XS_SYNTAX_DECL_VARIABLE) == 2);
    CHECK(xs_syntax_find_first(tree.root, XS_SYNTAX_EXPR_OBJECT_LITERAL) != nullptr);
    CHECK(xs_syntax_find_first(tree.root, XS_SYNTAX_OBJECT_FIELD) != nullptr);
    xs_syntax_tree_free(&tree);
    xs_diagnostics_free(&diagnostics);

    const char *type_first = "public class Person { public Str Name { getter; setter; } }\n";
    source = (XsSource){.path = "TypeFirstPropertyInvalid.vxs", .text = type_first, .length = strlen(type_first)};
    xs_diagnostics_init(&diagnostics);
    CHECK(!xs_syntax_parse(&source, 67, &diagnostics, &tree));
    CHECK(xs_diagnostics_has_error(&diagnostics));
    xs_syntax_tree_free(&tree);
    xs_diagnostics_free(&diagnostics);
}

static void test_inferred_variable_declaration_structure(void)
{
    const char *text = "fn Main() { age := 26; val name := \"XS\"; const enabled := true; }\n";
    XsSource source = {.path = "InferredVariables.vxs", .text = text, .length = strlen(text)};
    XsDiagnostics diagnostics;
    XsSyntaxTree tree;
    xs_diagnostics_init(&diagnostics);
    CHECK(xs_syntax_parse(&source, 20, &diagnostics, &tree));
    CHECK(count_kind(tree.root, XS_SYNTAX_DECL_VARIABLE) == 3);
    const XsSyntaxNode *declaration = xs_syntax_find_first(tree.root, XS_SYNTAX_DECL_VARIABLE);
    CHECK(declaration != nullptr);
    CHECK((declaration->flags & XS_SYNTAX_FLAG_INFERRED_TYPE) != 0);
    CHECK(declaration->child_count == 2);
    xs_syntax_tree_free(&tree);
    xs_diagnostics_free(&diagnostics);
}

static void test_data_callables_and_overloads(void)
{
    const char *valid = "data User { name: Str; User(name: Str) {} User(name: Str, age: Int) {} "
                        "fn Get(value: Int) {} fn Get(value: Str) {} fn operator +(right: User) -> User {} }\n";
    XsSource source = {.path = "DataOverloads.vxs", .text = valid, .length = strlen(valid)};
    XsDiagnostics diagnostics;
    XsSyntaxTree tree;
    xs_diagnostics_init(&diagnostics);
    CHECK(xs_syntax_parse(&source, 26, &diagnostics, &tree));
    CHECK(count_kind(tree.root, XS_SYNTAX_DATA_FIELD) == 1);
    CHECK(count_kind(tree.root, XS_SYNTAX_CLASS_CONSTRUCTOR) == 2);
    CHECK(count_kind(tree.root, XS_SYNTAX_DECL_FUNCTION) == 3);
    CHECK(count_kind_with_flag(tree.root, XS_SYNTAX_CLASS_CONSTRUCTOR, XS_SYNTAX_FLAG_OVERLOAD) == 1);
    CHECK(count_kind_with_flag(tree.root, XS_SYNTAX_DECL_FUNCTION, XS_SYNTAX_FLAG_OVERLOAD) == 1);
    CHECK(count_kind_with_flag(tree.root, XS_SYNTAX_DECL_FUNCTION, XS_SYNTAX_FLAG_OPERATOR) == 1);
    xs_syntax_tree_free(&tree);
    xs_diagnostics_free(&diagnostics);

    const char *duplicate_constructor = "data User { User(name: Str) {} User(value: Str) {} }\n";
    source = (XsSource){
        .path = "DataConstructorDuplicate.vxs", .text = duplicate_constructor, .length = strlen(duplicate_constructor)};
    xs_diagnostics_init(&diagnostics);
    CHECK(!xs_syntax_parse(&source, 27, &diagnostics, &tree));
    CHECK(xs_diagnostics_has_error(&diagnostics));
    xs_syntax_tree_free(&tree);
    xs_diagnostics_free(&diagnostics);

    const char *duplicate_method = "data User { fn Get(value: Int) {} fn Get(other: Int) {} }\n";
    source = (XsSource){.path = "DataMethodDuplicate.vxs", .text = duplicate_method, .length = strlen(duplicate_method)};
    xs_diagnostics_init(&diagnostics);
    CHECK(!xs_syntax_parse(&source, 28, &diagnostics, &tree));
    CHECK(xs_diagnostics_has_error(&diagnostics));
    xs_syntax_tree_free(&tree);
    xs_diagnostics_free(&diagnostics);

    const char *data_inheritance = "data User : Person { name: Str; }\n";
    source = (XsSource){.path = "DataInheritance.vxs", .text = data_inheritance, .length = strlen(data_inheritance)};
    xs_diagnostics_init(&diagnostics);
    CHECK(xs_syntax_parse(&source, 29, &diagnostics, &tree));
    CHECK(!xs_diagnostics_has_error(&diagnostics));
    CHECK(count_kind(tree.root, XS_SYNTAX_BASE_SPECIFIER) == 1);
    xs_syntax_tree_free(&tree);
    xs_diagnostics_free(&diagnostics);
}

static void test_op_declaration_structure(void)
{
    const char *text = "op Add(left: Int, right: Int) -> Int { return left + right; }\n";
    XsSource source = {.path = "Ops.vxs", .text = text, .length = strlen(text)};
    XsDiagnostics diagnostics;
    XsSyntaxTree tree;
    xs_diagnostics_init(&diagnostics);
    CHECK(xs_syntax_parse(&source, 61, &diagnostics, &tree));
    CHECK(count_kind(tree.root, XS_SYNTAX_DECL_FUNCTION) == 1);
    CHECK(count_kind_with_flag(tree.root, XS_SYNTAX_DECL_FUNCTION, XS_SYNTAX_FLAG_REFERENTIAL_TRANSPARENT) == 1);
    CHECK(count_kind_with_flag(tree.root, XS_SYNTAX_DECL_FUNCTION, XS_SYNTAX_FLAG_OPERATOR) == 0);
    xs_syntax_tree_free(&tree);
    xs_diagnostics_free(&diagnostics);
}

static void test_class_constructor_rules(void)
{
    const char *valid = "class User { User(name: Str) {} }\n";
    XsSource source = {.path = "ConstructorValid.vxs", .text = valid, .length = strlen(valid)};
    XsDiagnostics diagnostics;
    XsSyntaxTree tree;
    xs_diagnostics_init(&diagnostics);
    CHECK(xs_syntax_parse(&source, 31, &diagnostics, &tree));
    CHECK(count_kind(tree.root, XS_SYNTAX_CLASS_CONSTRUCTOR) == 1);
    xs_syntax_tree_free(&tree);
    xs_diagnostics_free(&diagnostics);

    const char *invalid_name = "class User { Person() {} }\n";
    source = (XsSource){.path = "ConstructorNameInvalid.vxs", .text = invalid_name, .length = strlen(invalid_name)};
    xs_diagnostics_init(&diagnostics);
    CHECK(!xs_syntax_parse(&source, 32, &diagnostics, &tree));
    CHECK(xs_diagnostics_has_error(&diagnostics));
    xs_syntax_tree_free(&tree);
    xs_diagnostics_free(&diagnostics);

    const char *overload = "class User { User() {} User(name: Str) {} }\n";
    source = (XsSource){.path = "ConstructorOverload.vxs", .text = overload, .length = strlen(overload)};
    xs_diagnostics_init(&diagnostics);
    CHECK(xs_syntax_parse(&source, 33, &diagnostics, &tree));
    CHECK(count_kind(tree.root, XS_SYNTAX_CLASS_CONSTRUCTOR) == 2);
    CHECK(count_kind_with_flag(tree.root, XS_SYNTAX_CLASS_CONSTRUCTOR, XS_SYNTAX_FLAG_OVERLOAD) == 1);
    xs_syntax_tree_free(&tree);
    xs_diagnostics_free(&diagnostics);

    const char *duplicate = "class User { User(name: Str) {} User(value: Str) {} }\n";
    source = (XsSource){.path = "ConstructorDuplicate.vxs", .text = duplicate, .length = strlen(duplicate)};
    xs_diagnostics_init(&diagnostics);
    CHECK(!xs_syntax_parse(&source, 64, &diagnostics, &tree));
    CHECK(xs_diagnostics_has_error(&diagnostics));
    xs_syntax_tree_free(&tree);
    xs_diagnostics_free(&diagnostics);
}

static void test_class_property_accessors(void)
{
    const char *text = "class User { private _age: Int; name: Str { getter; setter; } "
                       "age: Int { getter { return self._age; } setter { self._age = value; } } }\n";
    XsSource source = {.path = "ClassProperties.vxs", .text = text, .length = strlen(text)};
    XsDiagnostics diagnostics;
    XsSyntaxTree tree;
    xs_diagnostics_init(&diagnostics);
    CHECK(xs_syntax_parse(&source, 65, &diagnostics, &tree));
    CHECK(count_kind(tree.root, XS_SYNTAX_CLASS_FIELD) == 3);
    CHECK(count_kind(tree.root, XS_SYNTAX_PROPERTY_ACCESSOR) == 4);
    xs_syntax_tree_free(&tree);
    xs_diagnostics_free(&diagnostics);

    const char *property_visibility = "public class Person { public Name: Str { getter; setter; } "
                                      "public Age: Int { getter; setter; } }\n";
    source =
        (XsSource){.path = "PropertyVisibility.vxs", .text = property_visibility, .length = strlen(property_visibility)};
    xs_diagnostics_init(&diagnostics);
    CHECK(xs_syntax_parse(&source, 66, &diagnostics, &tree));
    CHECK(count_kind(tree.root, XS_SYNTAX_DECL_CLASS) == 1);
    CHECK(count_kind(tree.root, XS_SYNTAX_CLASS_FIELD) == 2);
    CHECK(count_kind(tree.root, XS_SYNTAX_PROPERTY_ACCESSOR) == 4);
    xs_syntax_tree_free(&tree);
    xs_diagnostics_free(&diagnostics);
}

static void test_interface_member_rules(void)
{
    const char *valid = "interface Runnable { fn Run(); fn Close(); }\n";
    XsSource source = {.path = "InterfaceValid.vxs", .text = valid, .length = strlen(valid)};
    XsDiagnostics diagnostics;
    XsSyntaxTree tree;
    xs_diagnostics_init(&diagnostics);
    CHECK(xs_syntax_parse(&source, 34, &diagnostics, &tree));
    CHECK(count_kind(tree.root, XS_SYNTAX_DECL_FUNCTION) == 2);
    xs_syntax_tree_free(&tree);
    xs_diagnostics_free(&diagnostics);

    const char *field = "interface Runnable { name: Str; }\n";
    source = (XsSource){.path = "InterfaceFieldInvalid.vxs", .text = field, .length = strlen(field)};
    xs_diagnostics_init(&diagnostics);
    CHECK(!xs_syntax_parse(&source, 35, &diagnostics, &tree));
    CHECK(xs_diagnostics_has_error(&diagnostics));
    xs_syntax_tree_free(&tree);
    xs_diagnostics_free(&diagnostics);

    const char *body = "interface Runnable { fn Run() {} }\n";
    source = (XsSource){.path = "InterfaceBodyInvalid.vxs", .text = body, .length = strlen(body)};
    xs_diagnostics_init(&diagnostics);
    CHECK(!xs_syntax_parse(&source, 36, &diagnostics, &tree));
    CHECK(xs_diagnostics_has_error(&diagnostics));
    xs_syntax_tree_free(&tree);
    xs_diagnostics_free(&diagnostics);
}

static void test_class_inheritance_rules(void)
{
    const char *valid = "class Animal {}\n"
                        "interface Runnable { fn Run(); }\n"
                        "interface Closeable { fn Close(); }\n"
                        "class Program : Animal, Runnable, Closeable { }\n";
    XsSource source = {.path = "ClassInheritanceValid.vxs", .text = valid, .length = strlen(valid)};
    XsDiagnostics diagnostics;
    XsSyntaxTree tree;
    xs_diagnostics_init(&diagnostics);
    CHECK(xs_syntax_parse(&source, 45, &diagnostics, &tree));
    CHECK(count_kind(tree.root, XS_SYNTAX_TYPE_NAMED) == 3);
    xs_syntax_tree_free(&tree);
    xs_diagnostics_free(&diagnostics);

    const char *interface_base = "interface Runnable : Base { fn Run(); }\n";
    source = (XsSource){.path = "InterfaceBaseValid.vxs", .text = interface_base, .length = strlen(interface_base)};
    xs_diagnostics_init(&diagnostics);
    CHECK(xs_syntax_parse(&source, 46, &diagnostics, &tree));
    CHECK(count_kind(tree.root, XS_SYNTAX_TYPE_NAMED) == 1);
    xs_syntax_tree_free(&tree);
    xs_diagnostics_free(&diagnostics);

    const char *old_extends = "class Program { extends Animal; }\n";
    source = (XsSource){.path = "OldExtendsInvalid.vxs", .text = old_extends, .length = strlen(old_extends)};
    xs_diagnostics_init(&diagnostics);
    CHECK(!xs_syntax_parse(&source, 47, &diagnostics, &tree));
    CHECK(xs_diagnostics_has_error(&diagnostics));
    xs_syntax_tree_free(&tree);
    xs_diagnostics_free(&diagnostics);

    const char *old_implements = "class Program { implements Runnable; }\n";
    source = (XsSource){.path = "OldImplementsInvalid.vxs", .text = old_implements, .length = strlen(old_implements)};
    xs_diagnostics_init(&diagnostics);
    CHECK(!xs_syntax_parse(&source, 48, &diagnostics, &tree));
    CHECK(xs_diagnostics_has_error(&diagnostics));
    xs_syntax_tree_free(&tree);
    xs_diagnostics_free(&diagnostics);
}

static void test_incomplete_function_rules(void)
{
    const char *valid = "incomplete class Animal { incomplete fn Speak(); }\n";
    XsSource source = {.path = "IncompleteValid.vxs", .text = valid, .length = strlen(valid)};
    XsDiagnostics diagnostics;
    XsSyntaxTree tree;
    xs_diagnostics_init(&diagnostics);
    CHECK(xs_syntax_parse(&source, 37, &diagnostics, &tree));
    const XsSyntaxNode *function = xs_syntax_find_first(tree.root, XS_SYNTAX_DECL_FUNCTION);
    CHECK(function != nullptr);
    CHECK(function == nullptr || (function->flags & XS_SYNTAX_FLAG_INCOMPLETE) != 0);
    xs_syntax_tree_free(&tree);
    xs_diagnostics_free(&diagnostics);

    const char *missing_modifier = "class Animal { fn Speak(); }\n";
    source = (XsSource){
        .path = "IncompleteMissingModifier.vxs", .text = missing_modifier, .length = strlen(missing_modifier)};
    xs_diagnostics_init(&diagnostics);
    CHECK(!xs_syntax_parse(&source, 38, &diagnostics, &tree));
    CHECK(xs_diagnostics_has_error(&diagnostics));
    xs_syntax_tree_free(&tree);
    xs_diagnostics_free(&diagnostics);

    const char *body = "class Animal { incomplete fn Speak() {} }\n";
    source = (XsSource){.path = "IncompleteBodyInvalid.vxs", .text = body, .length = strlen(body)};
    xs_diagnostics_init(&diagnostics);
    CHECK(!xs_syntax_parse(&source, 39, &diagnostics, &tree));
    CHECK(xs_diagnostics_has_error(&diagnostics));
    xs_syntax_tree_free(&tree);
    xs_diagnostics_free(&diagnostics);
}

static void test_enum_payload_rules(void)
{
    const char *valid = "enum data Token { Identifier: Str, Integer: Int, Plus, }\n";
    XsSource source = {.path = "DataEnumValid.vxs", .text = valid, .length = strlen(valid)};
    XsDiagnostics diagnostics;
    XsSyntaxTree tree;
    xs_diagnostics_init(&diagnostics);
    CHECK(xs_syntax_parse(&source, 40, &diagnostics, &tree));
    const XsSyntaxNode *declaration = xs_syntax_find_first(tree.root, XS_SYNTAX_DECL_ENUM);
    CHECK(declaration != nullptr);
    CHECK(declaration == nullptr || (declaration->flags & XS_SYNTAX_FLAG_DATA_ENUM) != 0);
    CHECK(count_kind(tree.root, XS_SYNTAX_ENUM_VARIANT) == 3);
    xs_syntax_tree_free(&tree);
    xs_diagnostics_free(&diagnostics);

    const char *typed_regular = "enum Color { Red: Int, }\n";
    source = (XsSource){.path = "RegularEnumPayloadInvalid.vxs", .text = typed_regular, .length = strlen(typed_regular)};
    xs_diagnostics_init(&diagnostics);
    CHECK(!xs_syntax_parse(&source, 41, &diagnostics, &tree));
    CHECK(xs_diagnostics_has_error(&diagnostics));
    xs_syntax_tree_free(&tree);
    xs_diagnostics_free(&diagnostics);

    const char *empty_data = "enum data Empty { }\n";
    source = (XsSource){.path = "DataEnumEmptyInvalid.vxs", .text = empty_data, .length = strlen(empty_data)};
    xs_diagnostics_init(&diagnostics);
    CHECK(!xs_syntax_parse(&source, 42, &diagnostics, &tree));
    CHECK(xs_diagnostics_has_error(&diagnostics));
    xs_syntax_tree_free(&tree);
    xs_diagnostics_free(&diagnostics);

    const char *tuple_payload = "enum data Token { Position: (Int, Int), }\n";
    source = (XsSource){.path = "DataEnumTupleInvalid.vxs", .text = tuple_payload, .length = strlen(tuple_payload)};
    xs_diagnostics_init(&diagnostics);
    CHECK(!xs_syntax_parse(&source, 43, &diagnostics, &tree));
    CHECK(xs_diagnostics_has_error(&diagnostics));
    xs_syntax_tree_free(&tree);
    xs_diagnostics_free(&diagnostics);

    const char *overloaded = "enum data Value { Number: Int, Number: Long, Text: Str, }\n";
    source = (XsSource){.path = "DataEnumOverload.vxs", .text = overloaded, .length = strlen(overloaded)};
    xs_diagnostics_init(&diagnostics);
    CHECK(xs_syntax_parse(&source, 44, &diagnostics, &tree));
    CHECK(count_kind_with_flag(tree.root, XS_SYNTAX_ENUM_VARIANT, XS_SYNTAX_FLAG_ENUM_VARIANT_OVERLOAD) == 1);
    xs_syntax_tree_free(&tree);
    xs_diagnostics_free(&diagnostics);

    const char *duplicate_regular = "enum Color { Red, Red, }\n";
    source =
        (XsSource){.path = "RegularEnumDuplicate.vxs", .text = duplicate_regular, .length = strlen(duplicate_regular)};
    xs_diagnostics_init(&diagnostics);
    CHECK(!xs_syntax_parse(&source, 45, &diagnostics, &tree));
    CHECK(xs_diagnostics_has_error(&diagnostics));
    xs_syntax_tree_free(&tree);
    xs_diagnostics_free(&diagnostics);

    const char *duplicate_payload = "enum data Value { Number: Int, Number: Int, }\n";
    source = (XsSource){
        .path = "DataEnumDuplicatePayload.vxs", .text = duplicate_payload, .length = strlen(duplicate_payload)};
    xs_diagnostics_init(&diagnostics);
    CHECK(!xs_syntax_parse(&source, 46, &diagnostics, &tree));
    CHECK(xs_diagnostics_has_error(&diagnostics));
    xs_syntax_tree_free(&tree);
    xs_diagnostics_free(&diagnostics);
}

static void test_generic_constraint_structure(void)
{
    const char *text = "interface Runnable { fn Run(); }\n"
                       "interface Printable { fn Print(); }\n"
                       "fn Execute<T: Runnable, Printable, U: Runnable>(value: T, worker: U) {}\n";
    XsSource source = {.path = "GenericConstraints.vxs", .text = text, .length = strlen(text)};
    XsDiagnostics diagnostics;
    XsSyntaxTree tree;
    xs_diagnostics_init(&diagnostics);
    CHECK(xs_syntax_parse(&source, 44, &diagnostics, &tree));
    CHECK(count_kind(tree.root, XS_SYNTAX_GENERIC_PARAMETER) == 2);
    CHECK(count_kind(tree.root, XS_SYNTAX_TYPE_NAMED) >= 4);
    xs_syntax_tree_free(&tree);
    xs_diagnostics_free(&diagnostics);
}

static void test_extern_c_function_structure(void)
{
    const char *text = "import cffi;\n"
                       "#[repr(C)]\n"
                       "extern \"C\" {\n"
                       "  #[LinkName(\"puts\")]\n"
                       "  fn puts(text: std::cffi::CStr) -> Int;\n"
                       "  #[ThreadLocal]\n"
                       "  static errno: Int;\n"
                       "}\n";
    XsSource source = {.path = "ExternC.vxs", .text = text, .length = strlen(text)};
    XsDiagnostics diagnostics;
    XsSyntaxTree tree;
    xs_diagnostics_init(&diagnostics);
    CHECK(xs_syntax_parse(&source, 47, &diagnostics, &tree));
    const XsSyntaxNode *function = xs_syntax_find_first(tree.root, XS_SYNTAX_DECL_FUNCTION);
    CHECK(xs_syntax_find_first(tree.root, XS_SYNTAX_DECL_EXTERN_BLOCK) != nullptr);
    CHECK(function != nullptr);
    CHECK(function == nullptr || (function->flags & XS_SYNTAX_FLAG_EXTERN) != 0);
    CHECK(function == nullptr || (function->flags & XS_SYNTAX_FLAG_INCOMPLETE) != 0);
    CHECK(function == nullptr || xs_syntax_find_first(function, XS_SYNTAX_ATTRIBUTE_LIST) != nullptr);
    CHECK(function == nullptr || count_kind(function, XS_SYNTAX_TOKEN) == 1);
    CHECK(count_kind_with_flag(tree.root, XS_SYNTAX_DECL_VARIABLE, XS_SYNTAX_FLAG_EXTERN) == 1);
    CHECK(count_kind_with_flag(tree.root, XS_SYNTAX_DECL_VARIABLE, XS_SYNTAX_FLAG_STATIC) == 1);
    xs_syntax_tree_free(&tree);
    xs_diagnostics_free(&diagnostics);

    const char *body = "extern \"C\" { fn puts(text: std::cffi::CStr) -> Int {} }\n";
    source = (XsSource){.path = "ExternCBodyInvalid.vxs", .text = body, .length = strlen(body)};
    xs_diagnostics_init(&diagnostics);
    CHECK(!xs_syntax_parse(&source, 48, &diagnostics, &tree));
    CHECK(xs_diagnostics_has_error(&diagnostics));
    xs_syntax_tree_free(&tree);
    xs_diagnostics_free(&diagnostics);

    const char *initializer = "extern \"C\" { static errno: Int = 0; }\n";
    source =
        (XsSource){.path = "ExternCStaticInitializerInvalid.vxs", .text = initializer, .length = strlen(initializer)};
    xs_diagnostics_init(&diagnostics);
    CHECK(!xs_syntax_parse(&source, 49, &diagnostics, &tree));
    CHECK(xs_diagnostics_has_error(&diagnostics));
    xs_syntax_tree_free(&tree);
    xs_diagnostics_free(&diagnostics);
}

static void test_using_declaration_structure(void)
{
    const char *text = "using namespace Math::Advanced;\n"
                       "using namespace Math::Advanced::*;\n"
                       "using Math::Add;\n"
                       "using Sum = Math::Add;\n";
    XsSource source = {.path = "Using.vxs", .text = text, .length = strlen(text)};
    XsDiagnostics diagnostics;
    XsSyntaxTree tree;
    xs_diagnostics_init(&diagnostics);
    CHECK(xs_syntax_parse(&source, 50, &diagnostics, &tree));
    CHECK(count_kind(tree.root, XS_SYNTAX_DECL_IMPORT) == 4);
    CHECK(count_kind_with_flag(tree.root, XS_SYNTAX_DECL_IMPORT, XS_SYNTAX_FLAG_USING) == 4);
    CHECK(count_kind_with_flag(tree.root, XS_SYNTAX_DECL_IMPORT, XS_SYNTAX_FLAG_WILDCARD) == 2);
    CHECK(count_kind_with_flag(tree.root, XS_SYNTAX_DECL_IMPORT, XS_SYNTAX_FLAG_USING_ALIAS) == 1);
    xs_syntax_tree_free(&tree);
    xs_diagnostics_free(&diagnostics);

    const char *glob = "using Math::*;\n";
    source = (XsSource){.path = "UsingGlobInvalid.vxs", .text = glob, .length = strlen(glob)};
    xs_diagnostics_init(&diagnostics);
    CHECK(!xs_syntax_parse(&source, 51, &diagnostics, &tree));
    CHECK(xs_diagnostics_has_error(&diagnostics));
    xs_syntax_tree_free(&tree);
    xs_diagnostics_free(&diagnostics);
}

static void test_namespace_scoping_forms(void)
{
    const char *blocks = "namespace first { fn one() {} }\n"
                         "namespace second { namespace nested { fn two() {} } }\n";
    XsSource source = {.path = "BlockNamespaces.vxs", .text = blocks, .length = strlen(blocks)};
    XsDiagnostics diagnostics;
    XsSyntaxTree tree;
    xs_diagnostics_init(&diagnostics);
    CHECK(xs_syntax_parse(&source, 52, &diagnostics, &tree));
    CHECK(count_kind(tree.root, XS_SYNTAX_DECL_NAMESPACE) == 3);
    CHECK(count_kind_with_flag(tree.root, XS_SYNTAX_DECL_NAMESPACE, XS_SYNTAX_FLAG_BLOCK_NAMESPACE) == 3);
    xs_syntax_tree_free(&tree);
    xs_diagnostics_free(&diagnostics);

    const char *mixed = "namespace scoped;\nnamespace blocked { namespace nested { fn value() {} } }\n";
    source = (XsSource){.path = "CombinedNamespaces.vxs", .text = mixed, .length = strlen(mixed)};
    xs_diagnostics_init(&diagnostics);
    CHECK(xs_syntax_parse(&source, 53, &diagnostics, &tree));
    CHECK(count_kind_with_flag(tree.root, XS_SYNTAX_DECL_NAMESPACE, XS_SYNTAX_FLAG_BLOCK_NAMESPACE) == 2);
    xs_syntax_tree_free(&tree);
    xs_diagnostics_free(&diagnostics);

    const char *duplicate = "namespace first;\nnamespace second;\n";
    source = (XsSource){.path = "DuplicateScopedNamespace.vxs", .text = duplicate, .length = strlen(duplicate)};
    xs_diagnostics_init(&diagnostics);
    CHECK(!xs_syntax_parse(&source, 54, &diagnostics, &tree));
    CHECK(xs_diagnostics_has_error(&diagnostics));
    xs_syntax_tree_free(&tree);
    xs_diagnostics_free(&diagnostics);
}

static void test_dispatch_and_multiple_inheritance_modifiers(void)
{
    const char *valid = "class Root { public virtual fn Run() -> Long { return 1; } }\n"
                        "class Left : public virtual Root { public virtual override fn Run() -> Long { return 2; } }\n"
                        "class Right { public virtual fn Close() {} }\n"
                        "interface Printable { fn Print(); }\n"
                        "sealed class Leaf : public Left, protected Right, Printable { "
                        "public sealed override fn Run() -> Long { return 3; } }\n";
    XsSource source = {.path = "DispatchModifiers.vxs", .text = valid, .length = strlen(valid)};
    XsDiagnostics diagnostics;
    XsSyntaxTree tree;
    xs_diagnostics_init(&diagnostics);
    CHECK(xs_syntax_parse(&source, 65, &diagnostics, &tree));
    CHECK(count_kind_with_flag(tree.root, XS_SYNTAX_DECL_FUNCTION, XS_SYNTAX_FLAG_VIRTUAL) == 3);
    CHECK(count_kind_with_flag(tree.root, XS_SYNTAX_DECL_FUNCTION, XS_SYNTAX_FLAG_OVERRIDE) == 2);
    CHECK(count_kind_with_flag(tree.root, XS_SYNTAX_DECL_FUNCTION, XS_SYNTAX_FLAG_SEALED) == 1);
    CHECK(count_kind_with_flag(tree.root, XS_SYNTAX_DECL_CLASS, XS_SYNTAX_FLAG_SEALED) == 1);
    xs_syntax_tree_free(&tree);
    xs_diagnostics_free(&diagnostics);

    const char *invalid[] = {
        "virtual fn TopLevel() {}\n",           "class Bad { static virtual fn Run() {} }\n",
        "class Bad { sealed fn Run() {} }\n",   "sealed incomplete class Bad {}\n",
        "sealed interface Bad { fn Run(); }\n", "class Bad { virtual value: Long; }\n",
    };
    for(size_t index = 0; index < sizeof(invalid) / sizeof(invalid[0]); ++index)
    {
        source =
            (XsSource){.path = "InvalidDispatchModifier.vxs", .text = invalid[index], .length = strlen(invalid[index])};
        xs_diagnostics_init(&diagnostics);
        CHECK(!xs_syntax_parse(&source, 66 + index, &diagnostics, &tree));
        CHECK(xs_diagnostics_has_error(&diagnostics));
        xs_syntax_tree_free(&tree);
        xs_diagnostics_free(&diagnostics);
    }
}

static void test_builtin_collection_type_syntax(void)
{
    const char *text = "fn Collections(values: [Int], fixed: [Long; 4], ages: [String: Optional<Int>]) {"
                       "numbers: [Int] = [1, 2, 3]; unique: [Int] = {1, 2, 3}; "
                       "scores: [Str: Int] = [\"Alpha\": 7, \"Beta\": 9]; }\n";
    XsSource source = {.path = "BuiltinCollections.vxs", .text = text, .length = strlen(text)};
    XsDiagnostics diagnostics;
    XsSyntaxTree tree;
    xs_diagnostics_init(&diagnostics);
    CHECK(xs_syntax_parse(&source, 90, &diagnostics, &tree));
    CHECK(count_kind(tree.root, XS_SYNTAX_TYPE_ARRAY) == 3);
    CHECK(count_kind(tree.root, XS_SYNTAX_TYPE_FIXED_ARRAY) == 1);
    CHECK(count_kind(tree.root, XS_SYNTAX_TYPE_MAP) == 2);
    CHECK(count_kind(tree.root, XS_SYNTAX_EXPR_ARRAY_LITERAL) == 1);
    CHECK(count_kind(tree.root, XS_SYNTAX_EXPR_SET_LITERAL) == 1);
    CHECK(count_kind(tree.root, XS_SYNTAX_EXPR_MAP_LITERAL) == 1);
    CHECK(count_kind(tree.root, XS_SYNTAX_MAP_ENTRY) == 2);
    xs_syntax_tree_free(&tree);
    xs_diagnostics_free(&diagnostics);
}

int main(void)
{
    test_top_level_variable_declaration_structure();
    test_inferred_variable_declaration_structure();
    test_data_callables_and_overloads();
    test_op_declaration_structure();
    test_class_constructor_rules();
    test_class_property_accessors();
    test_interface_member_rules();
    test_class_inheritance_rules();
    test_incomplete_function_rules();
    test_enum_payload_rules();
    test_generic_constraint_structure();
    test_extern_c_function_structure();
    test_using_declaration_structure();
    test_namespace_scoping_forms();
    test_dispatch_and_multiple_inheritance_modifiers();
    test_builtin_collection_type_syntax();
    return failures == 0 ? 0 : 1;
}
