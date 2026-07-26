/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <xs-lang.chess031@slmails.com>
 * SPDX-License-Identifier: MPL-2.0
 */

package org.xsslang.ffi.c;

import java.lang.foreign.MemorySegment;
import java.lang.foreign.SegmentAllocator;
import java.nio.charset.Charset;
import java.nio.charset.StandardCharsets;
import java.util.Objects;

/** UTF-8, NUL-terminated C string conversion helpers for Java FFM bindings. */
public final class CString {
  private static final long DEFAULT_MAX_BYTES = 1L << 20;

  private CString() {}

  /**
   * Allocates a UTF-8 C string in the supplied arena or allocator.
   *
   * @param allocator owner of the allocated native memory
   * @param value Java text without an embedded NUL
   * @return native NUL-terminated bytes
   */
  public static MemorySegment allocate(SegmentAllocator allocator, String value) {
    return allocate(allocator, value, StandardCharsets.UTF_8);
  }

  /**
   * Allocates a C string with an explicit charset.
   *
   * @param allocator owner of the allocated native memory
   * @param value Java text without an embedded NUL
   * @param charset encoding used before the terminating NUL
   * @return native NUL-terminated bytes
   */
  public static MemorySegment allocate(
      SegmentAllocator allocator, String value, Charset charset) {
    Objects.requireNonNull(allocator, "allocator");
    Objects.requireNonNull(value, "value");
    Objects.requireNonNull(charset, "charset");
    if (value.indexOf('\0') >= 0) {
      throw new IllegalArgumentException("C strings cannot contain an embedded NUL");
    }
    return allocator.allocateFrom(value, charset);
  }

  /**
   * Reads a borrowed UTF-8 C string.
   *
   * @param address native address returned by C
   * @return Java string, or {@code null} for a null pointer
   */
  public static String read(MemorySegment address) {
    return read(address, StandardCharsets.UTF_8);
  }

  /**
   * Reads a borrowed C string with an explicit charset.
   *
   * @param address native address returned by C
   * @param charset byte encoding
   * @return Java string, or {@code null} for a null pointer
   */
  public static String read(MemorySegment address, Charset charset) {
    return read(address, charset, DEFAULT_MAX_BYTES);
  }

  /**
   * Reads a borrowed C string without scanning beyond {@code maxBytes}.
   *
   * @param address native address returned by C
   * @param charset byte encoding
   * @param maxBytes maximum accessible byte count, including the terminating NUL
   * @return Java string, or {@code null} for a null pointer
   */
  public static String read(MemorySegment address, Charset charset, long maxBytes) {
    Objects.requireNonNull(address, "address");
    Objects.requireNonNull(charset, "charset");
    if (maxBytes <= 0) {
      throw new IllegalArgumentException("maxBytes must be positive");
    }
    if (address.address() == 0) {
      return null;
    }
    return address.reinterpret(maxBytes).getString(0, charset);
  }
}
