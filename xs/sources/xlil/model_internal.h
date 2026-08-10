/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 */

#ifndef XS_XLIL_MODEL_INTERNAL_H
#define XS_XLIL_MODEL_INTERNAL_H

#include "Visual/XSharp/lil.hh"

typedef struct
{
    XsLilTerminatorKind kind;
    bool has_value;
    XsLilValueId value;
    XsLilValueId condition;
    XsLilBlockId target;
    XsLilBlockId else_target;
} XsLilTerminator;

typedef struct
{
    XsLilType type;
} XsLilValue;

typedef struct
{
    XsLilType type;
} XsLilSlot;

typedef struct
{
    char *name;
    XsLilType *fields;
    size_t field_count;
} XsLilAggregateType;

typedef struct
{
    XsLilType element_type;
    uint64_t length;
    bool dynamic;
} XsLilArrayType;

typedef struct
{
    XsLilInstructionKind kind;
    XsLilValueId result;
    int64_t immediate_i64;
    XsUInt128 immediate_integer_bits;
    XsLilIntegerBinaryOperation integer_operation;
    XsLilType integer_type;
    uint64_t immediate_float_bits;
    bool immediate_bool;
    XsLilUtf32Encoding utf32_encoding;
    uint32_t *utf32_units;
    size_t utf32_length;
    XsLilValueId left;
    XsLilValueId right;
    XsLilSlotId slot;
    char *callee;
    XsLilValueId *arguments;
    size_t argument_count;
} XsLilInstruction;

struct XsLilBlock
{
    char *label;
    XsLilFunction *owner;
    uint32_t id;
    XsLilInstruction *instructions;
    size_t instruction_count;
    size_t instruction_capacity;
    XsLilTerminator terminator;
};

struct XsLilFunction
{
    XsLilModule *owner;
    char *name;
    XsLilType return_type;
    XsLilType *parameters;
    size_t parameter_count;
    bool is_definition;
    XsLilValue *values;
    size_t value_count;
    size_t value_capacity;
    XsLilSlot *slots;
    size_t slot_count;
    size_t slot_capacity;
    XsLilBlock **blocks;
    size_t block_count;
    size_t block_capacity;
};

struct XsLilModule
{
    char *name;
    XsLilAggregateType *aggregate_types;
    size_t aggregate_type_count;
    size_t aggregate_type_capacity;
    XsLilArrayType *array_types;
    size_t array_type_count;
    size_t array_type_capacity;
    XsLilFunction **functions;
    size_t function_count;
    size_t function_capacity;
};

void xs_lil_clear_error(XsLilError *error);
XsLilStatus xs_lil_set_error(XsLilError *error, XsLilStatus status, const char *message);
char *xs_lil_copy_text(const char *text);
char *xs_lil_copy_span(const char *text, size_t length);
XsLilStatus xs_lil_write_checked(FILE *stream, XsLilError *error, const char *text);
XsLilStatus xs_lil_add_value(XsLilFunction *function, XsLilType type, XsLilValueId *value, XsLilError *error);
XsLilStatus xs_lil_append_instruction(XsLilBlock *block, XsLilInstruction instruction, XsLilError *error);
XsLilStatus xs_lil_parse_const_str(XsLilBlock *block, XsLilType result_type, const char *operation,
                                   size_t operation_length, XsLilValueId expected_result, bool *matched,
                                   XsLilError *error);
XsLilStatus xs_lil_parse_str_comparison(XsLilBlock *block, XsLilType result_type, const char *operation,
                                        size_t operation_length, XsLilValueId expected_result, bool *matched,
                                        XsLilError *error);
XsLilStatus xs_lil_parse_const_u16(XsLilBlock *block, XsLilType result_type, const char *operation,
                                   size_t operation_length, XsLilValueId expected_result, bool *matched,
                                   XsLilError *error);
XsLilStatus xs_lil_parse_const_integer(XsLilBlock *block, XsLilType result_type, const char *operation,
                                       size_t operation_length, XsLilValueId expected_result, bool *matched,
                                       XsLilError *error);
XsLilStatus xs_lil_parse_integer_operation(XsLilBlock *block, XsLilType result_type, const char *operation,
                                           size_t operation_length, XsLilValueId expected_result, bool *matched,
                                           XsLilError *error);
const char *xs_lil_integer_operation_name(XsLilIntegerBinaryOperation operation);
bool xs_lil_integer_operation_is_comparison(XsLilIntegerBinaryOperation operation);
bool xs_lil_parse_type_name(const XsLilModule *module, const char *text, size_t length, XsLilType *type);
bool xs_lil_parse_signature(const XsLilModule *module, const char *line, size_t length, const char *prefix, char **name,
                            XsLilType *return_type, XsLilType **parameters, size_t *parameter_count);
XsLilStatus xs_lil_parse_aggregate_record(XsLilModule *module, const char *text, size_t length, XsLilError *error);
XsLilStatus xs_lil_parse_array_record(XsLilModule *module, const char *text, size_t length, XsLilError *error);
XsLilStatus xs_lil_parse_aggregate_instruction(XsLilBlock *block, XsLilType result_type, const char *operation,
                                               size_t operation_length, XsLilValueId expected_result, bool *matched,
                                               XsLilError *error);
XsLilStatus xs_lil_add_const_integer_bits(XsLilBlock *block, XsLilType type, XsUInt128 bits, XsLilValueId *result,
                                          XsLilError *error);

#endif
