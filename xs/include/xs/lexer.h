/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 */

#ifndef XS_LEXER_H
#define XS_LEXER_H

#include "xs/diagnostic.h"
#include "xs/token.h"

typedef struct
{
  const XsSource *source;
  XsDiagnostics *diagnostics;
  size_t cursor;
} XsLexer;

void xs_lexer_init(XsLexer *lexer, const XsSource *source, XsDiagnostics *diagnostics);
XsToken xs_lexer_next(XsLexer *lexer);

#endif
