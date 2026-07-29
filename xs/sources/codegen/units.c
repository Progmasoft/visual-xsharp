/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 */

#include "xs/codegen/units.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct
{
  char *name;
  char **functions;
  size_t function_count;
  size_t function_capacity;
} XsCodegenUnitPlan;

struct XsCodegenPlan
{
  XsCodegenUnitPlan *units;
  size_t unit_count;
  size_t unit_capacity;
};

static void clear_error(XsCodegenUnitsError *error)
{
  if(error != nullptr)
    *error = (XsCodegenUnitsError){.status = XS_CODEGEN_UNITS_OK};
}

static XsCodegenUnitsStatus set_error(XsCodegenUnitsError *error, XsCodegenUnitsStatus status, const char *message)
{
  if(error != nullptr)
  {
    error->status = status;
    snprintf(error->message, sizeof(error->message), "%s", message == nullptr ? "codegen unit planning error" : message);
  }
  return status;
}

static char *copy_range(const char *text, size_t length)
{
  char *copy = malloc(length + 1);
  if(copy == nullptr)
    return nullptr;
  memcpy(copy, text, length);
  copy[length] = '\0';
  return copy;
}

static char *copy_text(const char *text)
{
  return copy_range(text, strlen(text));
}

static void free_unit(XsCodegenUnitPlan *unit)
{
  for(size_t i = 0; i < unit->function_count; ++i)
    free(unit->functions[i]);
  free(unit->functions);
  free(unit->name);
  *unit = (XsCodegenUnitPlan){0};
}

void xs_codegen_plan_destroy(XsCodegenPlan *plan)
{
  if(plan == nullptr)
    return;
  for(size_t i = 0; i < plan->unit_count; ++i)
    free_unit(&plan->units[i]);
  free(plan->units);
  free(plan);
}

static const char *last_dot(const char *text)
{
  const char *result = nullptr;
  for(const char *cursor = text; *cursor != '\0'; ++cursor)
  {
    if(*cursor == '.')
      result = cursor;
  }
  return result;
}

static char *unit_name_for_function(const XsMirModule *module, const char *function_name)
{
  const char *dot = last_dot(function_name);
  if(dot != nullptr && dot != function_name)
    return copy_range(function_name, (size_t)(dot - function_name));
  return copy_text(xs_mir_module_name(module));
}

static XsCodegenUnitPlan *find_unit(XsCodegenPlan *plan, const char *name)
{
  for(size_t i = 0; i < plan->unit_count; ++i)
  {
    if(strcmp(plan->units[i].name, name) == 0)
      return &plan->units[i];
  }
  return nullptr;
}

static XsCodegenUnitsStatus append_unit(XsCodegenPlan *plan, char *name, XsCodegenUnitPlan **unit,
                                        XsCodegenUnitsError *error)
{
  if(plan->unit_count == plan->unit_capacity)
  {
    size_t capacity = plan->unit_capacity == 0 ? 4 : plan->unit_capacity * 2;
    XsCodegenUnitPlan *units = realloc(plan->units, capacity * sizeof(*units));
    if(units == nullptr)
      return set_error(error, XS_CODEGEN_UNITS_ALLOCATION_FAILED, "out of memory while adding a codegen unit");
    plan->units = units;
    plan->unit_capacity = capacity;
  }
  plan->units[plan->unit_count] = (XsCodegenUnitPlan){.name = name};
  *unit = &plan->units[plan->unit_count++];
  return XS_CODEGEN_UNITS_OK;
}

static XsCodegenUnitsStatus append_function(XsCodegenUnitPlan *unit, const char *function_name,
                                            XsCodegenUnitsError *error)
{
  if(unit->function_count == unit->function_capacity)
  {
    size_t capacity = unit->function_capacity == 0 ? 8 : unit->function_capacity * 2;
    char **functions = realloc(unit->functions, capacity * sizeof(*functions));
    if(functions == nullptr)
      return set_error(error, XS_CODEGEN_UNITS_ALLOCATION_FAILED, "out of memory while adding a codegen function");
    unit->functions = functions;
    unit->function_capacity = capacity;
  }
  unit->functions[unit->function_count] = copy_text(function_name);
  if(unit->functions[unit->function_count] == nullptr)
    return set_error(error, XS_CODEGEN_UNITS_ALLOCATION_FAILED, "out of memory while naming a codegen function");
  ++unit->function_count;
  return XS_CODEGEN_UNITS_OK;
}

XsCodegenUnitsStatus xs_codegen_plan_create_from_mir(const XsMirModule *module, XsCodegenPlan **plan,
                                                     XsCodegenUnitsError *error)
{
  clear_error(error);
  if(plan != nullptr)
    *plan = nullptr;
  if(module == nullptr || plan == nullptr)
    return set_error(error, XS_CODEGEN_UNITS_INVALID_ARGUMENT, "valid MIR module and output plan are required");
  XsCodegenPlan *created = calloc(1, sizeof(*created));
  if(created == nullptr)
    return set_error(error, XS_CODEGEN_UNITS_ALLOCATION_FAILED, "out of memory while creating a codegen plan");
  for(size_t i = 0; i < xs_mir_module_function_count(module); ++i)
  {
    const XsMirFunction *function = xs_mir_module_function_at(module, i);
    char *unit_name = unit_name_for_function(module, xs_mir_function_name(function));
    if(unit_name == nullptr)
    {
      xs_codegen_plan_destroy(created);
      return set_error(error, XS_CODEGEN_UNITS_ALLOCATION_FAILED, "out of memory while naming a codegen unit");
    }
    XsCodegenUnitPlan *unit = find_unit(created, unit_name);
    if(unit == nullptr)
    {
      XsCodegenUnitsStatus status = append_unit(created, unit_name, &unit, error);
      if(status != XS_CODEGEN_UNITS_OK)
      {
        free(unit_name);
        xs_codegen_plan_destroy(created);
        return status;
      }
    }
    else
    {
      free(unit_name);
    }
    XsCodegenUnitsStatus status = append_function(unit, xs_mir_function_name(function), error);
    if(status != XS_CODEGEN_UNITS_OK)
    {
      xs_codegen_plan_destroy(created);
      return status;
    }
  }
  *plan = created;
  return XS_CODEGEN_UNITS_OK;
}

size_t xs_codegen_plan_unit_count(const XsCodegenPlan *plan)
{
  return plan == nullptr ? 0 : plan->unit_count;
}

const char *xs_codegen_plan_unit_name(const XsCodegenPlan *plan, size_t unit_index)
{
  if(plan == nullptr || unit_index >= plan->unit_count)
    return nullptr;
  return plan->units[unit_index].name;
}

size_t xs_codegen_plan_unit_function_count(const XsCodegenPlan *plan, size_t unit_index)
{
  if(plan == nullptr || unit_index >= plan->unit_count)
    return 0;
  return plan->units[unit_index].function_count;
}

const char *xs_codegen_plan_unit_function_name(const XsCodegenPlan *plan, size_t unit_index, size_t function_index)
{
  if(plan == nullptr || unit_index >= plan->unit_count || function_index >= plan->units[unit_index].function_count)
    return nullptr;
  return plan->units[unit_index].functions[function_index];
}

XsCodegenUnitsStatus xs_codegen_plan_create_from_mono(const XsMonoPlan *mono, XsCodegenPlan **plan,
                                                      XsCodegenUnitsError *error)
{
  clear_error(error);
  if(plan != nullptr)
    *plan = nullptr;
  if(mono == nullptr || plan == nullptr)
    return set_error(error, XS_CODEGEN_UNITS_INVALID_ARGUMENT, "valid mono plan and output plan are required");
  XsCodegenPlan *created = calloc(1, sizeof(*created));
  if(created == nullptr)
    return set_error(error, XS_CODEGEN_UNITS_ALLOCATION_FAILED, "out of memory while creating a codegen plan");
  for(size_t i = 0; i < xs_mono_plan_entry_count(mono); ++i)
  {
    const char *unit_name_view = xs_mono_plan_entry_unit_name(mono, i);
    const char *symbol_name = xs_mono_plan_entry_symbol_name(mono, i);
    if(unit_name_view == nullptr || unit_name_view[0] == '\0' || symbol_name == nullptr || symbol_name[0] == '\0')
    {
      xs_codegen_plan_destroy(created);
      return set_error(error, XS_CODEGEN_UNITS_INVALID_ARGUMENT, "mono plan contains an invalid codegen entry");
    }
    char *unit_name = copy_text(unit_name_view);
    if(unit_name == nullptr)
    {
      xs_codegen_plan_destroy(created);
      return set_error(error, XS_CODEGEN_UNITS_ALLOCATION_FAILED, "out of memory while naming a codegen unit");
    }
    XsCodegenUnitPlan *unit = find_unit(created, unit_name);
    if(unit == nullptr)
    {
      XsCodegenUnitsStatus status = append_unit(created, unit_name, &unit, error);
      if(status != XS_CODEGEN_UNITS_OK)
      {
        free(unit_name);
        xs_codegen_plan_destroy(created);
        return status;
      }
    }
    else
    {
      free(unit_name);
    }
    XsCodegenUnitsStatus status = append_function(unit, symbol_name, error);
    if(status != XS_CODEGEN_UNITS_OK)
    {
      xs_codegen_plan_destroy(created);
      return status;
    }
  }
  *plan = created;
  return XS_CODEGEN_UNITS_OK;
}
