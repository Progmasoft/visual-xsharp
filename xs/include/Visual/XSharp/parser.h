/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 *
 */

#ifndef XS_PARSER_H
#define XS_PARSER_H

#include "Visual/XSharp/ast.hh"
#include "Visual/XSharp/lexer.h"

typedef struct
{
    XsLexer lexer;
    XsDiagnostics *diagnostics;
    XsToken current;
    XsToken previous;
} XsParser;

void xs_parser_init(XsParser *parser, const XsSource *source, XsDiagnostics *diagnostics);
bool xs_parser_parse(XsParser *parser, XsAst *ast);

#endif
