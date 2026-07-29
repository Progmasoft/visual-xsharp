/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 */

#include "xs/mono/plan.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct
{
  char *unit_name;
  char *source_name;
  char *symbol_name;
} XsMonoEntry;

struct XsMonoPlan
{
  XsMonoEntry *entries;
  size_t entry_count;
  size_t entry_capacity;
};

static void clear_error(XsMonoError *error)
{
  if(error != nullptr)
    *error = (XsMonoError){.status = XS_MONO_OK};
}

static XsMonoStatus set_error(XsMonoError *error, XsMonoStatus status, const char *message)
{
  if(error != nullptr)
  {
    error->status = status;
    snprintf(error->message, sizeof(error->message), "%s",
             message == nullptr ? "monomorphization planning error" : message);
  }
  return status;
}

static char *copy_text(const char *text)
{
  size_t length = strlen(text);
  char *copy = malloc(length + 1);
  if(copy != nullptr)
    memcpy(copy, text, length + 1);
  return copy;
}

static char *mangle_concrete_function(const char *name)
{
  size_t length = strlen(name);
  char *symbol = malloc(length + 11);
  if(symbol == nullptr)
    return nullptr;
  memcpy(symbol, "_XS_FN_", 7);
  for(size_t i = 0; i < length; ++i)
  {
    unsigned char character = (unsigned char)name[i];
    symbol[7 + i] = isalnum(character) != 0 ? (char)character : '_';
  }
  memcpy(symbol + 7 + length, "_G0", 4);
  return symbol;
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

static char *copy_range(const char *text, size_t length)
{
  char *copy = malloc(length + 1);
  if(copy == nullptr)
    return nullptr;
  memcpy(copy, text, length);
  copy[length] = '\0';
  return copy;
}

static char *unit_name_for_function(const XsMirModule *module, const char *function_name)
{
  const char *dot = last_dot(function_name);
  if(dot != nullptr && dot != function_name)
    return copy_range(function_name, (size_t)(dot - function_name));
  return copy_text(xs_mir_module_name(module));
}

static void free_entry(XsMonoEntry *entry)
{
  free(entry->unit_name);
  free(entry->source_name);
  free(entry->symbol_name);
  *entry = (XsMonoEntry){0};
}

void xs_mono_plan_destroy(XsMonoPlan *plan)
{
  if(plan == nullptr)
    return;
  for(size_t i = 0; i < plan->entry_count; ++i)
    free_entry(&plan->entries[i]);
  free(plan->entries);
  free(plan);
}

static XsMonoStatus append_entry(XsMonoPlan *plan, XsMonoEntry entry, XsMonoError *error)
{
  if(plan->entry_count == plan->entry_capacity)
  {
    size_t capacity = plan->entry_capacity == 0 ? 8 : plan->entry_capacity * 2;
    XsMonoEntry *entries = realloc(plan->entries, capacity * sizeof(*entries));
    if(entries == nullptr)
      return set_error(error, XS_MONO_ALLOCATION_FAILED, "out of memory while adding a monomorphization entry");
    plan->entries = entries;
    plan->entry_capacity = capacity;
  }
  plan->entries[plan->entry_count++] = entry;
  return XS_MONO_OK;
}

XsMonoStatus xs_mono_plan_create_for_concrete_mir(const XsMirModule *module, XsMonoPlan **plan, XsMonoError *error)
{
  clear_error(error);
  if(plan != nullptr)
    *plan = nullptr;
  if(module == nullptr || plan == nullptr)
    return set_error(error, XS_MONO_INVALID_ARGUMENT, "valid MIR module and output plan are required");
  XsMonoPlan *created = calloc(1, sizeof(*created));
  if(created == nullptr)
    return set_error(error, XS_MONO_ALLOCATION_FAILED, "out of memory while creating a monomorphization plan");
  for(size_t i = 0; i < xs_mir_module_function_count(module); ++i)
  {
    const XsMirFunction *function = xs_mir_module_function_at(module, i);
    const char *name = xs_mir_function_name(function);
    XsMonoEntry entry = {
        .unit_name = unit_name_for_function(module, name),
        .source_name = copy_text(name),
        .symbol_name = mangle_concrete_function(name),
    };
    if(entry.unit_name == nullptr || entry.source_name == nullptr || entry.symbol_name == nullptr)
    {
      free_entry(&entry);
      xs_mono_plan_destroy(created);
      return set_error(error, XS_MONO_ALLOCATION_FAILED, "out of memory while naming a monomorphization entry");
    }
    XsMonoStatus status = append_entry(created, entry, error);
    if(status != XS_MONO_OK)
    {
      free_entry(&entry);
      xs_mono_plan_destroy(created);
      return status;
    }
  }
  *plan = created;
  return XS_MONO_OK;
}

size_t xs_mono_plan_entry_count(const XsMonoPlan *plan)
{
  return plan == nullptr ? 0 : plan->entry_count;
}

const char *xs_mono_plan_entry_unit_name(const XsMonoPlan *plan, size_t index)
{
  if(plan == nullptr || index >= plan->entry_count)
    return nullptr;
  return plan->entries[index].unit_name;
}

const char *xs_mono_plan_entry_source_name(const XsMonoPlan *plan, size_t index)
{
  if(plan == nullptr || index >= plan->entry_count)
    return nullptr;
  return plan->entries[index].source_name;
}

const char *xs_mono_plan_entry_symbol_name(const XsMonoPlan *plan, size_t index)
{
  if(plan == nullptr || index >= plan->entry_count)
    return nullptr;
  return plan->entries[index].symbol_name;
}
