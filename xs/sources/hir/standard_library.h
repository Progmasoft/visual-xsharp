/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 */

#ifndef XS_HIR_STANDARD_LIBRARY_H
#define XS_HIR_STANDARD_LIBRARY_H

#include "Visual/XSharp/hir/symbol_table.h"

typedef struct
{
    const char *canonical_name;
    const char *short_name;
    const char *required_import;
    const char *alternate_import;
    size_t min_arity;
    size_t max_arity;
} XsHirStandardTypeInfo;

typedef enum
{
    XS_HIR_STANDARD_UNKNOWN,
    XS_HIR_STANDARD_AVAILABLE,
    XS_HIR_STANDARD_MISSING_IMPORT,
} XsHirStandardLookup;

const XsHirStandardTypeInfo *xs_hir_standard_type_find(const char *name);
XsHirStandardLookup xs_hir_standard_type_lookup(const XsHirStandardTypeInfo *type, const XsHirImportScope *import);
XsHirStandardLookup xs_hir_standard_call_lookup(const char *name, const XsHirImportScope *import);
bool xs_hir_standard_module_name(const char *name);

#endif
