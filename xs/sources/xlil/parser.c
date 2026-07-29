/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 */

#include "model_internal.h"

#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>

typedef struct
{
  const char *path;
  const char *text;
  size_t length;
  size_t cursor;
  size_t line;
  XsLilModule *module;
} Parser;

typedef struct
{
  XsLilBlock *block;
  bool is_conditional;
  XsLilValueId condition;
  XsLilBlockId target;
  XsLilBlockId else_target;
} PendingBranch;

static XsLilStatus parse_error(Parser *parser, XsLilError *error, const char *message)
{
  char buffer[256];
  snprintf(buffer, sizeof(buffer), "%s:%zu: %s", parser->path == nullptr ? "<xlil>" : parser->path, parser->line,
           message);
  return xs_lil_set_error(error, XS_LIL_INVALID_ARGUMENT, buffer);
}

static const char *skip_space(const char *cursor, const char *end)
{
  while(cursor < end && (*cursor == ' ' || *cursor == '\t'))
    ++cursor;
  return cursor;
}

static bool next_line(Parser *parser, const char **line, size_t *length)
{
  if(parser->cursor >= parser->length)
    return false;
  size_t start = parser->cursor;
  while(parser->cursor < parser->length && parser->text[parser->cursor] != '\n' && parser->text[parser->cursor] != '\r')
    ++parser->cursor;
  *line = parser->text + start;
  *length = parser->cursor - start;
  if(parser->cursor < parser->length && parser->text[parser->cursor] == '\r')
    ++parser->cursor;
  if(parser->cursor < parser->length && parser->text[parser->cursor] == '\n')
    ++parser->cursor;
  ++parser->line;
  return true;
}

static bool span_equals(const char *text, size_t length, const char *expected)
{
  return strlen(expected) == length && strncmp(text, expected, length) == 0;
}

static bool parse_type(const XsLilModule *module, const char *start, size_t length, XsLilType *type)
{
  return xs_lil_parse_type_name(module, start, length, type);
}

static bool parse_u32_after_prefix(const char *line, size_t length, const char *prefix, uint32_t *value)
{
  size_t prefix_length = strlen(prefix);
  if(length <= prefix_length || strncmp(line, prefix, prefix_length) != 0)
    return false;
  uint32_t result = 0;
  for(size_t i = prefix_length; i < length; ++i)
  {
    if(line[i] < '0' || line[i] > '9')
      return false;
    uint32_t digit = (uint32_t)(line[i] - '0');
    if(result > (UINT32_MAX - digit) / 10U)
      return false;
    result = result * 10U + digit;
  }
  *value = result;
  return true;
}

static bool parse_i64_tail(const char *start, const char *end, int64_t *value)
{
  char *copy = xs_lil_copy_span(start, (size_t)(end - start));
  if(copy == nullptr)
    return false;
  char *tail = nullptr;
  errno = 0;
  long long parsed = strtoll(copy, &tail, 10);
  bool ok = errno == 0 && tail != copy && *skip_space(tail, copy + strlen(copy)) == '\0';
  free(copy);
  if(!ok)
    return false;
  *value = (int64_t)parsed;
  return true;
}

static bool append_branch(PendingBranch **branches, size_t *count, PendingBranch branch)
{
  PendingBranch *grown = realloc(*branches, (*count + 1U) * sizeof(*grown));
  if(grown == nullptr)
    return false;
  *branches = grown;
  (*branches)[(*count)++] = branch;
  return true;
}

static bool append_argument(XsLilValueId **arguments, size_t *count, XsLilValueId argument)
{
  XsLilValueId *grown = realloc(*arguments, (*count + 1U) * sizeof(*grown));
  if(grown == nullptr)
    return false;
  *arguments = grown;
  (*arguments)[(*count)++] = argument;
  return true;
}

static XsLilStatus parse_call(Parser *parser, XsLilBlock *block, const char *operation, size_t length,
                              const XsLilType *return_type, uint32_t expected_result, XsLilError *error)
{
  static const char prefix[] = "call ";
  if(length <= sizeof(prefix) - 1U || strncmp(operation, prefix, sizeof(prefix) - 1U) != 0)
    return parse_error(parser, error, "unsupported XLIL call instruction");
  const char *end = operation + length;
  const char *callee = operation + sizeof(prefix) - 1U;
  const char *open = callee;
  while(open < end && *open != '(')
    ++open;
  if(open == callee || open == end || end[-1] != ')')
    return parse_error(parser, error, "XLIL call instruction is invalid");
  const char *callee_end = open;
  while(callee_end > callee && (callee_end[-1] == ' ' || callee_end[-1] == '\t'))
    --callee_end;
  char *name = xs_lil_copy_span(callee, (size_t)(callee_end - callee));
  if(name == nullptr)
    return xs_lil_set_error(error, XS_LIL_ALLOCATION_FAILED, "out of memory while parsing XLIL call target");
  XsLilValueId *arguments = nullptr;
  size_t argument_count = 0;
  const char *cursor = skip_space(open + 1, end - 1);
  while(cursor < end - 1)
  {
    const char *comma = cursor;
    while(comma < end - 1 && *comma != ',')
      ++comma;
    uint32_t argument = 0;
    if(!parse_u32_after_prefix(cursor, (size_t)(comma - cursor), "%r", &argument) ||
       !append_argument(&arguments, &argument_count, argument))
    {
      free(arguments);
      free(name);
      return parse_error(parser, error, "XLIL call argument is invalid");
    }
    if(comma == end - 1)
      break;
    cursor = skip_space(comma + 1, end - 1);
    if(cursor == end - 1)
    {
      free(arguments);
      free(name);
      return parse_error(parser, error, "XLIL call argument is invalid");
    }
  }
  XsLilStatus status = XS_LIL_OK;
  if(return_type == nullptr)
  {
    status = xs_lil_block_add_void_call(block, name, arguments, argument_count, error);
  }
  else
  {
    XsLilValueId result = 0;
    status = xs_lil_block_add_call(block, name, *return_type, arguments, argument_count, &result, error);
    if(status == XS_LIL_OK && result != expected_result)
      status = parse_error(parser, error, "XLIL value ids must be sequential");
  }
  free(arguments);
  free(name);
  return status;
}

static const char *trim_end(const char *start, const char *end)
{
  while(end > start && (end[-1] == ' ' || end[-1] == '\t'))
    --end;
  return end;
}

static XsLilStatus parse_binary_integer(Parser *parser, XsLilBlock *block, const char *operands, size_t length,
                                        XsLilInstructionKind kind, uint32_t expected_result, XsLilError *error)
{
  const char *end = operands + length;
  const char *comma = operands;
  while(comma < end && *comma != ',')
    ++comma;
  uint32_t left = 0;
  uint32_t right = 0;
  const char *left_end = trim_end(operands, comma);
  const char *right_start = comma == end ? end : skip_space(comma + 1, end);
  const char *right_end = trim_end(right_start, end);
  if(comma == end || !parse_u32_after_prefix(operands, (size_t)(left_end - operands), "%r", &left) ||
     !parse_u32_after_prefix(right_start, (size_t)(right_end - right_start), "%r", &right))
    return parse_error(parser, error, "XLIL binary integer operands are invalid");
  XsLilValueId result = 0;
  XsLilStatus status = XS_LIL_OK;
  switch(kind)
  {
  case XS_LIL_INSTRUCTION_ADD_I64:
    status = xs_lil_block_add_i64(block, left, right, &result, error);
    break;
  case XS_LIL_INSTRUCTION_SUB_I64:
    status = xs_lil_block_sub_i64(block, left, right, &result, error);
    break;
  case XS_LIL_INSTRUCTION_MUL_I64:
    status = xs_lil_block_mul_i64(block, left, right, &result, error);
    break;
  case XS_LIL_INSTRUCTION_DIV_I64:
    status = xs_lil_block_div_i64(block, left, right, &result, error);
    break;
  case XS_LIL_INSTRUCTION_REM_I64:
    status = xs_lil_block_rem_i64(block, left, right, &result, error);
    break;
  case XS_LIL_INSTRUCTION_AND_I64:
    status = xs_lil_block_and_i64(block, left, right, &result, error);
    break;
  case XS_LIL_INSTRUCTION_OR_I64:
    status = xs_lil_block_or_i64(block, left, right, &result, error);
    break;
  case XS_LIL_INSTRUCTION_XOR_I64:
    status = xs_lil_block_xor_i64(block, left, right, &result, error);
    break;
  case XS_LIL_INSTRUCTION_SHL_I64:
    status = xs_lil_block_shl_i64(block, left, right, &result, error);
    break;
  case XS_LIL_INSTRUCTION_SHR_I64:
    status = xs_lil_block_shr_i64(block, left, right, &result, error);
    break;
  case XS_LIL_INSTRUCTION_EQ_I64:
    status = xs_lil_block_eq_i64(block, left, right, &result, error);
    break;
  case XS_LIL_INSTRUCTION_NE_I64:
    status = xs_lil_block_ne_i64(block, left, right, &result, error);
    break;
  case XS_LIL_INSTRUCTION_LT_I64:
    status = xs_lil_block_lt_i64(block, left, right, &result, error);
    break;
  case XS_LIL_INSTRUCTION_LE_I64:
    status = xs_lil_block_le_i64(block, left, right, &result, error);
    break;
  case XS_LIL_INSTRUCTION_GT_I64:
    status = xs_lil_block_gt_i64(block, left, right, &result, error);
    break;
  case XS_LIL_INSTRUCTION_GE_I64:
    status = xs_lil_block_ge_i64(block, left, right, &result, error);
    break;
  case XS_LIL_INSTRUCTION_ADD_I32:
    status = xs_lil_block_add_i32(block, left, right, &result, error);
    break;
  case XS_LIL_INSTRUCTION_SUB_I32:
    status = xs_lil_block_sub_i32(block, left, right, &result, error);
    break;
  case XS_LIL_INSTRUCTION_MUL_I32:
    status = xs_lil_block_mul_i32(block, left, right, &result, error);
    break;
  case XS_LIL_INSTRUCTION_DIV_I32:
    status = xs_lil_block_div_i32(block, left, right, &result, error);
    break;
  case XS_LIL_INSTRUCTION_REM_I32:
    status = xs_lil_block_rem_i32(block, left, right, &result, error);
    break;
  case XS_LIL_INSTRUCTION_AND_I32:
    status = xs_lil_block_and_i32(block, left, right, &result, error);
    break;
  case XS_LIL_INSTRUCTION_OR_I32:
    status = xs_lil_block_or_i32(block, left, right, &result, error);
    break;
  case XS_LIL_INSTRUCTION_XOR_I32:
    status = xs_lil_block_xor_i32(block, left, right, &result, error);
    break;
  case XS_LIL_INSTRUCTION_SHL_I32:
    status = xs_lil_block_shl_i32(block, left, right, &result, error);
    break;
  case XS_LIL_INSTRUCTION_SHR_I32:
    status = xs_lil_block_shr_i32(block, left, right, &result, error);
    break;
  case XS_LIL_INSTRUCTION_EQ_I32:
    status = xs_lil_block_eq_i32(block, left, right, &result, error);
    break;
  case XS_LIL_INSTRUCTION_NE_I32:
    status = xs_lil_block_ne_i32(block, left, right, &result, error);
    break;
  case XS_LIL_INSTRUCTION_LT_I32:
    status = xs_lil_block_lt_i32(block, left, right, &result, error);
    break;
  case XS_LIL_INSTRUCTION_LE_I32:
    status = xs_lil_block_le_i32(block, left, right, &result, error);
    break;
  case XS_LIL_INSTRUCTION_GT_I32:
    status = xs_lil_block_gt_i32(block, left, right, &result, error);
    break;
  case XS_LIL_INSTRUCTION_GE_I32:
    status = xs_lil_block_ge_i32(block, left, right, &result, error);
    break;
  case XS_LIL_INSTRUCTION_ADD_F32:
  case XS_LIL_INSTRUCTION_ADD_F64:
    status = xs_lil_block_binary_float(
        block, XS_LIL_FLOAT_ADD,
        (XsLilType){.kind = kind == XS_LIL_INSTRUCTION_ADD_F32 ? XS_LIL_TYPE_F32 : XS_LIL_TYPE_F64}, left, right,
        &result, error);
    break;
  case XS_LIL_INSTRUCTION_SUB_F32:
  case XS_LIL_INSTRUCTION_SUB_F64:
    status = xs_lil_block_binary_float(
        block, XS_LIL_FLOAT_SUB,
        (XsLilType){.kind = kind == XS_LIL_INSTRUCTION_SUB_F32 ? XS_LIL_TYPE_F32 : XS_LIL_TYPE_F64}, left, right,
        &result, error);
    break;
  case XS_LIL_INSTRUCTION_MUL_F32:
  case XS_LIL_INSTRUCTION_MUL_F64:
    status = xs_lil_block_binary_float(
        block, XS_LIL_FLOAT_MUL,
        (XsLilType){.kind = kind == XS_LIL_INSTRUCTION_MUL_F32 ? XS_LIL_TYPE_F32 : XS_LIL_TYPE_F64}, left, right,
        &result, error);
    break;
  case XS_LIL_INSTRUCTION_DIV_F32:
  case XS_LIL_INSTRUCTION_DIV_F64:
    status = xs_lil_block_binary_float(
        block, XS_LIL_FLOAT_DIV,
        (XsLilType){.kind = kind == XS_LIL_INSTRUCTION_DIV_F32 ? XS_LIL_TYPE_F32 : XS_LIL_TYPE_F64}, left, right,
        &result, error);
    break;
  case XS_LIL_INSTRUCTION_REM_F32:
  case XS_LIL_INSTRUCTION_REM_F64:
    status = xs_lil_block_binary_float(
        block, XS_LIL_FLOAT_REM,
        (XsLilType){.kind = kind == XS_LIL_INSTRUCTION_REM_F32 ? XS_LIL_TYPE_F32 : XS_LIL_TYPE_F64}, left, right,
        &result, error);
    break;
  case XS_LIL_INSTRUCTION_EQ_F32:
  case XS_LIL_INSTRUCTION_EQ_F64:
  case XS_LIL_INSTRUCTION_NE_F32:
  case XS_LIL_INSTRUCTION_NE_F64:
  case XS_LIL_INSTRUCTION_LT_F32:
  case XS_LIL_INSTRUCTION_LT_F64:
  case XS_LIL_INSTRUCTION_LE_F32:
  case XS_LIL_INSTRUCTION_LE_F64:
  case XS_LIL_INSTRUCTION_GT_F32:
  case XS_LIL_INSTRUCTION_GT_F64:
  case XS_LIL_INSTRUCTION_GE_F32:
  case XS_LIL_INSTRUCTION_GE_F64:
  {
    XsLilFloatComparisonOperation operation = XS_LIL_FLOAT_GE;
    if(kind == XS_LIL_INSTRUCTION_EQ_F32 || kind == XS_LIL_INSTRUCTION_EQ_F64)
      operation = XS_LIL_FLOAT_EQ;
    else if(kind == XS_LIL_INSTRUCTION_NE_F32 || kind == XS_LIL_INSTRUCTION_NE_F64)
      operation = XS_LIL_FLOAT_NE;
    else if(kind == XS_LIL_INSTRUCTION_LT_F32 || kind == XS_LIL_INSTRUCTION_LT_F64)
      operation = XS_LIL_FLOAT_LT;
    else if(kind == XS_LIL_INSTRUCTION_LE_F32 || kind == XS_LIL_INSTRUCTION_LE_F64)
      operation = XS_LIL_FLOAT_LE;
    else if(kind == XS_LIL_INSTRUCTION_GT_F32 || kind == XS_LIL_INSTRUCTION_GT_F64)
      operation = XS_LIL_FLOAT_GT;
    XsLilTypeKind type = kind <= XS_LIL_INSTRUCTION_GE_F32 ? XS_LIL_TYPE_F32 : XS_LIL_TYPE_F64;
    status = xs_lil_block_compare_float(block, operation, (XsLilType){.kind = type}, left, right, &result, error);
    break;
  }
  default:
    return parse_error(parser, error, "unsupported XLIL binary instruction");
  }
  if(status != XS_LIL_OK)
    return status;
  return result == expected_result ? XS_LIL_OK : parse_error(parser, error, "XLIL value ids must be sequential");
}

static XsLilStatus parse_instruction(Parser *parser, XsLilBlock *block, const char *line, size_t length,
                                     XsLilError *error)
{
  uint32_t result = 0;
  const char *colon = line;
  while(colon < line + length && *colon != ':')
    ++colon;
  if(colon == line + length || !parse_u32_after_prefix(line, (size_t)(colon - line), "%r", &result))
    return parse_error(parser, error, "unsupported XLIL instruction");
  const char *type_start = colon + 1;
  const char *type_end = type_start;
  while(type_end < line + length && *type_end != ' ' && *type_end != '\t')
    ++type_end;
  XsLilType result_type = {0};
  const char *equals = skip_space(type_end, line + length);
  if(type_end > type_start && parse_type(parser->module, type_start, (size_t)(type_end - type_start), &result_type) &&
     equals < line + length && *equals == '=')
  {
    const char *operation = skip_space(equals + 1, line + length);
    bool matched_string = false;
    XsLilStatus string_status = xs_lil_parse_const_str(
        block, result_type, operation, (size_t)(line + length - operation), result, &matched_string, error);
    if(matched_string)
      return string_status;
    bool matched_aggregate = false;
    XsLilStatus aggregate_status = xs_lil_parse_aggregate_instruction(
        block, result_type, operation, (size_t)(line + length - operation), result, &matched_aggregate, error);
    if(matched_aggregate)
      return aggregate_status;
    bool matched_u16 = false;
    XsLilStatus u16_status = xs_lil_parse_const_u16(block, result_type, operation, (size_t)(line + length - operation),
                                                    result, &matched_u16, error);
    if(matched_u16)
      return u16_status;
    bool matched_integer = false;
    XsLilStatus integer_status = xs_lil_parse_const_integer(
        block, result_type, operation, (size_t)(line + length - operation), result, &matched_integer, error);
    if(matched_integer)
      return integer_status;
    static const char load_prefix[] = "load %s";
    if((size_t)(line + length - operation) > sizeof(load_prefix) - 1U &&
       strncmp(operation, load_prefix, sizeof(load_prefix) - 1U) == 0)
    {
      uint32_t slot = 0;
      if(!parse_u32_after_prefix(operation, (size_t)(line + length - operation), load_prefix, &slot) ||
         xs_lil_function_slot_type(block->owner, slot).kind != result_type.kind)
        return parse_error(parser, error, "XLIL load type or stack slot is invalid");
      XsLilValueId actual = 0;
      XsLilStatus status = xs_lil_block_add_load(block, slot, &actual, error);
      if(status != XS_LIL_OK)
        return status;
      return actual == result ? XS_LIL_OK : parse_error(parser, error, "XLIL value ids must be sequential");
    }
    if((size_t)(line + length - operation) >= 5U && strncmp(operation, "call ", 5) == 0)
      return parse_call(parser, block, operation, (size_t)(line + length - operation), &result_type, result, error);
    const char *float_prefix = result_type.kind == XS_LIL_TYPE_F32 ? "const.f32 0x" : "const.f64 0x";
    size_t float_digits = result_type.kind == XS_LIL_TYPE_F32 ? 8U : 16U;
    size_t float_prefix_length = strlen(float_prefix);
    size_t operation_length = (size_t)(line + length - operation);
    if((result_type.kind == XS_LIL_TYPE_F32 || result_type.kind == XS_LIL_TYPE_F64) &&
       operation_length == float_prefix_length + float_digits &&
       strncmp(operation, float_prefix, float_prefix_length) == 0)
    {
      char digits[17] = {0};
      memcpy(digits, operation + float_prefix_length, float_digits);
      char *end = nullptr;
      errno = 0;
      unsigned long long bits = strtoull(digits, &end, 16);
      if(errno != 0 || end == digits || *end != '\0')
        return parse_error(parser, error, "XLIL floating constant bit pattern is invalid");
      XsLilValueId actual = 0;
      XsLilStatus status = result_type.kind == XS_LIL_TYPE_F32
                               ? xs_lil_block_add_const_f32_bits(block, (uint32_t)bits, &actual, error)
                               : xs_lil_block_add_const_f64_bits(block, (uint64_t)bits, &actual, error);
      if(status != XS_LIL_OK)
        return status;
      return actual == result ? XS_LIL_OK : parse_error(parser, error, "XLIL value ids must be sequential");
    }
    bool integer_operation_matched = false;
    XsLilStatus integer_operation_status = xs_lil_parse_integer_operation(
        block, result_type, operation, operation_length, result, &integer_operation_matched, error);
    if(integer_operation_matched)
      return integer_operation_status;
    bool str_comparison_matched = false;
    XsLilStatus str_comparison_status = xs_lil_parse_str_comparison(block, result_type, operation, operation_length,
                                                                    result, &str_comparison_matched, error);
    if(str_comparison_matched)
      return str_comparison_status;
    static const struct
    {
      const char *name;
      XsLilInstructionKind kind;
      XsLilTypeKind result_type;
    } binary_instructions[] = {
        {"add.i64 ", XS_LIL_INSTRUCTION_ADD_I64, XS_LIL_TYPE_I64},
        {"sub.i64 ", XS_LIL_INSTRUCTION_SUB_I64, XS_LIL_TYPE_I64},
        {"mul.i64 ", XS_LIL_INSTRUCTION_MUL_I64, XS_LIL_TYPE_I64},
        {"div.i64 ", XS_LIL_INSTRUCTION_DIV_I64, XS_LIL_TYPE_I64},
        {"rem.i64 ", XS_LIL_INSTRUCTION_REM_I64, XS_LIL_TYPE_I64},
        {"and.i64 ", XS_LIL_INSTRUCTION_AND_I64, XS_LIL_TYPE_I64},
        {"or.i64 ", XS_LIL_INSTRUCTION_OR_I64, XS_LIL_TYPE_I64},
        {"xor.i64 ", XS_LIL_INSTRUCTION_XOR_I64, XS_LIL_TYPE_I64},
        {"shl.i64 ", XS_LIL_INSTRUCTION_SHL_I64, XS_LIL_TYPE_I64},
        {"shr.i64 ", XS_LIL_INSTRUCTION_SHR_I64, XS_LIL_TYPE_I64},
        {"eq.i64 ", XS_LIL_INSTRUCTION_EQ_I64, XS_LIL_TYPE_BOOL},
        {"ne.i64 ", XS_LIL_INSTRUCTION_NE_I64, XS_LIL_TYPE_BOOL},
        {"lt.i64 ", XS_LIL_INSTRUCTION_LT_I64, XS_LIL_TYPE_BOOL},
        {"le.i64 ", XS_LIL_INSTRUCTION_LE_I64, XS_LIL_TYPE_BOOL},
        {"gt.i64 ", XS_LIL_INSTRUCTION_GT_I64, XS_LIL_TYPE_BOOL},
        {"ge.i64 ", XS_LIL_INSTRUCTION_GE_I64, XS_LIL_TYPE_BOOL},
        {"add.i32 ", XS_LIL_INSTRUCTION_ADD_I32, XS_LIL_TYPE_I32},
        {"sub.i32 ", XS_LIL_INSTRUCTION_SUB_I32, XS_LIL_TYPE_I32},
        {"mul.i32 ", XS_LIL_INSTRUCTION_MUL_I32, XS_LIL_TYPE_I32},
        {"div.i32 ", XS_LIL_INSTRUCTION_DIV_I32, XS_LIL_TYPE_I32},
        {"rem.i32 ", XS_LIL_INSTRUCTION_REM_I32, XS_LIL_TYPE_I32},
        {"and.i32 ", XS_LIL_INSTRUCTION_AND_I32, XS_LIL_TYPE_I32},
        {"or.i32 ", XS_LIL_INSTRUCTION_OR_I32, XS_LIL_TYPE_I32},
        {"xor.i32 ", XS_LIL_INSTRUCTION_XOR_I32, XS_LIL_TYPE_I32},
        {"shl.i32 ", XS_LIL_INSTRUCTION_SHL_I32, XS_LIL_TYPE_I32},
        {"shr.i32 ", XS_LIL_INSTRUCTION_SHR_I32, XS_LIL_TYPE_I32},
        {"eq.i32 ", XS_LIL_INSTRUCTION_EQ_I32, XS_LIL_TYPE_BOOL},
        {"ne.i32 ", XS_LIL_INSTRUCTION_NE_I32, XS_LIL_TYPE_BOOL},
        {"lt.i32 ", XS_LIL_INSTRUCTION_LT_I32, XS_LIL_TYPE_BOOL},
        {"le.i32 ", XS_LIL_INSTRUCTION_LE_I32, XS_LIL_TYPE_BOOL},
        {"gt.i32 ", XS_LIL_INSTRUCTION_GT_I32, XS_LIL_TYPE_BOOL},
        {"ge.i32 ", XS_LIL_INSTRUCTION_GE_I32, XS_LIL_TYPE_BOOL},
        {"add.f32 ", XS_LIL_INSTRUCTION_ADD_F32, XS_LIL_TYPE_F32},
        {"sub.f32 ", XS_LIL_INSTRUCTION_SUB_F32, XS_LIL_TYPE_F32},
        {"mul.f32 ", XS_LIL_INSTRUCTION_MUL_F32, XS_LIL_TYPE_F32},
        {"div.f32 ", XS_LIL_INSTRUCTION_DIV_F32, XS_LIL_TYPE_F32},
        {"rem.f32 ", XS_LIL_INSTRUCTION_REM_F32, XS_LIL_TYPE_F32},
        {"eq.f32 ", XS_LIL_INSTRUCTION_EQ_F32, XS_LIL_TYPE_BOOL},
        {"ne.f32 ", XS_LIL_INSTRUCTION_NE_F32, XS_LIL_TYPE_BOOL},
        {"lt.f32 ", XS_LIL_INSTRUCTION_LT_F32, XS_LIL_TYPE_BOOL},
        {"le.f32 ", XS_LIL_INSTRUCTION_LE_F32, XS_LIL_TYPE_BOOL},
        {"gt.f32 ", XS_LIL_INSTRUCTION_GT_F32, XS_LIL_TYPE_BOOL},
        {"ge.f32 ", XS_LIL_INSTRUCTION_GE_F32, XS_LIL_TYPE_BOOL},
        {"add.f64 ", XS_LIL_INSTRUCTION_ADD_F64, XS_LIL_TYPE_F64},
        {"sub.f64 ", XS_LIL_INSTRUCTION_SUB_F64, XS_LIL_TYPE_F64},
        {"mul.f64 ", XS_LIL_INSTRUCTION_MUL_F64, XS_LIL_TYPE_F64},
        {"div.f64 ", XS_LIL_INSTRUCTION_DIV_F64, XS_LIL_TYPE_F64},
        {"rem.f64 ", XS_LIL_INSTRUCTION_REM_F64, XS_LIL_TYPE_F64},
        {"eq.f64 ", XS_LIL_INSTRUCTION_EQ_F64, XS_LIL_TYPE_BOOL},
        {"ne.f64 ", XS_LIL_INSTRUCTION_NE_F64, XS_LIL_TYPE_BOOL},
        {"lt.f64 ", XS_LIL_INSTRUCTION_LT_F64, XS_LIL_TYPE_BOOL},
        {"le.f64 ", XS_LIL_INSTRUCTION_LE_F64, XS_LIL_TYPE_BOOL},
        {"gt.f64 ", XS_LIL_INSTRUCTION_GT_F64, XS_LIL_TYPE_BOOL},
        {"ge.f64 ", XS_LIL_INSTRUCTION_GE_F64, XS_LIL_TYPE_BOOL},
    };
    for(size_t binary = 0; binary < sizeof(binary_instructions) / sizeof(binary_instructions[0]); ++binary)
    {
      size_t name_length = strlen(binary_instructions[binary].name);
      if(result_type.kind == binary_instructions[binary].result_type && operation_length > name_length &&
         strncmp(operation, binary_instructions[binary].name, name_length) == 0)
        return parse_binary_integer(parser, block, operation + name_length, operation_length - name_length,
                                    binary_instructions[binary].kind, result, error);
    }
  }
  if((size_t)(line + length - colon) >= 7U && span_equals(colon, 7, ":bool ="))
  {
    const char *operation = skip_space(colon + 7, line + length);
    static const char not_bool_prefix[] = "not.bool %r";
    if((size_t)(line + length - operation) > sizeof(not_bool_prefix) - 1U &&
       strncmp(operation, not_bool_prefix, sizeof(not_bool_prefix) - 1U) == 0)
    {
      uint32_t operand = 0;
      if(!parse_u32_after_prefix(operation, (size_t)(line + length - operation), not_bool_prefix, &operand))
        return parse_error(parser, error, "unsupported XLIL instruction");
      XsLilValueId actual = 0;
      XsLilStatus status = xs_lil_block_not_bool(block, operand, &actual, error);
      if(status != XS_LIL_OK)
        return status;
      if(actual != result)
        return parse_error(parser, error, "XLIL value ids must be sequential");
      return XS_LIL_OK;
    }
    const char *true_text = "const.bool true";
    const char *false_text = "const.bool false";
    bool value = false;
    if((size_t)(line + length - operation) == strlen(true_text) &&
       strncmp(operation, true_text, strlen(true_text)) == 0)
      value = true;
    else if((size_t)(line + length - operation) == strlen(false_text) &&
            strncmp(operation, false_text, strlen(false_text)) == 0)
      value = false;
    else
      return parse_error(parser, error, "unsupported XLIL instruction");
    XsLilValueId actual = 0;
    XsLilStatus status = xs_lil_block_add_const_bool(block, value, &actual, error);
    if(status != XS_LIL_OK)
      return status;
    if(actual != result)
      return parse_error(parser, error, "XLIL value ids must be sequential");
    return XS_LIL_OK;
  }
  if((size_t)(line + length - colon) < 6U || !span_equals(colon, 6, ":i64 ="))
  {
    if((size_t)(line + length - colon) < 6U || !span_equals(colon, 6, ":i32 ="))
      return parse_error(parser, error, "unsupported XLIL instruction");
    const char *operation = skip_space(colon + 6, line + length);
    static const char const_i32_prefix[] = "const.i32 ";
    int64_t value = 0;
    if((size_t)(line + length - operation) < sizeof(const_i32_prefix) - 1U ||
       strncmp(operation, const_i32_prefix, sizeof(const_i32_prefix) - 1U) != 0 ||
       !parse_i64_tail(operation + sizeof(const_i32_prefix) - 1U, line + length, &value) || value < INT32_MIN ||
       value > INT32_MAX)
      return parse_error(parser, error, "unsupported XLIL instruction");
    XsLilValueId actual = 0;
    XsLilStatus status = xs_lil_block_add_const_i32(block, (int32_t)value, &actual, error);
    if(status != XS_LIL_OK)
      return status;
    if(actual != result)
      return parse_error(parser, error, "XLIL value ids must be sequential");
    return XS_LIL_OK;
  }
  const char *operation = skip_space(colon + 6, line + length);
  const char *const_i64_prefix = "const.i64 ";
  int64_t value = 0;
  bool parsed = (size_t)(line + length - operation) >= strlen(const_i64_prefix) &&
                strncmp(operation, const_i64_prefix, strlen(const_i64_prefix)) == 0 &&
                parse_i64_tail(operation + strlen(const_i64_prefix), line + length, &value);
  if(!parsed)
    return parse_error(parser, error, "unsupported XLIL instruction");
  XsLilValueId actual = 0;
  XsLilStatus status = xs_lil_block_add_const_i64(block, value, &actual, error);
  if(status != XS_LIL_OK)
    return status;
  if(actual != result)
    return parse_error(parser, error, "XLIL value ids must be sequential");
  return XS_LIL_OK;
}

static XsLilStatus parse_store(Parser *parser, XsLilBlock *block, const char *line, size_t length, XsLilError *error)
{
  static const char prefix[] = "store %r";
  if(length <= sizeof(prefix) - 1U || strncmp(line, prefix, sizeof(prefix) - 1U) != 0)
    return parse_error(parser, error, "XLIL store instruction is invalid");
  const char *comma = line + sizeof(prefix) - 1U;
  while(comma < line + length && *comma != ',')
    ++comma;
  uint32_t value = 0;
  if(comma == line + length ||
     !parse_u32_after_prefix(line + sizeof(prefix) - 1U, (size_t)(comma - (line + sizeof(prefix) - 1U)), "", &value))
    return parse_error(parser, error, "XLIL store value is invalid");
  const char *slot = skip_space(comma + 1, line + length);
  uint32_t slot_id = 0;
  if(!parse_u32_after_prefix(slot, (size_t)(line + length - slot), "%s", &slot_id))
    return parse_error(parser, error, "XLIL store stack slot is invalid");
  return xs_lil_block_add_store(block, slot_id, value, error);
}

static XsLilStatus parse_terminator(Parser *parser, XsLilBlock *block, const char *line, size_t length,
                                    PendingBranch **branches, size_t *branch_count, XsLilError *error)
{
  if(span_equals(line, length, "ret"))
    return xs_lil_block_set_return(block, error);
  if(span_equals(line, length, "panic"))
    return xs_lil_block_set_panic(block, error);
  uint32_t value = 0;
  if(parse_u32_after_prefix(line, length, "ret %r", &value))
    return xs_lil_block_set_return_value(block, value, error);
  uint32_t target = 0;
  if(parse_u32_after_prefix(line, length, "br bb", &target))
  {
    if(!append_branch(branches, branch_count, (PendingBranch){.block = block, .target = target}))
      return xs_lil_set_error(error, XS_LIL_ALLOCATION_FAILED, "out of memory while parsing XLIL branch");
    return XS_LIL_OK;
  }
  const char *branch_if_prefix = "br_if %r";
  size_t prefix_length = strlen(branch_if_prefix);
  if(length > prefix_length && strncmp(line, branch_if_prefix, prefix_length) == 0)
  {
    const char *cursor = line + prefix_length;
    const char *comma = cursor;
    while(comma < line + length && *comma != ',')
      ++comma;
    uint32_t condition = 0;
    if(comma == line + length || !parse_u32_after_prefix(cursor, (size_t)(comma - cursor), "", &condition))
      return parse_error(parser, error, "unsupported XLIL terminator");
    cursor = skip_space(comma + 1, line + length);
    comma = cursor;
    while(comma < line + length && *comma != ',')
      ++comma;
    uint32_t then_block = 0;
    uint32_t else_block = 0;
    if(comma == line + length || !parse_u32_after_prefix(cursor, (size_t)(comma - cursor), "bb", &then_block))
      return parse_error(parser, error, "unsupported XLIL terminator");
    cursor = skip_space(comma + 1, line + length);
    if(!parse_u32_after_prefix(cursor, (size_t)(line + length - cursor), "bb", &else_block))
      return parse_error(parser, error, "unsupported XLIL terminator");
    PendingBranch branch = {.block = block,
                            .is_conditional = true,
                            .condition = condition,
                            .target = then_block,
                            .else_target = else_block};
    if(!append_branch(branches, branch_count, branch))
      return xs_lil_set_error(error, XS_LIL_ALLOCATION_FAILED, "out of memory while parsing XLIL branch_if");
    return XS_LIL_OK;
  }
  return parse_error(parser, error, "unsupported XLIL terminator");
}

static XsLilStatus parse_function_body(Parser *parser, XsLilFunction *function, XsLilError *error)
{
  PendingBranch *branches = nullptr;
  size_t branch_count = 0;
  XsLilBlock *current = nullptr;
  bool current_terminated = false;
  size_t parameter = 0;
  const char *line = nullptr;
  size_t length = 0;
  while(next_line(parser, &line, &length))
  {
    const char *trimmed = skip_space(line, line + length);
    size_t trimmed_length = (size_t)(line + length - trimmed);
    if(trimmed_length == 0)
      continue;
    if(span_equals(trimmed, trimmed_length, ".end"))
      break;
    if(trimmed_length > 7 && strncmp(trimmed, ".param ", 7) == 0)
    {
      if(current != nullptr || parameter >= function->parameter_count)
        return parse_error(parser, error, "XLIL parameter record is misplaced or duplicated");
      const char *record = trimmed + 7;
      const char *colon = record;
      while(colon < trimmed + trimmed_length && *colon != ':')
        ++colon;
      uint32_t value = 0;
      XsLilType type = {0};
      if(colon == trimmed + trimmed_length || !parse_u32_after_prefix(record, (size_t)(colon - record), "%r", &value) ||
         value != parameter ||
         !parse_type(parser->module, colon + 1, (size_t)(trimmed + trimmed_length - (colon + 1)), &type) ||
         !xs_lil_type_equal(type, function->parameters[parameter]))
        return parse_error(parser, error, "XLIL parameter record does not match function signature");
      ++parameter;
      continue;
    }
    if(trimmed_length > 6 && strncmp(trimmed, ".slot ", 6) == 0)
    {
      if(current != nullptr || parameter != function->parameter_count)
        return parse_error(parser, error, "XLIL stack slot record is misplaced");
      const char *record = trimmed + 6;
      const char *colon = record;
      while(colon < trimmed + trimmed_length && *colon != ':')
        ++colon;
      uint32_t expected = 0;
      XsLilType type = {0};
      if(colon == trimmed + trimmed_length ||
         !parse_u32_after_prefix(record, (size_t)(colon - record), "%s", &expected) ||
         !parse_type(parser->module, colon + 1, (size_t)(trimmed + trimmed_length - (colon + 1)), &type))
        return parse_error(parser, error, "XLIL stack slot record is invalid");
      XsLilSlotId actual = UINT32_MAX;
      XsLilStatus status = xs_lil_function_add_slot(function, type, &actual, error);
      if(status != XS_LIL_OK)
        return status;
      if(actual != expected)
        return parse_error(parser, error, "XLIL stack slot ids must be sequential");
      continue;
    }
    if(trimmed_length > 4 && trimmed[0] == 'b' && trimmed[1] == 'b')
    {
      if(parameter != function->parameter_count)
        return parse_error(parser, error, "XLIL function parameter records are incomplete");
      const char *dot = trimmed;
      while(dot < trimmed + trimmed_length && *dot != '.')
        ++dot;
      if(dot == trimmed + trimmed_length || trimmed[trimmed_length - 1] != ':')
        return parse_error(parser, error, "XLIL block header is invalid");
      uint32_t id = 0;
      if(!parse_u32_after_prefix(trimmed, (size_t)(dot - trimmed), "bb", &id))
        return parse_error(parser, error, "XLIL block id is invalid");
      char *label = xs_lil_copy_span(dot + 1, (size_t)(trimmed + trimmed_length - 1 - (dot + 1)));
      if(label == nullptr)
        return xs_lil_set_error(error, XS_LIL_ALLOCATION_FAILED, "out of memory while parsing XLIL block label");
      XsLilStatus status = xs_lil_function_append_block(function, label, &current, error);
      free(label);
      if(status != XS_LIL_OK)
        return status;
      if(current->id != id)
        return parse_error(parser, error, "XLIL block ids must be sequential");
      current_terminated = false;
      continue;
    }
    if(current == nullptr)
      return parse_error(parser, error, "XLIL instruction appears before a block");
    if(current_terminated)
      return parse_error(parser, error, "XLIL instruction appears after a terminator");
    XsLilStatus status = XS_LIL_OK;
    if(trimmed[0] == '%' || (trimmed_length >= 5 && strncmp(trimmed, "call ", 5) == 0) ||
       (trimmed_length >= 6 && strncmp(trimmed, "store ", 6) == 0))
    {
      if(trimmed[0] == '%')
        status = parse_instruction(parser, current, trimmed, trimmed_length, error);
      else if(strncmp(trimmed, "store ", 6) == 0)
        status = parse_store(parser, current, trimmed, trimmed_length, error);
      else
        status = parse_call(parser, current, trimmed, trimmed_length, nullptr, 0, error);
    }
    else
    {
      status = parse_terminator(parser, current, trimmed, trimmed_length, &branches, &branch_count, error);
      if(status == XS_LIL_OK)
        current_terminated = true;
    }
    if(status != XS_LIL_OK)
    {
      free(branches);
      return status;
    }
  }
  if(line == nullptr ||
     !span_equals(skip_space(line, line + length), (size_t)(line + length - skip_space(line, line + length)), ".end"))
  {
    free(branches);
    return parse_error(parser, error, "XLIL function body is not closed");
  }
  if(parameter != function->parameter_count)
  {
    free(branches);
    return parse_error(parser, error, "XLIL function parameter records are incomplete");
  }
  for(size_t i = 0; i < branch_count; ++i)
  {
    XsLilStatus status = branches[i].is_conditional
                             ? xs_lil_block_set_branch_if(branches[i].block, branches[i].condition, branches[i].target,
                                                          branches[i].else_target, error)
                             : xs_lil_block_set_branch(branches[i].block, branches[i].target, error);
    if(status != XS_LIL_OK)
    {
      free(branches);
      return status;
    }
  }
  free(branches);
  for(size_t i = 0; i < function->block_count; ++i)
  {
    if(function->blocks[i]->terminator.kind == XS_LIL_TERMINATOR_NONE)
      return parse_error(parser, error, "XLIL block is missing a terminator");
  }
  return XS_LIL_OK;
}

XsLilStatus xs_lil_module_parse_text(const char *path, const char *text, size_t length, XsLilModule **module,
                                     XsLilError *error)
{
  xs_lil_clear_error(error);
  if(text == nullptr || module == nullptr)
    return xs_lil_set_error(error, XS_LIL_INVALID_ARGUMENT, "XLIL text and output module are required");
  *module = nullptr;
  Parser parser = {.path = path, .text = text, .length = length};
  const char *line = nullptr;
  size_t line_length = 0;
  if(!next_line(&parser, &line, &line_length) ||
     (!span_equals(line, line_length, ".xlil version 0") && !span_equals(line, line_length, ".xlil version 1")))
    return parse_error(&parser, error, "XLIL version header is invalid or unsupported");
  if(!next_line(&parser, &line, &line_length))
    return parse_error(&parser, error, "XLIL module header is missing");
  const char *module_prefix = ".xlil module ";
  size_t module_prefix_length = strlen(module_prefix);
  if(line_length <= module_prefix_length || strncmp(line, module_prefix, module_prefix_length) != 0)
    return parse_error(&parser, error, "XLIL module header is invalid");
  char *module_name = xs_lil_copy_span(line + module_prefix_length, line_length - module_prefix_length);
  if(module_name == nullptr)
    return xs_lil_set_error(error, XS_LIL_ALLOCATION_FAILED, "out of memory while parsing XLIL module name");
  XsLilModule *result = nullptr;
  XsLilStatus status = xs_lil_module_create(module_name, &result, error);
  free(module_name);
  if(status != XS_LIL_OK)
    return status;
  parser.module = result;
  while(next_line(&parser, &line, &line_length))
  {
    const char *trimmed = skip_space(line, line + line_length);
    size_t trimmed_length = (size_t)(line + line_length - trimmed);
    if(trimmed_length == 0)
      continue;
    if(trimmed_length > 6 && strncmp(trimmed, ".type ", 6) == 0)
    {
      status = xs_lil_parse_aggregate_record(result, trimmed, trimmed_length, error);
      if(status != XS_LIL_OK)
      {
        xs_lil_module_destroy(result);
        return status;
      }
      continue;
    }
    if(trimmed_length > 7 && strncmp(trimmed, ".array ", 7) == 0)
    {
      status = xs_lil_parse_array_record(result, trimmed, trimmed_length, error);
      if(status != XS_LIL_OK)
      {
        xs_lil_module_destroy(result);
        return status;
      }
      continue;
    }
    const char *prefix = nullptr;
    bool is_definition = false;
    if(trimmed_length > 8 && strncmp(trimmed, ".extern ", 8) == 0)
      prefix = ".extern ";
    else if(trimmed_length > 6 && strncmp(trimmed, ".func ", 6) == 0)
    {
      prefix = ".func ";
      is_definition = true;
    }
    else
    {
      xs_lil_module_destroy(result);
      return parse_error(&parser, error, "XLIL function record is invalid");
    }
    char *name = nullptr;
    XsLilType return_type = {0};
    XsLilType *parameters = nullptr;
    size_t parameter_count = 0;
    bool signature_ok =
        xs_lil_parse_signature(result, trimmed, trimmed_length, prefix, &name, &return_type, &parameters,
                               &parameter_count);
    if(!signature_ok)
    {
      free(name);
      free(parameters);
      xs_lil_module_destroy(result);
      return parse_error(&parser, error, "XLIL signature is invalid");
    }
    XsLilFunction *function = nullptr;
    status = is_definition ? xs_lil_module_add_function_definition(result, name, return_type, parameters,
                                                                   parameter_count, &function, error)
                           : xs_lil_module_add_function(result, name, return_type, parameters, parameter_count, error);
    free(name);
    free(parameters);
    if(status != XS_LIL_OK)
    {
      xs_lil_module_destroy(result);
      return status;
    }
    if(is_definition)
    {
      status = parse_function_body(&parser, function, error);
      if(status != XS_LIL_OK)
      {
        xs_lil_module_destroy(result);
        return status;
      }
    }
  }
  status = xs_lil_module_verify(result, error);
  if(status != XS_LIL_OK)
  {
    xs_lil_module_destroy(result);
    return status;
  }
  *module = result;
  return XS_LIL_OK;
}
