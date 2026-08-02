/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 */

package org.xsslang.xlil.tests;

import static org.junit.jupiter.api.Assertions.assertEquals;
import static org.junit.jupiter.api.Assertions.assertThrows;
import static org.junit.jupiter.api.Assertions.assertTrue;

import java.util.List;
import org.junit.jupiter.api.Test;
import org.xsslang.xlil.reader.InstructionKind;
import org.xsslang.xlil.reader.XlilModule;
import org.xsslang.xlil.reader.XlilReadException;
import org.xsslang.xlil.reader.XlilReader;
import org.xsslang.xlil.writer.XlilBlockWriter;
import org.xsslang.xlil.writer.XlilFunctionWriter;
import org.xsslang.xlil.writer.XlilType;
import org.xsslang.xlil.writer.XlilValue;
import org.xsslang.xlil.writer.XlilWriter;

final class XlilBindingTest
{
    @Test
    void writesReadsAndVerifiesModuleThroughTheCAbi()
    {
        String text;
        try (XlilWriter writer = XlilWriter.create("JavaBinding"))
        {
            XlilFunctionWriter main = writer.defineFunction("main", XlilType.I32, List.of());
            XlilBlockWriter entry = main.block("entry");
            XlilValue answer = entry.constI32(42);
            entry.returnValue(answer);
            writer.verify();
            text = writer.emit();
        }

        assertTrue(text.startsWith(".xlil version 1\n.xlil module JavaBinding\n"));
        assertTrue(text.contains("%r0:i32 = const.i32 42"));
        try
        {
            XlilModule module = XlilReader.parse("JavaBinding.xlil", text);
            assertEquals(1, module.textVersion());
            assertEquals("JavaBinding", module.name());
            assertEquals(1, module.functions().size());
            assertEquals("main", module.functions().getFirst().name());
            assertEquals(XlilType.I32, module.functions().getFirst().returnType());
            assertEquals(InstructionKind.CONST_I32,
                    module.functions().getFirst().blocks().getFirst().instructions().getFirst().kind());
            assertEquals(42, module.functions().getFirst().blocks().getFirst().instructions().getFirst().integerBits());
            assertEquals(text, module.text());
        }
        catch(XlilReadException failure)
        {
            throw new AssertionError(failure);
        }
    }

    @Test
    void readerRejectsUnsupportedTextVersion()
    {
        assertThrows(XlilReadException.class,
                () -> XlilReader.parse("future.xlil", ".xlil version 99\n.xlil module Future\n"));
    }
}
