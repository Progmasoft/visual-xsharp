/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 */

#include "xs/hir/expression_check.h"

bool xs_hir_check_expression_types(const XsSyntaxTree *tree, XsDiagnostics *diagnostics)
{
    return xs_hir_check_expression_types_with_macros(tree, nullptr, nullptr, diagnostics);
}
