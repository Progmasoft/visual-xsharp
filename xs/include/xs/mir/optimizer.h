/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 */

#ifndef XS_MIR_OPTIMIZER_H
#define XS_MIR_OPTIMIZER_H

#include "xs/mir.h"

XsMirStatus xs_mir_optimize_module_cfg(XsMirModule *module, XsMirError *error);
XsMirStatus xs_mir_optimize_module_constants(XsMirModule *module, XsMirError *error);

#endif
