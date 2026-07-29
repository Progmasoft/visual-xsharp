/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 */

import java.io.IOException;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Path;
import java.util.ArrayList;
import java.util.List;

class Release
{
  static final int USAGE_ERROR = 2;

  record Check(String name, boolean ok, String detail)
  {
  }

  static void help()
  {
    System.out.println("""
        usage:
          java --source=21 release.java check <version>
          java --source=21 release.java help

        commands:
          check   Validate release metadata for the requested version.
          help    Show this help.

        examples:
          java --source=21 release.java check 0.2.3
        """);
  }

  static String read(Path path) throws IOException
  {
    return Files.readString(path, StandardCharsets.UTF_8);
  }

  static Check contains(Path path, String needle, String name) throws IOException
  {
    boolean ok = read(path).contains(needle);
    return new Check(name, ok, ok ? path + " contains expected text" : path + " is missing: " + needle);
  }

  static Check containsOnce(Path path, String needle, String name) throws IOException
  {
    String text = read(path);
    int first = text.indexOf(needle);
    int last = text.lastIndexOf(needle);
    boolean ok = first >= 0 && first == last;
    return new Check(name, ok, ok ? path + " contains one expected entry" : path + " should contain exactly one: " + needle);
  }

  static Check xsVersion(String version) throws IOException, InterruptedException
  {
    Path binary = Path.of("build/clang-debug/xs");
    if (!Files.isExecutable(binary))
    {
      return new Check("xs --version", true, "build/clang-debug/xs is not built; skipped runtime version check");
    }

    Process process = new ProcessBuilder(binary.toString(), "--version")
        .redirectError(ProcessBuilder.Redirect.INHERIT)
        .start();
    String output = new String(process.getInputStream().readAllBytes(), StandardCharsets.UTF_8).trim();
    int code = process.waitFor();
    boolean ok = code == 0 && output.equals("xs " + version);
    return new Check("xs --version", ok, ok ? output : "expected 'xs " + version + "', got '" + output + "'");
  }

  static int check(String version) throws IOException, InterruptedException
  {
    List<Check> checks = new ArrayList<>();
    checks.add(contains(Path.of("CMakeLists.txt"), "project(xs_project VERSION " + version + " LANGUAGES C)",
        "CMake project version"));
    checks.add(containsOnce(Path.of("CHANGELOG.md"), "## " + version + " - ", "CHANGELOG heading"));
    checks.add(contains(Path.of("docs/RELEASES.md"), "Current project version: `" + version + "`", "release docs"));
    checks.add(
        contains(Path.of("docs/CLI.md"), "`xs --version` prints the compiler version, such as `xs " + version + "`",
            "CLI version docs"));
    checks.add(xsVersion(version));

    boolean ok = true;
    for (Check check : checks)
    {
      System.out.println((check.ok ? "ok: " : "error: ") + check.name + " - " + check.detail);
      ok = ok && check.ok;
    }
    return ok ? 0 : 1;
  }

  public static void main(String[] args) throws Exception
  {
    if (args.length == 1 && args[0].equals("help"))
    {
      help();
      return;
    }
    if (args.length == 2 && args[0].equals("check"))
    {
      System.exit(check(args[1]));
    }
    help();
    System.exit(USAGE_ERROR);
  }
}
