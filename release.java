/*
 * SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
 * SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.0
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
                  java --source=21 release.java check 0.3.0
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
        return new Check(name, ok,
                ok ? path + " contains one expected entry" : path + " should contain exactly one: " + needle);
    }

    static Check xsVersion(String version) throws IOException, InterruptedException
    {
        Path binary = Path.of("build/clangcl-debug/vxs.exe");
        if(!Files.isExecutable(binary))
        {
            return new Check("vxs --version", true, "build/clangcl-debug/vxs.exe is not built; skipped runtime version check");
        }

        Process process = new ProcessBuilder(binary.toString(), "--version")
                .redirectError(ProcessBuilder.Redirect.INHERIT).start();
        String output = new String(process.getInputStream().readAllBytes(), StandardCharsets.UTF_8).trim();
        int code = process.waitFor();
        boolean ok = code == 0 && output.equals("vxs " + version);
        return new Check("vxs --version", ok, ok ? output : "expected 'vxs " + version + "', got '" + output + "'");
    }

    static int check(String version) throws IOException, InterruptedException
    {
        List<Check> checks = new ArrayList<>();
        checks.add(contains(Path.of("CMakeLists.txt"), "project(vxs_project VERSION " + version + " LANGUAGES C CXX)",
                "CMake project version"));
        checks.add(containsOnce(Path.of("CHANGELOG.md"), "## " + version + " - ", "CHANGELOG heading"));
        checks.add(contains(Path.of("Compiler/Haskell/Driver/visual-xsharp-compiler.cabal"),
                "version: " + version, "Haskell compiler version"));
        checks.add(contains(Path.of("ProjectSystem/build.gradle.kts"), "version = \"" + version + "\"",
                "Kotlin project runtime version"));
        checks.add(contains(Path.of("xslang/Cargo.toml"), "version = \"" + version + "\"",
                "Rust compiler core version"));
        checks.add(xsVersion(version));

        boolean ok = true;
        for(Check check : checks)
        {
            System.out.println((check.ok ? "ok: " : "error: ") + check.name + " - " + check.detail);
            ok = ok && check.ok;
        }
        return ok ? 0 : 1;
    }

    public static void main(String[] args) throws Exception
    {
        if(args.length == 1 && args[0].equals("help"))
        {
            help();
            return;
        }
        if(args.length == 2 && args[0].equals("check"))
        {
            System.exit(check(args[1]));
        }
        help();
        System.exit(USAGE_ERROR);
    }
}
