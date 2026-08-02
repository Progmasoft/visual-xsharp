/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 */

package org.xsslang.xlil.reader;

import java.util.OptionalInt;

/** Immutable snapshot of an XLIL block terminator. */
public record XlilTerminator(TerminatorKind kind, OptionalInt value, OptionalInt target, OptionalInt condition,
        OptionalInt thenBlock, OptionalInt elseBlock)
{
}
