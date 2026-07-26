/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <xs-lang.chess031@slmails.com>
 * SPDX-License-Identifier: MPL-2.0
 */

package org.xsslang.xlil.reader;

import java.util.List;

/** Immutable snapshot of one XLIL basic block. */
public record XlilBlock(
    int id, String label, List<XlilInstruction> instructions, XlilTerminator terminator) {
  public XlilBlock {
    instructions = List.copyOf(instructions);
  }
}
