/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 * Shared C and C++ ABI surface.
 *
 */

#ifndef XS_LIL_C_BUILDER_H
#define XS_LIL_C_BUILDER_H

#include "Visual/XSharp/lil-c/instruction.hh"

typedef struct XsLilBuilder XsLilBuilder;

#ifdef __cplusplus
extern "C"
{
#endif

    XS_LIL_API XsLilStatus xs_lil_builder_create(XsLilModule *module, XsLilBuilder **builder, XsLilError *error);
    XS_LIL_API void xs_lil_builder_destroy(XsLilBuilder *builder);
    XS_LIL_API XsLilModule *xs_lil_builder_module(XsLilBuilder *builder);
    XS_LIL_API XsLilBlock *xs_lil_builder_block(XsLilBuilder *builder);
    XS_LIL_API XsLilStatus xs_lil_builder_position_at_end(XsLilBuilder *builder, XsLilBlock *block, XsLilError *error);
    XS_LIL_API XsLilStatus xs_lil_builder_append_block(XsLilBuilder *builder, XsLilFunction *function,
                                                       const char *label, XsLilBlock **block, XsLilError *error);

    XS_LIL_API XsLilStatus xs_lil_builder_const_i32(XsLilBuilder *builder, int32_t value, XsLilValueId *result,
                                                    XsLilError *error);
    XS_LIL_API XsLilStatus xs_lil_builder_const_i64(XsLilBuilder *builder, int64_t value, XsLilValueId *result,
                                                    XsLilError *error);
    XS_LIL_API XsLilStatus xs_lil_builder_const_bool(XsLilBuilder *builder, bool value, XsLilValueId *result,
                                                     XsLilError *error);
    XS_LIL_API XsLilStatus xs_lil_builder_const_str(XsLilBuilder *builder, XsLilUtf32Encoding encoding,
                                                    const uint32_t *units, size_t unit_count, XsLilValueId *result,
                                                    XsLilError *error);
    XS_LIL_API XsLilStatus xs_lil_builder_const_f32_bits(XsLilBuilder *builder, uint32_t bits, XsLilValueId *result,
                                                         XsLilError *error);
    XS_LIL_API XsLilStatus xs_lil_builder_const_f64_bits(XsLilBuilder *builder, uint64_t bits, XsLilValueId *result,
                                                         XsLilError *error);
    XS_LIL_API XsLilStatus xs_lil_builder_binary_integer(XsLilBuilder *builder, XsLilIntegerBinaryOperation operation,
                                                         XsLilValueId left, XsLilValueId right, XsLilValueId *result,
                                                         XsLilError *error);
    XS_LIL_API XsLilStatus xs_lil_builder_binary_float(XsLilBuilder *builder, XsLilFloatBinaryOperation operation,
                                                       XsLilValueId left, XsLilValueId right, XsLilValueId *result,
                                                       XsLilError *error);
    XS_LIL_API XsLilStatus xs_lil_builder_compare_float(XsLilBuilder *builder, XsLilFloatComparisonOperation operation,
                                                        XsLilValueId left, XsLilValueId right, XsLilValueId *result,
                                                        XsLilError *error);
    XS_LIL_API XsLilStatus xs_lil_builder_compare_str(XsLilBuilder *builder, XsLilStrComparisonOperation operation,
                                                      XsLilValueId left, XsLilValueId right, XsLilValueId *result,
                                                      XsLilError *error);
    XS_LIL_API XsLilStatus xs_lil_builder_not_bool(XsLilBuilder *builder, XsLilValueId operand, XsLilValueId *result,
                                                   XsLilError *error);
    XS_LIL_API XsLilStatus xs_lil_builder_call(XsLilBuilder *builder, const char *callee, const XsLilValueId *arguments,
                                               size_t argument_count, XsLilValueId *result, XsLilError *error);
    XS_LIL_API XsLilStatus xs_lil_builder_extract(XsLilBuilder *builder, XsLilValueId aggregate, uint32_t field,
                                                  XsLilValueId *result, XsLilError *error);
    XS_LIL_API XsLilStatus xs_lil_builder_aggregate(XsLilBuilder *builder, XsLilType type, const XsLilValueId *fields,
                                                    size_t field_count, XsLilValueId *result, XsLilError *error);
    XS_LIL_API XsLilStatus xs_lil_builder_array(XsLilBuilder *builder, XsLilType type, const XsLilValueId *elements,
                                                size_t element_count, XsLilValueId *result, XsLilError *error);
    XS_LIL_API XsLilStatus xs_lil_builder_array_get(XsLilBuilder *builder, XsLilValueId array, XsLilValueId index,
                                                    XsLilValueId *result, XsLilError *error);
    XS_LIL_API XsLilStatus xs_lil_builder_array_set(XsLilBuilder *builder, XsLilValueId array, XsLilValueId index,
                                                    XsLilValueId replacement, XsLilValueId *result, XsLilError *error);
    XS_LIL_API XsLilStatus xs_lil_builder_array_length(XsLilBuilder *builder, XsLilValueId array, XsLilValueId *result,
                                                       XsLilError *error);
    XS_LIL_API XsLilStatus xs_lil_builder_load(XsLilBuilder *builder, XsLilSlotId slot, XsLilValueId *result,
                                               XsLilError *error);
    XS_LIL_API XsLilStatus xs_lil_builder_store(XsLilBuilder *builder, XsLilSlotId slot, XsLilValueId value,
                                                XsLilError *error);
    XS_LIL_API XsLilStatus xs_lil_builder_return(XsLilBuilder *builder, XsLilError *error);
    XS_LIL_API XsLilStatus xs_lil_builder_return_value(XsLilBuilder *builder, XsLilValueId value, XsLilError *error);
    XS_LIL_API XsLilStatus xs_lil_builder_branch(XsLilBuilder *builder, const XsLilBlock *target, XsLilError *error);
    XS_LIL_API XsLilStatus xs_lil_builder_branch_if(XsLilBuilder *builder, XsLilValueId condition,
                                                    const XsLilBlock *then_block, const XsLilBlock *else_block,
                                                    XsLilError *error);
    XS_LIL_API XsLilStatus xs_lil_builder_panic(XsLilBuilder *builder, XsLilError *error);

#ifdef __cplusplus
}
#endif

#endif
