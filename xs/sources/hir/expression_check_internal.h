/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 */

#ifndef XS_HIR_EXPRESSION_CHECK_INTERNAL_H
#define XS_HIR_EXPRESSION_CHECK_INTERNAL_H

#include "xs/hir/expression_check.h"

bool xs_hir_check_result_constructor_call(const XsSyntaxNode *node, bool enclosing_returns_result,
                                          XsDiagnostics *diagnostics);
bool xs_hir_type_returns_result(const XsSyntaxNode *type);
bool xs_hir_type_is_borrowed_str(const XsSyntaxNode *type);
bool xs_hir_check_borrowed_str_value(const XsSyntaxNode *expression, XsDiagnostics *diagnostics);

#endif
