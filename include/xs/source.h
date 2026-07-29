/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 */

#ifndef XS_SOURCE_H
#define XS_SOURCE_H

#include <stddef.h>

typedef struct
{
  size_t start;
  size_t end;
} XsSpan;

typedef struct
{
  const char *path;
  const char *module_name;
  const char *text;
  size_t length;
} XsSource;

typedef struct
{
  const char *data;
  size_t length;
} XsText;

static inline XsText xs_source_text(const XsSource *source, XsSpan span)
{
  return (XsText){.data = source->text + span.start, .length = span.end - span.start};
}

#endif
