/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 */

#include "model_internal.h"

#include <stdlib.h>
#include <string.h>

static const char *skip_space(const char *cursor, const char *end)
{
  while(cursor < end && (*cursor == ' ' || *cursor == '\t'))
    ++cursor;
  return cursor;
}

static bool parse_type_cursor(const XsLilModule *module, const char **cursor, const char *end, XsLilType *type)
{
  const char *start = skip_space(*cursor, end);
  const char *type_end = start;
  while(type_end < end && *type_end != ' ' && *type_end != '\t' && *type_end != ',' && *type_end != ')')
    ++type_end;
  if(type_end == start || !xs_lil_parse_type_name(module, start, (size_t)(type_end - start), type))
    return false;
  *cursor = skip_space(type_end, end);
  return true;
}

static bool append_type(XsLilType **items, size_t *count, XsLilType type)
{
  XsLilType *grown = realloc(*items, (*count + 1U) * sizeof(*grown));
  if(grown == nullptr)
    return false;
  *items = grown;
  (*items)[(*count)++] = type;
  return true;
}

bool xs_lil_parse_signature(const XsLilModule *module, const char *line, size_t length, const char *prefix, char **name,
                            XsLilType *return_type, XsLilType **parameters, size_t *parameter_count)
{
  const char *end = line + length;
  const char *cursor = line + strlen(prefix);
  const char *colon = cursor;
  while(colon < end && *colon != ':')
    ++colon;
  if(colon == end)
    return false;
  const char *name_end = colon;
  while(name_end > cursor && (name_end[-1] == ' ' || name_end[-1] == '\t'))
    --name_end;
  if(name_end == cursor)
    return false;
  *name = xs_lil_copy_span(cursor, (size_t)(name_end - cursor));
  if(*name == nullptr)
    return false;
  cursor = skip_space(colon + 1, end);
  if(cursor == end || *cursor++ != '(')
    return false;
  cursor = skip_space(cursor, end);
  while(cursor < end && *cursor != ')')
  {
    XsLilType parameter = {0};
    if(!parse_type_cursor(module, &cursor, end, &parameter) || parameter.kind == XS_LIL_TYPE_VOID ||
       !append_type(parameters, parameter_count, parameter))
      return false;
    if(cursor < end && *cursor == ',')
      cursor = skip_space(cursor + 1, end);
    else if(cursor < end && *cursor != ')')
      return false;
  }
  if(cursor == end || *cursor++ != ')')
    return false;
  cursor = skip_space(cursor, end);
  if((size_t)(end - cursor) < 2U || cursor[0] != '-' || cursor[1] != '>')
    return false;
  cursor = skip_space(cursor + 2, end);
  return parse_type_cursor(module, &cursor, end, return_type) && skip_space(cursor, end) == end;
}
