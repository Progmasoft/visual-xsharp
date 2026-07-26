/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <xs-lang.chess031@slmails.com>
 * SPDX-License-Identifier: MPL-2.0
 */

package org.xsslang.xlil.writer;

/** Failure reported by the native XLIL verifier or construction API. */
public final class XlilWriteException extends RuntimeException {
  private static final long serialVersionUID = 1L;
  private final int status;

  XlilWriteException(int status, String message) {
    super(message);
    this.status = status;
  }

  public int status() {
    return status;
  }
}
