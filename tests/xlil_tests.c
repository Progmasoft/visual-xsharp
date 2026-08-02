/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 */

#include "xs/lil.h"
#include "xs/lil-c/aot.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int failures;

#define CHECK(condition)                                                                                               \
  do                                                                                                                   \
  {                                                                                                                    \
    if(!(condition))                                                                                                   \
    {                                                                                                                  \
      fprintf(stderr, "%s:%d: check failed: %s\n", __FILE__, __LINE__, #condition);                                    \
      ++failures;                                                                                                      \
    }                                                                                                                  \
  } while(0)

static void test_module_and_text_writer(void)
{
  XsLilError error = {0};
  XsLilModule *module = nullptr;
  CHECK(xs_lil_module_create("App.Main", &module, &error) == XS_LIL_OK);
  CHECK(module != nullptr);
  CHECK(strcmp(xs_lil_module_name(module), "App.Main") == 0);
  const XsLilType parameters[] = {
      {.kind = XS_LIL_TYPE_U8},
      {.kind = XS_LIL_TYPE_I8},
      {.kind = XS_LIL_TYPE_BOOL},
      {.kind = XS_LIL_TYPE_STR},
  };
  CHECK(xs_lil_module_add_function(module, "Check", (XsLilType){.kind = XS_LIL_TYPE_BOOL}, parameters, 4, &error) ==
        XS_LIL_OK);
  CHECK(xs_lil_module_function_count(module) == 1);
  CHECK(strcmp(xs_lil_type_name((XsLilType){.kind = XS_LIL_TYPE_BOOL}), "bool") == 0);
  CHECK(strcmp(xs_lil_type_name((XsLilType){.kind = XS_LIL_TYPE_U8}), "u8") == 0);
  CHECK(strcmp(xs_lil_type_name((XsLilType){.kind = XS_LIL_TYPE_I8}), "i8") == 0);
  CHECK(strcmp(xs_lil_type_name((XsLilType){.kind = XS_LIL_TYPE_STR}), "str") == 0);

  FILE *stream = tmpfile();
  if(stream == nullptr)
  {
    ++failures;
    xs_lil_module_destroy(module);
    return;
  }
  CHECK(xs_lil_module_write_text(module, stream, &error) == XS_LIL_OK);
  CHECK(fseek(stream, 0, SEEK_SET) == 0);
  char buffer[128] = {0};
  CHECK(fgets(buffer, sizeof(buffer), stream) != nullptr);
  CHECK(strstr(buffer, ".xlil version 1\n") != nullptr);
  CHECK(fgets(buffer, sizeof(buffer), stream) != nullptr);
  CHECK(strstr(buffer, ".xlil module App.Main\n") != nullptr);
  CHECK(fgets(buffer, sizeof(buffer), stream) != nullptr);
  CHECK(strstr(buffer, ".extern Check : (u8, i8, bool, str) -> bool\n") != nullptr);
  fclose(stream);
  xs_lil_module_destroy(module);
}

static void test_aggregate_type_registry_round_trip(void)
{
  const char *text = ".xlil version 1\n.xlil module Geometry\n.type %t0 Point : (i32, i32)\n"
                     ".type %t1 Line : (%t0, %t0)\n.extern midpoint : (%t1) -> %t0\n"
                     ".func point_x : (i32, i32) -> i32\n.param %r0:i32\n.param %r1:i32\n"
                     "bb0.entry:\n  %r2:%t0 = aggregate %r0, %r1\n  %r3:i32 = extract %r2, 0\n  ret %r3\n.end\n";
  XsLilError error = {0};
  XsLilModule *module = nullptr;
  CHECK(xs_lil_module_parse_text("geometry.xlil", text, strlen(text), &module, &error) == XS_LIL_OK);
  CHECK(module != nullptr);
  if(module == nullptr)
    return;
  CHECK(xs_lil_module_aggregate_type_count(module) == 2);
  CHECK(strcmp(xs_lil_module_aggregate_type_name(module, 0), "Point") == 0);
  CHECK(xs_lil_module_aggregate_field_count(module, 1) == 2);
  CHECK(xs_lil_type_equal(xs_lil_module_aggregate_field_type(module, 1, 0), xs_lil_aggregate_type(0)));
  CHECK(xs_lil_type_equal(xs_lil_function_return_type(xs_lil_module_function_at(module, 0)), xs_lil_aggregate_type(0)));
  const XsLilBlock *block = xs_lil_function_block_at(xs_lil_module_function_at(module, 1), 0);
  CHECK(xs_lil_block_instruction_kind(block, 0) == XS_LIL_INSTRUCTION_AGGREGATE);
  CHECK(xs_lil_block_instruction_kind(block, 1) == XS_LIL_INSTRUCTION_EXTRACT);
  CHECK(xs_lil_block_instruction_argument_count(block, 0) == 2);
  FILE *stream = tmpfile();
  CHECK(stream != nullptr);
  if(stream != nullptr)
  {
    CHECK(xs_lil_module_write_text(module, stream, &error) == XS_LIL_OK);
    CHECK(fseek(stream, 0, SEEK_SET) == 0);
    char output[512] = {0};
    size_t read = fread(output, 1, sizeof(output) - 1, stream);
    output[read] = '\0';
    CHECK(strcmp(output, text) == 0);
    fclose(stream);
  }
  xs_lil_module_destroy(module);
}

static void test_array_type_registry_round_trip(void)
{
  static const char text[] = ".xlil version 1\n.xlil module Arrays\n.array %a0 : i32 x 3\n"
                             ".func second : (i32, i32, i32) -> i32\n.param %r0:i32\n.param %r1:i32\n"
                             ".param %r2:i32\nbb0.entry:\n  %r3:%a0 = array %r0, %r1, %r2\n"
                             "  %r4:i32 = extract.array %r3, 1\n  ret %r4\n.end\n";
  XsLilError error = {0};
  XsLilModule *module = nullptr;
  CHECK(xs_lil_module_parse_text("arrays.xlil", text, strlen(text), &module, &error) == XS_LIL_OK);
  CHECK(module != nullptr);
  if(module == nullptr)
    return;
  CHECK(xs_lil_module_array_type_count(module) == 1);
  CHECK(xs_lil_module_array_element_type(module, 0).kind == XS_LIL_TYPE_I32);
  CHECK(xs_lil_module_array_length(module, 0) == 3);
  CHECK(xs_lil_module_verify(module, &error) == XS_LIL_OK);
  const XsLilBlock *block = xs_lil_function_block_at(xs_lil_module_function_at(module, 0), 0);
  CHECK(xs_lil_block_instruction_kind(block, 0) == XS_LIL_INSTRUCTION_AGGREGATE);
  CHECK(xs_lil_function_value_type(xs_lil_module_function_at(module, 0), xs_lil_block_instruction_result(block, 0))
            .kind == XS_LIL_TYPE_ARRAY);
  FILE *stream = tmpfile();
  CHECK(stream != nullptr);
  if(stream != nullptr)
  {
    CHECK(xs_lil_module_write_text(module, stream, &error) == XS_LIL_OK);
    CHECK(fseek(stream, 0, SEEK_SET) == 0);
    char output[512] = {0};
    size_t read = fread(output, 1, sizeof(output) - 1U, stream);
    output[read] = '\0';
    CHECK(strcmp(output, text) == 0);
    fclose(stream);
  }
  xs_lil_module_destroy(module);
}

static void test_dynamic_array_access_round_trip(void)
{
  static const char text[] = ".xlil version 1\n.xlil module DynamicArrays\n.array %a0 : i32\n"
                             ".func replace : (%a0, i64, i32) -> i32\n.param %r0:%a0\n.param %r1:i64\n"
                             ".param %r2:i32\nbb0.entry:\n  %r3:%a0 = array.set %r0, %r1, %r2\n"
                             "  %r4:i64 = len.array %r3\n  %r5:i32 = array.get %r3, %r1\n  ret %r5\n.end\n";
  XsLilError error = {0};
  XsLilModule *module = nullptr;
  CHECK(xs_lil_module_parse_text("dynamic_arrays.xlil", text, strlen(text), &module, &error) == XS_LIL_OK);
  CHECK(module != nullptr);
  if(module == nullptr)
    return;
  CHECK(xs_lil_module_array_is_dynamic(module, 0));
  CHECK(xs_lil_module_verify(module, &error) == XS_LIL_OK);
  const XsLilBlock *block = xs_lil_function_block_at(xs_lil_module_function_at(module, 0), 0);
  CHECK(xs_lil_block_instruction_kind(block, 0) == XS_LIL_INSTRUCTION_ARRAY_SET);
  CHECK(xs_lil_block_instruction_kind(block, 1) == XS_LIL_INSTRUCTION_ARRAY_LENGTH);
  CHECK(xs_lil_block_instruction_kind(block, 2) == XS_LIL_INSTRUCTION_ARRAY_GET);
  CHECK(xs_lil_block_instruction_argument(block, 0, 0) == 2);
  FILE *stream = tmpfile();
  CHECK(stream != nullptr);
  if(stream != nullptr)
  {
    CHECK(xs_lil_module_write_text(module, stream, &error) == XS_LIL_OK);
    CHECK(fseek(stream, 0, SEEK_SET) == 0);
    char output[512] = {0};
    size_t read = fread(output, 1, sizeof(output) - 1U, stream);
    output[read] = '\0';
    CHECK(strcmp(output, text) == 0);
    fclose(stream);
  }
  xs_lil_module_destroy(module);
}

static void test_parser_accepts_non_null_terminated_registry_text(void)
{
  static const char source[] = ".xlil version 1\n.xlil module Bounded\n.type %t0 Pair : (i32, i32)\n"
                               ".array %a0 : i32\n.array %a1 : i32 x 2\n"
                               ".extern consume : (%a0, %a1, %t0) -> void\n";
  const size_t length = sizeof(source) - 1U;
  char *text = malloc(length);
  CHECK(text != nullptr);
  if(text == nullptr)
    return;
  memcpy(text, source, length);
  XsLilError error = {0};
  XsLilModule *module = nullptr;
  CHECK(xs_lil_module_parse_text("bounded.xlil", text, length, &module, &error) == XS_LIL_OK);
  CHECK(module != nullptr);
  if(module != nullptr)
  {
    CHECK(xs_lil_module_aggregate_type_count(module) == 1);
    CHECK(xs_lil_module_array_type_count(module) == 2);
    CHECK(xs_lil_module_array_is_dynamic(module, 0));
    CHECK(!xs_lil_module_array_is_dynamic(module, 1));
  }
  xs_lil_module_destroy(module);
  free(text);
}

static void test_function_body_text_writer(void)
{
  XsLilError error = {0};
  XsLilModule *module = nullptr;
  CHECK(xs_lil_module_create("App.Body", &module, &error) == XS_LIL_OK);
  XsLilFunction *function = nullptr;
  CHECK(xs_lil_module_add_function_definition(module, "Answer", (XsLilType){.kind = XS_LIL_TYPE_I64}, nullptr, 0,
                                              &function, &error) == XS_LIL_OK);
  CHECK(function != nullptr);
  XsLilBlock *entry = nullptr;
  CHECK(xs_lil_function_append_block(function, "entry", &entry, &error) == XS_LIL_OK);
  XsLilValueId value = 0;
  CHECK(xs_lil_block_add_const_i64(entry, 42, &value, &error) == XS_LIL_OK);
  CHECK(xs_lil_block_set_return_value(entry, value, &error) == XS_LIL_OK);

  FILE *stream = tmpfile();
  if(stream == nullptr)
  {
    ++failures;
    xs_lil_module_destroy(module);
    return;
  }
  CHECK(xs_lil_module_write_text(module, stream, &error) == XS_LIL_OK);
  CHECK(fseek(stream, 0, SEEK_SET) == 0);
  char buffer[256] = {0};
  size_t read = fread(buffer, 1, sizeof(buffer) - 1, stream);
  buffer[read] = '\0';
  CHECK(strstr(buffer, ".func Answer : () -> i64\n") != nullptr);
  CHECK(strstr(buffer, "bb0.entry:\n  %r0:i64 = const.i64 42\n  ret %r0\n.end\n") != nullptr);
  fclose(stream);
  xs_lil_module_destroy(module);
}

static void test_function_body_rejects_missing_return_value(void)
{
  XsLilError error = {0};
  XsLilModule *module = nullptr;
  CHECK(xs_lil_module_create("App.BadBody", &module, &error) == XS_LIL_OK);
  XsLilFunction *function = nullptr;
  CHECK(xs_lil_module_add_function_definition(module, "Bad", (XsLilType){.kind = XS_LIL_TYPE_I64}, nullptr, 0,
                                              &function, &error) == XS_LIL_OK);
  XsLilBlock *entry = nullptr;
  CHECK(xs_lil_function_append_block(function, "entry", &entry, &error) == XS_LIL_OK);
  CHECK(xs_lil_block_set_return(entry, &error) == XS_LIL_INVALID_ARGUMENT);
  xs_lil_module_destroy(module);
}

static void test_floating_constant_bits(void)
{
  const char *text = ".xlil version 1\n.xlil module FloatConstants\n"
                     ".func F32Value : () -> f32\nbb0.entry:\n"
                     "  %r0:f32 = const.f32 0x3fc00000\n  ret %r0\n.end\n"
                     ".func F64Value : () -> f64\nbb0.entry:\n"
                     "  %r0:f64 = const.f64 0x3ff8000000000000\n  ret %r0\n.end\n";
  XsLilError error = {0};
  XsLilModule *module = nullptr;
  CHECK(xs_lil_module_parse_text("FloatConstants.xlil", text, strlen(text), &module, &error) == XS_LIL_OK);
  CHECK(module != nullptr);
  CHECK(xs_lil_module_verify(module, &error) == XS_LIL_OK);
  const XsLilFunction *f32 = xs_lil_module_function_at(module, 0);
  const XsLilFunction *f64 = xs_lil_module_function_at(module, 1);
  CHECK(xs_lil_block_instruction_kind(xs_lil_function_block_at(f32, 0), 0) == XS_LIL_INSTRUCTION_CONST_F32);
  CHECK(xs_lil_block_instruction_float_bits(xs_lil_function_block_at(f32, 0), 0) == UINT32_C(0x3fc00000));
  CHECK(xs_lil_block_instruction_kind(xs_lil_function_block_at(f64, 0), 0) == XS_LIL_INSTRUCTION_CONST_F64);
  CHECK(xs_lil_block_instruction_float_bits(xs_lil_function_block_at(f64, 0), 0) == UINT64_C(0x3ff8000000000000));
  xs_lil_module_destroy(module);
}

static void test_function_body_branch_text_writer(void)
{
  XsLilError error = {0};
  XsLilModule *module = nullptr;
  CHECK(xs_lil_module_create("App.Branch", &module, &error) == XS_LIL_OK);
  XsLilFunction *function = nullptr;
  CHECK(xs_lil_module_add_function_definition(module, "Jump", (XsLilType){.kind = XS_LIL_TYPE_VOID}, nullptr, 0,
                                              &function, &error) == XS_LIL_OK);
  XsLilBlock *entry = nullptr;
  XsLilBlock *exit = nullptr;
  CHECK(xs_lil_function_append_block(function, "entry", &entry, &error) == XS_LIL_OK);
  CHECK(xs_lil_function_append_block(function, "exit", &exit, &error) == XS_LIL_OK);
  CHECK(xs_lil_block_set_branch(entry, 1, &error) == XS_LIL_OK);
  CHECK(xs_lil_block_set_return(exit, &error) == XS_LIL_OK);

  FILE *stream = tmpfile();
  if(stream == nullptr)
  {
    ++failures;
    xs_lil_module_destroy(module);
    return;
  }
  CHECK(xs_lil_module_write_text(module, stream, &error) == XS_LIL_OK);
  CHECK(fseek(stream, 0, SEEK_SET) == 0);
  char buffer[256] = {0};
  size_t read = fread(buffer, 1, sizeof(buffer) - 1, stream);
  buffer[read] = '\0';
  CHECK(strstr(buffer, "bb0.entry:\n  br bb1\nbb1.exit:\n  ret\n.end\n") != nullptr);
  fclose(stream);
  xs_lil_module_destroy(module);
}

static void test_function_body_branch_if_text_writer(void)
{
  XsLilError error = {0};
  XsLilModule *module = nullptr;
  CHECK(xs_lil_module_create("App.BranchIf", &module, &error) == XS_LIL_OK);
  XsLilFunction *function = nullptr;
  CHECK(xs_lil_module_add_function_definition(module, "Choose", (XsLilType){.kind = XS_LIL_TYPE_VOID}, nullptr, 0,
                                              &function, &error) == XS_LIL_OK);
  XsLilBlock *entry = nullptr;
  XsLilBlock *then_block = nullptr;
  XsLilBlock *else_block = nullptr;
  CHECK(xs_lil_function_append_block(function, "entry", &entry, &error) == XS_LIL_OK);
  CHECK(xs_lil_function_append_block(function, "then", &then_block, &error) == XS_LIL_OK);
  CHECK(xs_lil_function_append_block(function, "else", &else_block, &error) == XS_LIL_OK);
  XsLilValueId condition = 0;
  CHECK(xs_lil_block_add_const_bool(entry, true, &condition, &error) == XS_LIL_OK);
  CHECK(xs_lil_block_set_branch_if(entry, condition, 1, 2, &error) == XS_LIL_OK);
  CHECK(xs_lil_block_set_return(then_block, &error) == XS_LIL_OK);
  CHECK(xs_lil_block_set_return(else_block, &error) == XS_LIL_OK);

  FILE *stream = tmpfile();
  if(stream == nullptr)
  {
    ++failures;
    xs_lil_module_destroy(module);
    return;
  }
  CHECK(xs_lil_module_write_text(module, stream, &error) == XS_LIL_OK);
  CHECK(fseek(stream, 0, SEEK_SET) == 0);
  char buffer[1024] = {0};
  size_t read = fread(buffer, 1, sizeof(buffer) - 1, stream);
  buffer[read] = '\0';
  CHECK(strstr(buffer, "bb0.entry:\n  %r0:bool = const.bool true\n  br_if %r0, bb1, bb2\n") != nullptr);
  fclose(stream);
  xs_lil_module_destroy(module);
}

static void test_function_body_rejects_non_bool_branch_if(void)
{
  XsLilError error = {0};
  XsLilModule *module = nullptr;
  CHECK(xs_lil_module_create("App.BadBranchIf", &module, &error) == XS_LIL_OK);
  XsLilFunction *function = nullptr;
  CHECK(xs_lil_module_add_function_definition(module, "Bad", (XsLilType){.kind = XS_LIL_TYPE_VOID}, nullptr, 0,
                                              &function, &error) == XS_LIL_OK);
  XsLilBlock *entry = nullptr;
  XsLilBlock *then_block = nullptr;
  XsLilBlock *else_block = nullptr;
  CHECK(xs_lil_function_append_block(function, "entry", &entry, &error) == XS_LIL_OK);
  CHECK(xs_lil_function_append_block(function, "then", &then_block, &error) == XS_LIL_OK);
  CHECK(xs_lil_function_append_block(function, "else", &else_block, &error) == XS_LIL_OK);
  XsLilValueId condition = 0;
  CHECK(xs_lil_block_add_const_i64(entry, 1, &condition, &error) == XS_LIL_OK);
  CHECK(xs_lil_block_set_branch_if(entry, condition, 1, 2, &error) == XS_LIL_INVALID_ARGUMENT);
  xs_lil_module_destroy(module);
}

static void test_text_parser_reads_external_signature(void)
{
  const char text[] = ".xlil version 0\n.xlil module App\n.extern Import : (i64) -> i64\n";
  XsLilError error = {0};
  XsLilModule *module = nullptr;
  CHECK(xs_lil_module_parse_text("extern.xlil", text, strlen(text), &module, &error) == XS_LIL_OK);
  CHECK(module != nullptr);
  CHECK(strcmp(xs_lil_module_name(module), "App") == 0);
  CHECK(xs_lil_module_function_count(module) == 1);
  const XsLilFunction *function = xs_lil_module_function_at(module, 0);
  CHECK(function != nullptr);
  CHECK(strcmp(xs_lil_function_name(function), "Import") == 0);
  CHECK(!xs_lil_function_is_definition(function));
  CHECK(xs_lil_function_return_type(function).kind == XS_LIL_TYPE_I64);
  CHECK(xs_lil_function_parameter_count(function) == 1);
  CHECK(xs_lil_function_parameter_type(function, 0).kind == XS_LIL_TYPE_I64);
  xs_lil_module_destroy(module);
}

static void test_text_parser_reads_function_definition(void)
{
  const char text[] = ".xlil version 1\n.xlil module App\n.func Main : () -> void\nbb0.entry:\n  ret\n.end\n";
  XsLilError error = {0};
  XsLilModule *module = nullptr;
  CHECK(xs_lil_module_parse_text("func.xlil", text, strlen(text), &module, &error) == XS_LIL_OK);
  const XsLilFunction *function = xs_lil_module_function_at(module, 0);
  CHECK(function != nullptr);
  CHECK(strcmp(xs_lil_function_name(function), "Main") == 0);
  CHECK(xs_lil_function_is_definition(function));
  CHECK(xs_lil_function_return_type(function).kind == XS_LIL_TYPE_VOID);
  xs_lil_module_destroy(module);
}

static void test_text_parser_round_trips_supported_body_subset(void)
{
  const char text[] =
      ".xlil version 1\n.xlil module App\n.func Answer : () -> i64\nbb0.entry:\n  %r0:i64 = const.i64 42\n "
      " ret %r0\n.end\n";
  XsLilError error = {0};
  XsLilModule *module = nullptr;
  CHECK(xs_lil_module_parse_text("body.xlil", text, strlen(text), &module, &error) == XS_LIL_OK);
  FILE *stream = tmpfile();
  if(stream == nullptr)
  {
    ++failures;
    xs_lil_module_destroy(module);
    return;
  }
  CHECK(xs_lil_module_write_text(module, stream, &error) == XS_LIL_OK);
  CHECK(fseek(stream, 0, SEEK_SET) == 0);
  char buffer[256] = {0};
  size_t read = fread(buffer, 1, sizeof(buffer) - 1, stream);
  buffer[read] = '\0';
  CHECK(strstr(buffer, ".func Answer : () -> i64\n") != nullptr);
  CHECK(strstr(buffer, "bb0.entry:\n  %r0:i64 = const.i64 42\n  ret %r0\n.end\n") != nullptr);
  fclose(stream);
  xs_lil_module_destroy(module);
}

static void test_text_parser_round_trips_branch_subset(void)
{
  const char text[] =
      ".xlil version 1\n.xlil module App\n.func Jump : () -> void\nbb0.entry:\n  br bb1\nbb1.exit:\n  ret\n.end\n";
  XsLilError error = {0};
  XsLilModule *module = nullptr;
  CHECK(xs_lil_module_parse_text("branch.xlil", text, strlen(text), &module, &error) == XS_LIL_OK);
  FILE *stream = tmpfile();
  if(stream == nullptr)
  {
    ++failures;
    xs_lil_module_destroy(module);
    return;
  }
  CHECK(xs_lil_module_write_text(module, stream, &error) == XS_LIL_OK);
  CHECK(fseek(stream, 0, SEEK_SET) == 0);
  char buffer[256] = {0};
  size_t read = fread(buffer, 1, sizeof(buffer) - 1, stream);
  buffer[read] = '\0';
  CHECK(strstr(buffer, "bb0.entry:\n  br bb1\nbb1.exit:\n  ret\n.end\n") != nullptr);
  fclose(stream);
  xs_lil_module_destroy(module);
}

static void test_text_parser_round_trips_branch_if_subset(void)
{
  const char text[] = ".xlil version 1\n.xlil module App\n.func Choose : () -> void\nbb0.entry:\n  %r0:bool = "
                      "const.bool true\n  br_if %r0, bb1, bb2\nbb1.then:\n  ret\nbb2.else:\n  ret\n.end\n";
  XsLilError error = {0};
  XsLilModule *module = nullptr;
  CHECK(xs_lil_module_parse_text("branch_if.xlil", text, strlen(text), &module, &error) == XS_LIL_OK);
  FILE *stream = tmpfile();
  if(stream == nullptr)
  {
    ++failures;
    xs_lil_module_destroy(module);
    return;
  }
  CHECK(xs_lil_module_write_text(module, stream, &error) == XS_LIL_OK);
  CHECK(fseek(stream, 0, SEEK_SET) == 0);
  char buffer[1024] = {0};
  size_t read = fread(buffer, 1, sizeof(buffer) - 1, stream);
  buffer[read] = '\0';
  CHECK(strstr(buffer, "br_if %r0, bb1, bb2\nbb1.then:\n  ret\nbb2.else:\n  ret\n.end\n") != nullptr);
  fclose(stream);
  xs_lil_module_destroy(module);
}

static void test_text_parser_round_trips_bool_not_subset(void)
{
  const char text[] = ".xlil version 1\n.xlil module App\n.func Flip : () -> bool\nbb0.entry:\n"
                      "  %r0:bool = const.bool false\n  %r1:bool = not.bool %r0\n  ret %r1\n.end\n";
  XsLilError error = {0};
  XsLilModule *module = nullptr;
  CHECK(xs_lil_module_parse_text("not_bool.xlil", text, strlen(text), &module, &error) == XS_LIL_OK);
  CHECK(module != nullptr);
  CHECK(xs_lil_module_verify(module, &error) == XS_LIL_OK);
  const XsLilFunction *function = xs_lil_module_function_at(module, 0);
  CHECK(function != nullptr);
  const XsLilBlock *block = xs_lil_function_block_at(function, 0);
  CHECK(block != nullptr);
  CHECK(xs_lil_block_instruction_kind(block, 1) == XS_LIL_INSTRUCTION_NOT_BOOL);
  CHECK(xs_lil_function_value_type(function, 1).kind == XS_LIL_TYPE_BOOL);
  FILE *stream = tmpfile();
  if(stream == nullptr)
  {
    ++failures;
    xs_lil_module_destroy(module);
    return;
  }
  CHECK(xs_lil_module_write_text(module, stream, &error) == XS_LIL_OK);
  CHECK(fseek(stream, 0, SEEK_SET) == 0);
  char buffer[512] = {0};
  size_t read = fread(buffer, 1, sizeof(buffer) - 1U, stream);
  buffer[read] = '\0';
  CHECK(strstr(buffer, "%r1:bool = not.bool %r0\n  ret %r1\n") != nullptr);
  fclose(stream);
  xs_lil_module_destroy(module);
}

static void test_text_parser_round_trips_parameters_and_calls(void)
{
  const char text[] = ".xlil version 1\n.xlil module App\n.func Forward : (i64) -> i64\n.param %r0:i64\nbb0.entry:\n"
                      "  %r1:i64 = call Import(%r0)\n  call Sink(%r1)\n  ret %r1\n.end\n.extern Import : (i64) -> i64\n"
                      ".extern Sink : (i64) -> void\n";
  XsLilError error = {0};
  XsLilModule *module = nullptr;
  CHECK(xs_lil_module_parse_text("calls.xlil", text, strlen(text), &module, &error) == XS_LIL_OK);
  CHECK(module != nullptr);
  CHECK(xs_lil_module_verify(module, &error) == XS_LIL_OK);
  const XsLilFunction *function = xs_lil_module_function_at(module, 0);
  CHECK(function != nullptr);
  CHECK(xs_lil_function_value_count(function) == 2);
  CHECK(xs_lil_function_value_type(function, 0).kind == XS_LIL_TYPE_I64);
  const XsLilBlock *block = xs_lil_function_block_at(function, 0);
  CHECK(block != nullptr);
  CHECK(xs_lil_block_instruction_kind(block, 0) == XS_LIL_INSTRUCTION_CALL);
  CHECK(strcmp(xs_lil_block_instruction_callee(block, 0), "Import") == 0);
  CHECK(xs_lil_block_instruction_argument_count(block, 0) == 1);
  CHECK(xs_lil_block_instruction_argument(block, 0, 0) == 0);
  CHECK(xs_lil_block_instruction_result(block, 1) == UINT32_MAX);
  FILE *stream = tmpfile();
  if(stream == nullptr)
  {
    ++failures;
    xs_lil_module_destroy(module);
    return;
  }
  CHECK(xs_lil_module_write_text(module, stream, &error) == XS_LIL_OK);
  CHECK(fseek(stream, 0, SEEK_SET) == 0);
  char buffer[1024] = {0};
  size_t read = fread(buffer, 1, sizeof(buffer) - 1, stream);
  buffer[read] = '\0';
  CHECK(strstr(buffer, ".param %r0:i64\n") != nullptr);
  CHECK(strstr(buffer, "%r1:i64 = call Import(%r0)\n  call Sink(%r1)\n") != nullptr);
  fclose(stream);
  xs_lil_module_destroy(module);
}

static void test_text_parser_round_trips_panic_terminator(void)
{
  const char text[] = ".xlil version 1\n.xlil module App\n.func Fail : () -> void\nbb0.entry:\n  panic\n.end\n";
  XsLilError error = {0};
  XsLilModule *module = nullptr;
  CHECK(xs_lil_module_parse_text("panic.xlil", text, strlen(text), &module, &error) == XS_LIL_OK);
  CHECK(module != nullptr);
  CHECK(xs_lil_module_verify(module, &error) == XS_LIL_OK);
  const XsLilFunction *function = xs_lil_module_function_at(module, 0);
  const XsLilBlock *block = xs_lil_function_block_at(function, 0);
  CHECK(xs_lil_block_terminator_kind(block) == XS_LIL_TERMINATOR_PANIC);
  FILE *stream = tmpfile();
  CHECK(stream != nullptr);
  if(stream != nullptr)
  {
    CHECK(xs_lil_module_write_text(module, stream, &error) == XS_LIL_OK);
    CHECK(fseek(stream, 0, SEEK_SET) == 0);
    char buffer[256] = {0};
    size_t read = fread(buffer, 1, sizeof(buffer) - 1U, stream);
    buffer[read] = '\0';
    CHECK(strstr(buffer, "bb0.entry:\n  panic\n.end\n") != nullptr);
    fclose(stream);
  }
  xs_lil_module_destroy(module);
}

static void test_text_parser_round_trips_stack_slots(void)
{
  const char text[] = ".xlil version 1\n.xlil module App\n.func Memory : () -> i32\n.slot %s0:i32\n"
                      "bb0.entry:\n  %r0:i32 = const.i32 7\n  store %r0, %s0\n"
                      "  %r1:i32 = load %s0\n  ret %r1\n.end\n";
  XsLilError error = {0};
  XsLilModule *module = nullptr;
  CHECK(xs_lil_module_parse_text("memory.xlil", text, strlen(text), &module, &error) == XS_LIL_OK);
  CHECK(xs_lil_module_verify(module, &error) == XS_LIL_OK);
  const XsLilFunction *function = xs_lil_module_function_at(module, 0);
  CHECK(xs_lil_function_slot_count(function) == 1);
  CHECK(xs_lil_function_slot_type(function, 0).kind == XS_LIL_TYPE_I32);
  const XsLilBlock *block = xs_lil_function_block_at(function, 0);
  CHECK(xs_lil_block_instruction_kind(block, 1) == XS_LIL_INSTRUCTION_STORE);
  CHECK(xs_lil_block_instruction_kind(block, 2) == XS_LIL_INSTRUCTION_LOAD);
  CHECK(xs_lil_block_instruction_slot(block, 1) == 0);
  CHECK(xs_lil_block_instruction_slot(block, 2) == 0);
  FILE *stream = tmpfile();
  CHECK(stream != nullptr);
  if(stream != nullptr)
  {
    CHECK(xs_lil_module_write_text(module, stream, &error) == XS_LIL_OK);
    CHECK(fseek(stream, 0, SEEK_SET) == 0);
    char buffer[512] = {0};
    size_t read = fread(buffer, 1, sizeof(buffer) - 1U, stream);
    buffer[read] = '\0';
    CHECK(strstr(buffer, ".slot %s0:i32\n") != nullptr);
    CHECK(strstr(buffer, "store %r0, %s0\n  %r1:i32 = load %s0\n") != nullptr);
    fclose(stream);
  }
  xs_lil_module_destroy(module);
}

static void test_text_parser_round_trips_binary_i64_instructions(void)
{
  const char text[] = ".xlil version 1\n.xlil module App\n.func Arithmetic : () -> i64\nbb0.entry:\n"
                      "  %r0:i64 = const.i64 9\n  %r1:i64 = const.i64 3\n  %r2:i64 = add.i64 %r0, %r1\n"
                      "  %r3:i64 = sub.i64 %r2, %r1\n  %r4:i64 = mul.i64 %r3, %r1\n"
                      "  %r5:i64 = div.i64 %r4, %r1\n  %r6:i64 = rem.i64 %r5, %r1\n"
                      "  %r7:i64 = and.i64 %r0, %r1\n  %r8:i64 = or.i64 %r0, %r1\n"
                      "  %r9:i64 = xor.i64 %r7, %r8\n  %r10:i64 = shl.i64 %r1, %r1\n"
                      "  %r11:i64 = shr.i64 %r0, %r1\n  %r12:bool = eq.i64 %r6, %r0\n"
                      "  %r13:bool = ne.i64 %r6, %r0\n  %r14:bool = lt.i64 %r1, %r0\n"
                      "  %r15:bool = le.i64 %r1, %r0\n  %r16:bool = gt.i64 %r0, %r1\n"
                      "  %r17:bool = ge.i64 %r0, %r1\n"
                      "  ret %r6\n.end\n";
  XsLilError error = {0};
  XsLilModule *module = nullptr;
  CHECK(xs_lil_module_parse_text("arithmetic.xlil", text, strlen(text), &module, &error) == XS_LIL_OK);
  FILE *stream = tmpfile();
  if(stream == nullptr)
  {
    ++failures;
    xs_lil_module_destroy(module);
    return;
  }
  CHECK(xs_lil_module_write_text(module, stream, &error) == XS_LIL_OK);
  CHECK(fseek(stream, 0, SEEK_SET) == 0);
  char buffer[1024] = {0};
  size_t read = fread(buffer, 1, sizeof(buffer) - 1, stream);
  buffer[read] = '\0';
  CHECK(strstr(buffer, "%r2:i64 = add.i64 %r0, %r1\n") != nullptr);
  CHECK(strstr(buffer, "%r3:i64 = sub.i64 %r2, %r1\n") != nullptr);
  CHECK(strstr(buffer, "%r4:i64 = mul.i64 %r3, %r1\n") != nullptr);
  CHECK(strstr(buffer, "%r5:i64 = div.i64 %r4, %r1\n") != nullptr);
  CHECK(strstr(buffer, "%r6:i64 = rem.i64 %r5, %r1\n") != nullptr);
  CHECK(strstr(buffer, "%r7:i64 = and.i64 %r0, %r1\n") != nullptr);
  CHECK(strstr(buffer, "%r8:i64 = or.i64 %r0, %r1\n") != nullptr);
  CHECK(strstr(buffer, "%r9:i64 = xor.i64 %r7, %r8\n") != nullptr);
  CHECK(strstr(buffer, "%r10:i64 = shl.i64 %r1, %r1\n") != nullptr);
  CHECK(strstr(buffer, "%r11:i64 = shr.i64 %r0, %r1\n") != nullptr);
  CHECK(strstr(buffer, "%r12:bool = eq.i64 %r6, %r0\n") != nullptr);
  CHECK(strstr(buffer, "%r13:bool = ne.i64 %r6, %r0\n") != nullptr);
  CHECK(strstr(buffer, "%r14:bool = lt.i64 %r1, %r0\n") != nullptr);
  CHECK(strstr(buffer, "%r15:bool = le.i64 %r1, %r0\n") != nullptr);
  CHECK(strstr(buffer, "%r16:bool = gt.i64 %r0, %r1\n") != nullptr);
  CHECK(strstr(buffer, "%r17:bool = ge.i64 %r0, %r1\n") != nullptr);
  fclose(stream);
  xs_lil_module_destroy(module);
}

static void test_text_parser_round_trips_i32_constant(void)
{
  const char text[] = ".xlil version 1\n.xlil module App\n.func main : () -> i32\nbb0.entry:\n"
                      "  %r0:i32 = const.i32 0\n  ret %r0\n.end\n";
  XsLilError error = {0};
  XsLilModule *module = nullptr;
  CHECK(xs_lil_module_parse_text("main.xlil", text, strlen(text), &module, &error) == XS_LIL_OK);
  FILE *stream = tmpfile();
  if(stream == nullptr)
  {
    ++failures;
    xs_lil_module_destroy(module);
    return;
  }
  CHECK(xs_lil_module_write_text(module, stream, &error) == XS_LIL_OK);
  CHECK(fseek(stream, 0, SEEK_SET) == 0);
  char buffer[256] = {0};
  size_t read = fread(buffer, 1, sizeof(buffer) - 1U, stream);
  buffer[read] = '\0';
  CHECK(strstr(buffer, "%r0:i32 = const.i32 0\n") != nullptr);
  fclose(stream);
  xs_lil_module_destroy(module);
}

static void test_text_parser_accepts_integer_bit_patterns(void)
{
  const char text[] = ".xlil version 1\n.xlil module Bits\n.func Values : () -> i32\nbb0.entry:\n"
                      "  %r0:i64 = const.i64 0xffffffffffffffff\n"
                      "  %r1:i32 = const.i32 0xffffffff\n  ret %r1\n.end\n";
  XsLilError error = {0};
  XsLilModule *module = nullptr;
  CHECK(xs_lil_module_parse_text("bits.xlil", text, strlen(text), &module, &error) == XS_LIL_OK);
  if(module != nullptr)
  {
    const XsLilBlock *entry = xs_lil_function_block_at(xs_lil_module_function_at(module, 0), 0);
    CHECK(xs_lil_block_instruction_i64(entry, 0) == -1);
    CHECK(xs_lil_block_instruction_i64(entry, 1) == -1);
  }
  xs_lil_module_destroy(module);
}

static void test_text_parser_round_trips_binary_i32_instructions(void)
{
  const char text[] = ".xlil version 1\n.xlil module App\n.func Arithmetic32 : () -> i32\nbb0.entry:\n"
                      "  %r0:i32 = const.i32 9\n  %r1:i32 = const.i32 3\n  %r2:i32 = add.i32 %r0, %r1\n"
                      "  %r3:i32 = sub.i32 %r2, %r1\n  %r4:i32 = mul.i32 %r3, %r1\n"
                      "  %r5:i32 = div.i32 %r4, %r1\n  %r6:i32 = rem.i32 %r5, %r1\n"
                      "  %r7:i32 = and.i32 %r0, %r1\n  %r8:i32 = or.i32 %r7, %r1\n"
                      "  %r9:i32 = xor.i32 %r7, %r8\n  %r10:i32 = shl.i32 %r9, %r1\n"
                      "  %r11:i32 = shr.i32 %r10, %r1\n  %r12:bool = eq.i32 %r6, %r0\n"
                      "  %r13:bool = ne.i32 %r6, %r1\n  %r14:bool = lt.i32 %r1, %r0\n"
                      "  %r15:bool = le.i32 %r1, %r0\n  %r16:bool = gt.i32 %r0, %r1\n"
                      "  %r17:bool = ge.i32 %r0, %r1\n  ret %r11\n.end\n";
  XsLilError error = {0};
  XsLilModule *module = nullptr;
  CHECK(xs_lil_module_parse_text("arithmetic32.xlil", text, strlen(text), &module, &error) == XS_LIL_OK);
  FILE *stream = tmpfile();
  if(stream == nullptr)
  {
    ++failures;
    xs_lil_module_destroy(module);
    return;
  }
  CHECK(xs_lil_module_write_text(module, stream, &error) == XS_LIL_OK);
  CHECK(fseek(stream, 0, SEEK_SET) == 0);
  char buffer[1024] = {0};
  size_t read = fread(buffer, 1, sizeof(buffer) - 1U, stream);
  buffer[read] = '\0';
  CHECK(strstr(buffer, "%r2:i32 = add.i32 %r0, %r1\n") != nullptr);
  CHECK(strstr(buffer, "%r3:i32 = sub.i32 %r2, %r1\n") != nullptr);
  CHECK(strstr(buffer, "%r4:i32 = mul.i32 %r3, %r1\n") != nullptr);
  CHECK(strstr(buffer, "%r5:i32 = div.i32 %r4, %r1\n") != nullptr);
  CHECK(strstr(buffer, "%r6:i32 = rem.i32 %r5, %r1\n") != nullptr);
  CHECK(strstr(buffer, "%r7:i32 = and.i32 %r0, %r1\n") != nullptr);
  CHECK(strstr(buffer, "%r8:i32 = or.i32 %r7, %r1\n") != nullptr);
  CHECK(strstr(buffer, "%r9:i32 = xor.i32 %r7, %r8\n") != nullptr);
  CHECK(strstr(buffer, "%r10:i32 = shl.i32 %r9, %r1\n") != nullptr);
  CHECK(strstr(buffer, "%r11:i32 = shr.i32 %r10, %r1\n") != nullptr);
  CHECK(strstr(buffer, "%r12:bool = eq.i32 %r6, %r0\n") != nullptr);
  CHECK(strstr(buffer, "%r13:bool = ne.i32 %r6, %r1\n") != nullptr);
  CHECK(strstr(buffer, "%r14:bool = lt.i32 %r1, %r0\n") != nullptr);
  CHECK(strstr(buffer, "%r15:bool = le.i32 %r1, %r0\n") != nullptr);
  CHECK(strstr(buffer, "%r16:bool = gt.i32 %r0, %r1\n") != nullptr);
  CHECK(strstr(buffer, "%r17:bool = ge.i32 %r0, %r1\n") != nullptr);
  fclose(stream);
  xs_lil_module_destroy(module);
}

static void test_text_parser_rejects_invalid_inputs(void)
{
  static const char *const invalid_inputs[] = {
      ".xlil version 2\n.xlil module App\n",
      ".xlil version 1\n.extern Import : () -> void\n",
      ".xlil version 1\n.xlil module App\n.func Bad : () -> i64\nbb0.entry:\n  %r0:i64 = const 1\n  ret %r0\n.end\n",
      ".xlil version 1\n.xlil module App\n.extern Import : (bad) -> void\n",
      ".xlil version 1\n.xlil module App\n.extern Import (i64) -> i64\n",
      ".xlil version 1\n.xlil module App\n.func Bad : () -> void\nbb0.entry:\n  ret\n  %r0:i64 = const.i64 1\n.end\n",
      ".xlil version 1\n.xlil module App\n.func Bad : () -> i64\nbb0.entry:\n  %0:i64 = const.i64 1\n  ret %r0\n.end\n",
      ".xlil version 1\n.xlil module App\n.func Bad : () -> i64\nbb0.entry:\n  %r0:i64 = const.i64 1\n  ret %0\n.end\n",
      ".xlil version 1\n.xlil module App\n.func Bad : (i64) -> i64\nbb0.entry:\n  ret %r0\n.end\n",
      ".xlil version 1\n.xlil module App\n.func Bad : (i64) -> i64\n.param %r1:i64\nbb0.entry:\n  ret %r0\n.end\n",
      ".xlil version 1\n.xlil module App\n.func Bad : () -> i64\nbb0.entry:\n  %r0:i64 = call Missing()\n  ret "
      "%r0\n.end\n",
      ".xlil version 1\n.xlil module App\n.extern Import : (i64) -> i64\n.func Bad : () -> i64\nbb0.entry:\n"
      "  %r0:i64 = call Import()\n  ret %r0\n.end\n",
      ".xlil version 1\n.xlil module App\n.extern Sink : () -> void\n.func Bad : () -> void\nbb0.entry:\n"
      "  %r0:void = call Sink()\n  ret\n.end\n",
      ".xlil version 1\n.xlil module App\n.extern Import : () -> i64\n.func Bad : () -> void\nbb0.entry:\n"
      "  call Import()\n  ret\n.end\n",
      ".xlil version 1\n.xlil module App\n.func Bad : () -> bool\nbb0.entry:\n"
      "  %r0:i64 = const.i64 1\n  %r1:bool = not.bool %r0\n  ret %r1\n.end\n",
      ".xlil version 1\n.xlil module App\n.func Bad : () -> f32\nbb0.entry:\n"
      "  %r0:f32 = const.f32 0x1234\n  ret %r0\n.end\n",
      ".xlil version 1\n.xlil module App\n.func Bad : () -> f32\nbb0.entry:\n"
      "  %r0:f32 = const.f32 0x3f800000\n  %r1:f64 = const.f64 0x3ff0000000000000\n"
      "  %r2:f32 = add.f32 %r0, %r1\n  ret %r2\n.end\n",
      ".xlil version 1\n.xlil module App\n.func Bad : () -> void\n.slot %s0:void\nbb0.entry:\n  ret\n.end\n",
      ".xlil version 1\n.xlil module App\n.func Bad : () -> i64\n.slot %s0:i32\nbb0.entry:\n"
      "  %r0:i64 = load %s0\n  ret %r0\n.end\n",
      ".xlil version 1\n.xlil module App\n.func Bad : () -> void\n.slot %s0:i32\nbb0.entry:\n"
      "  %r0:i64 = const.i64 1\n  store %r0, %s0\n  ret\n.end\n",
      ".xlil version 1\n.xlil module App\n.func Bad : () -> str\nbb0.entry:\n"
      "  %r0:str = const.str utf32 [0x00000041]\n  ret %r0\n.end\n",
      ".xlil version 1\n.xlil module App\n.func Bad : () -> str\nbb0.entry:\n"
      "  %r0:str = const.str utf32le [0x0000d800]\n  ret %r0\n.end\n",
      ".xlil version 1\n.xlil module App\n.func Bad : () -> str\nbb0.entry:\n"
      "  %r0:str = const.str utf32be [0x00000041,]\n  ret %r0\n.end\n",
      ".xlil version 1\n.xlil module App\n.func Bad : () -> u16\nbb0.entry:\n"
      "  %r0:u16 = const.u16 0x041\n  ret %r0\n.end\n",
  };
  for(size_t i = 0; i < sizeof(invalid_inputs) / sizeof(invalid_inputs[0]); ++i)
  {
    XsLilError error = {0};
    XsLilModule *module = nullptr;
    CHECK(xs_lil_module_parse_text("bad.xlil", invalid_inputs[i], strlen(invalid_inputs[i]), &module, &error) ==
          XS_LIL_INVALID_ARGUMENT);
    CHECK(module == nullptr);
  }
}

static void test_text_parser_round_trips_explicit_utf32_strings(void)
{
  static const char text[] = ".xlil version 1\n.xlil module Strings\n.func greeting : () -> str\nbb0.entry:\n"
                             "  %r0:str = const.str utf32be [0x0000004c, 0x00000065, 0x00000069]\n  ret %r0\n.end\n";
  XsLilError error = {0};
  XsLilModule *module = nullptr;
  CHECK(xs_lil_module_parse_text("strings.xlil", text, strlen(text), &module, &error) == XS_LIL_OK);
  CHECK(module != nullptr);
  if(module == nullptr)
    return;
  const XsLilBlock *block = xs_lil_function_block_at(xs_lil_module_function_at(module, 0), 0);
  CHECK(xs_lil_block_instruction_kind(block, 0) == XS_LIL_INSTRUCTION_CONST_STR);
  CHECK(xs_lil_block_instruction_utf32_encoding(block, 0) == XS_LIL_UTF32_BE);
  CHECK(xs_lil_block_instruction_utf32_length(block, 0) == 3);
  CHECK(xs_lil_block_instruction_utf32_unit(block, 0, 0) == UINT16_C(0x004c));
  CHECK(xs_lil_block_instruction_utf32_unit(block, 0, 2) == UINT16_C(0x0069));
  CHECK(xs_lil_module_verify(module, &error) == XS_LIL_OK);
  FILE *stream = tmpfile();
  CHECK(stream != nullptr);
  if(stream != nullptr)
  {
    CHECK(xs_lil_module_write_text(module, stream, &error) == XS_LIL_OK);
    CHECK(fseek(stream, 0, SEEK_SET) == 0);
    char buffer[512] = {0};
    size_t read = fread(buffer, 1, sizeof(buffer) - 1U, stream);
    buffer[read] = '\0';
    CHECK(strcmp(buffer, text) == 0);
    fclose(stream);
  }
  xs_lil_module_destroy(module);
}

static void test_text_parser_round_trips_str_comparisons(void)
{
  static const char text[] =
      ".xlil version 1\n.xlil module Strings\n.func same : (str, str) -> bool\n.param %r0:str\n.param %r1:str\n"
      "bb0.entry:\n  %r2:bool = eq.str %r0, %r1\n  %r3:bool = ne.str %r0, %r1\n  ret %r2\n.end\n";
  XsLilError error = {0};
  XsLilModule *module = nullptr;
  CHECK(xs_lil_module_parse_text("comparisons.xlil", text, strlen(text), &module, &error) == XS_LIL_OK);
  CHECK(module != nullptr);
  if(module == nullptr)
    return;
  const XsLilBlock *block = xs_lil_function_block_at(xs_lil_module_function_at(module, 0), 0);
  CHECK(xs_lil_block_instruction_kind(block, 0) == XS_LIL_INSTRUCTION_EQ_STR);
  CHECK(xs_lil_block_instruction_kind(block, 1) == XS_LIL_INSTRUCTION_NE_STR);
  CHECK(xs_lil_module_verify(module, &error) == XS_LIL_OK);
  FILE *stream = tmpfile();
  CHECK(stream != nullptr);
  if(stream != nullptr)
  {
    CHECK(xs_lil_module_write_text(module, stream, &error) == XS_LIL_OK);
    CHECK(fseek(stream, 0, SEEK_SET) == 0);
    char buffer[512] = {0};
    size_t read = fread(buffer, 1, sizeof(buffer) - 1U, stream);
    buffer[read] = '\0';
    CHECK(strcmp(buffer, text) == 0);
    fclose(stream);
  }
  xs_lil_module_destroy(module);
}

static void test_text_parser_round_trips_u16_constant(void)
{
  static const char text[] = ".xlil version 1\n.xlil module Character\n.func omega : () -> u16\nbb0.entry:\n"
                             "  %r0:u16 = const.u16 0x03a9\n  ret %r0\n.end\n";
  XsLilError error = {0};
  XsLilModule *module = nullptr;
  CHECK(xs_lil_module_parse_text("character.xlil", text, strlen(text), &module, &error) == XS_LIL_OK);
  CHECK(module != nullptr);
  if(module == nullptr)
    return;
  const XsLilBlock *block = xs_lil_function_block_at(xs_lil_module_function_at(module, 0), 0);
  CHECK(xs_lil_block_instruction_kind(block, 0) == XS_LIL_INSTRUCTION_CONST_U16);
  CHECK(xs_lil_block_instruction_u16(block, 0) == UINT16_C(0x03a9));
  CHECK(xs_lil_module_verify(module, &error) == XS_LIL_OK);
  xs_lil_module_destroy(module);
}

static void test_public_integer_constant_api(void)
{
  XsLilError error = {0};
  XsLilModule *module = nullptr;
  XsLilFunction *function = nullptr;
  XsLilBlock *entry = nullptr;
  CHECK(xs_lil_module_create("IntegerApi", &module, &error) == XS_LIL_OK);
  CHECK(xs_lil_module_add_function_definition(module, "values", (XsLilType){.kind = XS_LIL_TYPE_VOID}, nullptr, 0,
                                              &function, &error) == XS_LIL_OK);
  CHECK(xs_lil_function_append_block(function, "entry", &entry, &error) == XS_LIL_OK);
  XsLilValueId value = 0;
  CHECK(xs_lil_block_add_const_u8(entry, UINT8_MAX, &value, &error) == XS_LIL_OK);
  CHECK(xs_lil_block_add_const_i8(entry, INT8_MIN, &value, &error) == XS_LIL_OK);
  CHECK(xs_lil_block_add_const_i16(entry, INT16_MIN, &value, &error) == XS_LIL_OK);
  CHECK(xs_lil_block_add_const_u32(entry, UINT32_MAX, &value, &error) == XS_LIL_OK);
  CHECK(xs_lil_block_add_const_u64(entry, UINT64_MAX, &value, &error) == XS_LIL_OK);
  CHECK(xs_lil_block_add_const_u128(entry, xs_uint128_from_words(UINT64_MAX, UINT64_MAX), &value, &error) == XS_LIL_OK);
  CHECK(xs_lil_block_add_const_i128(entry, xs_int128_from_words(INT64_MIN, 0), &value, &error) == XS_LIL_OK);
  CHECK(xs_lil_block_set_return(entry, &error) == XS_LIL_OK);
  CHECK(xs_lil_module_verify(module, &error) == XS_LIL_OK);
  CHECK(xs_lil_block_instruction_count(entry) == 7);
  CHECK(xs_lil_block_instruction_kind(entry, 0) == XS_LIL_INSTRUCTION_CONST_U8);
  CHECK(xs_lil_block_instruction_integer_bits(entry, 0).low == UINT8_MAX);
  CHECK(xs_lil_block_instruction_kind(entry, 6) == XS_LIL_INSTRUCTION_CONST_I128);
  CHECK(xs_lil_block_instruction_integer_bits(entry, 6).high == UINT64_C(0x8000000000000000));
  FILE *stream = tmpfile();
  CHECK(stream != nullptr);
  if(stream != nullptr)
  {
    CHECK(xs_lil_module_write_text(module, stream, &error) == XS_LIL_OK);
    CHECK(fseek(stream, 0, SEEK_SET) == 0);
    char text[1024] = {0};
    size_t read = fread(text, 1, sizeof(text) - 1U, stream);
    text[read] = '\0';
    CHECK(strstr(text, "%r0:u8 = const.u8 0xff") != nullptr);
    CHECK(strstr(text, "%r5:u128 = const.u128 0xffffffffffffffffffffffffffffffff") != nullptr);
    CHECK(strstr(text, "%r6:i128 = const.i128 0x80000000000000000000000000000000") != nullptr);
    fclose(stream);
  }
  xs_lil_module_destroy(module);
}

static void test_integer_operation_api_and_text(void)
{
  static const char text[] =
      ".xlil version 1\n.xlil module IntegerOperations\n.func add : (u8, u8) -> u8\n.param %r0:u8\n"
      ".param %r1:u8\nbb0.entry:\n  %r2:u8 = add.u8 %r0, %r1\n  ret %r2\n.end\n"
      ".func less : (u128, u128) -> bool\n.param %r0:u128\n.param %r1:u128\nbb0.entry:\n"
      "  %r2:bool = lt.u128 %r0, %r1\n  ret %r2\n.end\n"
      ".func shift : (i128, i128) -> i128\n.param %r0:i128\n.param %r1:i128\nbb0.entry:\n"
      "  %r2:i128 = shr.i128 %r0, %r1\n  ret %r2\n.end\n";
  XsLilError error = {0};
  XsLilModule *module = nullptr;
  CHECK(xs_lil_module_parse_text("integer_operations.xlil", text, strlen(text), &module, &error) == XS_LIL_OK);
  CHECK(module != nullptr);
  if(module == nullptr)
    return;
  CHECK(xs_lil_module_verify(module, &error) == XS_LIL_OK);
  const XsLilBlock *add = xs_lil_function_block_at(xs_lil_module_function_at(module, 0), 0);
  CHECK(xs_lil_block_instruction_kind(add, 0) == XS_LIL_INSTRUCTION_BINARY_INTEGER);
  CHECK(xs_lil_block_instruction_integer_operation(add, 0) == XS_LIL_INTEGER_ADD);
  CHECK(xs_lil_block_instruction_integer_type(add, 0).kind == XS_LIL_TYPE_U8);
  const XsLilBlock *less = xs_lil_function_block_at(xs_lil_module_function_at(module, 1), 0);
  CHECK(xs_lil_block_instruction_integer_operation(less, 0) == XS_LIL_INTEGER_LESS);
  FILE *stream = tmpfile();
  CHECK(stream != nullptr);
  if(stream != nullptr)
  {
    CHECK(xs_lil_module_write_text(module, stream, &error) == XS_LIL_OK);
    CHECK(fseek(stream, 0, SEEK_SET) == 0);
    char output[2048] = {0};
    size_t read = fread(output, 1, sizeof(output) - 1U, stream);
    output[read] = '\0';
    CHECK(strcmp(output, text) == 0);
    fclose(stream);
  }
  xs_lil_module_destroy(module);
}

int main(void)
{
  test_module_and_text_writer();
  test_aggregate_type_registry_round_trip();
  test_array_type_registry_round_trip();
  test_dynamic_array_access_round_trip();
  test_parser_accepts_non_null_terminated_registry_text();
  test_function_body_text_writer();
  test_function_body_rejects_missing_return_value();
  test_floating_constant_bits();
  test_function_body_branch_text_writer();
  test_function_body_branch_if_text_writer();
  test_function_body_rejects_non_bool_branch_if();
  test_text_parser_reads_external_signature();
  test_text_parser_reads_function_definition();
  test_text_parser_round_trips_supported_body_subset();
  test_text_parser_round_trips_branch_subset();
  test_text_parser_round_trips_branch_if_subset();
  test_text_parser_round_trips_bool_not_subset();
  test_text_parser_round_trips_parameters_and_calls();
  test_text_parser_round_trips_panic_terminator();
  test_text_parser_round_trips_stack_slots();
  test_text_parser_round_trips_binary_i64_instructions();
  test_text_parser_round_trips_i32_constant();
  test_text_parser_accepts_integer_bit_patterns();
  test_text_parser_round_trips_binary_i32_instructions();
  test_text_parser_round_trips_explicit_utf32_strings();
  test_text_parser_round_trips_str_comparisons();
  test_text_parser_round_trips_u16_constant();
  test_public_integer_constant_api();
  test_integer_operation_api_and_text();
  test_text_parser_rejects_invalid_inputs();
  return failures == 0 ? 0 : 1;
}
