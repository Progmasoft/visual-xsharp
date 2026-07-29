/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 */

package org.xsslang.xlil.reader;

/** Failure reported by the native XLIL parser or verifier. */
public final class XlilReadException extends RuntimeException {
  private static final long serialVersionUID = 1L;
  private final int status;

  XlilReadException(int status, String message) {
    super(message);
    this.status = status;
  }

  public int status() {
    return status;
  }
}
