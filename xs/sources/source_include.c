/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 */

#include "Visual/C23/source_include.h"

#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define XS_INCLUDE_MAX_DEPTH 64U

typedef struct
{
    char *data;
    size_t length;
    size_t capacity;
} IncludeBuffer;

static bool buffer_reserve(IncludeBuffer *buffer, size_t extra)
{
    if(extra > SIZE_MAX - buffer->length - 1U)
        return false;
    size_t required = buffer->length + extra + 1U;
    if(required <= buffer->capacity)
        return true;
    size_t capacity = buffer->capacity == 0 ? 256 : buffer->capacity;
    while(capacity < required)
    {
        if(capacity > SIZE_MAX / 2U)
            return false;
        capacity *= 2U;
    }
    char *data = realloc(buffer->data, capacity);
    if(data == nullptr)
        return false;
    buffer->data = data;
    buffer->capacity = capacity;
    return true;
}

static bool buffer_append(IncludeBuffer *buffer, const char *text, size_t length)
{
    if(!buffer_reserve(buffer, length))
        return false;
    memcpy(buffer->data + buffer->length, text, length);
    buffer->length += length;
    buffer->data[buffer->length] = '\0';
    return true;
}

static bool starts_with(const XsSource *source, size_t cursor, const char *text)
{
    size_t length = strlen(text);
    return cursor + length <= source->length && memcmp(source->text + cursor, text, length) == 0;
}

static bool is_identifier_continue(char value)
{
    unsigned char character = (unsigned char)value;
    return character == '_' || isalnum(character) != 0;
}

static bool is_include_call_start(const XsSource *source, size_t cursor)
{
    size_t length = strlen("include!");
    if(!starts_with(source, cursor, "include!"))
        return false;
    if(cursor != 0 && is_identifier_continue(source->text[cursor - 1U]))
        return false;
    size_t after = cursor + length;
    return after >= source->length || !is_identifier_continue(source->text[after]);
}

static size_t skip_space(const XsSource *source, size_t cursor)
{
    while(cursor < source->length)
    {
        char value = source->text[cursor];
        if(value != ' ' && value != '\t' && value != '\r' && value != '\n')
            break;
        ++cursor;
    }
    return cursor;
}

static size_t skip_line_comment(const XsSource *source, size_t cursor)
{
    while(cursor < source->length && source->text[cursor] != '\n')
        ++cursor;
    return cursor;
}

static size_t skip_string_or_character(const XsSource *source, size_t cursor)
{
    bool triple = starts_with(source, cursor, "\"\"\"");
    char quote = source->text[cursor];
    cursor += triple ? 3 : 1;
    while(cursor < source->length)
    {
        if(triple && starts_with(source, cursor, "\"\"\""))
            return cursor + 3;
        if(!triple && source->text[cursor] == quote)
            return cursor + 1;
        if(!triple && source->text[cursor] == '\\' && cursor + 1 < source->length)
            cursor += 2;
        else
            ++cursor;
    }
    return cursor;
}

static char *copy_slice(const char *text, size_t length)
{
    char *copy = malloc(length + 1U);
    if(copy == nullptr)
        return nullptr;
    memcpy(copy, text, length);
    copy[length] = '\0';
    return copy;
}

static char *read_file(const char *path, size_t *length)
{
    FILE *file = fopen(path, "rb");
    if(file == nullptr)
        return nullptr;
    if(fseek(file, 0, SEEK_END) != 0)
    {
        fclose(file);
        return nullptr;
    }
    long size = ftell(file);
    if(size < 0 || fseek(file, 0, SEEK_SET) != 0 || (uintmax_t)size > (uintmax_t)SIZE_MAX - 1U)
    {
        fclose(file);
        return nullptr;
    }
    char *text = calloc((size_t)size + 1U, 1U);
    if(text == nullptr)
    {
        fclose(file);
        return nullptr;
    }
    size_t read = fread(text, 1, (size_t)size, file);
    fclose(file);
    if(read != (size_t)size)
    {
        free(text);
        return nullptr;
    }
    *length = read;
    return text;
}

static char *source_relative_path(const char *source_path, const char *relative)
{
    const char *slash = strrchr(source_path, '/');
    size_t directory_length = slash == nullptr ? 0 : (size_t)(slash - source_path + 1);
    size_t relative_length = strlen(relative);
    char *path = malloc(directory_length + relative_length + 1U);
    if(path == nullptr)
        return nullptr;
    memcpy(path, source_path, directory_length);
    memcpy(path + directory_length, relative, relative_length + 1U);
    return path;
}

static bool parse_include_path(const XsSource *source, size_t cursor, size_t *end, char **path)
{
    cursor = skip_space(source, cursor);
    if(cursor >= source->length || source->text[cursor++] != '(')
        return false;
    cursor = skip_space(source, cursor);
    if(cursor >= source->length || source->text[cursor++] != '"')
        return false;
    size_t path_start = cursor;
    while(cursor < source->length && source->text[cursor] != '"' && source->text[cursor] != '\n')
    {
        if(source->text[cursor] == '\\')
            return false;
        ++cursor;
    }
    if(cursor >= source->length || source->text[cursor] != '"')
        return false;
    *path = copy_slice(source->text + path_start, cursor - path_start);
    if(*path == nullptr)
        return false;
    cursor = skip_space(source, cursor + 1U);
    if(cursor >= source->length || source->text[cursor++] != ')')
    {
        free(*path);
        *path = nullptr;
        return false;
    }
    cursor = skip_space(source, cursor);
    if(cursor < source->length && source->text[cursor] == ';')
        ++cursor;
    *end = cursor;
    return true;
}

static bool is_local_relative_path(const char *path)
{
    return path[0] != '\0' && path[0] != '/' && strchr(path, ':') == nullptr;
}

static bool expand_source(const XsSource *source, XsDiagnostics *diagnostics, IncludeBuffer *buffer, size_t depth)
{
    if(depth > XS_INCLUDE_MAX_DEPTH)
    {
        xs_diagnostics_add(diagnostics, XS_DIAGNOSTIC_ERROR, (XsSpan){0, 0}, "include! nesting is too deep");
        return false;
    }
    size_t cursor = 0;
    size_t chunk_start = 0;
    while(cursor < source->length)
    {
        if(source->text[cursor] == '"' || source->text[cursor] == '\'')
        {
            cursor = skip_string_or_character(source, cursor);
            continue;
        }
        if(starts_with(source, cursor, "//"))
        {
            cursor = skip_line_comment(source, cursor);
            continue;
        }
        if(!is_include_call_start(source, cursor))
        {
            ++cursor;
            continue;
        }
        char *relative = nullptr;
        size_t include_end = cursor;
        if(!parse_include_path(source, cursor + strlen("include!"), &include_end, &relative))
        {
            xs_diagnostics_add(diagnostics, XS_DIAGNOSTIC_ERROR, (XsSpan){cursor, cursor + strlen("include!")},
                               "include! requires a local string path");
            return false;
        }
        if(!is_local_relative_path(relative))
        {
            free(relative);
            xs_diagnostics_add(diagnostics, XS_DIAGNOSTIC_ERROR, (XsSpan){cursor, include_end},
                               "include! only accepts local relative file paths");
            return false;
        }
        char *path = source_relative_path(source->path, relative);
        free(relative);
        if(path == nullptr || !buffer_append(buffer, source->text + chunk_start, cursor - chunk_start))
        {
            free(path);
            return false;
        }
        size_t included_length = 0;
        char *included_text = read_file(path, &included_length);
        if(included_text == nullptr)
        {
            free(path);
            xs_diagnostics_add(diagnostics, XS_DIAGNOSTIC_ERROR, (XsSpan){cursor, include_end},
                               "included file cannot be read");
            return false;
        }
        XsSource included = {.path = path, .text = included_text, .length = included_length};
        bool success = expand_source(&included, diagnostics, buffer, depth + 1U);
        free(included_text);
        free(path);
        if(!success || !buffer_append(buffer, "\n", 1))
            return false;
        cursor = include_end;
        chunk_start = cursor;
    }
    return buffer_append(buffer, source->text + chunk_start, source->length - chunk_start);
}

bool xs_source_expand_include_macros(const XsSyntaxTree *tree, XsDiagnostics *diagnostics, XsIncludedSource *expanded)
{
    if(tree == nullptr || tree->root == nullptr || tree->source == nullptr || diagnostics == nullptr ||
       expanded == nullptr)
        return false;
    *expanded = (XsIncludedSource){0};
    IncludeBuffer buffer = {0};
    if(!expand_source(tree->source, diagnostics, &buffer, 0))
    {
        free(buffer.data);
        return false;
    }
    expanded->text = buffer.data;
    expanded->length = buffer.length;
    return !xs_diagnostics_has_error(diagnostics);
}

void xs_included_source_free(XsIncludedSource *expanded)
{
    if(expanded == nullptr)
        return;
    free(expanded->text);
    *expanded = (XsIncludedSource){0};
}
