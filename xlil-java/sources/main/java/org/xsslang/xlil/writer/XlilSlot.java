/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <xs-lang.chess031@slmails.com>
 * SPDX-License-Identifier: MPL-2.0
 */

package org.xsslang.xlil.writer;

/** Function-local XLIL stack slot. */
public record XlilSlot(int id) {
  public XlilSlot {
    if (id < 0) {
      throw new IllegalArgumentException("XLIL slot id must not be negative");
    }
  }
}
