/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 */

#ifndef XS_SOURCE_INCLUDE_H
#define XS_SOURCE_INCLUDE_H

#include "xs/diagnostic.h"
#include "xs/syntax_ast.h"

typedef struct
{
  char *text;
  size_t length;
} XsIncludedSource;

bool xs_source_expand_include_macros(const XsSyntaxTree *tree, XsDiagnostics *diagnostics, XsIncludedSource *expanded);
void xs_included_source_free(XsIncludedSource *expanded);

#endif
