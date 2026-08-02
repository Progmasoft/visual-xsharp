/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 */

package org.xsslang.xlil.reader;

import java.util.List;

/** Immutable snapshot of one XLIL basic block. */
public record XlilBlock(int id, String label, List<XlilInstruction> instructions, XlilTerminator terminator)
{
    public XlilBlock
    {
        instructions = List.copyOf(instructions);
    }
}
