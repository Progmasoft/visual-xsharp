/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 * C++-only surface.
 *
 */

#ifndef XS_CODEGEN_UNITS_H
#define XS_CODEGEN_UNITS_H

#include "Visual/XSharp/mir.hh"
#include "Visual/XSharp/mono/plan.hh"

#include <stddef.h>

#ifdef __cplusplus
extern "C"
{
#endif

    typedef enum
    {
        XS_CODEGEN_UNITS_OK,
        XS_CODEGEN_UNITS_INVALID_ARGUMENT,
        XS_CODEGEN_UNITS_ALLOCATION_FAILED,
    } XsCodegenUnitsStatus;

    typedef struct
    {
        XsCodegenUnitsStatus status;
        char message[256];
    } XsCodegenUnitsError;

    typedef struct XsCodegenPlan XsCodegenPlan;

    XsCodegenUnitsStatus xs_codegen_plan_create_from_mir(const XsMirModule *module, XsCodegenPlan **plan,
                                                         XsCodegenUnitsError *error);
    XsCodegenUnitsStatus xs_codegen_plan_create_from_mono(const XsMonoPlan *mono, XsCodegenPlan **plan,
                                                          XsCodegenUnitsError *error);
    void xs_codegen_plan_destroy(XsCodegenPlan *plan);

    size_t xs_codegen_plan_unit_count(const XsCodegenPlan *plan);
    const char *xs_codegen_plan_unit_name(const XsCodegenPlan *plan, size_t unit_index);
    size_t xs_codegen_plan_unit_function_count(const XsCodegenPlan *plan, size_t unit_index);
    const char *xs_codegen_plan_unit_function_name(const XsCodegenPlan *plan, size_t unit_index, size_t function_index);

#ifdef __cplusplus
}
#endif

#endif
