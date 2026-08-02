/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 */

#ifndef XS_HIR_MODULE_INTERNAL_H
#define XS_HIR_MODULE_INTERNAL_H

#include "xs/diagnostic.h"
#include "xs/hir/module_registry.h"
#include "xs/lexer.h"

typedef struct
{
    char **items;
    size_t count;
    size_t capacity;
} StringList;

typedef struct
{
    char *name;
    size_t start;
    size_t end;
} ImportView;

typedef struct
{
    ImportView *items;
    size_t count;
    size_t capacity;
} ImportList;

typedef struct
{
    XsLexer lexer;
    XsToken current;
    const XsSource *source;
} ModuleScanner;

char *xs_module_copy_text(const char *text);
char *xs_module_join_path(const char *left, const char *right);
bool xs_module_append_issue(XsModuleIssues *issues, const char *path, size_t start, size_t end, const char *message);
char *xs_module_read_file(const char *path, size_t *length);
void xs_module_scanner_advance(ModuleScanner *scanner);
char *xs_module_scan_path(ModuleScanner *scanner, size_t *start, size_t *end);

#define copy_text xs_module_copy_text
#define join_path xs_module_join_path
#define append_issue xs_module_append_issue
#define read_file xs_module_read_file
#define scanner_advance xs_module_scanner_advance
#define scan_path xs_module_scan_path

#endif
