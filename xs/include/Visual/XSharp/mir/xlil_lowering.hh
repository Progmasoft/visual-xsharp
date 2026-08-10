/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 * Shared C and C++ ABI surface.
 *
 */

#ifndef XS_MIR_XLIL_LOWERING_H
#define XS_MIR_XLIL_LOWERING_H

#include "Visual/XSharp/lil.hh"
#include "Visual/XSharp/mir.hh"

XsMirStatus xs_lil_module_add_mir_function_declarations(XsLilModule *module, const XsMirModule *mir, XsMirError *error);
XsMirStatus xs_lil_module_add_mir_function_bodies(XsLilModule *module, const XsMirModule *mir, XsMirError *error);

#endif
