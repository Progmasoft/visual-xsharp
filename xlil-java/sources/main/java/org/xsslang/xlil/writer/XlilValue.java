/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <xs-lang.chess031@slmails.com>
 * SPDX-License-Identifier: MPL-2.0
 */

package org.xsslang.xlil.writer;

/** Function-local XLIL register value. */
public record XlilValue(int id) {
  public XlilValue {
    if (id < 0) {
      throw new IllegalArgumentException("XLIL value id must not be negative");
    }
  }
}
