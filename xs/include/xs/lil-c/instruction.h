/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 */

#ifndef XS_LIL_C_INSTRUCTION_H
#define XS_LIL_C_INSTRUCTION_H

#include "xs/lil-c/function.h"

#ifdef __cplusplus
extern "C"
{
#endif

    XS_LIL_API XsLilStatus xs_lil_block_add_const_i64(XsLilBlock *block, int64_t value, XsLilValueId *result,
                                                      XsLilError *error);
    XS_LIL_API XsLilStatus xs_lil_block_add_const_i32(XsLilBlock *block, int32_t value, XsLilValueId *result,
                                                      XsLilError *error);
    XS_LIL_API XsLilStatus xs_lil_block_add_const_u16(XsLilBlock *block, uint16_t value, XsLilValueId *result,
                                                      XsLilError *error);
    XS_LIL_API XsLilStatus xs_lil_block_add_const_u8(XsLilBlock *block, uint8_t value, XsLilValueId *result,
                                                     XsLilError *error);
    XS_LIL_API XsLilStatus xs_lil_block_add_const_i8(XsLilBlock *block, int8_t value, XsLilValueId *result,
                                                     XsLilError *error);
    XS_LIL_API XsLilStatus xs_lil_block_add_const_i16(XsLilBlock *block, int16_t value, XsLilValueId *result,
                                                      XsLilError *error);
    XS_LIL_API XsLilStatus xs_lil_block_add_const_u32(XsLilBlock *block, uint32_t value, XsLilValueId *result,
                                                      XsLilError *error);
    XS_LIL_API XsLilStatus xs_lil_block_add_const_u64(XsLilBlock *block, uint64_t value, XsLilValueId *result,
                                                      XsLilError *error);
    XS_LIL_API XsLilStatus xs_lil_block_add_const_u128(XsLilBlock *block, XsUInt128 value, XsLilValueId *result,
                                                       XsLilError *error);
    XS_LIL_API XsLilStatus xs_lil_block_add_const_i128(XsLilBlock *block, XsInt128 value, XsLilValueId *result,
                                                       XsLilError *error);
    XS_LIL_API XsLilStatus xs_lil_block_binary_integer(XsLilBlock *block, XsLilIntegerBinaryOperation operation,
                                                       XsLilType type, XsLilValueId left, XsLilValueId right,
                                                       XsLilValueId *result, XsLilError *error);
    XS_LIL_API XsLilStatus xs_lil_block_add_const_bool(XsLilBlock *block, bool value, XsLilValueId *result,
                                                       XsLilError *error);
    XS_LIL_API XsLilStatus xs_lil_block_add_const_str(XsLilBlock *block, XsLilUtf32Encoding encoding,
                                                      const uint32_t *units, size_t unit_count, XsLilValueId *result,
                                                      XsLilError *error);
    XS_LIL_API XsLilStatus xs_lil_block_add_const_f32_bits(XsLilBlock *block, uint32_t bits, XsLilValueId *result,
                                                           XsLilError *error);
    XS_LIL_API XsLilStatus xs_lil_block_add_const_f64_bits(XsLilBlock *block, uint64_t bits, XsLilValueId *result,
                                                           XsLilError *error);
    XS_LIL_API XsLilStatus xs_lil_block_binary_float(XsLilBlock *block, XsLilFloatBinaryOperation operation,
                                                     XsLilType type, XsLilValueId left, XsLilValueId right,
                                                     XsLilValueId *result, XsLilError *error);
    XS_LIL_API XsLilStatus xs_lil_block_compare_float(XsLilBlock *block, XsLilFloatComparisonOperation operation,
                                                      XsLilType type, XsLilValueId left, XsLilValueId right,
                                                      XsLilValueId *result, XsLilError *error);
    XS_LIL_API XsLilStatus xs_lil_block_compare_str(XsLilBlock *block, XsLilStrComparisonOperation operation,
                                                    XsLilValueId left, XsLilValueId right, XsLilValueId *result,
                                                    XsLilError *error);
    XS_LIL_API XsLilStatus xs_lil_block_add_i64(XsLilBlock *block, XsLilValueId left, XsLilValueId right,
                                                XsLilValueId *result, XsLilError *error);
    XS_LIL_API XsLilStatus xs_lil_block_sub_i64(XsLilBlock *block, XsLilValueId left, XsLilValueId right,
                                                XsLilValueId *result, XsLilError *error);
    XS_LIL_API XsLilStatus xs_lil_block_mul_i64(XsLilBlock *block, XsLilValueId left, XsLilValueId right,
                                                XsLilValueId *result, XsLilError *error);
    XS_LIL_API XsLilStatus xs_lil_block_div_i64(XsLilBlock *block, XsLilValueId left, XsLilValueId right,
                                                XsLilValueId *result, XsLilError *error);
    XS_LIL_API XsLilStatus xs_lil_block_rem_i64(XsLilBlock *block, XsLilValueId left, XsLilValueId right,
                                                XsLilValueId *result, XsLilError *error);
    XS_LIL_API XsLilStatus xs_lil_block_and_i64(XsLilBlock *block, XsLilValueId left, XsLilValueId right,
                                                XsLilValueId *result, XsLilError *error);
    XS_LIL_API XsLilStatus xs_lil_block_or_i64(XsLilBlock *block, XsLilValueId left, XsLilValueId right,
                                               XsLilValueId *result, XsLilError *error);
    XS_LIL_API XsLilStatus xs_lil_block_xor_i64(XsLilBlock *block, XsLilValueId left, XsLilValueId right,
                                                XsLilValueId *result, XsLilError *error);
    XS_LIL_API XsLilStatus xs_lil_block_shl_i64(XsLilBlock *block, XsLilValueId left, XsLilValueId right,
                                                XsLilValueId *result, XsLilError *error);
    XS_LIL_API XsLilStatus xs_lil_block_shr_i64(XsLilBlock *block, XsLilValueId left, XsLilValueId right,
                                                XsLilValueId *result, XsLilError *error);
    XS_LIL_API XsLilStatus xs_lil_block_eq_i64(XsLilBlock *block, XsLilValueId left, XsLilValueId right,
                                               XsLilValueId *result, XsLilError *error);
    XS_LIL_API XsLilStatus xs_lil_block_ne_i64(XsLilBlock *block, XsLilValueId left, XsLilValueId right,
                                               XsLilValueId *result, XsLilError *error);
    XS_LIL_API XsLilStatus xs_lil_block_lt_i64(XsLilBlock *block, XsLilValueId left, XsLilValueId right,
                                               XsLilValueId *result, XsLilError *error);
    XS_LIL_API XsLilStatus xs_lil_block_le_i64(XsLilBlock *block, XsLilValueId left, XsLilValueId right,
                                               XsLilValueId *result, XsLilError *error);
    XS_LIL_API XsLilStatus xs_lil_block_gt_i64(XsLilBlock *block, XsLilValueId left, XsLilValueId right,
                                               XsLilValueId *result, XsLilError *error);
    XS_LIL_API XsLilStatus xs_lil_block_ge_i64(XsLilBlock *block, XsLilValueId left, XsLilValueId right,
                                               XsLilValueId *result, XsLilError *error);
    XS_LIL_API XsLilStatus xs_lil_block_add_i32(XsLilBlock *block, XsLilValueId left, XsLilValueId right,
                                                XsLilValueId *result, XsLilError *error);
    XS_LIL_API XsLilStatus xs_lil_block_sub_i32(XsLilBlock *block, XsLilValueId left, XsLilValueId right,
                                                XsLilValueId *result, XsLilError *error);
    XS_LIL_API XsLilStatus xs_lil_block_mul_i32(XsLilBlock *block, XsLilValueId left, XsLilValueId right,
                                                XsLilValueId *result, XsLilError *error);
    XS_LIL_API XsLilStatus xs_lil_block_div_i32(XsLilBlock *block, XsLilValueId left, XsLilValueId right,
                                                XsLilValueId *result, XsLilError *error);
    XS_LIL_API XsLilStatus xs_lil_block_rem_i32(XsLilBlock *block, XsLilValueId left, XsLilValueId right,
                                                XsLilValueId *result, XsLilError *error);
    XS_LIL_API XsLilStatus xs_lil_block_and_i32(XsLilBlock *block, XsLilValueId left, XsLilValueId right,
                                                XsLilValueId *result, XsLilError *error);
    XS_LIL_API XsLilStatus xs_lil_block_or_i32(XsLilBlock *block, XsLilValueId left, XsLilValueId right,
                                               XsLilValueId *result, XsLilError *error);
    XS_LIL_API XsLilStatus xs_lil_block_xor_i32(XsLilBlock *block, XsLilValueId left, XsLilValueId right,
                                                XsLilValueId *result, XsLilError *error);
    XS_LIL_API XsLilStatus xs_lil_block_shl_i32(XsLilBlock *block, XsLilValueId left, XsLilValueId right,
                                                XsLilValueId *result, XsLilError *error);
    XS_LIL_API XsLilStatus xs_lil_block_shr_i32(XsLilBlock *block, XsLilValueId left, XsLilValueId right,
                                                XsLilValueId *result, XsLilError *error);
    XS_LIL_API XsLilStatus xs_lil_block_eq_i32(XsLilBlock *block, XsLilValueId left, XsLilValueId right,
                                               XsLilValueId *result, XsLilError *error);
    XS_LIL_API XsLilStatus xs_lil_block_ne_i32(XsLilBlock *block, XsLilValueId left, XsLilValueId right,
                                               XsLilValueId *result, XsLilError *error);
    XS_LIL_API XsLilStatus xs_lil_block_lt_i32(XsLilBlock *block, XsLilValueId left, XsLilValueId right,
                                               XsLilValueId *result, XsLilError *error);
    XS_LIL_API XsLilStatus xs_lil_block_le_i32(XsLilBlock *block, XsLilValueId left, XsLilValueId right,
                                               XsLilValueId *result, XsLilError *error);
    XS_LIL_API XsLilStatus xs_lil_block_gt_i32(XsLilBlock *block, XsLilValueId left, XsLilValueId right,
                                               XsLilValueId *result, XsLilError *error);
    XS_LIL_API XsLilStatus xs_lil_block_ge_i32(XsLilBlock *block, XsLilValueId left, XsLilValueId right,
                                               XsLilValueId *result, XsLilError *error);
    XS_LIL_API XsLilStatus xs_lil_block_not_bool(XsLilBlock *block, XsLilValueId operand, XsLilValueId *result,
                                                 XsLilError *error);
    XS_LIL_API XsLilStatus xs_lil_block_add_call(XsLilBlock *block, const char *callee, XsLilType return_type,
                                                 const XsLilValueId *arguments, size_t argument_count,
                                                 XsLilValueId *result, XsLilError *error);
    XS_LIL_API XsLilStatus xs_lil_block_add_void_call(XsLilBlock *block, const char *callee,
                                                      const XsLilValueId *arguments, size_t argument_count,
                                                      XsLilError *error);
    XS_LIL_API XsLilStatus xs_lil_block_add_aggregate(XsLilBlock *block, XsLilType type, const XsLilValueId *fields,
                                                      size_t field_count, XsLilValueId *result, XsLilError *error);
    XS_LIL_API XsLilStatus xs_lil_block_add_array(XsLilBlock *block, XsLilType type, const XsLilValueId *elements,
                                                  size_t element_count, XsLilValueId *result, XsLilError *error);
    XS_LIL_API XsLilStatus xs_lil_block_add_extract(XsLilBlock *block, XsLilValueId aggregate, uint32_t field,
                                                    XsLilType field_type, XsLilValueId *result, XsLilError *error);
    XS_LIL_API XsLilStatus xs_lil_block_add_array_get(XsLilBlock *block, XsLilValueId array, XsLilValueId index,
                                                      XsLilType element_type, XsLilValueId *result, XsLilError *error);
    XS_LIL_API XsLilStatus xs_lil_block_add_array_set(XsLilBlock *block, XsLilValueId array, XsLilValueId index,
                                                      XsLilValueId value, XsLilValueId *result, XsLilError *error);
    XS_LIL_API XsLilStatus xs_lil_block_add_array_length(XsLilBlock *block, XsLilValueId array, XsLilValueId *result,
                                                         XsLilError *error);
    XS_LIL_API XsLilStatus xs_lil_block_add_load(XsLilBlock *block, XsLilSlotId slot, XsLilValueId *result,
                                                 XsLilError *error);
    XS_LIL_API XsLilStatus xs_lil_block_add_store(XsLilBlock *block, XsLilSlotId slot, XsLilValueId value,
                                                  XsLilError *error);
    XS_LIL_API XsLilStatus xs_lil_block_set_return(XsLilBlock *block, XsLilError *error);
    XS_LIL_API XsLilStatus xs_lil_block_set_return_value(XsLilBlock *block, XsLilValueId value, XsLilError *error);
    XS_LIL_API XsLilStatus xs_lil_block_set_branch(XsLilBlock *block, XsLilBlockId target, XsLilError *error);
    XS_LIL_API XsLilStatus xs_lil_block_set_branch_if(XsLilBlock *block, XsLilValueId condition,
                                                      XsLilBlockId then_block, XsLilBlockId else_block,
                                                      XsLilError *error);
    XS_LIL_API XsLilStatus xs_lil_block_set_panic(XsLilBlock *block, XsLilError *error);
    XS_LIL_API XsLilBlockId xs_lil_block_id(const XsLilBlock *block);
    XS_LIL_API const char *xs_lil_block_label(const XsLilBlock *block);
    XS_LIL_API size_t xs_lil_block_instruction_count(const XsLilBlock *block);
    XS_LIL_API XsLilInstructionKind xs_lil_block_instruction_kind(const XsLilBlock *block, size_t index);
    XS_LIL_API bool xs_lil_block_instruction_has_result(const XsLilBlock *block, size_t index);
    XS_LIL_API XsLilValueId xs_lil_block_instruction_result(const XsLilBlock *block, size_t index);
    XS_LIL_API XsLilType xs_lil_block_instruction_result_type(const XsLilBlock *block, size_t index);
    XS_LIL_API int64_t xs_lil_block_instruction_i64(const XsLilBlock *block, size_t index);
    XS_LIL_API uint16_t xs_lil_block_instruction_u16(const XsLilBlock *block, size_t index);
    XS_LIL_API XsUInt128 xs_lil_block_instruction_integer_bits(const XsLilBlock *block, size_t index);
    XS_LIL_API XsLilIntegerBinaryOperation xs_lil_block_instruction_integer_operation(const XsLilBlock *block,
                                                                                      size_t index);
    XS_LIL_API XsLilType xs_lil_block_instruction_integer_type(const XsLilBlock *block, size_t index);
    XS_LIL_API uint64_t xs_lil_block_instruction_float_bits(const XsLilBlock *block, size_t index);
    XS_LIL_API bool xs_lil_block_instruction_bool(const XsLilBlock *block, size_t index);
    XS_LIL_API XsLilUtf32Encoding xs_lil_block_instruction_utf32_encoding(const XsLilBlock *block, size_t index);
    XS_LIL_API size_t xs_lil_block_instruction_utf32_length(const XsLilBlock *block, size_t index);
    XS_LIL_API uint32_t xs_lil_block_instruction_utf32_unit(const XsLilBlock *block, size_t index, size_t unit);
    XS_LIL_API const char *xs_lil_block_instruction_callee(const XsLilBlock *block, size_t index);
    XS_LIL_API size_t xs_lil_block_instruction_argument_count(const XsLilBlock *block, size_t index);
    XS_LIL_API XsLilValueId xs_lil_block_instruction_argument(const XsLilBlock *block, size_t index, size_t argument);
    XS_LIL_API XsLilValueId xs_lil_block_instruction_left(const XsLilBlock *block, size_t index);
    XS_LIL_API XsLilValueId xs_lil_block_instruction_right(const XsLilBlock *block, size_t index);
    XS_LIL_API uint32_t xs_lil_block_instruction_field(const XsLilBlock *block, size_t index);
    XS_LIL_API XsLilSlotId xs_lil_block_instruction_slot(const XsLilBlock *block, size_t index);
    XS_LIL_API XsLilTerminatorKind xs_lil_block_terminator_kind(const XsLilBlock *block);
    XS_LIL_API bool xs_lil_block_terminator_has_value(const XsLilBlock *block);
    XS_LIL_API XsLilValueId xs_lil_block_terminator_value(const XsLilBlock *block);
    XS_LIL_API XsLilBlockId xs_lil_block_terminator_target(const XsLilBlock *block);
    XS_LIL_API XsLilValueId xs_lil_block_terminator_condition(const XsLilBlock *block);
    XS_LIL_API XsLilBlockId xs_lil_block_terminator_then_block(const XsLilBlock *block);
    XS_LIL_API XsLilBlockId xs_lil_block_terminator_else_block(const XsLilBlock *block);

#ifdef __cplusplus
}
#endif

#endif
