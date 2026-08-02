/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 */

#include "model_internal.h"

#include <stdlib.h>

XsLilType xs_lil_array_type(uint32_t registry_id)
{
    return (XsLilType){.kind = XS_LIL_TYPE_ARRAY, .registry_id = registry_id};
}

static XsLilStatus add_array_type(XsLilModule *module, XsLilType element_type, uint64_t length, bool dynamic,
                                  XsLilType *type, XsLilError *error)
{
    xs_lil_clear_error(error);
    if(module == nullptr || type == nullptr || (!dynamic && length == 0) || element_type.kind == XS_LIL_TYPE_VOID ||
       (unsigned)element_type.kind > (unsigned)XS_LIL_TYPE_ARRAY ||
       (element_type.kind == XS_LIL_TYPE_AGGREGATE && element_type.registry_id >= module->aggregate_type_count) ||
       (element_type.kind == XS_LIL_TYPE_ARRAY && element_type.registry_id >= module->array_type_count))
        return xs_lil_set_error(error, XS_LIL_INVALID_ARGUMENT, "XLIL array type definition is invalid");
    for(size_t index = 0; index < module->array_type_count; ++index)
        if(xs_lil_type_equal(module->array_types[index].element_type, element_type) &&
           module->array_types[index].length == length && module->array_types[index].dynamic == dynamic)
            return xs_lil_set_error(error, XS_LIL_INVALID_ARGUMENT, "XLIL array type is duplicated");
    if(module->array_type_count == module->array_type_capacity)
    {
        size_t capacity = module->array_type_capacity == 0 ? 4 : module->array_type_capacity * 2;
        XsLilArrayType *entries = realloc(module->array_types, capacity * sizeof(*entries));
        if(entries == nullptr)
            return xs_lil_set_error(error, XS_LIL_ALLOCATION_FAILED, "out of memory while adding XLIL array type");
        module->array_types = entries;
        module->array_type_capacity = capacity;
    }
    uint32_t id = (uint32_t)module->array_type_count;
    module->array_types[module->array_type_count++] =
        (XsLilArrayType){.element_type = element_type, .length = length, .dynamic = dynamic};
    *type = xs_lil_array_type(id);
    return XS_LIL_OK;
}

XsLilStatus xs_lil_module_add_array_type(XsLilModule *module, XsLilType element_type, uint64_t length, XsLilType *type,
                                         XsLilError *error)
{
    return add_array_type(module, element_type, length, false, type, error);
}

XsLilStatus xs_lil_module_add_dynamic_array_type(XsLilModule *module, XsLilType element_type, XsLilType *type,
                                                 XsLilError *error)
{
    return add_array_type(module, element_type, 0, true, type, error);
}

size_t xs_lil_module_array_type_count(const XsLilModule *module)
{
    return module == nullptr ? 0 : module->array_type_count;
}

XsLilType xs_lil_module_array_element_type(const XsLilModule *module, uint32_t registry_id)
{
    if(module == nullptr || registry_id >= module->array_type_count)
        return (XsLilType){.kind = XS_LIL_TYPE_VOID};
    return module->array_types[registry_id].element_type;
}

uint64_t xs_lil_module_array_length(const XsLilModule *module, uint32_t registry_id)
{
    return module == nullptr || registry_id >= module->array_type_count ? 0 : module->array_types[registry_id].length;
}

bool xs_lil_module_array_is_dynamic(const XsLilModule *module, uint32_t registry_id)
{
    return module != nullptr && registry_id < module->array_type_count && module->array_types[registry_id].dynamic;
}
