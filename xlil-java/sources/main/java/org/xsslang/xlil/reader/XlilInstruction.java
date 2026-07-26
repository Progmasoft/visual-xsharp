/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <xs-lang.chess031@slmails.com>
 * SPDX-License-Identifier: MPL-2.0
 */

package org.xsslang.xlil.reader;

import java.util.List;
import java.util.Optional;
import java.util.OptionalInt;
import org.xsslang.xlil.writer.XlilType;

/** Immutable snapshot of one XLIL instruction. */
public record XlilInstruction(
    InstructionKind kind,
    OptionalInt result,
    Optional<XlilType> resultType,
    long integerBits,
    boolean booleanValue,
    Optional<String> stringValue,
    Optional<String> callee,
    List<Integer> arguments,
    int left,
    int right,
    int field,
    int slot) {

  public XlilInstruction {
    arguments = List.copyOf(arguments);
  }
}
