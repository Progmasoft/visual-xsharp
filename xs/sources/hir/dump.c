/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 */

#include "xs/hir/dump.h"

static const char *visibility_name(XsSyntaxVisibility visibility)
{
  switch(visibility)
  {
  case XS_SYNTAX_VISIBILITY_PUBLIC:
    return "public";
  case XS_SYNTAX_VISIBILITY_PRIVATE:
    return "private";
  case XS_SYNTAX_VISIBILITY_PROTECTED:
    return "protected";
  case XS_SYNTAX_VISIBILITY_INTERNAL:
    return "internal";
  case XS_SYNTAX_VISIBILITY_DEFAULT:
    return "default";
  }
  return "default";
}

bool xs_hir_write_symbols(const XsHirSymbolTable *symbols, FILE *stream)
{
  if(symbols == nullptr || stream == nullptr)
    return false;
  if(fprintf(stream, "hir symbols %zu\n", symbols->count) < 0)
    return false;
  for(size_t i = 0; i < symbols->count; ++i)
  {
    const XsHirSymbol *symbol = &symbols->symbols[i];
    if(fprintf(stream, "%s %s %s\n", xs_hir_symbol_kind_name(symbol->kind), visibility_name(symbol->visibility),
               symbol->qualified_name) < 0)
      return false;
  }
  return true;
}
