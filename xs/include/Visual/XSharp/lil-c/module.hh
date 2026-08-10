/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 * Shared C and C++ ABI surface.
 *
 */

#ifndef XS_LIL_C_MODULE_H
#define XS_LIL_C_MODULE_H

#include "Visual/XSharp/lil-c/model.hh"

#ifdef __cplusplus
extern "C"
{
#endif

    XS_LIL_API XsLilStatus xs_lil_module_create(const char *name, XsLilModule **module, XsLilError *error);
    XS_LIL_API XsLilStatus xs_lil_module_parse_text(const char *path, const char *text, size_t length,
                                                    XsLilModule **module, XsLilError *error);
    XS_LIL_API XsLilStatus xs_lil_module_verify(const XsLilModule *module, XsLilError *error);
    XS_LIL_API void xs_lil_module_destroy(XsLilModule *module);
    XS_LIL_API const char *xs_lil_module_name(const XsLilModule *module);
    XS_LIL_API uint32_t xs_lil_module_text_version(const XsLilModule *module);
    XS_LIL_API bool xs_lil_module_type_is_valid(const XsLilModule *module, XsLilType type);
    XS_LIL_API XsLilType xs_lil_aggregate_type(uint32_t registry_id);
    XS_LIL_API XsLilType xs_lil_array_type(uint32_t registry_id);
    XS_LIL_API bool xs_lil_type_equal(XsLilType left, XsLilType right);
    XS_LIL_API XsLilStatus xs_lil_module_add_aggregate_type(XsLilModule *module, const char *name,
                                                            const XsLilType *fields, size_t field_count,
                                                            XsLilType *type, XsLilError *error);
    XS_LIL_API size_t xs_lil_module_aggregate_type_count(const XsLilModule *module);
    XS_LIL_API const char *xs_lil_module_aggregate_type_name(const XsLilModule *module, uint32_t registry_id);
    XS_LIL_API size_t xs_lil_module_aggregate_field_count(const XsLilModule *module, uint32_t registry_id);
    XS_LIL_API XsLilType xs_lil_module_aggregate_field_type(const XsLilModule *module, uint32_t registry_id,
                                                            size_t field);
    XS_LIL_API XsLilStatus xs_lil_module_add_array_type(XsLilModule *module, XsLilType element_type, uint64_t length,
                                                        XsLilType *type, XsLilError *error);
    XS_LIL_API XsLilStatus xs_lil_module_add_dynamic_array_type(XsLilModule *module, XsLilType element_type,
                                                                XsLilType *type, XsLilError *error);
    XS_LIL_API size_t xs_lil_module_array_type_count(const XsLilModule *module);
    XS_LIL_API XsLilType xs_lil_module_array_element_type(const XsLilModule *module, uint32_t registry_id);
    XS_LIL_API bool xs_lil_module_array_is_dynamic(const XsLilModule *module, uint32_t registry_id);
    XS_LIL_API uint64_t xs_lil_module_array_length(const XsLilModule *module, uint32_t registry_id);

#ifdef __cplusplus
}
#endif

#endif
