/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 */

#include "Visual/XSharp/hir/cffi.h"
#include "syntax_helpers.h"

#include <stdlib.h>
#include <string.h>

static bool text_equals(XsText text, const char *expected)
{
    size_t length = strlen(expected);
    return text.length == length && memcmp(text.data, expected, length) == 0;
}

static bool path_text_equals(const XsSyntaxNode *path, const char *expected)
{
    char *text = xs_hir_path_to_string(path);
    bool same = text != nullptr && strcmp(text, expected) == 0;
    free(text);
    return same;
}

static char *attribute_name(const XsSyntaxNode *attribute)
{
    const XsSyntaxNode *path = xs_hir_first_child_kind(attribute, XS_SYNTAX_PATH);
    char *text = xs_hir_path_to_string(path);
    if(text == nullptr)
        return nullptr;
    char *last_dot = strrchr(text, '.');
    if(last_dot == nullptr)
        return text;
    char *name = xs_hir_copy_cstr(last_dot + 1);
    free(text);
    return name;
}

static bool error_at(XsDiagnostics *diagnostics, const XsSyntaxNode *node, const char *message)
{
    xs_diagnostics_add(diagnostics, XS_DIAGNOSTIC_ERROR, xs_hir_node_span(node), message);
    return false;
}

static size_t attribute_argument_count(const XsSyntaxNode *attribute)
{
    size_t count = 0;
    for(size_t i = 0; i < attribute->child_count; ++i)
    {
        if(attribute->children[i]->kind != XS_SYNTAX_PATH)
            ++count;
    }
    return count;
}

static const XsSyntaxNode *attribute_first_argument(const XsSyntaxNode *attribute)
{
    for(size_t i = 0; i < attribute->child_count; ++i)
    {
        if(attribute->children[i]->kind != XS_SYNTAX_PATH)
            return attribute->children[i];
    }
    return nullptr;
}

static bool attribute_has_no_arguments(const XsSyntaxNode *attribute)
{
    return attribute_argument_count(attribute) == 0;
}

static bool attribute_has_one_string(const XsSyntaxNode *attribute)
{
    const XsSyntaxNode *argument = attribute_first_argument(attribute);
    return attribute_argument_count(attribute) == 1 && argument != nullptr &&
           argument->kind == XS_SYNTAX_EXPR_LITERAL && argument->token_kind == XS_TOKEN_STRING;
}

static bool attribute_has_one_identifier(const XsSyntaxNode *attribute)
{
    const XsSyntaxNode *argument = attribute_first_argument(attribute);
    return attribute_argument_count(attribute) == 1 && argument != nullptr &&
           argument->kind == XS_SYNTAX_EXPR_IDENTIFIER;
}

static bool attribute_argument_is_c(const XsSyntaxNode *attribute)
{
    const XsSyntaxNode *argument = attribute_first_argument(attribute);
    return attribute_argument_count(attribute) == 1 && argument != nullptr &&
           argument->kind == XS_SYNTAX_EXPR_IDENTIFIER && text_equals(argument->text, "C");
}

static bool validate_repr_attribute(const XsSyntaxNode *attribute, XsDiagnostics *diagnostics)
{
    if(attribute_argument_is_c(attribute))
        return true;
    return error_at(diagnostics, attribute, "C ABI extern blocks require #[repr(C)]");
}

static bool has_valid_repr_c(const XsSyntaxNode *node, XsDiagnostics *diagnostics)
{
    const XsSyntaxNode *attributes = xs_hir_first_child_kind(node, XS_SYNTAX_ATTRIBUTE_LIST);
    if(attributes == nullptr)
        return error_at(diagnostics, node, "C ABI extern blocks require #[repr(C)]");
    for(size_t i = 0; i < attributes->child_count; ++i)
    {
        const XsSyntaxNode *attribute = attributes->children[i];
        const XsSyntaxNode *path = xs_hir_first_child_kind(attribute, XS_SYNTAX_PATH);
        if(path_text_equals(path, "repr") || path_text_equals(path, "Repr"))
            return validate_repr_attribute(attribute, diagnostics);
    }
    return error_at(diagnostics, node, "C ABI extern blocks require #[repr(C)]");
}

typedef enum
{
    XS_CFFI_ATTRIBUTE_BLOCK,
    XS_CFFI_ATTRIBUTE_FUNCTION,
    XS_CFFI_ATTRIBUTE_STATIC,
} XsCffiAttributeScope;

static bool name_is_one_of(const char *name, const char *const *values, size_t count)
{
    for(size_t i = 0; i < count; ++i)
    {
        if(strcmp(name, values[i]) == 0)
            return true;
    }
    return false;
}

static bool validate_attribute_shape(const XsSyntaxNode *attribute, const char *name, XsCffiAttributeScope scope,
                                     XsDiagnostics *diagnostics)
{
    static const char *const string_attributes[] = {"Abi", "Header", "LinkLibrary", "LinkName", "ExportName"};
    static const char *const identifier_attributes[] = {"CallingConvention", "SymbolVisibility", "Ownership"};
    static const char *const marker_attributes[] = {"NoUnwind",
                                                    "MayUnwind",
                                                    "Variadic",
                                                    "Unsafe",
                                                    "Safe",
                                                    "ForeignThreadSafe",
                                                    "NoCallbackIntoRuntime",
                                                    "ThreadLocal",
                                                    "MayBlock",
                                                    "CancellationSafe",
                                                    "CancellationUnsafe"};
    if(strcmp(name, "repr") == 0 || strcmp(name, "Repr") == 0)
        return scope == XS_CFFI_ATTRIBUTE_BLOCK
                   ? validate_repr_attribute(attribute, diagnostics)
                   : error_at(diagnostics, attribute, "#[repr(C)] belongs on extern blocks");
    if(name_is_one_of(name, string_attributes, sizeof(string_attributes) / sizeof(string_attributes[0])))
        return attribute_has_one_string(attribute)
                   ? true
                   : error_at(diagnostics, attribute, "CFFI attribute expects one string");
    if(name_is_one_of(name, identifier_attributes, sizeof(identifier_attributes) / sizeof(identifier_attributes[0])))
        return attribute_has_one_identifier(attribute)
                   ? true
                   : error_at(diagnostics, attribute, "CFFI attribute expects one identifier argument");
    if(name_is_one_of(name, marker_attributes, sizeof(marker_attributes) / sizeof(marker_attributes[0])))
        return attribute_has_no_arguments(attribute)
                   ? true
                   : error_at(diagnostics, attribute, "CFFI marker attribute takes no arguments");
    return true;
}

static bool validate_attribute_scope(const XsSyntaxNode *attribute, const char *name, XsCffiAttributeScope scope,
                                     XsDiagnostics *diagnostics)
{
    if(scope == XS_CFFI_ATTRIBUTE_BLOCK)
        return true;
    if(strcmp(name, "LinkLibrary") == 0 || strcmp(name, "Header") == 0 || strcmp(name, "Abi") == 0 ||
       strcmp(name, "CallingConvention") == 0)
        return error_at(diagnostics, attribute, "this CFFI attribute belongs on an extern block");
    if(scope == XS_CFFI_ATTRIBUTE_STATIC &&
       (strcmp(name, "NoUnwind") == 0 || strcmp(name, "MayUnwind") == 0 || strcmp(name, "Variadic") == 0 ||
        strcmp(name, "ForeignThreadSafe") == 0 || strcmp(name, "NoCallbackIntoRuntime") == 0 ||
        strcmp(name, "MayBlock") == 0 || strcmp(name, "Ownership") == 0))
        return error_at(diagnostics, attribute, "this CFFI attribute belongs on an extern function");
    if(scope == XS_CFFI_ATTRIBUTE_FUNCTION && strcmp(name, "ThreadLocal") == 0)
        return error_at(diagnostics, attribute, "ThreadLocal belongs on an extern static declaration");
    return true;
}

static bool validate_attributes(const XsSyntaxNode *node, XsCffiAttributeScope scope, XsDiagnostics *diagnostics)
{
    bool success = true;
    const XsSyntaxNode *attributes = xs_hir_first_child_kind(node, XS_SYNTAX_ATTRIBUTE_LIST);
    if(attributes == nullptr)
        return true;
    for(size_t i = 0; i < attributes->child_count; ++i)
    {
        const XsSyntaxNode *attribute = attributes->children[i];
        char *name = attribute_name(attribute);
        if(name == nullptr)
        {
            success = error_at(diagnostics, attribute, "compiler ran out of memory while validating CFFI attributes") &&
                      success;
            continue;
        }
        success = validate_attribute_shape(attribute, name, scope, diagnostics) && success;
        success = validate_attribute_scope(attribute, name, scope, diagnostics) && success;
        free(name);
    }
    return success;
}

static XsText empty_text(void)
{
    return (XsText){0};
}

static XsText attribute_argument_text(const XsSyntaxNode *attribute)
{
    const XsSyntaxNode *argument = attribute_first_argument(attribute);
    return argument == nullptr ? empty_text() : argument->text;
}

static void read_common_boundary_attribute(const char *name, bool *unsafe_boundary, bool *safe_boundary)
{
    if(strcmp(name, "Unsafe") == 0)
        *unsafe_boundary = true;
    if(strcmp(name, "Safe") == 0)
        *safe_boundary = true;
}

static void read_block_attribute(const XsSyntaxNode *attribute, const char *name, XsHirCffiExternBlock *metadata)
{
    if(strcmp(name, "LinkLibrary") == 0)
        metadata->link_library = attribute_argument_text(attribute);
    else if(strcmp(name, "Header") == 0)
        metadata->header = attribute_argument_text(attribute);
    else if(strcmp(name, "CallingConvention") == 0)
        metadata->calling_convention = attribute_argument_text(attribute);
    else
        read_common_boundary_attribute(name, &metadata->unsafe_boundary, &metadata->safe_boundary);
}

static void read_function_attribute(const XsSyntaxNode *attribute, const char *name, XsHirCffiFunction *metadata)
{
    if(strcmp(name, "LinkName") == 0)
        metadata->link_name = attribute_argument_text(attribute);
    else if(strcmp(name, "ExportName") == 0)
        metadata->export_name = attribute_argument_text(attribute);
    else if(strcmp(name, "SymbolVisibility") == 0)
        metadata->symbol_visibility = attribute_argument_text(attribute);
    else if(strcmp(name, "Ownership") == 0)
        metadata->ownership = attribute_argument_text(attribute);
    else if(strcmp(name, "NoUnwind") == 0)
        metadata->no_unwind = true;
    else if(strcmp(name, "MayUnwind") == 0)
        metadata->may_unwind = true;
    else if(strcmp(name, "Variadic") == 0)
        metadata->variadic = true;
    else if(strcmp(name, "ForeignThreadSafe") == 0)
        metadata->foreign_thread_safe = true;
    else if(strcmp(name, "NoCallbackIntoRuntime") == 0)
        metadata->no_callback_into_runtime = true;
    else if(strcmp(name, "MayBlock") == 0)
        metadata->may_block = true;
    else if(strcmp(name, "CancellationSafe") == 0)
        metadata->cancellation_safe = true;
    else if(strcmp(name, "CancellationUnsafe") == 0)
        metadata->cancellation_unsafe = true;
    else
        read_common_boundary_attribute(name, &metadata->unsafe_boundary, &metadata->safe_boundary);
}

static void read_static_attribute(const XsSyntaxNode *attribute, const char *name, XsHirCffiStatic *metadata)
{
    if(strcmp(name, "LinkName") == 0)
        metadata->link_name = attribute_argument_text(attribute);
    else if(strcmp(name, "ThreadLocal") == 0)
        metadata->thread_local_storage = true;
    else
        read_common_boundary_attribute(name, &metadata->unsafe_boundary, &metadata->safe_boundary);
}

typedef void (*XsCffiReadAttribute)(const XsSyntaxNode *attribute, const char *name, void *metadata);

static void read_attribute_list(const XsSyntaxNode *node, XsCffiReadAttribute read_attribute, void *metadata)
{
    const XsSyntaxNode *attributes = xs_hir_first_child_kind(node, XS_SYNTAX_ATTRIBUTE_LIST);
    if(attributes == nullptr)
        return;
    for(size_t i = 0; i < attributes->child_count; ++i)
    {
        const XsSyntaxNode *attribute = attributes->children[i];
        char *name = attribute_name(attribute);
        if(name == nullptr)
            continue;
        read_attribute(attribute, name, metadata);
        free(name);
    }
}

static void read_block_attribute_bridge(const XsSyntaxNode *attribute, const char *name, void *metadata)
{
    read_block_attribute(attribute, name, metadata);
}

static void read_function_attribute_bridge(const XsSyntaxNode *attribute, const char *name, void *metadata)
{
    read_function_attribute(attribute, name, metadata);
}

static void read_static_attribute_bridge(const XsSyntaxNode *attribute, const char *name, void *metadata)
{
    read_static_attribute(attribute, name, metadata);
}

static XsText extern_abi_text(const XsSyntaxNode *node)
{
    const XsSyntaxNode *abi = xs_hir_first_child_kind(node, XS_SYNTAX_TOKEN);
    return abi == nullptr ? empty_text() : abi->text;
}

bool xs_hir_cffi_read_extern_block(const XsSyntaxNode *node, XsHirCffiExternBlock *metadata)
{
    if(node == nullptr || metadata == nullptr || node->kind != XS_SYNTAX_DECL_EXTERN_BLOCK)
        return false;
    *metadata = (XsHirCffiExternBlock){.abi = extern_abi_text(node)};
    read_attribute_list(node, read_block_attribute_bridge, metadata);
    return true;
}

bool xs_hir_cffi_read_function(const XsSyntaxNode *node, XsHirCffiFunction *metadata)
{
    if(node == nullptr || metadata == nullptr || node->kind != XS_SYNTAX_DECL_FUNCTION ||
       (node->flags & XS_SYNTAX_FLAG_EXTERN) == 0)
        return false;
    *metadata = (XsHirCffiFunction){.abi = extern_abi_text(node)};
    read_attribute_list(node, read_function_attribute_bridge, metadata);
    return true;
}

bool xs_hir_cffi_read_static(const XsSyntaxNode *node, XsHirCffiStatic *metadata)
{
    if(node == nullptr || metadata == nullptr || node->kind != XS_SYNTAX_DECL_VARIABLE ||
       (node->flags & XS_SYNTAX_FLAG_EXTERN) == 0)
        return false;
    *metadata = (XsHirCffiStatic){.abi = extern_abi_text(node)};
    read_attribute_list(node, read_static_attribute_bridge, metadata);
    return true;
}

static bool validate_extern_block(const XsSyntaxNode *node, XsDiagnostics *diagnostics)
{
    bool success = validate_attributes(node, XS_CFFI_ATTRIBUTE_BLOCK, diagnostics);
    const XsSyntaxNode *abi = xs_hir_first_child_kind(node, XS_SYNTAX_TOKEN);
    if(abi == nullptr || !text_equals(abi->text, "\"C\""))
        success = error_at(diagnostics, node, "only extern \"C\" blocks are supported") && success;
    return has_valid_repr_c(node, diagnostics) && success;
}

static bool validate_node(const XsSyntaxNode *node, XsDiagnostics *diagnostics)
{
    bool success = true;
    if(node->kind == XS_SYNTAX_DECL_EXTERN_BLOCK)
        success = validate_extern_block(node, diagnostics);
    if(node->kind == XS_SYNTAX_DECL_FUNCTION && (node->flags & XS_SYNTAX_FLAG_EXTERN) != 0)
        success = validate_attributes(node, XS_CFFI_ATTRIBUTE_FUNCTION, diagnostics) && success;
    if(node->kind == XS_SYNTAX_DECL_VARIABLE && (node->flags & XS_SYNTAX_FLAG_EXTERN) != 0)
        success = validate_attributes(node, XS_CFFI_ATTRIBUTE_STATIC, diagnostics) && success;
    for(size_t i = 0; i < node->child_count; ++i)
        success = validate_node(node->children[i], diagnostics) && success;
    return success;
}

bool xs_hir_validate_cffi(const XsSyntaxTree *tree, XsDiagnostics *diagnostics)
{
    if(tree == nullptr || tree->root == nullptr)
        return true;
    return validate_node(tree->root, diagnostics);
}
