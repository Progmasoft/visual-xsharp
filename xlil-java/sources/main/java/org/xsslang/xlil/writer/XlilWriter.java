/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 */

package org.xsslang.xlil.writer;

import static org.xsslang.xlil.internal.LilNative.ADDRESS;
import static org.xsslang.xlil.internal.LilNative.C_INT;
import static org.xsslang.xlil.internal.LilNative.C_INT64;
import static org.xsslang.xlil.internal.LilNative.C_SIZE;
import static org.xsslang.xlil.internal.LilNative.TYPE_LAYOUT;

import java.lang.foreign.Arena;
import java.lang.foreign.FunctionDescriptor;
import java.lang.foreign.MemorySegment;
import java.nio.charset.StandardCharsets;
import java.util.List;
import java.util.Objects;
import org.xsslang.ffi.c.CString;
import org.xsslang.xlil.internal.LilNative;

/** Mutable XLIL v0 module writer backed by the official lil-c ABI. */
public final class XlilWriter implements AutoCloseable {
  private final Arena arena;
  private final MemorySegment error;
  private final MemorySegment module;
  private final MemorySegment builder;
  private boolean closed;

  private XlilWriter(String name) {
    arena = Arena.ofConfined();
    error = LilNative.errorCreate();
    if (error.address() == 0) {
      arena.close();
      throw new OutOfMemoryError("could not allocate XLIL error state");
    }
    MemorySegment output = arena.allocate(ADDRESS);
    int status =
        LilNative.callInt(
            "xs_lil_module_create",
            FunctionDescriptor.of(C_INT, ADDRESS, ADDRESS, ADDRESS),
            CString.allocate(arena, name),
            output,
            error);
    if (status != LilNative.OK) {
      String message = LilNative.errorMessage(error);
      LilNative.errorDestroy(error);
      arena.close();
      throw new XlilWriteException(status, message);
    }
    module = output.get(ADDRESS, 0);

    output.set(ADDRESS, 0, MemorySegment.NULL);
    status =
        LilNative.callInt(
            "xs_lil_builder_create",
            FunctionDescriptor.of(C_INT, ADDRESS, ADDRESS, ADDRESS),
            module,
            output,
            error);
    if (status != LilNative.OK) {
      String message = LilNative.errorMessage(error);
      LilNative.callVoid(
          "xs_lil_module_destroy", FunctionDescriptor.ofVoid(ADDRESS), module);
      LilNative.errorDestroy(error);
      arena.close();
      throw new XlilWriteException(status, message);
    }
    builder = output.get(ADDRESS, 0);
  }

  public static XlilWriter create(String name) {
    if (Objects.requireNonNull(name, "name").isBlank()) {
      throw new IllegalArgumentException("XLIL module name must not be blank");
    }
    return new XlilWriter(name);
  }

  public XlilType aggregateType(String name, List<XlilType> fields) {
    ensureOpen();
    Objects.requireNonNull(fields, "fields");
    MemorySegment fieldTypes = typeArray(fields);
    MemorySegment output = arena.allocate(TYPE_LAYOUT);
    int status =
        LilNative.callInt(
            "xs_lil_module_add_aggregate_type",
            FunctionDescriptor.of(
                C_INT, ADDRESS, ADDRESS, ADDRESS, C_SIZE, ADDRESS, ADDRESS),
            module,
            CString.allocate(arena, name),
            fieldTypes,
            (long) fields.size(),
            output,
            error);
    check(status);
    return readType(output);
  }

  public XlilType fixedArrayType(XlilType elementType, long length) {
    if (length < 0) {
      throw new IllegalArgumentException("array length must not be negative");
    }
    ensureOpen();
    MemorySegment output = arena.allocate(TYPE_LAYOUT);
    int status =
        LilNative.callInt(
            "xs_lil_module_add_array_type",
            FunctionDescriptor.of(
                C_INT, ADDRESS, TYPE_LAYOUT, C_INT64, ADDRESS, ADDRESS),
            module,
            nativeType(elementType),
            length,
            output,
            error);
    check(status);
    return readType(output);
  }

  public XlilType dynamicArrayType(XlilType elementType) {
    ensureOpen();
    MemorySegment output = arena.allocate(TYPE_LAYOUT);
    int status =
        LilNative.callInt(
            "xs_lil_module_add_dynamic_array_type",
            FunctionDescriptor.of(C_INT, ADDRESS, TYPE_LAYOUT, ADDRESS, ADDRESS),
            module,
            nativeType(elementType),
            output,
            error);
    check(status);
    return readType(output);
  }

  public void declareFunction(
      String name, XlilType returnType, List<XlilType> parameters) {
    ensureOpen();
    Objects.requireNonNull(parameters, "parameters");
    int status =
        LilNative.callInt(
            "xs_lil_module_add_function",
            FunctionDescriptor.of(
                C_INT, ADDRESS, ADDRESS, TYPE_LAYOUT, ADDRESS, C_SIZE, ADDRESS),
            module,
            CString.allocate(arena, name),
            nativeType(returnType),
            typeArray(parameters),
            (long) parameters.size(),
            error);
    check(status);
  }

  public XlilFunctionWriter defineFunction(
      String name, XlilType returnType, List<XlilType> parameters) {
    ensureOpen();
    Objects.requireNonNull(parameters, "parameters");
    MemorySegment output = arena.allocate(ADDRESS);
    int status =
        LilNative.callInt(
            "xs_lil_module_add_function_definition",
            FunctionDescriptor.of(
                C_INT, ADDRESS, ADDRESS, TYPE_LAYOUT, ADDRESS, C_SIZE, ADDRESS, ADDRESS),
            module,
            CString.allocate(arena, name),
            nativeType(returnType),
            typeArray(parameters),
            (long) parameters.size(),
            output,
            error);
    check(status);
    return new XlilFunctionWriter(this, output.get(ADDRESS, 0), parameters.size());
  }

  public void verify() {
    ensureOpen();
    int status =
        LilNative.callInt(
            "xs_lil_module_verify",
            FunctionDescriptor.of(C_INT, ADDRESS, ADDRESS),
            module,
            error);
    check(status);
  }

  public String emit() {
    ensureOpen();
    MemorySegment text = LilNative.callAddress("xs_lil_text_create", FunctionDescriptor.of(ADDRESS));
    if (text.address() == 0) {
      throw new OutOfMemoryError("could not allocate XLIL text output");
    }
    try {
      int status =
          LilNative.callInt(
              "xs_lil_module_emit_text",
              FunctionDescriptor.of(C_INT, ADDRESS, ADDRESS, ADDRESS),
              module,
              text,
              error);
      check(status);
      MemorySegment data =
          LilNative.callAddress(
              "xs_lil_text_data", FunctionDescriptor.of(ADDRESS, ADDRESS), text);
      long length =
          LilNative.callLong(
              "xs_lil_text_length", FunctionDescriptor.of(C_SIZE, ADDRESS), text);
      if (length > Integer.MAX_VALUE) {
        throw new IllegalStateException("XLIL text is too large for a Java String");
      }
      return data.reinterpret(Math.addExact(length, 1)).getString(0, StandardCharsets.UTF_8);
    } finally {
      LilNative.callVoid(
          "xs_lil_text_delete", FunctionDescriptor.ofVoid(ADDRESS), text);
    }
  }

  @Override
  public void close() {
    if (closed) {
      return;
    }
    closed = true;
    LilNative.callVoid(
        "xs_lil_builder_destroy", FunctionDescriptor.ofVoid(ADDRESS), builder);
    LilNative.callVoid(
        "xs_lil_module_destroy", FunctionDescriptor.ofVoid(ADDRESS), module);
    LilNative.errorDestroy(error);
    arena.close();
  }

  Arena arena() {
    ensureOpen();
    return arena;
  }

  MemorySegment builder() {
    ensureOpen();
    return builder;
  }

  MemorySegment error() {
    ensureOpen();
    return error;
  }

  MemorySegment nativeType(XlilType type) {
    Objects.requireNonNull(type, "type");
    return LilNative.type(arena, type.kind().ordinal(), type.registryId());
  }

  void check(int status) {
    try {
      LilNative.check(status, error);
    } catch (LilNative.NativeFailure failure) {
      throw new XlilWriteException(failure.status(), failure.getMessage());
    }
  }

  void position(MemorySegment block) {
    int status =
        LilNative.callInt(
            "xs_lil_builder_position_at_end",
            FunctionDescriptor.of(C_INT, ADDRESS, ADDRESS, ADDRESS),
            builder,
            block,
            error);
    check(status);
  }

  private MemorySegment typeArray(List<XlilType> types) {
    if (types.isEmpty()) {
      return MemorySegment.NULL;
    }
    MemorySegment array = arena.allocate(TYPE_LAYOUT, types.size());
    for (int index = 0; index < types.size(); ++index) {
      MemorySegment target = array.asSlice((long) index * TYPE_LAYOUT.byteSize(), TYPE_LAYOUT);
      target.copyFrom(nativeType(types.get(index)));
    }
    return array;
  }

  private static XlilType readType(MemorySegment type) {
    int kind = LilNative.typeKind(type);
    XlilType.Kind[] kinds = XlilType.Kind.values();
    if (kind < 0 || kind >= kinds.length) {
      throw new IllegalStateException("native XLIL API returned an unknown type kind");
    }
    return new XlilType(kinds[kind], LilNative.typeRegistryId(type));
  }

  private void ensureOpen() {
    if (closed) {
      throw new IllegalStateException("XLIL writer is closed");
    }
  }
}
