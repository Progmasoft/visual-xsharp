/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 *
 */

#ifndef XS_HIR_EXPRESSION_CHECK_H
#define XS_HIR_EXPRESSION_CHECK_H

#include "Visual/C23/diagnostic.hh"
#include "Visual/XSharp/macro.h"
#include "Visual/XSharp/syntax_ast.hh"

bool xs_hir_check_expression_types(const XsSyntaxTree *tree, XsDiagnostics *diagnostics);
bool xs_hir_check_expression_types_with_macros(const XsSyntaxTree *tree,
                                               const XsMacroDeclarationExpansionSet *macro_declarations,
                                               const XsMacroStatementExpansionSet *macro_statements,
                                               XsDiagnostics *diagnostics);

#endif
