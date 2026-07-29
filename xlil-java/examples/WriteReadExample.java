/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 */

import java.util.List;
import org.xsslang.xlil.reader.XlilModule;
import org.xsslang.xlil.reader.XlilReader;
import org.xsslang.xlil.writer.XlilBlockWriter;
import org.xsslang.xlil.writer.XlilFunctionWriter;
import org.xsslang.xlil.writer.XlilType;
import org.xsslang.xlil.writer.XlilValue;
import org.xsslang.xlil.writer.XlilWriter;

/** Minimal writer and reader example for the Java 25 FFM binding. */
public final class WriteReadExample {
  private WriteReadExample() {}

  public static void main(String[] arguments) {
    String text;
    try (XlilWriter writer = XlilWriter.create("JavaExample")) {
      XlilFunctionWriter function =
          writer.defineFunction("main", XlilType.I32, List.of());
      XlilBlockWriter entry = function.block("entry");
      XlilValue exitCode = entry.constI32(0);
      entry.returnValue(exitCode);
      writer.verify();
      text = writer.emit();
    }

    XlilModule module = XlilReader.parse("JavaExample.xlil", text);
    System.out.print(module.text());
  }
}
