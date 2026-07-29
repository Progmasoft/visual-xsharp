/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *read_text(const char *path)
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
  if(text == nullptr)
  {
    fclose(file);
    return nullptr;
  }
  bool success = fread(text, 1, (size_t)size, file) == (size_t)size;
  if(fclose(file) != 0)
    success = false;
  if(!success)
  {
    free(text);
    return nullptr;
  }
  text[size] = '\0';
  return text;
}

int main(int argc, char **argv)
{
  if(argc < 3)
  {
    fprintf(stderr, "text artifact test requires a path and expected text fragments\n");
    return 2;
  }
  char *text = read_text(argv[1]);
  if(text == nullptr)
  {
    fprintf(stderr, "could not read text artifact %s\n", argv[1]);
    return 1;
  }
  int failures = 0;
  for(int index = 2; index < argc; ++index)
  {
    if(strstr(text, argv[index]) == nullptr)
    {
      fprintf(stderr, "%s does not contain expected text: %s\n", argv[1], argv[index]);
      ++failures;
    }
  }
  free(text);
  return failures == 0 ? 0 : 1;
}
