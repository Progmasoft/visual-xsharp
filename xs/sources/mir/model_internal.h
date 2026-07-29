/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 */

#ifndef XS_MIR_MODEL_INTERNAL_H
#define XS_MIR_MODEL_INTERNAL_H

#include "xs/mir.h"

typedef struct
{
  XsMirTerminatorKind kind;
  bool has_value;
  XsMirValueId value;
  XsMirBlockId target;
  XsMirBlockId else_target;
} XsMirTerminator;

typedef struct
{
  XsMirPlaceProjectionKind kind;
  uint32_t payload;
} XsMirPlaceProjection;

typedef struct
{
  XsMirType type;
} XsMirValue;

typedef struct
{
  XsMirInstructionKind kind;
  XsMirValueId result;
  XsMirValueId operand_left;
  XsMirValueId operand_right;
  XsMirPlaceId place;
  int64_t immediate_i64;
  char *callee;
  XsMirValueId *arguments;
  size_t argument_count;
} XsMirInstruction;

typedef struct
{
  char *name;
  XsMirType type;
  XsMirLocalKind kind;
  bool is_mutable;
} XsMirLocal;

struct XsMirPlace
{
  XsMirPlaceId id;
  XsMirLocalId root_local;
  XsMirPlaceProjection *projections;
  size_t projection_count;
  size_t projection_capacity;
};

struct XsMirBlock
{
  char *label;
  XsMirFunction *owner;
  XsMirBlockId id;
  XsMirInstruction *instructions;
  size_t instruction_count;
  size_t instruction_capacity;
  XsMirTerminator terminator;
};

struct XsMirFunction
{
  char *qualified_name;
  XsMirType return_type;
  XsMirType *parameters;
  size_t parameter_count;
  bool is_definition;
  XsMirLocal *locals;
  size_t local_count;
  size_t local_capacity;
  XsMirPlace **places;
  size_t place_count;
  size_t place_capacity;
  XsMirValue *values;
  size_t value_count;
  size_t value_capacity;
  XsMirBlock **blocks;
  size_t block_count;
  size_t block_capacity;
};

struct XsMirModule
{
  char *name;
  XsMirFunction *functions;
  size_t function_count;
  size_t function_capacity;
};

void xs_mir_clear_error(XsMirError *error);
XsMirStatus xs_mir_set_error(XsMirError *error, XsMirStatus status, const char *message);
char *xs_mir_copy_text(const char *text);
void xs_mir_block_free(XsMirBlock *block);
XsMirStatus xs_mir_function_add_value(XsMirFunction *function, XsMirType type, XsMirValueId *value, XsMirError *error);

#endif
