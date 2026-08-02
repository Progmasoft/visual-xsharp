/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 */

package org.xsslang.xlil.reader;

import java.util.List;

/** Immutable Java snapshot of a parsed and verified XLIL module. */
public record XlilModule(int textVersion, String name, List<XlilFunction> functions, String text)
{
    public XlilModule
    {
        functions = List.copyOf(functions);
    }
}
