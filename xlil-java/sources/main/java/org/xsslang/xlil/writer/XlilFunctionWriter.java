/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 */

package org.xsslang.xlil.writer;

import static org.xsslang.xlil.internal.LilNative.ADDRESS;
import static org.xsslang.xlil.internal.LilNative.C_INT;
import static org.xsslang.xlil.internal.LilNative.TYPE_LAYOUT;

import java.lang.foreign.FunctionDescriptor;
import java.lang.foreign.MemorySegment;
import org.xsslang.xlil.internal.LilNative;

/** Writer for one defined function in an {@link XlilWriter}. */
public final class XlilFunctionWriter {
  private final XlilWriter owner;
  private final MemorySegment function;
  private final int parameterCount;

  XlilFunctionWriter(XlilWriter owner, MemorySegment function, int parameterCount) {
    this.owner = owner;
    this.function = function;
    this.parameterCount = parameterCount;
  }

  public XlilValue parameter(int index) {
    if (index < 0 || index >= parameterCount) {
      throw new IndexOutOfBoundsException(index);
    }
    return new XlilValue(index);
  }

  public XlilSlot slot(XlilType type) {
    MemorySegment output = owner.arena().allocate(C_INT);
    int status =
        LilNative.callInt(
            "xs_lil_function_add_slot",
            FunctionDescriptor.of(C_INT, ADDRESS, TYPE_LAYOUT, ADDRESS, ADDRESS),
            function,
            owner.nativeType(type),
            output,
            owner.error());
    owner.check(status);
    return new XlilSlot(output.get(C_INT, 0));
  }

  public XlilBlockWriter block(String label) {
    MemorySegment output = owner.arena().allocate(ADDRESS);
    int status =
        LilNative.callInt(
            "xs_lil_builder_append_block",
            FunctionDescriptor.of(
                C_INT, ADDRESS, ADDRESS, ADDRESS, ADDRESS, ADDRESS),
            owner.builder(),
            function,
            org.xsslang.ffi.c.CString.allocate(owner.arena(), label),
            output,
            owner.error());
    owner.check(status);
    return new XlilBlockWriter(owner, output.get(ADDRESS, 0));
  }
}
