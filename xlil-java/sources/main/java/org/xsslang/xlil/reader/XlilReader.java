/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 */

package org.xsslang.xlil.reader;

import static org.xsslang.xlil.internal.LilNative.ADDRESS;
import static org.xsslang.xlil.internal.LilNative.C_BOOL;
import static org.xsslang.xlil.internal.LilNative.C_INT;
import static org.xsslang.xlil.internal.LilNative.C_INT64;
import static org.xsslang.xlil.internal.LilNative.C_SIZE;
import static org.xsslang.xlil.internal.LilNative.C_UINT16;
import static org.xsslang.xlil.internal.LilNative.TYPE_LAYOUT;

import java.lang.foreign.Arena;
import java.lang.foreign.FunctionDescriptor;
import java.lang.foreign.MemorySegment;
import java.nio.charset.StandardCharsets;
import java.util.ArrayList;
import java.util.List;
import java.util.Objects;
import java.util.Optional;
import java.util.OptionalInt;
import org.xsslang.ffi.c.CString;
import org.xsslang.xlil.internal.LilNative;
import org.xsslang.xlil.writer.XlilType;

/** XLIL v0 reader backed by the official native parser and read-only C ABI. */
public final class XlilReader {
  private static final int INVALID_ID = -1;

  private XlilReader() {}

  public static XlilModule parse(String path, String source) {
    Objects.requireNonNull(path, "path");
    Objects.requireNonNull(source, "source");
    try (Arena arena = Arena.ofConfined()) {
      MemorySegment error = LilNative.errorCreate();
      if (error.address() == 0) {
        throw new OutOfMemoryError("could not allocate XLIL error state");
      }
      MemorySegment module = MemorySegment.NULL;
      try {
        byte[] bytes = source.getBytes(StandardCharsets.UTF_8);
        MemorySegment nativeSource = arena.allocate(bytes.length + 1L);
        if (bytes.length != 0) {
          nativeSource.asSlice(0, bytes.length).copyFrom(MemorySegment.ofArray(bytes));
        }
        nativeSource.set(java.lang.foreign.ValueLayout.JAVA_BYTE, bytes.length, (byte) 0);
        MemorySegment output = arena.allocate(ADDRESS);
        int status =
            LilNative.callInt(
                "xs_lil_module_parse_text",
                FunctionDescriptor.of(
                    C_INT, ADDRESS, ADDRESS, C_SIZE, ADDRESS, ADDRESS),
                CString.allocate(arena, path),
                nativeSource,
                (long) bytes.length,
                output,
                error);
        check(status, error);
        module = output.get(ADDRESS, 0);
        status =
            LilNative.callInt(
                "xs_lil_module_verify",
                FunctionDescriptor.of(C_INT, ADDRESS, ADDRESS),
                module,
                error);
        check(status, error);
        return snapshot(arena, module, error);
      } finally {
        if (module.address() != 0) {
          LilNative.callVoid(
              "xs_lil_module_destroy", FunctionDescriptor.ofVoid(ADDRESS), module);
        }
        LilNative.errorDestroy(error);
      }
    }
  }

  private static XlilModule snapshot(
      Arena arena, MemorySegment module, MemorySegment error) {
    int version =
        LilNative.callInt(
            "xs_lil_module_text_version", FunctionDescriptor.of(C_INT, ADDRESS), module);
    String name =
        CString.read(
            LilNative.callAddress(
                "xs_lil_module_name", FunctionDescriptor.of(ADDRESS, ADDRESS), module));
    long count =
        LilNative.callLong(
            "xs_lil_module_function_count",
            FunctionDescriptor.of(C_SIZE, ADDRESS),
            module);
    List<XlilFunction> functions = new ArrayList<>(checkedSize(count));
    for (long index = 0; index < count; ++index) {
      MemorySegment function =
          LilNative.callAddress(
              "xs_lil_module_function_at",
              FunctionDescriptor.of(ADDRESS, ADDRESS, C_SIZE),
              module,
              index);
      functions.add(readFunction(arena, function));
    }
    return new XlilModule(version, Objects.requireNonNull(name), functions, emit(module, error));
  }

  private static XlilFunction readFunction(Arena arena, MemorySegment function) {
    String name =
        CString.read(
            LilNative.callAddress(
                "xs_lil_function_name",
                FunctionDescriptor.of(ADDRESS, ADDRESS),
                function));
    XlilType returnType = callType(arena, "xs_lil_function_return_type", function);
    long parameterCount =
        callSize("xs_lil_function_parameter_count", function);
    List<XlilType> parameterTypes = new ArrayList<>(checkedSize(parameterCount));
    List<Integer> parameterValues = new ArrayList<>(checkedSize(parameterCount));
    for (long index = 0; index < parameterCount; ++index) {
      parameterTypes.add(callType(arena, "xs_lil_function_parameter_type", function, index));
      parameterValues.add(callIndexedInt("xs_lil_function_parameter_value", function, index));
    }
    long valueCount = callSize("xs_lil_function_value_count", function);
    List<XlilType> valueTypes = new ArrayList<>(checkedSize(valueCount));
    for (int value = 0; value < valueCount; ++value) {
      valueTypes.add(callType(arena, "xs_lil_function_value_type", function, value));
    }
    long slotCount = callSize("xs_lil_function_slot_count", function);
    List<XlilType> slots = new ArrayList<>(checkedSize(slotCount));
    for (int slot = 0; slot < slotCount; ++slot) {
      slots.add(callType(arena, "xs_lil_function_slot_type", function, slot));
    }
    long blockCount = callSize("xs_lil_function_block_count", function);
    List<XlilBlock> blocks = new ArrayList<>(checkedSize(blockCount));
    for (long index = 0; index < blockCount; ++index) {
      MemorySegment block =
          LilNative.callAddress(
              "xs_lil_function_block_at",
              FunctionDescriptor.of(ADDRESS, ADDRESS, C_SIZE),
              function,
              index);
      blocks.add(readBlock(arena, block));
    }
    boolean definition =
        LilNative.callBoolean(
            "xs_lil_function_is_definition",
            FunctionDescriptor.of(C_BOOL, ADDRESS),
            function);
    return new XlilFunction(
        Objects.requireNonNull(name),
        returnType,
        parameterTypes,
        parameterValues,
        valueTypes,
        slots,
        definition,
        blocks);
  }

  private static XlilBlock readBlock(Arena arena, MemorySegment block) {
    int id = callBlockInt("xs_lil_block_id", block);
    String label =
        CString.read(
            LilNative.callAddress(
                "xs_lil_block_label", FunctionDescriptor.of(ADDRESS, ADDRESS), block));
    long count = callSize("xs_lil_block_instruction_count", block);
    List<XlilInstruction> instructions = new ArrayList<>(checkedSize(count));
    for (long index = 0; index < count; ++index) {
      instructions.add(readInstruction(arena, block, index));
    }
    return new XlilBlock(id, Objects.requireNonNull(label), instructions, readTerminator(block));
  }

  private static XlilInstruction readInstruction(
      Arena arena, MemorySegment block, long index) {
    int nativeKind = callIndexedInt("xs_lil_block_instruction_kind", block, index);
    boolean hasResult =
        LilNative.callBoolean(
            "xs_lil_block_instruction_has_result",
            FunctionDescriptor.of(C_BOOL, ADDRESS, C_SIZE),
            block,
            index);
    OptionalInt result =
        hasResult
            ? OptionalInt.of(callIndexedInt("xs_lil_block_instruction_result", block, index))
            : OptionalInt.empty();
    Optional<XlilType> resultType =
        hasResult
            ? Optional.of(
                callType(arena, "xs_lil_block_instruction_result_type", block, index))
            : Optional.empty();
    long argumentCount =
        LilNative.callLong(
            "xs_lil_block_instruction_argument_count",
            FunctionDescriptor.of(C_SIZE, ADDRESS, C_SIZE),
            block,
            index);
    List<Integer> arguments = new ArrayList<>(checkedSize(argumentCount));
    for (long argument = 0; argument < argumentCount; ++argument) {
      arguments.add(
          LilNative.callInt(
              "xs_lil_block_instruction_argument",
              FunctionDescriptor.of(C_INT, ADDRESS, C_SIZE, C_SIZE),
              block,
              index,
              argument));
    }
    InstructionKind kind = InstructionKind.fromNative(nativeKind);
    Optional<String> stringValue =
        kind == InstructionKind.CONST_STR
            ? Optional.of(readString(block, index))
            : Optional.empty();
    return new XlilInstruction(
        kind,
        result,
        resultType,
        LilNative.callLong(
            "xs_lil_block_instruction_i64",
            FunctionDescriptor.of(C_INT64, ADDRESS, C_SIZE),
            block,
            index),
        LilNative.callBoolean(
            "xs_lil_block_instruction_bool",
            FunctionDescriptor.of(C_BOOL, ADDRESS, C_SIZE),
            block,
            index),
        stringValue,
        Optional.ofNullable(
            CString.read(
                LilNative.callAddress(
                    "xs_lil_block_instruction_callee",
                    FunctionDescriptor.of(ADDRESS, ADDRESS, C_SIZE),
                    block,
                    index))),
        arguments,
        callIndexedInt("xs_lil_block_instruction_left", block, index),
        callIndexedInt("xs_lil_block_instruction_right", block, index),
        callIndexedInt("xs_lil_block_instruction_field", block, index),
        callIndexedInt("xs_lil_block_instruction_slot", block, index));
  }

  private static String readString(MemorySegment block, long instruction) {
    long length =
        LilNative.callLong(
            "xs_lil_block_instruction_utf16_length",
            FunctionDescriptor.of(C_SIZE, ADDRESS, C_SIZE),
            block,
            instruction);
    char[] units = new char[checkedSize(length)];
    for (long index = 0; index < length; ++index) {
      int unit =
          Short.toUnsignedInt(
              LilNative.callShort(
              "xs_lil_block_instruction_utf16_unit",
              FunctionDescriptor.of(C_UINT16, ADDRESS, C_SIZE, C_SIZE),
              block,
              instruction,
              index));
      units[(int) index] = (char) unit;
    }
    return new String(units);
  }

  private static XlilTerminator readTerminator(MemorySegment block) {
    TerminatorKind kind =
        TerminatorKind.fromNative(callBlockInt("xs_lil_block_terminator_kind", block));
    boolean hasValue =
        LilNative.callBoolean(
            "xs_lil_block_terminator_has_value",
            FunctionDescriptor.of(C_BOOL, ADDRESS),
            block);
    return new XlilTerminator(
        kind,
        hasValue
            ? OptionalInt.of(callBlockInt("xs_lil_block_terminator_value", block))
            : OptionalInt.empty(),
        optionalId(kind == TerminatorKind.BRANCH,
            callBlockInt("xs_lil_block_terminator_target", block)),
        optionalId(kind == TerminatorKind.BRANCH_IF,
            callBlockInt("xs_lil_block_terminator_condition", block)),
        optionalId(kind == TerminatorKind.BRANCH_IF,
            callBlockInt("xs_lil_block_terminator_then_block", block)),
        optionalId(kind == TerminatorKind.BRANCH_IF,
            callBlockInt("xs_lil_block_terminator_else_block", block)));
  }

  private static String emit(MemorySegment module, MemorySegment error) {
    MemorySegment text =
        LilNative.callAddress("xs_lil_text_create", FunctionDescriptor.of(ADDRESS));
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
      check(status, error);
      MemorySegment data =
          LilNative.callAddress(
              "xs_lil_text_data", FunctionDescriptor.of(ADDRESS, ADDRESS), text);
      long length =
          LilNative.callLong(
              "xs_lil_text_length", FunctionDescriptor.of(C_SIZE, ADDRESS), text);
      return data.reinterpret(Math.addExact(length, 1))
          .getString(0, StandardCharsets.UTF_8);
    } finally {
      LilNative.callVoid("xs_lil_text_delete", FunctionDescriptor.ofVoid(ADDRESS), text);
    }
  }

  private static XlilType callType(
      Arena arena, String name, MemorySegment owner, Object... suffix) {
    Object[] arguments = new Object[suffix.length + 1];
    arguments[0] = owner;
    System.arraycopy(suffix, 0, arguments, 1, suffix.length);
    FunctionDescriptor descriptor =
        suffix.length == 0
            ? FunctionDescriptor.of(TYPE_LAYOUT, ADDRESS)
            : FunctionDescriptor.of(TYPE_LAYOUT, ADDRESS, suffix[0] instanceof Long ? C_SIZE : C_INT);
    MemorySegment nativeType = LilNative.callType(arena, name, descriptor, arguments);
    int kind = LilNative.typeKind(nativeType);
    XlilType.Kind[] kinds = XlilType.Kind.values();
    if (kind < 0 || kind >= kinds.length) {
      throw new IllegalStateException("unknown native XLIL type kind: " + kind);
    }
    return new XlilType(kinds[kind], LilNative.typeRegistryId(nativeType));
  }

  private static long callSize(String name, MemorySegment owner) {
    return LilNative.callLong(name, FunctionDescriptor.of(C_SIZE, ADDRESS), owner);
  }

  private static int callIndexedInt(String name, MemorySegment owner, long index) {
    return LilNative.callInt(
        name, FunctionDescriptor.of(C_INT, ADDRESS, C_SIZE), owner, index);
  }

  private static int callBlockInt(String name, MemorySegment block) {
    return LilNative.callInt(name, FunctionDescriptor.of(C_INT, ADDRESS), block);
  }

  private static OptionalInt optionalId(boolean present, int value) {
    return present && value != INVALID_ID ? OptionalInt.of(value) : OptionalInt.empty();
  }

  private static int checkedSize(long size) {
    if (size < 0 || size > Integer.MAX_VALUE) {
      throw new IllegalStateException("native XLIL collection is too large");
    }
    return (int) size;
  }

  private static void check(int status, MemorySegment error) {
    try {
      LilNative.check(status, error);
    } catch (LilNative.NativeFailure failure) {
      throw new XlilReadException(failure.status(), failure.getMessage());
    }
  }
}
