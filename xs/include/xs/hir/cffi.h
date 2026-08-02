/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 */

#ifndef XS_HIR_CFFI_H
#define XS_HIR_CFFI_H

#include "xs/diagnostic.h"
#include "xs/syntax_ast.h"

typedef struct
{
    XsText abi;
    XsText link_library;
    XsText header;
    XsText calling_convention;
    bool unsafe_boundary;
    bool safe_boundary;
} XsHirCffiExternBlock;

typedef struct
{
    XsText abi;
    XsText link_name;
    XsText export_name;
    XsText symbol_visibility;
    XsText ownership;
    bool no_unwind;
    bool may_unwind;
    bool variadic;
    bool unsafe_boundary;
    bool safe_boundary;
    bool foreign_thread_safe;
    bool no_callback_into_runtime;
    bool may_block;
    bool cancellation_safe;
    bool cancellation_unsafe;
} XsHirCffiFunction;

typedef struct
{
    XsText abi;
    XsText link_name;
    bool thread_local_storage;
    bool unsafe_boundary;
    bool safe_boundary;
} XsHirCffiStatic;

bool xs_hir_validate_cffi(const XsSyntaxTree *tree, XsDiagnostics *diagnostics);
bool xs_hir_cffi_read_extern_block(const XsSyntaxNode *node, XsHirCffiExternBlock *metadata);
bool xs_hir_cffi_read_function(const XsSyntaxNode *node, XsHirCffiFunction *metadata);
bool xs_hir_cffi_read_static(const XsSyntaxNode *node, XsHirCffiStatic *metadata);

#endif
