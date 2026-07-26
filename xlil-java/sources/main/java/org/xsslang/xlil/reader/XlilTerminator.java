/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <xs-lang.chess031@slmails.com>
 * SPDX-License-Identifier: MPL-2.0
 */

package org.xsslang.xlil.reader;

import java.util.OptionalInt;

/** Immutable snapshot of an XLIL block terminator. */
public record XlilTerminator(
    TerminatorKind kind,
    OptionalInt value,
    OptionalInt target,
    OptionalInt condition,
    OptionalInt thenBlock,
    OptionalInt elseBlock) {}
