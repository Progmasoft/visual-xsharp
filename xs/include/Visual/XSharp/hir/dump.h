/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 *
 */

#ifndef XS_HIR_DUMP_H
#define XS_HIR_DUMP_H

#include "Visual/XSharp/hir/symbol_table.h"

#include <stdio.h>

bool xs_hir_write_symbols(const XsHirSymbolTable *symbols, FILE *stream);

#endif
