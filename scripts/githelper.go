// SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
// SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.0

// githelper preserves the repository's guarded update workflow without making
// commits or pushes part of the build graph. It intentionally passes arguments
// directly to Git rather than constructing a shell command.
package main

import (
	"bytes"
	"errors"
	"fmt"
	"io"
	"os"
	"os/exec"
	"runtime"
	"strings"
)

const (
	usageError = 2
	remoteName = "origin"
)

var generatedPaths = []string{
	"build/",
	".codex",
	"target/",
	"node_modules/",
	"dist/",
	"dist-newstyle/",
	"out/",
	"XS/",
	"Cargo.lock",
	":(glob)**/build/**",
	":(glob)**/target/**",
	":(glob)**/node_modules/**",
	":(glob)**/dist/**",
	":(glob)**/dist-newstyle/**",
	":(glob)**/out/**",
	":(glob)**/Cargo.lock",
}

const helpText = `usage:
  go run scripts/githelper.go update "Commit message"
  go run scripts/githelper.go clean
  go run scripts/githelper.go uncom
  go run scripts/githelper.go help

commands:
  clean    Stage Git hygiene fixes without committing: untrack generated/ignored files.
  update   Run safe git add, commit with the given message, then force-with-lease push to origin/current-branch.
  uncom    Show uncommitted changes.
  help     Show this help.

update excludes:
  generated paths from generatedPaths
  tracked files ignored by .gitignore / standard Git ignore rules

update automatically runs clean before and after git add --all:
  tracked generated files are removed from the index with git rm --cached
  local generated files stay on disk and remain ignored

generated paths:
  build/, .codex/, target/, node_modules/, dist/, dist-newstyle/, out/, Cargo.lock

update push:
  git push -u origin <current-branch> --force-with-lease

examples:
  go run scripts/githelper.go update "Fix parser"
  go run scripts/githelper.go clean
  go run scripts/githelper.go uncom
  go run scripts/githelper.go help`

type helperCommand int

const (
	commandHelp helperCommand = iota
	commandClean
	commandUpdate
	commandUncommitted
)

type invocation struct {
	command helperCommand
	message string
}

type processRunner interface {
	Run(input []byte, quiet bool, arguments ...string) (int, error)
	Capture(arguments ...string) ([]byte, error)
}

type gitProcessRunner struct {
	stdin  io.Reader
	stdout io.Writer
	stderr io.Writer
}

type exitError struct {
	code    int
	message string
}

func (failure exitError) Error() string {
	return failure.message
}

func main() {
	runner := gitProcessRunner{stdin: os.Stdin, stdout: os.Stdout, stderr: os.Stderr}
	if err := execute(os.Args[1:], runner, os.Stdout); err != nil {
		fmt.Fprintln(os.Stderr, err)
		var failure exitError
		if errors.As(err, &failure) {
			os.Exit(failure.code)
		}
		os.Exit(1)
	}
}

func execute(arguments []string, runner processRunner, output io.Writer) error {
	parsed, err := parseInvocation(arguments)
	if err != nil {
		fmt.Fprintln(output, helpText)
		return exitError{code: usageError, message: err.Error()}
	}
	if parsed.command == commandHelp {
		fmt.Fprintln(output, helpText)
		return nil
	}
	if err := requireGitRepository(runner); err != nil {
		return err
	}

	switch parsed.command {
	case commandClean:
		return cleanIndex(runner)
	case commandUpdate:
		return updateRepository(runner, parsed.message)
	case commandUncommitted:
		return showUncommitted(runner, output)
	case commandHelp:
		return nil
	default:
		return exitError{code: usageError, message: "error: unsupported command"}
	}
}

func parseInvocation(arguments []string) (invocation, error) {
	if len(arguments) == 1 {
		switch arguments[0] {
		case "help":
			return invocation{command: commandHelp}, nil
		case "clean":
			return invocation{command: commandClean}, nil
		case "uncom":
			return invocation{command: commandUncommitted}, nil
		}
	}
	if len(arguments) == 2 && arguments[0] == "update" {
		return invocation{command: commandUpdate, message: arguments[1]}, nil
	}
	return invocation{}, errors.New("error: invalid githelper arguments")
}

func (runner gitProcessRunner) Run(input []byte, quiet bool, arguments ...string) (int, error) {
	if !quiet {
		fmt.Fprintln(runner.stderr, "+ git "+strings.Join(arguments, " "))
	}
	command := exec.Command("git", arguments...)
	if input != nil {
		command.Stdin = bytes.NewReader(input)
	} else {
		command.Stdin = runner.stdin
	}
	if quiet {
		command.Stdout = io.Discard
	} else {
		command.Stdout = runner.stdout
	}
	command.Stderr = runner.stderr
	err := command.Run()
	if err == nil {
		return 0, nil
	}
	var processFailure *exec.ExitError
	if errors.As(err, &processFailure) {
		return processFailure.ExitCode(), nil
	}
	return -1, err
}

func (runner gitProcessRunner) Capture(arguments ...string) ([]byte, error) {
	command := exec.Command("git", arguments...)
	command.Stderr = runner.stderr
	output, err := command.Output()
	if err != nil {
		var processFailure *exec.ExitError
		if errors.As(err, &processFailure) {
			return nil, fmt.Errorf("git %s failed with code %d", strings.Join(arguments, " "), processFailure.ExitCode())
		}
		return nil, err
	}
	return output, nil
}

func requireGitRepository(runner processRunner) error {
	output, err := runner.Capture("rev-parse", "--is-inside-work-tree")
	if err != nil || strings.TrimSpace(string(output)) != "true" {
		return exitError{code: 1, message: "error: not inside a git work tree"}
	}
	return nil
}

func currentBranch(runner processRunner) (string, error) {
	output, err := runner.Capture("branch", "--show-current")
	if err != nil {
		return "", err
	}
	branch := strings.TrimSpace(string(output))
	if branch == "" {
		return "", exitError{code: 1, message: "error: detached HEAD state; cannot determine current branch"}
	}
	return branch, nil
}

func isGeneratedDirectory(path string, directory string) bool {
	return path == directory || strings.HasPrefix(path, directory+"/") || strings.Contains(path, "/"+directory+"/")
}

func isCoveredByGeneratedPaths(path string) bool {
	return isGeneratedDirectory(path, "build") ||
		isGeneratedDirectory(path, "target") ||
		isGeneratedDirectory(path, "node_modules") ||
		isGeneratedDirectory(path, "dist") ||
		isGeneratedDirectory(path, "dist-newstyle") ||
		isGeneratedDirectory(path, "out") ||
		path == ".codex" ||
		strings.HasPrefix(path, ".codex/") ||
		path == "Cargo.lock" ||
		strings.HasSuffix(path, "/Cargo.lock")
}

func splitNullSeparated(output []byte) []string {
	parts := bytes.Split(output, []byte{0})
	paths := make([]string, 0, len(parts))
	for _, part := range parts {
		if len(part) != 0 {
			paths = append(paths, string(part))
		}
	}
	return paths
}

func generatedAndIgnoredPathspecs(runner processRunner) ([]string, error) {
	output, err := runner.Capture("ls-files", "-ci", "-z", "--exclude-standard")
	if err != nil {
		return nil, err
	}
	// A small ordered set keeps the original pathspec order deterministic while
	// avoiding a duplicate when an ignored tracked path is already generated.
	seen := make(map[string]struct{}, len(generatedPaths))
	pathspecs := make([]string, 0, len(generatedPaths))
	appendUnique := func(path string) {
		if _, exists := seen[path]; exists {
			return
		}
		seen[path] = struct{}{}
		pathspecs = append(pathspecs, path)
	}
	for _, path := range generatedPaths {
		appendUnique(path)
	}
	for _, path := range splitNullSeparated(output) {
		if !isCoveredByGeneratedPaths(path) {
			appendUnique(path)
		}
	}
	return pathspecs, nil
}

func nullSeparated(paths []string) []byte {
	var output bytes.Buffer
	for _, path := range paths {
		output.WriteString(path)
		output.WriteByte(0)
	}
	return output.Bytes()
}

func untrackGeneratedAndIgnoredFiles(runner processRunner) error {
	pathspecs, err := generatedAndIgnoredPathspecs(runner)
	if err != nil {
		return err
	}
	if len(pathspecs) == 0 {
		return nil
	}
	code, err := runner.Run(
		nullSeparated(pathspecs),
		false,
		"rm",
		"--cached",
		"-r",
		"--ignore-unmatch",
		"--pathspec-from-file=-",
		"--pathspec-file-nul",
	)
	if err != nil {
		return err
	}
	if code != 0 {
		return exitError{code: code, message: "error: generated or ignored files could not be removed from the index"}
	}
	return nil
}

func normalizeWindowsSubmoduleFileModes(runner processRunner) error {
	if runtime.GOOS != "windows" {
		return nil
	}
	code, err := runner.Run(nil, false, "submodule", "foreach", "--recursive", "git config core.filemode false")
	if err != nil {
		return err
	}
	if code != 0 {
		return exitError{code: code, message: "error: Windows submodule filemode configuration failed"}
	}
	return nil
}

func cleanIndex(runner processRunner) error {
	if err := normalizeWindowsSubmoduleFileModes(runner); err != nil {
		return err
	}
	return untrackGeneratedAndIgnoredFiles(runner)
}

func updateRepository(runner processRunner, message string) error {
	if err := cleanIndex(runner); err != nil {
		return err
	}
	if err := requireSuccessfulRun(runner, "error: git add --all failed", "add", "--all"); err != nil {
		return err
	}
	if err := cleanIndex(runner); err != nil {
		return err
	}

	diffCode, err := runner.Run(nil, true, "diff", "--cached", "--quiet")
	if err != nil {
		return err
	}
	if diffCode == 0 {
		return exitError{code: 1, message: "error: nothing to commit"}
	}
	if diffCode != 1 {
		return exitError{code: diffCode, message: "error: git diff --cached --quiet failed"}
	}
	if err := requireSuccessfulRun(runner, "error: git commit failed", "commit", "--message", message); err != nil {
		return err
	}

	branch, err := currentBranch(runner)
	if err != nil {
		return err
	}
	if err := requireSuccessfulRun(
		runner,
		"error: git push --force-with-lease failed",
		"push",
		"-u",
		remoteName,
		branch,
		"--force-with-lease",
	); err != nil {
		return err
	}

	status, err := runner.Capture("status", "--short")
	if err != nil {
		return err
	}
	if len(bytes.TrimSpace(status)) != 0 {
		fmt.Fprint(os.Stderr, string(status))
		return exitError{code: 1, message: "error: update completed, but the work tree is still dirty"}
	}
	return nil
}

func requireSuccessfulRun(runner processRunner, message string, arguments ...string) error {
	code, err := runner.Run(nil, false, arguments...)
	if err != nil {
		return err
	}
	if code != 0 {
		return exitError{code: code, message: message}
	}
	return nil
}

func showUncommitted(runner processRunner, output io.Writer) error {
	status, err := runner.Capture("status", "--short")
	if err != nil {
		return err
	}
	if len(bytes.TrimSpace(status)) == 0 {
		fmt.Fprintln(output, "nothing uncommitted")
		return nil
	}
	_, err = output.Write(status)
	return err
}
