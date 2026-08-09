/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 */

import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.nio.charset.StandardCharsets;
import java.util.ArrayList;
import java.util.LinkedHashSet;
import java.util.List;
import java.util.Set;

class Git
{
    static final int USAGE_ERROR = 2;

    static final String REMOTE = "origin";

    static final String[] GENERATED_PATHS =
    {"build/", ".codex", "target/", "node_modules/", "dist/", "dist-newstyle/", "out/", "XS/", "Cargo.lock", ":(glob)**/build/**",
            ":(glob)**/target/**", ":(glob)**/node_modules/**", ":(glob)**/dist/**", ":(glob)**/dist-newstyle/**", ":(glob)**/out/**",
            ":(glob)**/Cargo.lock"};

    enum Command
    {
        CLEAN, UPDATE, UNCOM, HELP
    }

    record Cli(Command command, String message)
    {
    }

    static int run(String... command) throws IOException, InterruptedException
    {
        System.err.println("+ " + String.join(" ", command));

        return new ProcessBuilder(command).inheritIO().start().waitFor();
    }

    static int runQuiet(String... command) throws IOException, InterruptedException
    {
        return new ProcessBuilder(command).redirectOutput(ProcessBuilder.Redirect.DISCARD)
                .redirectError(ProcessBuilder.Redirect.INHERIT).start().waitFor();
    }

    static int runWithInput(byte[] input, String... command) throws IOException, InterruptedException
    {
        System.err.println("+ " + String.join(" ", command));

        Process process = new ProcessBuilder(command).redirectOutput(ProcessBuilder.Redirect.INHERIT)
                .redirectError(ProcessBuilder.Redirect.INHERIT).start();

        try (var output = process.getOutputStream())
        {
            output.write(input);
        }

        return process.waitFor();
    }

    static String capture(String... command) throws IOException, InterruptedException
    {
        Process process = new ProcessBuilder(command).redirectError(ProcessBuilder.Redirect.INHERIT).start();

        ByteArrayOutputStream output = new ByteArrayOutputStream();

        try (var input = process.getInputStream())
        {
            input.transferTo(output);
        }

        int code = process.waitFor();

        if(code != 0)
        {
            throw new RuntimeException(String.join(" ", command) + " failed with code " + code);
        }

        return output.toString(StandardCharsets.UTF_8);
    }

    static List<String> captureNullSeparated(String... command) throws IOException, InterruptedException
    {
        String raw = capture(command);
        List<String> result = new ArrayList<>();

        int start = 0;

        for(int i = 0; i < raw.length(); ++i)
        {
            if(raw.charAt(i) == '\0')
            {
                if(i > start)
                {
                    result.add(raw.substring(start, i));
                }

                start = i + 1;
            }
        }

        if(start < raw.length())
        {
            result.add(raw.substring(start));
        }

        return result;
    }

    static byte[] toNullSeparatedBytes(Set<String> values)
    {
        ByteArrayOutputStream output = new ByteArrayOutputStream();

        for(String value : values)
        {
            output.writeBytes(value.getBytes(StandardCharsets.UTF_8));
            output.write(0);
        }

        return output.toByteArray();
    }

    static void exit(int code, String message)
    {
        System.err.println(message);
        System.exit(code);
    }

    static void help()
    {
        System.out.println(
                """
                        usage:
                          git update "Commit message"
                          git clean
                          git uncom
                          git help

                        commands:
                          clean    Stage Git hygiene fixes without committing: untrack generated/ignored files.
                          update   Run safe git add, commit with the given message, then force-with-lease push to origin/current-branch.
                          uncom    Show uncommitted changes.
                          help     Show this help.

                        update excludes:
                          generated paths from GENERATED_PATHS
                          tracked files ignored by .gitignore / standard Git ignore rules

                        update automatically runs clean before and after git add --all:
                          tracked generated files are removed from the index with git rm --cached
                          local generated files stay on disk and remain ignored

                        generated paths:
                          build/, .codex/, target/, node_modules/, dist/, dist-newstyle/, out/, Cargo.lock

                        update push:
                          git push -u origin <current-branch> --force-with-lease

                        examples:
                          git update "Fix parser"
                          git clean
                          git uncom
                          git help
                        """);
    }

    static Cli parse(String[] args)
    {
        if(args.length == 1 && args[0].equals("help"))
        {
            return new Cli(Command.HELP, null);
        }

        if(args.length == 1 && args[0].equals("clean"))
        {
            return new Cli(Command.CLEAN, null);
        }

        if(args.length == 1 && args[0].equals("uncom"))
        {
            return new Cli(Command.UNCOM, null);
        }

        if(args.length == 2 && args[0].equals("update"))
        {
            return new Cli(Command.UPDATE, args[1]);
        }

        help();
        System.exit(USAGE_ERROR);

        throw new IllegalStateException("unreachable");
    }

    static void requireGitRepo() throws IOException, InterruptedException
    {
        String result = capture("git", "rev-parse", "--is-inside-work-tree").trim();

        if(!result.equals("true"))
        {
            exit(1, "error: not inside a git work tree");
        }
    }

    static String currentBranch() throws IOException, InterruptedException
    {
        String branch = capture("git", "branch", "--show-current").trim();

        if(branch.isBlank())
        {
            exit(1, "error: detached HEAD state; cannot determine current branch");
        }

        return branch;
    }

    static boolean isGeneratedDirectory(String path, String directory)
    {
        return path.equals(directory) || path.startsWith(directory + "/") || path.contains("/" + directory + "/");
    }

    static boolean isCoveredByGeneratedPaths(String path)
    {
        return isGeneratedDirectory(path, "build") || isGeneratedDirectory(path, "target")
                || isGeneratedDirectory(path, "node_modules") || isGeneratedDirectory(path, "dist")
                || isGeneratedDirectory(path, "dist-newstyle")
                || isGeneratedDirectory(path, "out") || path.equals(".codex") || path.startsWith(".codex/")
                || path.equals("Cargo.lock") || path.endsWith("/Cargo.lock");
    }

    static List<String> ignoredTrackedFiles() throws IOException, InterruptedException
    {
        return captureNullSeparated("git", "ls-files", "-ci", "-z", "--exclude-standard");
    }

    static LinkedHashSet<String> generatedAndIgnoredPathspecs() throws IOException, InterruptedException
    {
        LinkedHashSet<String> pathspecs = new LinkedHashSet<>();

        for(String path : GENERATED_PATHS)
        {
            pathspecs.add(path);
        }

        for(String path : ignoredTrackedFiles())
        {
            if(!isCoveredByGeneratedPaths(path))
            {
                pathspecs.add(path);
            }
        }

        return pathspecs;
    }

    static void untrackGeneratedAndIgnoredFiles() throws IOException, InterruptedException
    {
        LinkedHashSet<String> pathspecs = generatedAndIgnoredPathspecs();

        if(pathspecs.isEmpty())
        {
            return;
        }

        int rmCode = runWithInput(toNullSeparatedBytes(pathspecs), "git", "rm", "--cached", "-r", "--ignore-unmatch",
                "--pathspec-from-file=-", "--pathspec-file-nul");

        if(rmCode != 0)
        {
            exit(rmCode, "error: generated or ignored files could not be removed from the index");
        }
    }

    static void normalizeWindowsSubmoduleFileModes() throws IOException, InterruptedException
    {
        String osName = System.getProperty("os.name", "").toLowerCase();

        if(!osName.contains("windows"))
        {
            return;
        }

        int configCode = run("git", "submodule", "foreach", "--recursive", "git config core.filemode false");

        if(configCode != 0)
        {
            exit(configCode, "error: Windows submodule filemode configuration failed");
        }
    }

    static void clean() throws IOException, InterruptedException
    {
        normalizeWindowsSubmoduleFileModes();
        untrackGeneratedAndIgnoredFiles();
    }

    static void update(String message) throws IOException, InterruptedException
    {
        clean();

        int addCode = run("git", "add", "--all");

        if(addCode != 0)
        {
            exit(addCode, "error: git add --all failed");
        }

        clean();

        int diffCode = runQuiet("git", "diff", "--cached", "--quiet");

        if(diffCode == 0)
        {
            exit(1, "error: nothing to commit");
        }

        if(diffCode != 1)
        {
            exit(diffCode, "error: git diff --cached --quiet failed");
        }

        int commitCode = run("git", "commit", "--message", message);

        if(commitCode != 0)
        {
            exit(commitCode, "error: git commit failed");
        }

        String branch = currentBranch();

        int pushCode = run("git", "push", "-u", REMOTE, branch, "--force-with-lease");

        if(pushCode != 0)
        {
            exit(pushCode, "error: git push --force-with-lease failed");
        }

        String status = capture("git", "status", "--short");

        if(!status.isBlank())
        {
            System.err.print(status);
            exit(1, "error: update completed, but the work tree is still dirty");
        }
    }

    static void uncom() throws IOException, InterruptedException
    {
        String status = capture("git", "status", "--short");

        if(status.isBlank())
        {
            System.out.println("nothing uncommitted");
            return;
        }

        System.out.print(status);
    }

    public static void main(String[] args) throws Exception
    {
        Cli cli = parse(args);

        if(cli.command() == Command.HELP)
        {
            help();
            return;
        }

        requireGitRepo();

        switch(cli.command())
        {
            case CLEAN -> clean();
            case UPDATE -> update(cli.message());
            case UNCOM -> uncom();
            case HELP -> help();
        }
    }
}
