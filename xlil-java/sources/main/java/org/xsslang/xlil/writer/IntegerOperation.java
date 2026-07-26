/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <xs-lang.chess031@slmails.com>
 * SPDX-License-Identifier: MPL-2.0
 */

package org.xsslang.xlil.writer;

/** Integer operations accepted by the generic XLIL v0 integer builder. */
public enum IntegerOperation {
  ADD,
  SUBTRACT,
  MULTIPLY,
  DIVIDE,
  REMAINDER,
  BIT_AND,
  BIT_OR,
  BIT_XOR,
  SHIFT_LEFT,
  SHIFT_RIGHT,
  EQUAL,
  NOT_EQUAL,
  LESS,
  LESS_EQUAL,
  GREATER,
  GREATER_EQUAL
}
