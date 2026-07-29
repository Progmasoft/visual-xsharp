/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 */

#include "xs/diagnostic.h"
#include "xs/syntax_parser.h"

#include <stdio.h>
#include <stdlib.h>

static char *read_source(const char *path, size_t *length)
{
  FILE *file = fopen(path, "rb");
  if(file == nullptr || fseek(file, 0, SEEK_END) != 0)
  {
    if(file != nullptr)
      fclose(file);
    return nullptr;
  }
  long size = ftell(file);
  if(size < 0 || fseek(file, 0, SEEK_SET) != 0)
  {
    fclose(file);
    return nullptr;
  }
  char *text = malloc((size_t)size + 1);
  if(text == nullptr || fread(text, 1, (size_t)size, file) != (size_t)size)
  {
    free(text);
    fclose(file);
    return nullptr;
  }
  fclose(file);
  text[(size_t)size] = '\0';
  *length = (size_t)size;
  return text;
}

static bool parses(const char *path, uint64_t file_id)
{
  size_t length = 0;
  char *text = read_source(path, &length);
  if(text == nullptr)
  {
    fprintf(stderr, "could not read %s\n", path);
    return false;
  }
  XsSource source = {.path = path, .text = text, .length = length};
  XsDiagnostics diagnostics;
  XsSyntaxTree tree;
  xs_diagnostics_init(&diagnostics);
  bool success = xs_syntax_parse(&source, file_id, &diagnostics, &tree);
  if(!success)
    xs_diagnostics_print(&diagnostics, &source, stderr);
  xs_syntax_tree_free(&tree);
  xs_diagnostics_free(&diagnostics);
  free(text);
  return success;
}

int main(int argc, char **argv)
{
  bool success = argc > 1;
  for(int index = 1; index < argc; ++index)
    success = parses(argv[index], (uint64_t)index) && success;
  return success ? 0 : 1;
}
