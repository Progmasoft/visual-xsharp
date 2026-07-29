/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 */

package org.xsslang.xlil.reader;

/** Stable instruction tags exposed by the XLIL v0 model. */
public enum InstructionKind {
  CONST_I64, CONST_I32, CONST_BOOL,
  ADD_I64, SUB_I64, MUL_I64, DIV_I64, REM_I64,
  AND_I64, OR_I64, XOR_I64, SHL_I64, SHR_I64,
  EQ_I64, NE_I64, LT_I64, LE_I64, GT_I64, GE_I64,
  ADD_I32, SUB_I32, MUL_I32, DIV_I32, REM_I32,
  AND_I32, OR_I32, XOR_I32, SHL_I32, SHR_I32,
  EQ_I32, NE_I32, LT_I32, LE_I32, GT_I32, GE_I32,
  NOT_BOOL, CALL, LOAD, STORE,
  CONST_F32, CONST_F64,
  ADD_F32, SUB_F32, MUL_F32, DIV_F32, REM_F32,
  EQ_F32, NE_F32, LT_F32, LE_F32, GT_F32, GE_F32,
  ADD_F64, SUB_F64, MUL_F64, DIV_F64, REM_F64,
  EQ_F64, NE_F64, LT_F64, LE_F64, GT_F64, GE_F64,
  CONST_STR, EQ_STR, NE_STR,
  CONST_U16, CONST_U8, CONST_I8, CONST_I16, CONST_U32, CONST_U64,
  CONST_U128, CONST_I128, BINARY_INTEGER,
  AGGREGATE, EXTRACT, ARRAY_GET, ARRAY_SET, ARRAY_LENGTH;

  static InstructionKind fromNative(int value) {
    InstructionKind[] kinds = values();
    if (value < 0 || value >= kinds.length) {
      throw new IllegalStateException("unknown native XLIL instruction kind: " + value);
    }
    return kinds[value];
  }
}
