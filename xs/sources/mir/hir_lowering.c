/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 */

#include "xs/mir/hir_lowering.h"

#include "xs/hir/type_info.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static XsMirStatus unsupported(XsMirError *error, const char *message)
{
    if(error != nullptr)
    {
        error->status = XS_MIR_UNSUPPORTED;
        snprintf(error->message, sizeof(error->message), "%s", message);
    }
    return XS_MIR_UNSUPPORTED;
}

static bool type_node_kind(XsSyntaxKind kind)
{
    return kind == XS_SYNTAX_TYPE_NAMED || kind == XS_SYNTAX_TYPE_GENERIC || kind == XS_SYNTAX_TYPE_ARRAY ||
           kind == XS_SYNTAX_TYPE_FIXED_ARRAY || kind == XS_SYNTAX_TYPE_POINTER || kind == XS_SYNTAX_TYPE_REFERENCE ||
           kind == XS_SYNTAX_TYPE_MUTABLE_REFERENCE || kind == XS_SYNTAX_TYPE_TUPLE ||
           kind == XS_SYNTAX_TYPE_FUNCTION || kind == XS_SYNTAX_TYPE_UNIT;
}

static const XsSyntaxNode *first_child_kind(const XsSyntaxNode *node, XsSyntaxKind kind)
{
    if(node == nullptr)
        return nullptr;
    for(size_t i = 0; i < node->child_count; ++i)
    {
        if(node->children[i]->kind == kind)
            return node->children[i];
    }
    return nullptr;
}

static const XsSyntaxNode *first_direct_type_child(const XsSyntaxNode *node)
{
    if(node == nullptr)
        return nullptr;
    for(size_t i = 0; i < node->child_count; ++i)
    {
        if(type_node_kind(node->children[i]->kind))
            return node->children[i];
    }
    return nullptr;
}

static bool generic_function(const XsSyntaxNode *function)
{
    return first_child_kind(function, XS_SYNTAX_GENERIC_PARAMETER) != nullptr;
}

static bool primitive_named_type(const XsSyntaxNode *type, XsMirType *mir_type)
{
    if(type == nullptr || mir_type == nullptr || type->kind != XS_SYNTAX_TYPE_NAMED)
        return false;
    const XsSyntaxNode *path = first_child_kind(type, XS_SYNTAX_PATH);
    if(path == nullptr || path->child_count != 1 || path->children[0]->kind != XS_SYNTAX_IDENTIFIER)
        return false;
    const XsSyntaxNode *identifier = path->children[0];
    const XsHirPrimitiveInfo *primitive = xs_hir_primitive_find(identifier->text.data, identifier->text.length);
    if(primitive == nullptr || !primitive->has_xlil_type)
        return false;
    *mir_type = primitive->xlil_type;
    return true;
}

static XsMirStatus lower_type(const XsSyntaxNode *type, XsMirType *mir_type, XsMirError *error)
{
    if(!primitive_named_type(type, mir_type))
        return unsupported(error, "MIR signature lowering currently supports only primitive XLIL-mapped types");
    return XS_MIR_OK;
}

static size_t parameter_count(const XsSyntaxNode *function)
{
    size_t count = 0;
    for(size_t i = 0; i < function->child_count; ++i)
    {
        if(function->children[i]->kind == XS_SYNTAX_PARAMETER)
            ++count;
    }
    return count;
}

static XsMirStatus lower_function(const XsHirSymbol *symbol, XsMirModule *module, XsMirError *error)
{
    if(generic_function(symbol->syntax))
        return unsupported(error, "MIR signature lowering requires monomorphization for generic functions");
    size_t count = parameter_count(symbol->syntax);
    XsMirType *parameters = count == 0 ? nullptr : malloc(count * sizeof(*parameters));
    if(count != 0 && parameters == nullptr)
        return xs_mir_module_add_function_declaration(nullptr, nullptr, (XsMirType){0}, nullptr, 0, error);
    size_t parameter = 0;
    for(size_t i = 0; i < symbol->syntax->child_count; ++i)
    {
        const XsSyntaxNode *node = symbol->syntax->children[i];
        if(node->kind != XS_SYNTAX_PARAMETER)
            continue;
        if(parameter >= count)
        {
            free(parameters);
            return unsupported(error, "MIR signature lowering found an inconsistent parameter list");
        }
        XsMirStatus status = lower_type(first_direct_type_child(node), &parameters[parameter++], error);
        if(status != XS_MIR_OK)
        {
            free(parameters);
            return status;
        }
    }
    XsMirType return_type = {.kind = XS_LIL_TYPE_VOID};
    const XsSyntaxNode *return_node = first_direct_type_child(symbol->syntax);
    if(return_node != nullptr)
    {
        XsMirStatus status = lower_type(return_node, &return_type, error);
        if(status != XS_MIR_OK)
        {
            free(parameters);
            return status;
        }
    }
    XsMirStatus status =
        xs_mir_module_add_function_declaration(module, symbol->qualified_name, return_type, parameters, count, error);
    free(parameters);
    return status;
}

XsMirStatus xs_mir_module_add_hir_function_declarations(XsMirModule *module, const XsHirSymbolTable *symbols,
                                                        XsMirError *error)
{
    if(module == nullptr || symbols == nullptr)
        return xs_mir_module_add_function_declaration(nullptr, nullptr, (XsMirType){0}, nullptr, 0, error);
    for(size_t i = 0; i < symbols->count; ++i)
    {
        const XsHirSymbol *symbol = &symbols->symbols[i];
        if(symbol->kind != XS_HIR_SYMBOL_FUNCTION)
            continue;
        XsMirStatus status = lower_function(symbol, module, error);
        if(status != XS_MIR_OK)
            return status;
    }
    return XS_MIR_OK;
}
