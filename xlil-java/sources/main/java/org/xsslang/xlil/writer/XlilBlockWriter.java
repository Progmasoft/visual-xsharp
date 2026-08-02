/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 */

package org.xsslang.xlil.writer;

import static org.xsslang.xlil.internal.LilNative.ADDRESS;
import static org.xsslang.xlil.internal.LilNative.C_BOOL;
import static org.xsslang.xlil.internal.LilNative.C_INT;
import static org.xsslang.xlil.internal.LilNative.C_INT64;
import static org.xsslang.xlil.internal.LilNative.C_SIZE;
import static org.xsslang.xlil.internal.LilNative.TYPE_LAYOUT;

import java.lang.foreign.FunctionDescriptor;
import java.lang.foreign.MemorySegment;
import java.util.List;
import java.util.Objects;
import org.xsslang.xlil.internal.LilNative;

/** Insertion-point facade for one XLIL basic block. */
public final class XlilBlockWriter
{
    private final XlilWriter owner;
    private final MemorySegment block;

    XlilBlockWriter(XlilWriter owner, MemorySegment block)
    {
        this.owner = owner;
        this.block = block;
    }

    public int id()
    {
        return LilNative.callInt("xs_lil_block_id", FunctionDescriptor.of(C_INT, ADDRESS), block);
    }

    public XlilValue constI32(int value)
    {
        return valueResult("xs_lil_builder_const_i32", C_INT, value);
    }

    public XlilValue constI64(long value)
    {
        return valueResult("xs_lil_builder_const_i64", C_INT64, value);
    }

    public XlilValue constBool(boolean value)
    {
        return valueResult("xs_lil_builder_const_bool", C_BOOL, value);
    }

    public XlilValue constF32(float value)
    {
        return valueResult("xs_lil_builder_const_f32_bits", C_INT, Float.floatToRawIntBits(value));
    }

    public XlilValue constF64(double value)
    {
        return valueResult("xs_lil_builder_const_f64_bits", C_INT64, Double.doubleToRawLongBits(value));
    }

    public XlilValue constString(String value, Utf16Encoding encoding)
    {
        Objects.requireNonNull(value, "value");
        Objects.requireNonNull(encoding, "encoding");
        position();
        char[] units = value.toCharArray();
        MemorySegment nativeUnits = units.length == 0
                ? MemorySegment.NULL
                : owner.arena().allocateFrom(java.lang.foreign.ValueLayout.JAVA_CHAR, units);
        MemorySegment output = owner.arena().allocate(C_INT);
        int status = LilNative.callInt("xs_lil_builder_const_str",
                FunctionDescriptor.of(C_INT, ADDRESS, C_INT, ADDRESS, C_SIZE, ADDRESS, ADDRESS), owner.builder(),
                encoding.ordinal(), nativeUnits, (long) units.length, output, owner.error());
        owner.check(status);
        return new XlilValue(output.get(C_INT, 0));
    }

    public XlilValue integer(IntegerOperation operation, XlilValue left, XlilValue right)
    {
        Objects.requireNonNull(operation, "operation");
        return binaryResult("xs_lil_builder_binary_integer", operation.ordinal(), left, right);
    }

    public XlilValue floating(FloatOperation operation, XlilValue left, XlilValue right)
    {
        Objects.requireNonNull(operation, "operation");
        return binaryResult("xs_lil_builder_binary_float", operation.ordinal(), left, right);
    }

    public XlilValue compareFloat(FloatComparison operation, XlilValue left, XlilValue right)
    {
        Objects.requireNonNull(operation, "operation");
        return binaryResult("xs_lil_builder_compare_float", operation.ordinal(), left, right);
    }

    public XlilValue compareString(StringComparison operation, XlilValue left, XlilValue right)
    {
        Objects.requireNonNull(operation, "operation");
        return binaryResult("xs_lil_builder_compare_str", operation.ordinal(), left, right);
    }

    public XlilValue not(XlilValue operand)
    {
        Objects.requireNonNull(operand, "operand");
        position();
        MemorySegment output = owner.arena().allocate(C_INT);
        int status = LilNative.callInt("xs_lil_builder_not_bool",
                FunctionDescriptor.of(C_INT, ADDRESS, C_INT, ADDRESS, ADDRESS), owner.builder(), operand.id(), output,
                owner.error());
        owner.check(status);
        return new XlilValue(output.get(C_INT, 0));
    }

    public XlilValue call(String callee, List<XlilValue> arguments)
    {
        return callNative(callee, arguments, false);
    }

    public void callVoid(String callee, List<XlilValue> arguments)
    {
        callNative(callee, arguments, true);
    }

    public XlilValue aggregate(XlilType type, List<XlilValue> fields)
    {
        return composite("xs_lil_builder_aggregate", type, fields);
    }

    public XlilValue array(XlilType type, List<XlilValue> elements)
    {
        return composite("xs_lil_builder_array", type, elements);
    }

    public XlilValue extract(XlilValue aggregate, int field)
    {
        if(field < 0)
        {
            throw new IllegalArgumentException("field must not be negative");
        }
        return twoIdResult("xs_lil_builder_extract", aggregate.id(), field);
    }

    public XlilValue arrayGet(XlilValue array, XlilValue index)
    {
        return twoIdResult("xs_lil_builder_array_get", array.id(), index.id());
    }

    public XlilValue arraySet(XlilValue array, XlilValue index, XlilValue replacement)
    {
        position();
        MemorySegment output = owner.arena().allocate(C_INT);
        int status = LilNative.callInt("xs_lil_builder_array_set",
                FunctionDescriptor.of(C_INT, ADDRESS, C_INT, C_INT, C_INT, ADDRESS, ADDRESS), owner.builder(),
                array.id(), index.id(), replacement.id(), output, owner.error());
        owner.check(status);
        return new XlilValue(output.get(C_INT, 0));
    }

    public XlilValue arrayLength(XlilValue array)
    {
        return oneIdResult("xs_lil_builder_array_length", array.id());
    }

    public XlilValue load(XlilSlot slot)
    {
        return oneIdResult("xs_lil_builder_load", slot.id());
    }

    public void store(XlilSlot slot, XlilValue value)
    {
        position();
        int status = LilNative.callInt("xs_lil_builder_store",
                FunctionDescriptor.of(C_INT, ADDRESS, C_INT, C_INT, ADDRESS), owner.builder(), slot.id(), value.id(),
                owner.error());
        owner.check(status);
    }

    public void returnVoid()
    {
        terminator("xs_lil_builder_return");
    }

    public void returnValue(XlilValue value)
    {
        position();
        int status = LilNative.callInt("xs_lil_builder_return_value",
                FunctionDescriptor.of(C_INT, ADDRESS, C_INT, ADDRESS), owner.builder(), value.id(), owner.error());
        owner.check(status);
    }

    public void branch(XlilBlockWriter target)
    {
        position();
        int status = LilNative.callInt("xs_lil_builder_branch", FunctionDescriptor.of(C_INT, ADDRESS, ADDRESS, ADDRESS),
                owner.builder(), target.block, owner.error());
        owner.check(status);
    }

    public void branchIf(XlilValue condition, XlilBlockWriter thenBlock, XlilBlockWriter elseBlock)
    {
        position();
        int status = LilNative.callInt("xs_lil_builder_branch_if",
                FunctionDescriptor.of(C_INT, ADDRESS, C_INT, ADDRESS, ADDRESS, ADDRESS), owner.builder(),
                condition.id(), thenBlock.block, elseBlock.block, owner.error());
        owner.check(status);
    }

    public void panic()
    {
        terminator("xs_lil_builder_panic");
    }

    private XlilValue valueResult(String function, java.lang.foreign.MemoryLayout valueLayout, Object value)
    {
        position();
        MemorySegment output = owner.arena().allocate(C_INT);
        int status = LilNative.callInt(function, FunctionDescriptor.of(C_INT, ADDRESS, valueLayout, ADDRESS, ADDRESS),
                owner.builder(), value, output, owner.error());
        owner.check(status);
        return new XlilValue(output.get(C_INT, 0));
    }

    private XlilValue binaryResult(String function, int operation, XlilValue left, XlilValue right)
    {
        position();
        MemorySegment output = owner.arena().allocate(C_INT);
        int status = LilNative.callInt(function,
                FunctionDescriptor.of(C_INT, ADDRESS, C_INT, C_INT, C_INT, ADDRESS, ADDRESS), owner.builder(),
                operation, left.id(), right.id(), output, owner.error());
        owner.check(status);
        return new XlilValue(output.get(C_INT, 0));
    }

    private XlilValue callNative(String callee, List<XlilValue> arguments, boolean expectVoid)
    {
        Objects.requireNonNull(callee, "callee");
        Objects.requireNonNull(arguments, "arguments");
        position();
        MemorySegment nativeArguments = values(arguments);
        MemorySegment output = owner.arena().allocate(C_INT);
        int status = LilNative.callInt("xs_lil_builder_call",
                FunctionDescriptor.of(C_INT, ADDRESS, ADDRESS, ADDRESS, C_SIZE, ADDRESS, ADDRESS), owner.builder(),
                org.xsslang.ffi.c.CString.allocate(owner.arena(), callee), nativeArguments, (long) arguments.size(),
                output, owner.error());
        owner.check(status);
        int result = output.get(C_INT, 0);
        if(expectVoid)
        {
            if(result != -1)
            {
                throw new IllegalArgumentException("call target returns a value: " + callee);
            }
            return null;
        }
        if(result == -1)
        {
            throw new IllegalArgumentException("call target returns void: " + callee);
        }
        return new XlilValue(result);
    }

    private XlilValue composite(String function, XlilType type, List<XlilValue> values)
    {
        Objects.requireNonNull(values, "values");
        position();
        MemorySegment output = owner.arena().allocate(C_INT);
        int status = LilNative.callInt(function,
                FunctionDescriptor.of(C_INT, ADDRESS, TYPE_LAYOUT, ADDRESS, C_SIZE, ADDRESS, ADDRESS), owner.builder(),
                owner.nativeType(type), values(values), (long) values.size(), output, owner.error());
        owner.check(status);
        return new XlilValue(output.get(C_INT, 0));
    }

    private XlilValue oneIdResult(String function, int id)
    {
        position();
        MemorySegment output = owner.arena().allocate(C_INT);
        int status = LilNative.callInt(function, FunctionDescriptor.of(C_INT, ADDRESS, C_INT, ADDRESS, ADDRESS),
                owner.builder(), id, output, owner.error());
        owner.check(status);
        return new XlilValue(output.get(C_INT, 0));
    }

    private XlilValue twoIdResult(String function, int first, int second)
    {
        position();
        MemorySegment output = owner.arena().allocate(C_INT);
        int status = LilNative.callInt(function, FunctionDescriptor.of(C_INT, ADDRESS, C_INT, C_INT, ADDRESS, ADDRESS),
                owner.builder(), first, second, output, owner.error());
        owner.check(status);
        return new XlilValue(output.get(C_INT, 0));
    }

    private MemorySegment values(List<XlilValue> values)
    {
        if(values.isEmpty())
        {
            return MemorySegment.NULL;
        }
        MemorySegment result = owner.arena().allocate(C_INT, values.size());
        for(int index = 0; index < values.size(); ++index)
        {
            result.setAtIndex(C_INT, index, values.get(index).id());
        }
        return result;
    }

    private void terminator(String function)
    {
        position();
        int status = LilNative.callInt(function, FunctionDescriptor.of(C_INT, ADDRESS, ADDRESS), owner.builder(),
                owner.error());
        owner.check(status);
    }

    private void position()
    {
        owner.position(block);
    }
}
