/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 */

#ifndef XS_MIR_HIR_LOWERING_H
#define XS_MIR_HIR_LOWERING_H

#include "xs/hir/symbol_table.h"
#include "xs/mir.h"

XsMirStatus xs_mir_module_add_hir_function_declarations(XsMirModule *module, const XsHirSymbolTable *symbols,
                                                        XsMirError *error);

#endif
