/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 *
 */

#ifndef XS_SYNTAX_PARSER_H
#define XS_SYNTAX_PARSER_H

#include "Visual/C23/diagnostic.hh"
#include "Visual/XSharp/syntax_ast.hh"

#include <stdint.h>

bool xs_syntax_parse(const XsSource *source, uint64_t file_id, XsDiagnostics *diagnostics, XsSyntaxTree *tree);

#endif
