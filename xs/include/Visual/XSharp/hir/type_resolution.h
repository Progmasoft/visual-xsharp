/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 *
 */

#ifndef XS_HIR_TYPE_RESOLUTION_H
#define XS_HIR_TYPE_RESOLUTION_H

#include "Visual/XSharp/hir/symbol_table.h"

bool xs_hir_resolve_types(const XsSyntaxTree *tree, const XsHirSymbolTable *symbols, const XsHirImportScope *import,
                          XsDiagnostics *diagnostics);

bool xs_hir_resolve_types_expanded(const XsSyntaxTree *tree, const XsMacroStatementExpansionSet *macro_statements,
                                   const XsHirSymbolTable *symbols, const XsHirImportScope *import,
                                   XsDiagnostics *diagnostics);

bool xs_hir_resolve_types_with_macros(const XsSyntaxTree *tree,
                                      const XsMacroDeclarationExpansionSet *macro_declarations,
                                      const XsMacroStatementExpansionSet *macro_statements,
                                      const XsHirSymbolTable *symbols, const XsHirImportScope *import,
                                      XsDiagnostics *diagnostics);

#endif
