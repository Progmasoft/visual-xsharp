/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 */

package org.xsslang.xlil.writer;

import java.util.Objects;

/** A scalar or module-registry XLIL type handle. */
public record XlilType(Kind kind, int registryId) {
  public static final XlilType VOID = scalar(Kind.VOID);
  public static final XlilType BOOL = scalar(Kind.BOOL);
  public static final XlilType U8 = scalar(Kind.U8);
  public static final XlilType I8 = scalar(Kind.I8);
  public static final XlilType U16 = scalar(Kind.U16);
  public static final XlilType I16 = scalar(Kind.I16);
  public static final XlilType U32 = scalar(Kind.U32);
  public static final XlilType I32 = scalar(Kind.I32);
  public static final XlilType U64 = scalar(Kind.U64);
  public static final XlilType I64 = scalar(Kind.I64);
  public static final XlilType U128 = scalar(Kind.U128);
  public static final XlilType I128 = scalar(Kind.I128);
  public static final XlilType F16 = scalar(Kind.F16);
  public static final XlilType F32 = scalar(Kind.F32);
  public static final XlilType F64 = scalar(Kind.F64);
  public static final XlilType F128 = scalar(Kind.F128);
  public static final XlilType STR = scalar(Kind.STR);

  public XlilType {
    Objects.requireNonNull(kind, "kind");
    if (registryId < 0) {
      throw new IllegalArgumentException("registryId must not be negative");
    }
    if (kind.isScalar() && registryId != 0) {
      throw new IllegalArgumentException("scalar XLIL types have registry id zero");
    }
  }

  public static XlilType scalar(Kind kind) {
    if (!Objects.requireNonNull(kind, "kind").isScalar()) {
      throw new IllegalArgumentException("registry type requires a registry id");
    }
    return new XlilType(kind, 0);
  }

  /** XLIL v0 type kinds in C ABI order. */
  public enum Kind {
    VOID,
    BOOL,
    U8,
    I8,
    U16,
    I16,
    U32,
    I32,
    U64,
    I64,
    U128,
    I128,
    F16,
    F32,
    F64,
    F128,
    STR,
    AGGREGATE,
    ARRAY;

    public boolean isScalar() {
      return ordinal() <= STR.ordinal();
    }
  }
}
