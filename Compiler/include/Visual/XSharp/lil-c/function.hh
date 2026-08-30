/*
 * SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
 * SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.0
 * Shared C and C++ ABI surface.
 *
 */

#ifndef XS_LIL_C_FUNCTION_H
#define XS_LIL_C_FUNCTION_H

#include "Visual/XSharp/lil-c/module.hh"

#ifdef __cplusplus
extern "C"
{
#endif

    XS_LIL_API XsLilStatus
    xs_lil_module_add_function(XsLilModule *module, const char *name, XsLilType return_type, const XsLilType *parameters, size_t parameter_count, XsLilError *error);
    XS_LIL_API XsLilStatus
    xs_lil_module_add_function_definition(XsLilModule *module, const char *name, XsLilType return_type, const XsLilType *parameters, size_t parameter_count, XsLilFunction **function, XsLilError *error);
    XS_LIL_API size_t
    xs_lil_module_function_count(const XsLilModule *module);
    XS_LIL_API const XsLilFunction *
    xs_lil_module_function_at(const XsLilModule *module, size_t index);
    XS_LIL_API XsLilFunction *
    xs_lil_module_function_at_mut(XsLilModule *module, size_t index);
    XS_LIL_API const XsLilFunction *
    xs_lil_module_function_named(const XsLilModule *module, const char *name);
    XS_LIL_API XsLilFunction *
    xs_lil_module_function_named_mut(XsLilModule *module, const char *name);
    XS_LIL_API const char *
    xs_lil_function_name(const XsLilFunction *function);
    XS_LIL_API XsLilType
    xs_lil_function_return_type(const XsLilFunction *function);
    XS_LIL_API size_t
    xs_lil_function_parameter_count(const XsLilFunction *function);
    XS_LIL_API XsLilType
    xs_lil_function_parameter_type(const XsLilFunction *function, size_t index);
    XS_LIL_API XsLilValueId
    xs_lil_function_parameter_value(const XsLilFunction *function, size_t index);
    XS_LIL_API bool
    xs_lil_function_is_definition(const XsLilFunction *function);
    XS_LIL_API size_t
    xs_lil_function_value_count(const XsLilFunction *function);
    XS_LIL_API XsLilType
    xs_lil_function_value_type(const XsLilFunction *function, XsLilValueId value);
    XS_LIL_API XsLilStatus
    xs_lil_function_add_slot(XsLilFunction *function, XsLilType type, XsLilSlotId *slot, XsLilError *error);
    XS_LIL_API size_t
    xs_lil_function_slot_count(const XsLilFunction *function);
    XS_LIL_API XsLilType
    xs_lil_function_slot_type(const XsLilFunction *function, XsLilSlotId slot);
    XS_LIL_API size_t
    xs_lil_function_block_count(const XsLilFunction *function);
    XS_LIL_API const XsLilBlock *
    xs_lil_function_block_at(const XsLilFunction *function, size_t index);
    XS_LIL_API XsLilBlock *
    xs_lil_function_block_at_mut(XsLilFunction *function, size_t index);
    XS_LIL_API const XsLilBlock *
    xs_lil_function_block_named(const XsLilFunction *function, const char *label);
    XS_LIL_API XsLilBlock *
    xs_lil_function_block_named_mut(XsLilFunction *function, const char *label);
    XS_LIL_API XsLilStatus
    xs_lil_function_append_block(XsLilFunction *function, const char *label, XsLilBlock **block, XsLilError *error);

#ifdef __cplusplus
}
#endif

#endif
