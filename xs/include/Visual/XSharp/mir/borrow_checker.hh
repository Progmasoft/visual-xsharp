/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 * Shared C and C++ ABI surface.
 *
 */

#ifndef XS_MIR_BORROW_CHECKER_H
#define XS_MIR_BORROW_CHECKER_H

#include "Visual/XSharp/mir.hh"

XsMirStatus xs_mir_borrow_check_module(const XsMirModule *module, XsMirError *error);

#endif
