/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 */

package org.xsslang.xlil.reader;

/** XLIL basic-block terminator tags. */
public enum TerminatorKind
{
    NONE, RETURN, BRANCH, BRANCH_IF, PANIC;

    static TerminatorKind fromNative(int value)
    {
        TerminatorKind[] kinds = values();
        if(value < 0 || value >= kinds.length)
        {
            throw new IllegalStateException("unknown native XLIL terminator kind: " + value);
        }
        return kinds[value];
    }
}
