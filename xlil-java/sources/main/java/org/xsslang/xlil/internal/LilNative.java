/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <xs-lang.chess031@slmails.com>
 * SPDX-License-Identifier: MPL-2.0
 */

package org.xsslang.xlil.internal;

import java.lang.foreign.Arena;
import java.lang.foreign.FunctionDescriptor;
import java.lang.foreign.Linker;
import java.lang.foreign.MemoryLayout;
import java.lang.foreign.MemorySegment;
import java.lang.foreign.SymbolLookup;
import java.lang.foreign.ValueLayout;
import java.lang.invoke.MethodHandle;
import java.nio.file.Path;
import java.util.Map;
import java.util.Objects;
import java.util.concurrent.ConcurrentHashMap;
import org.xsslang.ffi.c.CString;

/** Internal Java 25 FFM bridge to the stable lil-c ABI. */
public final class LilNative {
  public static final ValueLayout.OfInt C_INT =
      (ValueLayout.OfInt) Linker.nativeLinker().canonicalLayouts().get("int");
  public static final ValueLayout.OfLong C_INT64 =
      (ValueLayout.OfLong) Linker.nativeLinker().canonicalLayouts().get("int64_t");
  public static final ValueLayout.OfLong C_SIZE =
      (ValueLayout.OfLong) Linker.nativeLinker().canonicalLayouts().get("size_t");
  public static final ValueLayout.OfShort C_UINT16 =
      (ValueLayout.OfShort) Linker.nativeLinker().canonicalLayouts().get("uint16_t");
  public static final ValueLayout.OfBoolean C_BOOL =
      (ValueLayout.OfBoolean) Linker.nativeLinker().canonicalLayouts().get("bool");
  public static final ValueLayout.OfInt C_INT32 =
      (ValueLayout.OfInt) Linker.nativeLinker().canonicalLayouts().get("int32_t");
  public static final java.lang.foreign.AddressLayout ADDRESS = ValueLayout.ADDRESS;
  public static final MemoryLayout TYPE_LAYOUT =
      MemoryLayout.structLayout(C_INT.withName("kind"), C_INT32.withName("registry_id"));
  public static final int API_VERSION = 0x0001_0000;
  public static final int OK = 0;

  private static final Linker LINKER = Linker.nativeLinker();
  private static final SymbolLookup LOOKUP = loadLibrary();
  private static final Map<Signature, MethodHandle> HANDLES = new ConcurrentHashMap<>();

  static {
    int version = callInt("xs_lil_c_api_version", FunctionDescriptor.of(C_INT));
    if (version != API_VERSION) {
      throw new ExceptionInInitializerError(
          "incompatible xs_lil C API: expected 0x"
              + Integer.toHexString(API_VERSION)
              + ", got 0x"
              + Integer.toHexString(version));
    }
  }

  private LilNative() {}

  public static MemorySegment type(Arena arena, int kind, int registryId) {
    MemorySegment type = arena.allocate(TYPE_LAYOUT);
    type.set(C_INT, 0, kind);
    type.set(C_INT32, C_INT.byteSize(), registryId);
    return type;
  }

  public static int typeKind(MemorySegment type) {
    return type.get(C_INT, 0);
  }

  public static int typeRegistryId(MemorySegment type) {
    return type.get(C_INT32, C_INT.byteSize());
  }

  public static MemorySegment errorCreate() {
    return callAddress("xs_lil_error_create", FunctionDescriptor.of(ADDRESS));
  }

  public static void errorDestroy(MemorySegment error) {
    callVoid("xs_lil_error_destroy", FunctionDescriptor.ofVoid(ADDRESS), error);
  }

  public static String errorMessage(MemorySegment error) {
    MemorySegment address =
        callAddress(
            "xs_lil_error_message", FunctionDescriptor.of(ADDRESS, ADDRESS), error);
    String message = CString.read(address);
    return message == null || message.isEmpty() ? "unknown XLIL C API error" : message;
  }

  public static void check(int status, MemorySegment error) {
    if (status != OK) {
      throw new NativeFailure(status, errorMessage(error));
    }
  }

  public static int callInt(String name, FunctionDescriptor descriptor, Object... arguments) {
    return (int) invoke(name, descriptor, arguments);
  }

  public static long callLong(String name, FunctionDescriptor descriptor, Object... arguments) {
    return (long) invoke(name, descriptor, arguments);
  }

  public static short callShort(
      String name, FunctionDescriptor descriptor, Object... arguments) {
    return (short) invoke(name, descriptor, arguments);
  }

  public static boolean callBoolean(
      String name, FunctionDescriptor descriptor, Object... arguments) {
    return (boolean) invoke(name, descriptor, arguments);
  }

  public static MemorySegment callAddress(
      String name, FunctionDescriptor descriptor, Object... arguments) {
    return (MemorySegment) invoke(name, descriptor, arguments);
  }

  public static MemorySegment callType(
      Arena arena, String name, FunctionDescriptor descriptor, Object... arguments) {
    Object[] withAllocator = new Object[arguments.length + 1];
    withAllocator[0] = arena;
    System.arraycopy(arguments, 0, withAllocator, 1, arguments.length);
    return (MemorySegment) invoke(name, descriptor, withAllocator);
  }

  public static void callVoid(String name, FunctionDescriptor descriptor, Object... arguments) {
    invoke(name, descriptor, arguments);
  }

  private static Object invoke(
      String name, FunctionDescriptor descriptor, Object... arguments) {
    try {
      return handle(name, descriptor).invokeWithArguments(arguments);
    } catch (NativeFailure failure) {
      throw failure;
    } catch (Throwable throwable) {
      throw new IllegalStateException("lil-c invocation failed: " + name, throwable);
    }
  }

  private static MethodHandle handle(String name, FunctionDescriptor descriptor) {
    Signature signature = new Signature(name, descriptor);
    return HANDLES.computeIfAbsent(
        signature,
        ignored -> LINKER.downcallHandle(LOOKUP.findOrThrow(name), descriptor));
  }

  private static SymbolLookup loadLibrary() {
    String configured = System.getProperty("org.xsslang.xlil.library");
    if (configured != null && !configured.isBlank()) {
      return SymbolLookup.libraryLookup(Path.of(configured), Arena.global());
    }
    System.loadLibrary("xs_lil");
    return SymbolLookup.loaderLookup();
  }

  private record Signature(String name, FunctionDescriptor descriptor) {
    Signature {
      Objects.requireNonNull(name, "name");
      Objects.requireNonNull(descriptor, "descriptor");
    }
  }

  /** Internal unchecked failure carrying the native status and message. */
  public static final class NativeFailure extends RuntimeException {
    private static final long serialVersionUID = 1L;
    private final int status;

    NativeFailure(int status, String message) {
      super(message);
      this.status = status;
    }

    public int status() {
      return status;
    }
  }
}
