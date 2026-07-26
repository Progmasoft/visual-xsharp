/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <xs-lang.chess031@slmails.com>
 * SPDX-License-Identifier: MPL-2.0
 */

package org.xsslang.xlil.tests;

import static org.junit.jupiter.api.Assertions.assertEquals;
import static org.junit.jupiter.api.Assertions.assertNull;
import static org.junit.jupiter.api.Assertions.assertThrows;

import java.lang.foreign.Arena;
import java.lang.foreign.MemorySegment;
import java.nio.charset.StandardCharsets;
import org.junit.jupiter.api.Test;
import org.xsslang.ffi.c.CString;

final class CStringTest {
  @Test
  void roundTripsUtf8Text() {
    try (Arena arena = Arena.ofConfined()) {
      assertEquals("XLIL · Java", CString.read(CString.allocate(arena, "XLIL · Java")));
    }
  }

  @Test
  void rejectsEmbeddedNullAndReadsNullPointer() {
    try (Arena arena = Arena.ofConfined()) {
      assertThrows(
          IllegalArgumentException.class, () -> CString.allocate(arena, "not\0a C string"));
      assertThrows(
          IllegalArgumentException.class,
          () -> CString.read(CString.allocate(arena, "text"), StandardCharsets.UTF_8, 0));
    }
    assertNull(CString.read(MemorySegment.NULL));
  }
}
