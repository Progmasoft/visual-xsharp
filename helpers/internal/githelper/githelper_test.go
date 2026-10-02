// SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
// SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.1

package githelper

import (
	"bytes"
	"errors"
	"os"
	"path/filepath"
	"reflect"
	"strings"
	"testing"
)

// scriptedGit answers captures from a table and records every command that
// would have changed the repository.
type scriptedGit struct {
	captures map[string]string
	// codes maps a command prefix to the exit code Run returns for it.
	codes map[string]int
	runs  []string
	input map[string]string
}

func (git *scriptedGit) Run(input []byte, _ bool, arguments ...string) (int, error) {
	command := strings.Join(arguments, " ")
	git.runs = append(git.runs, command)
	if input != nil {
		if git.input == nil {
			git.input = map[string]string{}
		}
		git.input[command] = string(input)
	}
	for prefix, code := range git.codes {
		if strings.HasPrefix(command, prefix) {
			return code, nil
		}
	}
	return 0, nil
}

func (git *scriptedGit) Capture(arguments ...string) ([]byte, error) {
	command := strings.Join(arguments, " ")
	if value, known := git.captures[command]; known {
		return []byte(value), nil
	}
	return nil, errors.New("git " + command + " failed")
}

// mutating returns the recorded commands that are not hygiene bookkeeping.
func (git *scriptedGit) mutating() []string {
	var commands []string
	for _, command := range git.runs {
		if strings.HasPrefix(command, "submodule foreach") || strings.HasPrefix(command, "rm --cached") {
			continue
		}
		commands = append(commands, command)
	}
	return commands
}

func topicRepository(staged ...string) *scriptedGit {
	return &scriptedGit{captures: map[string]string{
		"rev-parse --is-inside-work-tree":                           "true\n",
		"branch --show-current":                                     "feature/topic\n",
		"symbolic-ref --quiet --short refs/remotes/origin/HEAD":     "origin/main\n",
		"ls-files -ci -z --exclude-standard":                        "",
		"diff --cached --name-only --no-renames -z":                 string(nullSeparated(staged)),
		"status --short":                                            "",
		"status --porcelain=v1 -z --untracked-files=all":            "",
		"rev-list --count HEAD..origin/main":                        "0\n",
		"rev-parse --abbrev-ref --symbolic-full-name @{upstream}":   "origin/feature/topic\n",
		"rev-list --left-right --count HEAD...origin/feature/topic": "2\t1\n",
	}}
}

func invoke(git *scriptedGit, environment map[string]string, arguments ...string) (string, error) {
	var output bytes.Buffer
	command := newCommand(git, &output, &output, func(name string) string { return environment[name] })
	command.SetArgs(arguments)
	err := command.Execute()
	return output.String(), err
}

func exitCode(err error) int {
	var failure exitError
	if errors.As(err, &failure) {
		return failure.code
	}
	return -1
}

func TestClassifyTreatsOnlyLanguageSourceOutsideHelpersAsCode(t *testing.T) {
	cases := []struct {
		paths []string
		want  commitScope
	}{
		{nil, scopeEmpty},
		{[]string{"Documents/CORE-IR.md", "README.md", "CHANGELOG.md"}, scopeWithoutCode},
		{[]string{"helpers/internal/githelper/cli.go", "helpers/go.mod"}, scopeWithoutCode},
		{[]string{"helpers/cmd/githelper/main.go", "Documents/BUILDING.md"}, scopeWithoutCode},
		{[]string{".github/workflows/ci.yml", "justfile", "MODULE.bazel", "Compiler/Core/BUILD.bazel"}, scopeWithoutCode},
		{[]string{"Compiler/Fuzzing/Corpus/differential/arithmetic.seed", "fourmolu.yaml"}, scopeWithoutCode},
		{[]string{"Compiler/Core/IR.cpp"}, scopeCode},
		{[]string{"Compiler/Headers/Visual/XSharp/Core/IR.hpp"}, scopeCode},
		{[]string{"Compiler/Haskell/Core/src/Visual/XSharp/Core.hs"}, scopeCode},
		{[]string{"Tools/Example.cs"}, scopeCode},
		{[]string{"xide/modules/app/Main.kt"}, scopeCode},
		{[]string{"build.gradle.kts"}, scopeCode},
		{[]string{"Analyzer/client/extension.ts"}, scopeCode},
		{[]string{"Spec/Language/Operators.vxs"}, scopeCode},
		{[]string{"tools/other/main.go"}, scopeCode},
		{[]string{"Compiler/Core/IR.CPP"}, scopeCode},
		// One code file decides, whatever accompanies it.
		{[]string{"Documents/CORE-IR.md", "Compiler/Core/IR.cpp"}, scopeCode},
		{[]string{"helpers/cmd/githelper/main.go", "Compiler/Core/IR.cpp"}, scopeCode},
	}
	for _, test := range cases {
		if actual := classify(test.paths); actual != test.want {
			t.Errorf("classify(%v) = %v, want %v", test.paths, actual, test.want)
		}
	}
}

func TestComposeMessageAddsTheTrailerOnlyWithoutCode(t *testing.T) {
	const author = "Example Author <author@example.invalid>"
	const trailer = "Co-Authored-By: " + author
	body := "Subject\n\nBody text."

	if message := composeMessage(body, author, scopeWithoutCode); message != body+"\n\n"+trailer+"\n" {
		t.Errorf("message without code = %q", message)
	}
	if message := composeMessage(body, author, scopeCode); message != body+"\n" {
		t.Errorf("code message = %q", message)
	}
	if message := composeMessage(body, "", scopeWithoutCode); message != body+"\n" {
		t.Errorf("message without a configured co-author = %q", message)
	}
}

func TestComposeMessageNeverKeepsOrDuplicatesAWrittenTrailer(t *testing.T) {
	const author = "Example Author <author@example.invalid>"
	const trailer = "Co-Authored-By: " + author
	written := "Subject\r\n\r\nBody.\r\n\r\n" + trailer + "\r\n"

	if message := composeMessage(written, author, scopeCode); strings.Contains(message, trailer) {
		t.Errorf("a code commit kept the trailer: %q", message)
	}
	message := composeMessage(written, author, scopeWithoutCode)
	if strings.Count(message, trailer) != 1 || strings.Contains(message, "\r") {
		t.Errorf("documentation message = %q", message)
	}
	// A different co-author written by hand is the author's own statement.
	other := "Subject\n\nCo-Authored-By: Someone Else <else@example.invalid>"
	if message := composeMessage(other, author, scopeCode); !strings.Contains(message, "Someone Else") {
		t.Errorf("an unrelated trailer was removed: %q", message)
	}
}

func TestGeneratedAndPrivatePathsAreRecognizedAtAnyDepth(t *testing.T) {
	for _, path := range []string{
		"build", "Compiler/build/output.o", "ProjectSystem/node_modules/tool",
		"Compiler/Haskell/dist-newstyle/cache", ".codex/PLAN.md", ".claude/WORKLOG.md", ".claude",
	} {
		if !isGenerated(path) {
			t.Errorf("expected %q to be generated or private", path)
		}
	}
	for _, path := range []string{"Compiler/Builder.cpp", "Documents/building.md", "Package.lock", "notes/.claude.md", "outline/a"} {
		if isGenerated(path) {
			t.Errorf("did not expect %q to be generated or private", path)
		}
	}
}

func TestUntrackPathspecsAddEachIgnoredTrackedFileOnce(t *testing.T) {
	git := &scriptedGit{captures: map[string]string{
		"ls-files -ci -z --exclude-standard": string(nullSeparated([]string{"build/generated", "private.txt", "private.txt", "path with spaces/ö"})),
	}}
	pathspecs, err := untrackPathspecs(git)
	if err != nil {
		t.Fatal(err)
	}
	tail := pathspecs[len(pathspecs)-2:]
	if !reflect.DeepEqual(tail, []string{"private.txt", "path with spaces/ö"}) {
		t.Fatalf("tail = %#v", tail)
	}
	if actual := splitNullSeparated(nullSeparated(pathspecs)); !reflect.DeepEqual(actual, pathspecs) {
		t.Fatalf("null-separated round trip = %#v", actual)
	}
}

func TestUpdateStagesCommitsAndPushesInOrder(t *testing.T) {
	git := topicRepository("Compiler/Core/IR.cpp")
	output, err := invoke(git, nil, "update", "Change the IR")
	if err != nil {
		t.Fatalf("update failed: %v\n%s", err, output)
	}
	want := []string{"add --all", "commit --file=-", "push -u origin feature/topic"}
	if actual := git.mutating(); !reflect.DeepEqual(actual, want) {
		t.Fatalf("commands = %#v, want %#v", actual, want)
	}
	if message := git.input["commit --file=-"]; message != "Change the IR\n" {
		t.Fatalf("commit message = %q", message)
	}
	if !strings.Contains(output, "1 staged path(s), code") {
		t.Fatalf("scope was not reported:\n%s", output)
	}
}

func TestUpdateNeverForcesThePush(t *testing.T) {
	git := topicRepository("Compiler/Core/IR.cpp")
	if _, err := invoke(git, nil, "update", "Change"); err != nil {
		t.Fatal(err)
	}
	for _, command := range git.runs {
		if strings.Contains(command, "--force") || strings.Contains(command, " -f") || strings.Contains(command, "+") {
			t.Fatalf("a forcing command was issued: %q", command)
		}
	}
}

func TestUpdateAppliesTheCoAuthorFromTheEnvironmentByScope(t *testing.T) {
	const author = "Example Author <author@example.invalid>"
	environment := map[string]string{coAuthorVariable: author}

	helpers := topicRepository("helpers/internal/githelper/cli.go")
	if output, err := invoke(helpers, environment, "update", "Rewrite the helper"); err != nil {
		t.Fatalf("%v\n%s", err, output)
	}
	if message := helpers.input["commit --file=-"]; !strings.HasSuffix(message, "\n\nCo-Authored-By: "+author+"\n") {
		t.Fatalf("helpers-only message = %q", message)
	}

	code := topicRepository("helpers/internal/githelper/cli.go", "Compiler/Core/IR.cpp")
	output, err := invoke(code, environment, "update", "Change both")
	if err != nil {
		t.Fatalf("%v\n%s", err, output)
	}
	if message := code.input["commit --file=-"]; strings.Contains(message, "Co-Authored-By") {
		t.Fatalf("a commit with code carries the trailer: %q", message)
	}
	if !strings.Contains(output, "co-author trailer withheld") {
		t.Fatalf("the decision was not reported:\n%s", output)
	}
}

func TestUpdateFlagOverridesTheEnvironmentCoAuthor(t *testing.T) {
	git := topicRepository("Documents/BUILDING.md")
	environment := map[string]string{coAuthorVariable: "Environment <environment@example.invalid>"}
	if _, err := invoke(git, environment, "update", "--co-author", "Flag <flag@example.invalid>", "Document the build"); err != nil {
		t.Fatal(err)
	}
	message := git.input["commit --file=-"]
	if !strings.Contains(message, "Flag <flag@example.invalid>") || strings.Contains(message, "Environment") {
		t.Fatalf("message = %q", message)
	}
}

func TestUpdateRefusesTheDefaultBranchBeforeStaging(t *testing.T) {
	git := topicRepository("Compiler/Core/IR.cpp")
	git.captures["branch --show-current"] = "main\n"
	_, err := invoke(git, nil, "update", "Change")
	if err == nil || !strings.Contains(err.Error(), "default branch") {
		t.Fatalf("error = %v", err)
	}
	if len(git.runs) != 0 {
		t.Fatalf("commands ran on the default branch: %#v", git.runs)
	}

	allowed := topicRepository("Compiler/Core/IR.cpp")
	allowed.captures["branch --show-current"] = "main\n"
	if _, err := invoke(allowed, nil, "update", "--allow-default-branch", "Change"); err != nil {
		t.Fatalf("explicitly allowed commit failed: %v", err)
	}
}

func TestUpdateWithNothingStagedDoesNotCommit(t *testing.T) {
	git := topicRepository()
	_, err := invoke(git, nil, "update", "Change")
	if err == nil || !strings.Contains(err.Error(), "nothing to commit") {
		t.Fatalf("error = %v", err)
	}
	for _, command := range git.runs {
		if strings.HasPrefix(command, "commit") || strings.HasPrefix(command, "push") {
			t.Fatalf("unexpected %q", command)
		}
	}
}

func TestUpdateStopsWhenTheCommitFails(t *testing.T) {
	git := topicRepository("Compiler/Core/IR.cpp")
	git.codes = map[string]int{"commit": 1}
	if _, err := invoke(git, nil, "update", "Change"); err == nil {
		t.Fatal("a failed commit was reported as success")
	}
	for _, command := range git.runs {
		if strings.HasPrefix(command, "push") {
			t.Fatal("pushed after a failed commit")
		}
	}
}

func TestUpdateReportsADirtyTreeAfterThePush(t *testing.T) {
	git := topicRepository("Compiler/Core/IR.cpp")
	git.captures["status --short"] = " M Compiler/Core/IR.cpp\n"
	_, err := invoke(git, nil, "update", "Change")
	if err == nil || !strings.Contains(err.Error(), "still dirty") {
		t.Fatalf("error = %v", err)
	}
}

func TestUpdateNoPushOnlyCommits(t *testing.T) {
	git := topicRepository("Compiler/Core/IR.cpp")
	if _, err := invoke(git, nil, "update", "--no-push", "Change"); err != nil {
		t.Fatal(err)
	}
	if actual := git.mutating(); !reflect.DeepEqual(actual, []string{"add --all", "commit --file=-"}) {
		t.Fatalf("commands = %#v", actual)
	}
}

func TestUpdateDryRunChangesNothing(t *testing.T) {
	git := topicRepository()
	git.captures["status --porcelain=v1 -z --untracked-files=all"] = string(nullSeparated([]string{
		" M Documents/BUILDING.md", "?? Documents/NEW.md", "R  Documents/B.md", "Documents/A.md", "?? build/output.o", "?? .claude/WORKLOG.md",
	}))
	output, err := invoke(git, map[string]string{coAuthorVariable: "Example <example@example.invalid>"}, "update", "--dry-run", "Document")
	if err != nil {
		t.Fatalf("%v\n%s", err, output)
	}
	if len(git.runs) != 0 {
		t.Fatalf("a dry run issued commands: %#v", git.runs)
	}
	for _, expected := range []string{"4 staged path(s), no code", "Documents/A.md", "Documents/B.md", "co-author trailer added", "dry run"} {
		if !strings.Contains(output, expected) {
			t.Errorf("output lacks %q:\n%s", expected, output)
		}
	}
	if strings.Contains(output, "build/output.o") || strings.Contains(output, ".claude") {
		t.Errorf("generated or private paths were listed:\n%s", output)
	}
}

func TestUpdateReadsTheMessageFromAFile(t *testing.T) {
	file := filepath.Join(t.TempDir(), "message.txt")
	if err := os.WriteFile(file, []byte("Subject\n\nA body with \"quotes\" and $dollars.\n"), 0o600); err != nil {
		t.Fatal(err)
	}
	git := topicRepository("Compiler/Core/IR.cpp")
	if _, err := invoke(git, nil, "update", "--message-file", file); err != nil {
		t.Fatal(err)
	}
	if message := git.input["commit --file=-"]; message != "Subject\n\nA body with \"quotes\" and $dollars.\n" {
		t.Fatalf("message = %q", message)
	}
}

func TestUpdateRejectsMissingEmptyAndDoubleMessages(t *testing.T) {
	file := filepath.Join(t.TempDir(), "message.txt")
	if err := os.WriteFile(file, []byte("Subject\n"), 0o600); err != nil {
		t.Fatal(err)
	}
	for _, arguments := range [][]string{
		{"update"},
		{"update", "   "},
		{"update", "Message", "--message-file", file},
		{"update", "--message-file", filepath.Join(t.TempDir(), "missing.txt")},
		{"update", "one", "two"},
	} {
		git := topicRepository("Compiler/Core/IR.cpp")
		if _, err := invoke(git, nil, arguments...); err == nil {
			t.Errorf("%v was accepted", arguments)
		}
		if len(git.runs) != 0 {
			t.Errorf("%v ran %#v", arguments, git.runs)
		}
	}
}

func TestPushPublishesWithoutCommitting(t *testing.T) {
	git := topicRepository()
	if _, err := invoke(git, nil, "push"); err != nil {
		t.Fatal(err)
	}
	if !reflect.DeepEqual(git.runs, []string{"push -u origin feature/topic"}) {
		t.Fatalf("commands = %#v", git.runs)
	}

	protected := topicRepository()
	protected.captures["branch --show-current"] = "main\n"
	if _, err := invoke(protected, nil, "push"); err == nil || len(protected.runs) != 0 {
		t.Fatalf("push on the default branch: err=%v runs=%#v", err, protected.runs)
	}
}

func TestSyncMergesOnlyWhenTheDefaultBranchMoved(t *testing.T) {
	current := topicRepository()
	output, err := invoke(current, nil, "sync")
	if err != nil {
		t.Fatal(err)
	}
	if !reflect.DeepEqual(current.runs, []string{"fetch --prune origin"}) || !strings.Contains(output, "already contains") {
		t.Fatalf("runs = %#v, output = %q", current.runs, output)
	}

	behind := topicRepository()
	behind.captures["rev-list --count HEAD..origin/main"] = "3\n"
	output, err = invoke(behind, nil, "sync")
	if err != nil {
		t.Fatal(err)
	}
	if !reflect.DeepEqual(behind.runs, []string{"fetch --prune origin", "merge --no-edit origin/main"}) {
		t.Fatalf("runs = %#v", behind.runs)
	}
	if !strings.Contains(output, "merged 3 commit(s)") {
		t.Fatalf("output = %q", output)
	}
}

func TestSyncNeedsACleanTreeAndReportsConflicts(t *testing.T) {
	dirty := topicRepository()
	dirty.captures["status --short"] = " M file\n"
	if _, err := invoke(dirty, nil, "sync"); err == nil || len(dirty.runs) != 0 {
		t.Fatalf("sync ran on a dirty tree: err=%v runs=%#v", err, dirty.runs)
	}

	conflicted := topicRepository()
	conflicted.captures["rev-list --count HEAD..origin/main"] = "1\n"
	conflicted.codes = map[string]int{"merge": 1}
	_, err := invoke(conflicted, nil, "sync")
	if err == nil || !strings.Contains(err.Error(), "conflicts") {
		t.Fatalf("error = %v", err)
	}
	for _, command := range conflicted.runs {
		if strings.HasPrefix(command, "push") {
			t.Fatal("pushed a conflicted merge")
		}
	}
}

func TestSyncFastForwardsTheDefaultBranchOnly(t *testing.T) {
	git := topicRepository()
	git.captures["branch --show-current"] = "main\n"
	if _, err := invoke(git, nil, "sync"); err != nil {
		t.Fatal(err)
	}
	if !reflect.DeepEqual(git.runs, []string{"fetch --prune origin", "merge --ff-only origin/main"}) {
		t.Fatalf("runs = %#v", git.runs)
	}
}

func TestStartCreatesABranchFromTheRemoteDefault(t *testing.T) {
	git := topicRepository()
	if _, err := invoke(git, nil, "start", "feature/new"); err != nil {
		t.Fatal(err)
	}
	want := []string{"check-ref-format --branch feature/new", "fetch --prune origin", "switch --create feature/new --no-track origin/main"}
	if !reflect.DeepEqual(git.runs, want) {
		t.Fatalf("runs = %#v", git.runs)
	}

	invalid := topicRepository()
	invalid.codes = map[string]int{"check-ref-format": 1}
	if _, err := invoke(invalid, nil, "start", "bad..name"); exitCode(err) != 2 || len(invalid.runs) != 1 {
		t.Fatalf("err=%v runs=%#v", err, invalid.runs)
	}

	dirty := topicRepository()
	dirty.captures["status --short"] = "?? scratch\n"
	if _, err := invoke(dirty, nil, "start", "feature/new"); err == nil {
		t.Fatal("start ran on a dirty tree")
	}
}

func TestDefaultBranchFallsBackToAConventionalRemoteBranch(t *testing.T) {
	git := &scriptedGit{captures: map[string]string{
		"rev-parse --verify --quiet refs/remotes/origin/master": "abc\n",
	}}
	if branch, err := defaultBranch(git); err != nil || branch != "master" {
		t.Fatalf("branch = %q, err = %v", branch, err)
	}
	if _, err := defaultBranch(&scriptedGit{captures: map[string]string{}}); err == nil {
		t.Fatal("an unknown default branch was accepted")
	}
}

func TestStatusReportsUpstreamDefaultBranchAndChanges(t *testing.T) {
	git := topicRepository()
	git.captures["rev-list --count HEAD..origin/main"] = "4\n"
	output, err := invoke(git, nil, "status")
	if err != nil {
		t.Fatal(err)
	}
	for _, expected := range []string{"branch: feature/topic", "origin/feature/topic (2 ahead, 1 behind)", "origin/main (4 commit(s) not merged here", "nothing uncommitted"} {
		if !strings.Contains(output, expected) {
			t.Errorf("output lacks %q:\n%s", expected, output)
		}
	}
	// The earlier command name still works.
	git.captures["status --short"] = " M file\n"
	if output, err := invoke(git, nil, "uncom"); err != nil || !strings.Contains(output, " M file") {
		t.Fatalf("uncom: err=%v output=%q", err, output)
	}
	if len(git.runs) != 0 {
		t.Fatalf("status changed the repository: %#v", git.runs)
	}
}

func TestCommandsOutsideARepositoryDoNothing(t *testing.T) {
	git := &scriptedGit{captures: map[string]string{}}
	for _, arguments := range [][]string{{"update", "Message"}, {"push"}, {"sync"}, {"start", "x"}, {"status"}, {"clean"}} {
		if _, err := invoke(git, nil, arguments...); err == nil || !strings.Contains(err.Error(), "not inside a git work tree") {
			t.Errorf("%v: err = %v", arguments, err)
		}
	}
	if len(git.runs) != 0 {
		t.Fatalf("commands ran outside a repository: %#v", git.runs)
	}
}

func TestInvalidInvocationsNeverReachGit(t *testing.T) {
	for _, arguments := range [][]string{{"unknown"}, {"push", "extra"}, {"clean", "extra"}, {"start"}, {"update", "--force", "Message"}} {
		git := topicRepository("Compiler/Core/IR.cpp")
		if _, err := invoke(git, nil, arguments...); err == nil {
			t.Errorf("%v was accepted", arguments)
		}
		if len(git.runs) != 0 {
			t.Errorf("%v ran %#v", arguments, git.runs)
		}
	}
}

func TestHelpDescribesEveryCommand(t *testing.T) {
	git := &scriptedGit{}
	output, err := invoke(git, nil, "--help")
	if err != nil {
		t.Fatal(err)
	}
	for _, name := range []string{"update", "push", "sync", "start", "status", "clean"} {
		if !strings.Contains(output, "\n  "+name) {
			t.Errorf("help lacks %q:\n%s", name, output)
		}
	}
	if output, err := invoke(git, nil, "help", "update"); err != nil || !strings.Contains(output, "--message-file") {
		t.Fatalf("help update: err=%v\n%s", err, output)
	}
}
