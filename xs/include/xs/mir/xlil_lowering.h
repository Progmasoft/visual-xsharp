/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 */

#ifndef XS_MIR_XLIL_LOWERING_H
#define XS_MIR_XLIL_LOWERING_H

#include "xs/lil.h"
#include "xs/mir.h"

XsMirStatus xs_lil_module_add_mir_function_declarations(XsLilModule *module, const XsMirModule *mir, XsMirError *error);
XsMirStatus xs_lil_module_add_mir_function_bodies(XsLilModule *module, const XsMirModule *mir, XsMirError *error);

#endif
