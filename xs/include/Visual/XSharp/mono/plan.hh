/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 * Shared C and C++ ABI surface.
 */

#ifndef XS_MONO_PLAN_H
#define XS_MONO_PLAN_H

#include "Visual/XSharp/mir.hh"
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum { XS_MONO_OK, XS_MONO_INVALID_ARGUMENT, XS_MONO_ALLOCATION_FAILED } XsMonoStatus;
typedef struct { XsMonoStatus status; char message[256]; } XsMonoError;
typedef struct XsMonoPlan XsMonoPlan;

XsMonoStatus xs_mono_plan_create_for_concrete_mir(const XsMirModule *module, XsMonoPlan **plan, XsMonoError *error);
void xs_mono_plan_destroy(XsMonoPlan *plan);
size_t xs_mono_plan_entry_count(const XsMonoPlan *plan);
const char *xs_mono_plan_entry_unit_name(const XsMonoPlan *plan, size_t index);
const char *xs_mono_plan_entry_source_name(const XsMonoPlan *plan, size_t index);
const char *xs_mono_plan_entry_symbol_name(const XsMonoPlan *plan, size_t index);

#ifdef __cplusplus
}
#endif
#endif
