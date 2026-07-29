/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 */

package org.xsslang.xlil.reader;

import java.util.List;
import org.xsslang.xlil.writer.XlilType;

/** Immutable snapshot of an XLIL function declaration or definition. */
public record XlilFunction(
    String name,
    XlilType returnType,
    List<XlilType> parameterTypes,
    List<Integer> parameterValues,
    List<XlilType> valueTypes,
    List<XlilType> slots,
    boolean definition,
    List<XlilBlock> blocks) {

  public XlilFunction {
    parameterTypes = List.copyOf(parameterTypes);
    parameterValues = List.copyOf(parameterValues);
    valueTypes = List.copyOf(valueTypes);
    slots = List.copyOf(slots);
    blocks = List.copyOf(blocks);
  }
}
