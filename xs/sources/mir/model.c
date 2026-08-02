/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 */

#include "model_internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void xs_mir_clear_error(XsMirError *error)
{
    if(error != nullptr)
        *error = (XsMirError){.status = XS_MIR_OK};
}

XsMirStatus xs_mir_set_error(XsMirError *error, XsMirStatus status, const char *message)
{
    if(error != nullptr)
    {
        error->status = status;
        snprintf(error->message, sizeof(error->message), "%s", message == nullptr ? "MIR error" : message);
    }
    return status;
}

char *xs_mir_copy_text(const char *text)
{
    size_t length = strlen(text);
    char *copy = malloc(length + 1);
    if(copy != nullptr)
        memcpy(copy, text, length + 1);
    return copy;
}

void xs_mir_block_free(XsMirBlock *block)
{
    for(size_t i = 0; i < block->instruction_count; ++i)
    {
        free(block->instructions[i].callee);
        free(block->instructions[i].arguments);
    }
    free(block->instructions);
    free(block->label);
    *block = (XsMirBlock){0};
}

static void place_free(XsMirPlace *place)
{
    free(place->projections);
    *place = (XsMirPlace){0};
}

static void function_free(XsMirFunction *function)
{
    for(size_t i = 0; i < function->block_count; ++i)
    {
        xs_mir_block_free(function->blocks[i]);
        free(function->blocks[i]);
    }
    for(size_t i = 0; i < function->local_count; ++i)
        free(function->locals[i].name);
    for(size_t i = 0; i < function->place_count; ++i)
    {
        place_free(function->places[i]);
        free(function->places[i]);
    }
    free(function->locals);
    free(function->places);
    free(function->values);
    free(function->blocks);
    free(function->qualified_name);
    free(function->parameters);
    *function = (XsMirFunction){0};
}

XsMirStatus xs_mir_module_create(const char *name, XsMirModule **module, XsMirError *error)
{
    xs_mir_clear_error(error);
    if(name == nullptr || name[0] == '\0' || module == nullptr)
        return xs_mir_set_error(error, XS_MIR_INVALID_ARGUMENT, "MIR module name and output are required");
    *module = calloc(1, sizeof(**module));
    if(*module == nullptr)
        return xs_mir_set_error(error, XS_MIR_ALLOCATION_FAILED, "out of memory while creating MIR module");
    (*module)->name = xs_mir_copy_text(name);
    if((*module)->name == nullptr)
    {
        xs_mir_module_destroy(*module);
        *module = nullptr;
        return xs_mir_set_error(error, XS_MIR_ALLOCATION_FAILED, "out of memory while naming MIR module");
    }
    return XS_MIR_OK;
}

void xs_mir_module_destroy(XsMirModule *module)
{
    if(module == nullptr)
        return;
    for(size_t i = 0; i < module->function_count; ++i)
        function_free(&module->functions[i]);
    free(module->functions);
    free(module->name);
    free(module);
}

const char *xs_mir_module_name(const XsMirModule *module)
{
    return module == nullptr ? nullptr : module->name;
}

static XsMirStatus append_function(XsMirModule *module, XsMirFunction function, XsMirFunction **out, XsMirError *error)
{
    if(module->function_count == module->function_capacity)
    {
        size_t capacity = module->function_capacity == 0 ? 8 : module->function_capacity * 2;
        XsMirFunction *functions = realloc(module->functions, capacity * sizeof(*functions));
        if(functions == nullptr)
            return xs_mir_set_error(error, XS_MIR_ALLOCATION_FAILED, "out of memory while adding MIR function");
        module->functions = functions;
        module->function_capacity = capacity;
    }
    module->functions[module->function_count] = function;
    if(out != nullptr)
        *out = &module->functions[module->function_count];
    ++module->function_count;
    return XS_MIR_OK;
}

static XsMirStatus make_function(const char *qualified_name, XsMirType return_type, const XsMirType *parameters,
                                 size_t parameter_count, bool is_definition, XsMirFunction *function, XsMirError *error)
{
    if(qualified_name == nullptr || qualified_name[0] == '\0' || (parameter_count != 0 && parameters == nullptr))
        return xs_mir_set_error(error, XS_MIR_INVALID_ARGUMENT, "valid MIR function signature is required");
    *function = (XsMirFunction){
        .qualified_name = xs_mir_copy_text(qualified_name),
        .return_type = return_type,
        .parameter_count = parameter_count,
        .is_definition = is_definition,
    };
    if(function->qualified_name == nullptr)
        return xs_mir_set_error(error, XS_MIR_ALLOCATION_FAILED, "out of memory while naming MIR function");
    if(parameter_count == 0)
        return XS_MIR_OK;
    function->parameters = malloc(parameter_count * sizeof(*function->parameters));
    if(function->parameters == nullptr)
    {
        function_free(function);
        return xs_mir_set_error(error, XS_MIR_ALLOCATION_FAILED, "out of memory while copying MIR parameters");
    }
    memcpy(function->parameters, parameters, parameter_count * sizeof(*function->parameters));
    if(is_definition)
    {
        for(size_t index = 0; index < parameter_count; ++index)
        {
            XsMirValueId value = 0;
            XsMirStatus status = xs_mir_function_add_value(function, parameters[index], &value, error);
            if(status != XS_MIR_OK)
            {
                function_free(function);
                return status;
            }
        }
    }
    return XS_MIR_OK;
}

XsMirStatus xs_mir_module_add_function_declaration(XsMirModule *module, const char *qualified_name,
                                                   XsMirType return_type, const XsMirType *parameters,
                                                   size_t parameter_count, XsMirError *error)
{
    xs_mir_clear_error(error);
    if(module == nullptr)
        return xs_mir_set_error(error, XS_MIR_INVALID_ARGUMENT, "valid MIR module and function signature are required");
    XsMirFunction function = {0};
    XsMirStatus status =
        make_function(qualified_name, return_type, parameters, parameter_count, false, &function, error);
    if(status != XS_MIR_OK)
        return status;
    status = append_function(module, function, nullptr, error);
    if(status != XS_MIR_OK)
        function_free(&function);
    return status;
}

XsMirStatus xs_mir_module_add_function_definition(XsMirModule *module, const char *qualified_name,
                                                  XsMirType return_type, const XsMirType *parameters,
                                                  size_t parameter_count, XsMirFunction **function, XsMirError *error)
{
    xs_mir_clear_error(error);
    if(function != nullptr)
        *function = nullptr;
    if(module == nullptr)
        return xs_mir_set_error(error, XS_MIR_INVALID_ARGUMENT, "valid MIR module and function signature are required");
    XsMirFunction created = {0};
    XsMirStatus status = make_function(qualified_name, return_type, parameters, parameter_count, true, &created, error);
    if(status != XS_MIR_OK)
        return status;
    status = append_function(module, created, function, error);
    if(status != XS_MIR_OK)
        function_free(&created);
    return status;
}

size_t xs_mir_module_function_count(const XsMirModule *module)
{
    return module == nullptr ? 0 : module->function_count;
}

const XsMirFunction *xs_mir_module_function_at(const XsMirModule *module, size_t index)
{
    if(module == nullptr || index >= module->function_count)
        return nullptr;
    return &module->functions[index];
}

const char *xs_mir_function_name(const XsMirFunction *function)
{
    return function == nullptr ? nullptr : function->qualified_name;
}

XsMirType xs_mir_function_return_type(const XsMirFunction *function)
{
    return function == nullptr ? (XsMirType){.kind = XS_LIL_TYPE_VOID} : function->return_type;
}

const XsMirType *xs_mir_function_parameters(const XsMirFunction *function)
{
    return function == nullptr ? nullptr : function->parameters;
}

size_t xs_mir_function_parameter_count(const XsMirFunction *function)
{
    return function == nullptr ? 0 : function->parameter_count;
}

bool xs_mir_function_is_definition(const XsMirFunction *function)
{
    return function != nullptr && function->is_definition;
}

XsMirStatus xs_mir_function_add_local(XsMirFunction *function, XsMirLocalKind kind, const char *name, XsMirType type,
                                      bool is_mutable, XsMirLocalId *local, XsMirError *error)
{
    xs_mir_clear_error(error);
    if(local != nullptr)
        *local = 0;
    if(function == nullptr || !function->is_definition || name == nullptr || name[0] == '\0')
        return xs_mir_set_error(error, XS_MIR_INVALID_ARGUMENT,
                                "valid MIR function definition and local name are required");
    if(function->local_count == function->local_capacity)
    {
        size_t capacity = function->local_capacity == 0 ? 8 : function->local_capacity * 2;
        XsMirLocal *locals = realloc(function->locals, capacity * sizeof(*locals));
        if(locals == nullptr)
            return xs_mir_set_error(error, XS_MIR_ALLOCATION_FAILED, "out of memory while adding MIR local");
        function->locals = locals;
        function->local_capacity = capacity;
    }
    XsMirLocal created = {
        .name = xs_mir_copy_text(name),
        .type = type,
        .kind = kind,
        .is_mutable = is_mutable,
    };
    if(created.name == nullptr)
        return xs_mir_set_error(error, XS_MIR_ALLOCATION_FAILED, "out of memory while naming MIR local");
    XsMirLocalId id = (XsMirLocalId)function->local_count;
    function->locals[function->local_count++] = created;
    if(local != nullptr)
        *local = id;
    return XS_MIR_OK;
}

size_t xs_mir_function_local_count(const XsMirFunction *function)
{
    return function == nullptr ? 0 : function->local_count;
}

static const XsMirLocal *local_at(const XsMirFunction *function, XsMirLocalId local)
{
    if(function == nullptr || (size_t)local >= function->local_count)
        return nullptr;
    return &function->locals[local];
}

XsMirLocalKind xs_mir_function_local_kind(const XsMirFunction *function, XsMirLocalId local)
{
    const XsMirLocal *entry = local_at(function, local);
    return entry == nullptr ? XS_MIR_LOCAL_TEMPORARY : entry->kind;
}

const char *xs_mir_function_local_name(const XsMirFunction *function, XsMirLocalId local)
{
    const XsMirLocal *entry = local_at(function, local);
    return entry == nullptr ? nullptr : entry->name;
}

XsMirType xs_mir_function_local_type(const XsMirFunction *function, XsMirLocalId local)
{
    const XsMirLocal *entry = local_at(function, local);
    return entry == nullptr ? (XsMirType){.kind = XS_LIL_TYPE_VOID} : entry->type;
}

bool xs_mir_function_local_is_mutable(const XsMirFunction *function, XsMirLocalId local)
{
    const XsMirLocal *entry = local_at(function, local);
    return entry != nullptr && entry->is_mutable;
}

XsMirStatus xs_mir_function_add_value(XsMirFunction *function, XsMirType type, XsMirValueId *value, XsMirError *error)
{
    xs_mir_clear_error(error);
    if(value != nullptr)
        *value = 0;
    if(function == nullptr || !function->is_definition)
        return xs_mir_set_error(error, XS_MIR_INVALID_ARGUMENT, "valid MIR function definition is required");
    if(function->value_count == function->value_capacity)
    {
        size_t capacity = function->value_capacity == 0 ? 8 : function->value_capacity * 2;
        XsMirValue *values = realloc(function->values, capacity * sizeof(*values));
        if(values == nullptr)
            return xs_mir_set_error(error, XS_MIR_ALLOCATION_FAILED, "out of memory while adding MIR value");
        function->values = values;
        function->value_capacity = capacity;
    }
    XsMirValueId id = (XsMirValueId)function->value_count;
    function->values[function->value_count++] = (XsMirValue){.type = type};
    if(value != nullptr)
        *value = id;
    return XS_MIR_OK;
}

XsMirType xs_mir_function_value_type(const XsMirFunction *function, XsMirValueId value)
{
    if(function == nullptr || (size_t)value >= function->value_count)
        return (XsMirType){.kind = XS_LIL_TYPE_VOID};
    return function->values[value].type;
}

XsMirStatus xs_mir_function_add_local_place(XsMirFunction *function, XsMirLocalId local, XsMirPlace **place,
                                            XsMirError *error)
{
    xs_mir_clear_error(error);
    if(place != nullptr)
        *place = nullptr;
    if(function == nullptr || !function->is_definition || (size_t)local >= function->local_count)
        return xs_mir_set_error(error, XS_MIR_INVALID_ARGUMENT, "valid MIR function and local are required");
    if(function->place_count == function->place_capacity)
    {
        size_t capacity = function->place_capacity == 0 ? 8 : function->place_capacity * 2;
        XsMirPlace **places = realloc(function->places, capacity * sizeof(*places));
        if(places == nullptr)
            return xs_mir_set_error(error, XS_MIR_ALLOCATION_FAILED, "out of memory while adding MIR place");
        function->places = places;
        function->place_capacity = capacity;
    }
    XsMirPlace *created = calloc(1, sizeof(*created));
    if(created == nullptr)
        return xs_mir_set_error(error, XS_MIR_ALLOCATION_FAILED, "out of memory while adding MIR place");
    created->id = (XsMirPlaceId)function->place_count;
    created->root_local = local;
    function->places[function->place_count++] = created;
    if(place != nullptr)
        *place = created;
    return XS_MIR_OK;
}

XsMirPlaceId xs_mir_place_id(const XsMirPlace *place)
{
    return place == nullptr ? 0 : place->id;
}

XsMirLocalId xs_mir_place_root_local(const XsMirPlace *place)
{
    return place == nullptr ? 0 : place->root_local;
}

static XsMirStatus append_projection(XsMirPlace *place, XsMirPlaceProjection projection, XsMirError *error)
{
    xs_mir_clear_error(error);
    if(place == nullptr)
        return xs_mir_set_error(error, XS_MIR_INVALID_ARGUMENT, "valid MIR place is required");
    if(place->projection_count == place->projection_capacity)
    {
        size_t capacity = place->projection_capacity == 0 ? 4 : place->projection_capacity * 2;
        XsMirPlaceProjection *projections = realloc(place->projections, capacity * sizeof(*projections));
        if(projections == nullptr)
            return xs_mir_set_error(error, XS_MIR_ALLOCATION_FAILED, "out of memory while adding MIR place projection");
        place->projections = projections;
        place->projection_capacity = capacity;
    }
    place->projections[place->projection_count++] = projection;
    return XS_MIR_OK;
}

XsMirStatus xs_mir_place_add_field_projection(XsMirPlace *place, uint32_t field_index, XsMirError *error)
{
    return append_projection(
        place, (XsMirPlaceProjection){.kind = XS_MIR_PLACE_PROJECTION_FIELD, .payload = field_index}, error);
}

XsMirStatus xs_mir_place_add_deref_projection(XsMirPlace *place, XsMirError *error)
{
    return append_projection(place, (XsMirPlaceProjection){.kind = XS_MIR_PLACE_PROJECTION_DEREF}, error);
}

XsMirStatus xs_mir_place_add_index_projection(XsMirPlace *place, XsMirValueId index, XsMirError *error)
{
    return append_projection(place, (XsMirPlaceProjection){.kind = XS_MIR_PLACE_PROJECTION_INDEX, .payload = index},
                             error);
}

size_t xs_mir_place_projection_count(const XsMirPlace *place)
{
    return place == nullptr ? 0 : place->projection_count;
}

XsMirPlaceProjectionKind xs_mir_place_projection_kind(const XsMirPlace *place, size_t index)
{
    if(place == nullptr || index >= place->projection_count)
        return XS_MIR_PLACE_PROJECTION_FIELD;
    return place->projections[index].kind;
}
