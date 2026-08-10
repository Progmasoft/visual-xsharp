/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 *
 */

#ifndef XS_HIR_INHERITANCE_H
#define XS_HIR_INHERITANCE_H

#include "Visual/XSharp/hir/symbol_table.h"

bool xs_hir_validate_inheritance(const XsSyntaxTree *tree, const XsHirSymbolTable *symbols,
                                 const XsHirImportScope *import, XsDiagnostics *diagnostics);

#endif
