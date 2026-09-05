// SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
// SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.0

package main

import (
	"bytes"
	"errors"
	"reflect"
	"testing"
)

func TestParseGitHelperCommands(t *testing.T) {
	cases := []struct {
		arguments []string
		command   helperCommand
		message   string
	}{
		{[]string{"help"}, commandHelp, ""},
		{[]string{"clean"}, commandClean, ""},
		{[]string{"uncom"}, commandUncommitted, ""},
		{[]string{"update", "Detailed message"}, commandUpdate, "Detailed message"},
	}
	for _, test := range cases {
		actual, err := parseInvocation(test.arguments)
		if err != nil {
			t.Fatalf("parseInvocation(%v): %v", test.arguments, err)
		}
		if actual.command != test.command || actual.message != test.message {
			t.Fatalf("parseInvocation(%v) = %#v", test.arguments, actual)
		}
	}
}

func TestParseGitHelperRejectsInvalidShapes(t *testing.T) {
	for _, arguments := range [][]string{nil, {"update"}, {"clean", "extra"}} {
		if _, err := parseInvocation(arguments); err == nil {
			t.Fatalf("expected %v to be rejected", arguments)
		}
	}
}

func TestGeneratedCoverageMatchesNestedDirectories(t *testing.T) {
	generated := []string{
		"build",
		"Compiler/build/output.o",
		"ProjectSystem/node_modules/tool",
		"Compiler/Haskell/dist-newstyle/cache",
		"xslang/Cargo.lock",
		".codex/PLAN.md",
	}
	for _, path := range generated {
		if !isCoveredByGeneratedPaths(path) {
			t.Fatalf("expected %q to be generated", path)
		}
	}
	for _, path := range []string{"Compiler/Builder.cpp", "Documents/building.md", "Cargo.toml"} {
		if isCoveredByGeneratedPaths(path) {
			t.Fatalf("did not expect %q to be generated", path)
		}
	}
}

func TestNullSeparatedRoundTripPreservesSpacesAndUnicode(t *testing.T) {
	paths := []string{"plain", "path with spaces/file", "Türkçe/örnek"}
	if actual := splitNullSeparated(nullSeparated(paths)); !reflect.DeepEqual(actual, paths) {
		t.Fatalf("round trip = %#v", actual)
	}
}

type recordedRun struct {
	input     []byte
	quiet     bool
	arguments []string
}

type scriptedGitRunner struct {
	captures map[string][]byte
	runs     []recordedRun
	codes    []int
	failed   bool
}

func (runner *scriptedGitRunner) Run(input []byte, quiet bool, arguments ...string) (int, error) {
	runner.runs = append(runner.runs, recordedRun{input: append([]byte(nil), input...), quiet: quiet, arguments: append([]string(nil), arguments...)})
	if runner.failed {
		return -1, errors.New("process start failed")
	}
	if len(runner.codes) == 0 {
		return 0, nil
	}
	code := runner.codes[0]
	runner.codes = runner.codes[1:]
	return code, nil
}

func (runner *scriptedGitRunner) Capture(arguments ...string) ([]byte, error) {
	key := stringsJoin(arguments)
	if value, ok := runner.captures[key]; ok {
		return append([]byte(nil), value...), nil
	}
	return nil, errors.New("unexpected capture: " + key)
}

func stringsJoin(values []string) string {
	var output bytes.Buffer
	for index, value := range values {
		if index != 0 {
			output.WriteByte(' ')
		}
		output.WriteString(value)
	}
	return output.String()
}

func TestGeneratedPathspecsAppendOnlyUncoveredIgnoredFiles(t *testing.T) {
	runner := &scriptedGitRunner{captures: map[string][]byte{
		"ls-files -ci -z --exclude-standard": nullSeparated([]string{"build/generated", "private.txt", "private.txt"}),
	}}
	pathspecs, err := generatedAndIgnoredPathspecs(runner)
	if err != nil {
		t.Fatal(err)
	}
	if pathspecs[len(pathspecs)-1] != "private.txt" {
		t.Fatalf("last pathspec = %q", pathspecs[len(pathspecs)-1])
	}
	count := 0
	for _, path := range pathspecs {
		if path == "private.txt" {
			count++
		}
	}
	if count != 1 {
		t.Fatalf("private path appeared %d times", count)
	}
}

func TestShowUncommittedReportsCleanRepository(t *testing.T) {
	runner := &scriptedGitRunner{captures: map[string][]byte{"status --short": nil}}
	var output bytes.Buffer
	if err := showUncommitted(runner, &output); err != nil {
		t.Fatal(err)
	}
	if output.String() != "nothing uncommitted\n" {
		t.Fatalf("output = %q", output.String())
	}
}
