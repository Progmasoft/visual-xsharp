/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 */

#ifndef XS_MIR_H
#define XS_MIR_H

#include "xs/lil.h"

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

typedef XsLilType XsMirType;
typedef uint32_t XsMirBlockId;
typedef uint32_t XsMirLocalId;
typedef uint32_t XsMirPlaceId;
typedef uint32_t XsMirValueId;

typedef enum
{
  XS_MIR_OK,
  XS_MIR_INVALID_ARGUMENT,
  XS_MIR_ALLOCATION_FAILED,
  XS_MIR_IO_ERROR,
  XS_MIR_UNSUPPORTED,
} XsMirStatus;

typedef struct
{
  XsMirStatus status;
  char message[256];
} XsMirError;

typedef struct XsMirFunction XsMirFunction;
typedef struct XsMirBlock XsMirBlock;
typedef struct XsMirModule XsMirModule;
typedef struct XsMirPlace XsMirPlace;

typedef enum
{
  XS_MIR_LOCAL_PARAMETER,
  XS_MIR_LOCAL_VARIABLE,
  XS_MIR_LOCAL_TEMPORARY,
} XsMirLocalKind;

typedef enum
{
  XS_MIR_PLACE_PROJECTION_FIELD,
  XS_MIR_PLACE_PROJECTION_DEREF,
  XS_MIR_PLACE_PROJECTION_INDEX,
} XsMirPlaceProjectionKind;

typedef enum
{
  XS_MIR_INSTRUCTION_CONST_I64,
  XS_MIR_INSTRUCTION_CONST_I32,
  XS_MIR_INSTRUCTION_CONST_BOOL,
  XS_MIR_INSTRUCTION_ADD_I64,
  XS_MIR_INSTRUCTION_SUB_I64,
  XS_MIR_INSTRUCTION_MUL_I64,
  XS_MIR_INSTRUCTION_DIV_I64,
  XS_MIR_INSTRUCTION_REM_I64,
  XS_MIR_INSTRUCTION_AND_I64,
  XS_MIR_INSTRUCTION_OR_I64,
  XS_MIR_INSTRUCTION_XOR_I64,
  XS_MIR_INSTRUCTION_SHL_I64,
  XS_MIR_INSTRUCTION_SHR_I64,
  XS_MIR_INSTRUCTION_EQ_I64,
  XS_MIR_INSTRUCTION_NE_I64,
  XS_MIR_INSTRUCTION_LT_I64,
  XS_MIR_INSTRUCTION_LE_I64,
  XS_MIR_INSTRUCTION_GT_I64,
  XS_MIR_INSTRUCTION_GE_I64,
  XS_MIR_INSTRUCTION_ADD_I32,
  XS_MIR_INSTRUCTION_SUB_I32,
  XS_MIR_INSTRUCTION_MUL_I32,
  XS_MIR_INSTRUCTION_DIV_I32,
  XS_MIR_INSTRUCTION_REM_I32,
  XS_MIR_INSTRUCTION_AND_I32,
  XS_MIR_INSTRUCTION_OR_I32,
  XS_MIR_INSTRUCTION_XOR_I32,
  XS_MIR_INSTRUCTION_SHL_I32,
  XS_MIR_INSTRUCTION_SHR_I32,
  XS_MIR_INSTRUCTION_EQ_I32,
  XS_MIR_INSTRUCTION_NE_I32,
  XS_MIR_INSTRUCTION_LT_I32,
  XS_MIR_INSTRUCTION_LE_I32,
  XS_MIR_INSTRUCTION_GT_I32,
  XS_MIR_INSTRUCTION_GE_I32,
  XS_MIR_INSTRUCTION_NOT_BOOL,
  XS_MIR_INSTRUCTION_CALL,
  XS_MIR_INSTRUCTION_LOAD,
  XS_MIR_INSTRUCTION_STORE,
} XsMirInstructionKind;

typedef enum
{
  XS_MIR_TERMINATOR_NONE,
  XS_MIR_TERMINATOR_RETURN,
  XS_MIR_TERMINATOR_GOTO,
  XS_MIR_TERMINATOR_BRANCH,
  XS_MIR_TERMINATOR_UNREACHABLE,
  XS_MIR_TERMINATOR_PANIC,
} XsMirTerminatorKind;

XsMirStatus xs_mir_module_create(const char *name, XsMirModule **module, XsMirError *error);
void xs_mir_module_destroy(XsMirModule *module);
const char *xs_mir_module_name(const XsMirModule *module);

XsMirStatus xs_mir_module_add_function_declaration(XsMirModule *module, const char *qualified_name,
                                                   XsMirType return_type, const XsMirType *parameters,
                                                   size_t parameter_count, XsMirError *error);
XsMirStatus xs_mir_module_add_function_definition(XsMirModule *module, const char *qualified_name,
                                                  XsMirType return_type, const XsMirType *parameters,
                                                  size_t parameter_count, XsMirFunction **function, XsMirError *error);
size_t xs_mir_module_function_count(const XsMirModule *module);
const XsMirFunction *xs_mir_module_function_at(const XsMirModule *module, size_t index);
const char *xs_mir_function_name(const XsMirFunction *function);
XsMirType xs_mir_function_return_type(const XsMirFunction *function);
const XsMirType *xs_mir_function_parameters(const XsMirFunction *function);
size_t xs_mir_function_parameter_count(const XsMirFunction *function);
bool xs_mir_function_is_definition(const XsMirFunction *function);

XsMirStatus xs_mir_function_add_local(XsMirFunction *function, XsMirLocalKind kind, const char *name, XsMirType type,
                                      bool is_mutable, XsMirLocalId *local, XsMirError *error);
size_t xs_mir_function_local_count(const XsMirFunction *function);
XsMirLocalKind xs_mir_function_local_kind(const XsMirFunction *function, XsMirLocalId local);
const char *xs_mir_function_local_name(const XsMirFunction *function, XsMirLocalId local);
XsMirType xs_mir_function_local_type(const XsMirFunction *function, XsMirLocalId local);
bool xs_mir_function_local_is_mutable(const XsMirFunction *function, XsMirLocalId local);
XsMirType xs_mir_function_value_type(const XsMirFunction *function, XsMirValueId value);

XsMirStatus xs_mir_function_append_block(XsMirFunction *function, const char *label, XsMirBlock **block,
                                         XsMirError *error);
size_t xs_mir_function_block_count(const XsMirFunction *function);
const XsMirBlock *xs_mir_function_block_at(const XsMirFunction *function, size_t index);
XsMirBlockId xs_mir_block_id(const XsMirBlock *block);
const char *xs_mir_block_label(const XsMirBlock *block);
XsMirTerminatorKind xs_mir_block_terminator_kind(const XsMirBlock *block);
XsMirStatus xs_mir_block_set_return(XsMirBlock *block, XsMirError *error);
XsMirStatus xs_mir_block_set_return_value(XsMirBlock *block, XsMirValueId value, XsMirError *error);
XsMirStatus xs_mir_block_set_goto(XsMirBlock *block, const XsMirBlock *target, XsMirError *error);
XsMirStatus xs_mir_block_set_branch(XsMirBlock *block, XsMirValueId condition, const XsMirBlock *then_target,
                                    const XsMirBlock *else_target, XsMirError *error);
XsMirStatus xs_mir_block_set_unreachable(XsMirBlock *block, XsMirError *error);
XsMirStatus xs_mir_block_set_panic(XsMirBlock *block, XsMirError *error);
XsMirStatus xs_mir_block_add_const_i64(XsMirBlock *block, int64_t value, XsMirValueId *result, XsMirError *error);
XsMirStatus xs_mir_block_add_const_i32(XsMirBlock *block, int32_t value, XsMirValueId *result, XsMirError *error);
XsMirStatus xs_mir_block_add_const_bool(XsMirBlock *block, bool value, XsMirValueId *result, XsMirError *error);
XsMirStatus xs_mir_block_add_i64(XsMirBlock *block, XsMirValueId left, XsMirValueId right, XsMirValueId *result,
                                 XsMirError *error);
XsMirStatus xs_mir_block_sub_i64(XsMirBlock *block, XsMirValueId left, XsMirValueId right, XsMirValueId *result,
                                 XsMirError *error);
XsMirStatus xs_mir_block_mul_i64(XsMirBlock *block, XsMirValueId left, XsMirValueId right, XsMirValueId *result,
                                 XsMirError *error);
XsMirStatus xs_mir_block_div_i64(XsMirBlock *block, XsMirValueId left, XsMirValueId right, XsMirValueId *result,
                                 XsMirError *error);
XsMirStatus xs_mir_block_rem_i64(XsMirBlock *block, XsMirValueId left, XsMirValueId right, XsMirValueId *result,
                                 XsMirError *error);
XsMirStatus xs_mir_block_and_i64(XsMirBlock *block, XsMirValueId left, XsMirValueId right, XsMirValueId *result,
                                 XsMirError *error);
XsMirStatus xs_mir_block_or_i64(XsMirBlock *block, XsMirValueId left, XsMirValueId right, XsMirValueId *result,
                                XsMirError *error);
XsMirStatus xs_mir_block_xor_i64(XsMirBlock *block, XsMirValueId left, XsMirValueId right, XsMirValueId *result,
                                 XsMirError *error);
XsMirStatus xs_mir_block_shl_i64(XsMirBlock *block, XsMirValueId left, XsMirValueId right, XsMirValueId *result,
                                 XsMirError *error);
XsMirStatus xs_mir_block_shr_i64(XsMirBlock *block, XsMirValueId left, XsMirValueId right, XsMirValueId *result,
                                 XsMirError *error);
XsMirStatus xs_mir_block_eq_i64(XsMirBlock *block, XsMirValueId left, XsMirValueId right, XsMirValueId *result,
                                XsMirError *error);
XsMirStatus xs_mir_block_ne_i64(XsMirBlock *block, XsMirValueId left, XsMirValueId right, XsMirValueId *result,
                                XsMirError *error);
XsMirStatus xs_mir_block_lt_i64(XsMirBlock *block, XsMirValueId left, XsMirValueId right, XsMirValueId *result,
                                XsMirError *error);
XsMirStatus xs_mir_block_le_i64(XsMirBlock *block, XsMirValueId left, XsMirValueId right, XsMirValueId *result,
                                XsMirError *error);
XsMirStatus xs_mir_block_gt_i64(XsMirBlock *block, XsMirValueId left, XsMirValueId right, XsMirValueId *result,
                                XsMirError *error);
XsMirStatus xs_mir_block_ge_i64(XsMirBlock *block, XsMirValueId left, XsMirValueId right, XsMirValueId *result,
                                XsMirError *error);
XsMirStatus xs_mir_block_add_i32(XsMirBlock *block, XsMirValueId left, XsMirValueId right, XsMirValueId *result,
                                 XsMirError *error);
XsMirStatus xs_mir_block_sub_i32(XsMirBlock *block, XsMirValueId left, XsMirValueId right, XsMirValueId *result,
                                 XsMirError *error);
XsMirStatus xs_mir_block_mul_i32(XsMirBlock *block, XsMirValueId left, XsMirValueId right, XsMirValueId *result,
                                 XsMirError *error);
XsMirStatus xs_mir_block_div_i32(XsMirBlock *block, XsMirValueId left, XsMirValueId right, XsMirValueId *result,
                                 XsMirError *error);
XsMirStatus xs_mir_block_rem_i32(XsMirBlock *block, XsMirValueId left, XsMirValueId right, XsMirValueId *result,
                                 XsMirError *error);
XsMirStatus xs_mir_block_and_i32(XsMirBlock *block, XsMirValueId left, XsMirValueId right, XsMirValueId *result,
                                 XsMirError *error);
XsMirStatus xs_mir_block_or_i32(XsMirBlock *block, XsMirValueId left, XsMirValueId right, XsMirValueId *result,
                                XsMirError *error);
XsMirStatus xs_mir_block_xor_i32(XsMirBlock *block, XsMirValueId left, XsMirValueId right, XsMirValueId *result,
                                 XsMirError *error);
XsMirStatus xs_mir_block_shl_i32(XsMirBlock *block, XsMirValueId left, XsMirValueId right, XsMirValueId *result,
                                 XsMirError *error);
XsMirStatus xs_mir_block_shr_i32(XsMirBlock *block, XsMirValueId left, XsMirValueId right, XsMirValueId *result,
                                 XsMirError *error);
XsMirStatus xs_mir_block_eq_i32(XsMirBlock *block, XsMirValueId left, XsMirValueId right, XsMirValueId *result,
                                XsMirError *error);
XsMirStatus xs_mir_block_ne_i32(XsMirBlock *block, XsMirValueId left, XsMirValueId right, XsMirValueId *result,
                                XsMirError *error);
XsMirStatus xs_mir_block_lt_i32(XsMirBlock *block, XsMirValueId left, XsMirValueId right, XsMirValueId *result,
                                XsMirError *error);
XsMirStatus xs_mir_block_le_i32(XsMirBlock *block, XsMirValueId left, XsMirValueId right, XsMirValueId *result,
                                XsMirError *error);
XsMirStatus xs_mir_block_gt_i32(XsMirBlock *block, XsMirValueId left, XsMirValueId right, XsMirValueId *result,
                                XsMirError *error);
XsMirStatus xs_mir_block_ge_i32(XsMirBlock *block, XsMirValueId left, XsMirValueId right, XsMirValueId *result,
                                XsMirError *error);
XsMirStatus xs_mir_block_not_bool(XsMirBlock *block, XsMirValueId operand, XsMirValueId *result, XsMirError *error);
XsMirStatus xs_mir_block_add_call(XsMirBlock *block, const char *callee, XsMirType return_type,
                                  const XsMirValueId *arguments, size_t argument_count, XsMirValueId *result,
                                  XsMirError *error);
XsMirStatus xs_mir_block_add_load(XsMirBlock *block, const XsMirPlace *place, XsMirType result_type,
                                  XsMirValueId *result, XsMirError *error);
XsMirStatus xs_mir_block_add_store(XsMirBlock *block, const XsMirPlace *place, XsMirValueId value, XsMirError *error);
size_t xs_mir_block_instruction_count(const XsMirBlock *block);
XsMirInstructionKind xs_mir_block_instruction_kind(const XsMirBlock *block, size_t index);
const char *xs_mir_block_instruction_callee(const XsMirBlock *block, size_t index);
size_t xs_mir_block_instruction_argument_count(const XsMirBlock *block, size_t index);
XsMirValueId xs_mir_block_instruction_argument(const XsMirBlock *block, size_t index, size_t argument_index);

XsMirStatus xs_mir_function_add_local_place(XsMirFunction *function, XsMirLocalId local, XsMirPlace **place,
                                            XsMirError *error);
XsMirPlaceId xs_mir_place_id(const XsMirPlace *place);
XsMirLocalId xs_mir_place_root_local(const XsMirPlace *place);
XsMirStatus xs_mir_place_add_field_projection(XsMirPlace *place, uint32_t field_index, XsMirError *error);
XsMirStatus xs_mir_place_add_deref_projection(XsMirPlace *place, XsMirError *error);
XsMirStatus xs_mir_place_add_index_projection(XsMirPlace *place, XsMirValueId index, XsMirError *error);
size_t xs_mir_place_projection_count(const XsMirPlace *place);
XsMirPlaceProjectionKind xs_mir_place_projection_kind(const XsMirPlace *place, size_t index);

XsMirStatus xs_mir_module_write_text(const XsMirModule *module, FILE *stream, XsMirError *error);

#endif
