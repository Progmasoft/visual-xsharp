/*
 * SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
 * SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.0
 * Shared C and C++ ABI surface.
 *
 */

#ifndef XS_LIL_C_MODEL_H
#define XS_LIL_C_MODEL_H

#include <stddef.h>
#include <stdint.h>

#include "Visual/C23/int128.hh"
#include "Visual/XSharp/lil-c/api.hh"

#ifdef __cplusplus
extern "C"
{
#endif

#define XS_LIL_TEXT_VERSION 1U
#define XS_LIL_INVALID_VALUE_ID UINT32_MAX
#define XS_LIL_INVALID_BLOCK_ID UINT32_MAX
#define XS_LIL_INVALID_SLOT_ID UINT32_MAX

    typedef enum
    {
        XS_LIL_OK,
        XS_LIL_INVALID_ARGUMENT,
        XS_LIL_ALLOCATION_FAILED,
        XS_LIL_IO_ERROR,
    } XsLilStatus;

    typedef struct
    {
        XsLilStatus status;
        char message[256];
    } XsLilError;

    typedef enum
    {
        XS_LIL_TYPE_VOID,
        XS_LIL_TYPE_BOOL,
        XS_LIL_TYPE_U8,
        XS_LIL_TYPE_I8,
        XS_LIL_TYPE_U16,
        XS_LIL_TYPE_I16,
        XS_LIL_TYPE_U32,
        XS_LIL_TYPE_I32,
        XS_LIL_TYPE_U64,
        XS_LIL_TYPE_I64,
        XS_LIL_TYPE_U128,
        XS_LIL_TYPE_I128,
        XS_LIL_TYPE_F16,
        XS_LIL_TYPE_F32,
        XS_LIL_TYPE_F64,
        XS_LIL_TYPE_F128,
        XS_LIL_TYPE_STR,
        XS_LIL_TYPE_STRING,
        XS_LIL_TYPE_AGGREGATE,
        XS_LIL_TYPE_ARRAY,
    } XsLilTypeKind;

    typedef struct
    {
        XsLilTypeKind kind;
        uint32_t registry_id;
    } XsLilType;

    typedef uint32_t XsLilValueId;
    typedef uint32_t XsLilBlockId;
    typedef uint32_t XsLilSlotId;

    typedef enum
    {
        XS_LIL_UTF32_LE,
        XS_LIL_UTF32_BE,
    } XsLilUtf32Encoding;

    typedef enum
    {
        XS_LIL_INSTRUCTION_CONST_I64,
        XS_LIL_INSTRUCTION_CONST_I32,
        XS_LIL_INSTRUCTION_CONST_BOOL,
        XS_LIL_INSTRUCTION_ADD_I64,
        XS_LIL_INSTRUCTION_SUB_I64,
        XS_LIL_INSTRUCTION_MUL_I64,
        XS_LIL_INSTRUCTION_DIV_I64,
        XS_LIL_INSTRUCTION_REM_I64,
        XS_LIL_INSTRUCTION_AND_I64,
        XS_LIL_INSTRUCTION_OR_I64,
        XS_LIL_INSTRUCTION_XOR_I64,
        XS_LIL_INSTRUCTION_SHL_I64,
        XS_LIL_INSTRUCTION_SHR_I64,
        XS_LIL_INSTRUCTION_EQ_I64,
        XS_LIL_INSTRUCTION_NE_I64,
        XS_LIL_INSTRUCTION_LT_I64,
        XS_LIL_INSTRUCTION_LE_I64,
        XS_LIL_INSTRUCTION_GT_I64,
        XS_LIL_INSTRUCTION_GE_I64,
        XS_LIL_INSTRUCTION_ADD_I32,
        XS_LIL_INSTRUCTION_SUB_I32,
        XS_LIL_INSTRUCTION_MUL_I32,
        XS_LIL_INSTRUCTION_DIV_I32,
        XS_LIL_INSTRUCTION_REM_I32,
        XS_LIL_INSTRUCTION_AND_I32,
        XS_LIL_INSTRUCTION_OR_I32,
        XS_LIL_INSTRUCTION_XOR_I32,
        XS_LIL_INSTRUCTION_SHL_I32,
        XS_LIL_INSTRUCTION_SHR_I32,
        XS_LIL_INSTRUCTION_EQ_I32,
        XS_LIL_INSTRUCTION_NE_I32,
        XS_LIL_INSTRUCTION_LT_I32,
        XS_LIL_INSTRUCTION_LE_I32,
        XS_LIL_INSTRUCTION_GT_I32,
        XS_LIL_INSTRUCTION_GE_I32,
        XS_LIL_INSTRUCTION_NOT_BOOL,
        XS_LIL_INSTRUCTION_CALL,
        XS_LIL_INSTRUCTION_LOAD,
        XS_LIL_INSTRUCTION_STORE,
        XS_LIL_INSTRUCTION_CONST_F32,
        XS_LIL_INSTRUCTION_CONST_F64,
        XS_LIL_INSTRUCTION_ADD_F32,
        XS_LIL_INSTRUCTION_SUB_F32,
        XS_LIL_INSTRUCTION_MUL_F32,
        XS_LIL_INSTRUCTION_DIV_F32,
        XS_LIL_INSTRUCTION_REM_F32,
        XS_LIL_INSTRUCTION_EQ_F32,
        XS_LIL_INSTRUCTION_NE_F32,
        XS_LIL_INSTRUCTION_LT_F32,
        XS_LIL_INSTRUCTION_LE_F32,
        XS_LIL_INSTRUCTION_GT_F32,
        XS_LIL_INSTRUCTION_GE_F32,
        XS_LIL_INSTRUCTION_ADD_F64,
        XS_LIL_INSTRUCTION_SUB_F64,
        XS_LIL_INSTRUCTION_MUL_F64,
        XS_LIL_INSTRUCTION_DIV_F64,
        XS_LIL_INSTRUCTION_REM_F64,
        XS_LIL_INSTRUCTION_EQ_F64,
        XS_LIL_INSTRUCTION_NE_F64,
        XS_LIL_INSTRUCTION_LT_F64,
        XS_LIL_INSTRUCTION_LE_F64,
        XS_LIL_INSTRUCTION_GT_F64,
        XS_LIL_INSTRUCTION_GE_F64,
        XS_LIL_INSTRUCTION_CONST_STR,
        XS_LIL_INSTRUCTION_EQ_STR,
        XS_LIL_INSTRUCTION_NE_STR,
        XS_LIL_INSTRUCTION_CONST_U16,
        XS_LIL_INSTRUCTION_CONST_U8,
        XS_LIL_INSTRUCTION_CONST_I8,
        XS_LIL_INSTRUCTION_CONST_I16,
        XS_LIL_INSTRUCTION_CONST_U32,
        XS_LIL_INSTRUCTION_CONST_U64,
        XS_LIL_INSTRUCTION_CONST_U128,
        XS_LIL_INSTRUCTION_CONST_I128,
        XS_LIL_INSTRUCTION_BINARY_INTEGER,
        XS_LIL_INSTRUCTION_AGGREGATE,
        XS_LIL_INSTRUCTION_EXTRACT,
        XS_LIL_INSTRUCTION_ARRAY_GET,
        XS_LIL_INSTRUCTION_ARRAY_SET,
        XS_LIL_INSTRUCTION_ARRAY_LENGTH,
    } XsLilInstructionKind;

    typedef enum
    {
        XS_LIL_INTEGER_ADD,
        XS_LIL_INTEGER_SUB,
        XS_LIL_INTEGER_MUL,
        XS_LIL_INTEGER_DIV,
        XS_LIL_INTEGER_REM,
        XS_LIL_INTEGER_BIT_AND,
        XS_LIL_INTEGER_BIT_OR,
        XS_LIL_INTEGER_BIT_XOR,
        XS_LIL_INTEGER_SHIFT_LEFT,
        XS_LIL_INTEGER_SHIFT_RIGHT,
        XS_LIL_INTEGER_EQUAL,
        XS_LIL_INTEGER_NOT_EQUAL,
        XS_LIL_INTEGER_LESS,
        XS_LIL_INTEGER_LESS_EQUAL,
        XS_LIL_INTEGER_GREATER,
        XS_LIL_INTEGER_GREATER_EQUAL,
    } XsLilIntegerBinaryOperation;

    typedef enum
    {
        XS_LIL_FLOAT_ADD,
        XS_LIL_FLOAT_SUB,
        XS_LIL_FLOAT_MUL,
        XS_LIL_FLOAT_DIV,
        XS_LIL_FLOAT_REM,
    } XsLilFloatBinaryOperation;

    typedef enum
    {
        XS_LIL_FLOAT_EQ,
        XS_LIL_FLOAT_NE,
        XS_LIL_FLOAT_LT,
        XS_LIL_FLOAT_LE,
        XS_LIL_FLOAT_GT,
        XS_LIL_FLOAT_GE,
    } XsLilFloatComparisonOperation;

    typedef enum
    {
        XS_LIL_STR_EQ,
        XS_LIL_STR_NE,
    } XsLilStrComparisonOperation;

    typedef enum
    {
        XS_LIL_TERMINATOR_NONE,
        XS_LIL_TERMINATOR_RETURN,
        XS_LIL_TERMINATOR_BRANCH,
        XS_LIL_TERMINATOR_BRANCH_IF,
        XS_LIL_TERMINATOR_PANIC,
    } XsLilTerminatorKind;

    typedef struct XsLilBlock XsLilBlock;
    typedef struct XsLilFunction XsLilFunction;
    typedef struct XsLilModule XsLilModule;

    XS_LIL_API const char *
    xs_lil_status_name(XsLilStatus status);
    XS_LIL_API XsLilError *
    xs_lil_error_create(void);
    XS_LIL_API void
    xs_lil_error_destroy(XsLilError *error);
    XS_LIL_API void
    xs_lil_error_reset(XsLilError *error);
    XS_LIL_API XsLilStatus
    xs_lil_error_status(const XsLilError *error);
    XS_LIL_API const char *
    xs_lil_error_message(const XsLilError *error);
    XS_LIL_API XsLilType
    xs_lil_scalar_type(XsLilTypeKind kind);
    XS_LIL_API bool
    xs_lil_type_is_scalar(XsLilType type);
    XS_LIL_API XsLilTypeKind
    xs_lil_type_kind(XsLilType type);
    XS_LIL_API uint32_t
    xs_lil_type_registry_id(XsLilType type);

#ifdef __cplusplus
}
#endif

#endif
