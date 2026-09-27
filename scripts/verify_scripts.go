// SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
// SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.1

// verify_scripts applies the same formatting, static-analysis, and test gates
// to every standalone Go command in scripts/ before CI or a release accepts it.
package main

import (
	"errors"
	"flag"
	"fmt"
	"go/parser"
	"go/token"
	"io"
	"os"
	"os/exec"
	"path/filepath"
	"sort"
	"strings"
)

const (
	maximumGoScriptLines = 1500
	goLicenseIdentifier  = "// SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.1"
)

type goScriptPair struct {
	name       string
	sourcePath string
	testPath   string
}

type goQualityRunner interface {
	Run(directory string, command string, arguments ...string) ([]byte, error)
}

type systemGoQualityRunner struct{}

func (systemGoQualityRunner) Run(directory string, command string, arguments ...string) ([]byte, error) {
	process := exec.Command(command, arguments...)
	process.Dir = directory
	return process.CombinedOutput()
}

func main() {
	if err := runScriptQuality(os.Args[1:], os.Stdout, os.Stderr, systemGoQualityRunner{}); err != nil {
		fmt.Fprintln(os.Stderr, err)
		os.Exit(1)
	}
}

func runScriptQuality(arguments []string, output io.Writer, errorOutput io.Writer, runner goQualityRunner) error {
	flags := flag.NewFlagSet("verify_scripts", flag.ContinueOnError)
	flags.SetOutput(errorOutput)
	repositoryRoot := flags.String("Root", ".", "repository root containing scripts/")
	help := flags.Bool("Help", false, "print usage information")
	flags.Usage = func() {
		fmt.Fprintln(output, `Verify every standalone Go command in scripts/.

Usage:
  go run scripts/verify_scripts.go [-Root repository-path]
  go run scripts/verify_scripts.go -Help

The check requires a paired unit test for every command, Progmasoft SPDX
headers, package main, and at most 1500 lines per Go file. It then runs gofmt,
go vet, and go test for every command/test pair.`)
	}
	if err := flags.Parse(arguments); err != nil {
		return err
	}
	if *help {
		flags.Usage()
		return nil
	}
	if flags.NArg() != 0 {
		flags.Usage()
		return errors.New("unexpected positional arguments")
	}

	root, err := filepath.Abs(*repositoryRoot)
	if err != nil {
		return fmt.Errorf("resolve repository root: %w", err)
	}
	pairs, err := discoverGoScriptPairs(filepath.Join(root, "scripts"), maximumGoScriptLines)
	if err != nil {
		return err
	}
	if len(pairs) == 0 {
		return errors.New("scripts/ contains no paired Go commands and tests")
	}

	files := make([]string, 0, len(pairs)*2)
	for _, pair := range pairs {
		files = append(files, pair.sourcePath, pair.testPath)
	}
	fmt.Fprintf(output, "Verifying %d Go command/test pairs (maximum %d lines per file).\n", len(pairs), maximumGoScriptLines)
	if outputBytes, runErr := runner.Run(root, "gofmt", append([]string{"-l"}, files...)...); runErr != nil {
		return commandFailure("gofmt", outputBytes, runErr)
	} else if strings.TrimSpace(string(outputBytes)) != "" {
		return fmt.Errorf("Go files need formatting; run gofmt on:\n%s", strings.TrimSpace(string(outputBytes)))
	}
	fmt.Fprintf(output, "PASS gofmt (%d files)\n", len(files))

	var failures []string
	for _, pair := range pairs {
		arguments := []string{pair.sourcePath, pair.testPath}
		if outputBytes, runErr := runner.Run(root, "go", append([]string{"vet"}, arguments...)...); runErr != nil {
			failures = append(failures, commandFailureText("go vet "+pair.name, outputBytes, runErr))
		} else {
			fmt.Fprintf(output, "PASS go vet %s\n", pair.name)
		}
		if outputBytes, runErr := runner.Run(root, "go", append([]string{"test"}, arguments...)...); runErr != nil {
			failures = append(failures, commandFailureText("go test "+pair.name, outputBytes, runErr))
		} else {
			fmt.Fprintf(output, "PASS go test %s\n", pair.name)
		}
	}
	if len(failures) != 0 {
		return errors.New("Go script quality verification failed:\n - " + strings.Join(failures, "\n - "))
	}
	fmt.Fprintf(output, "Verified %d Go commands and their unit tests.\n", len(pairs))
	return nil
}

func discoverGoScriptPairs(directory string, maximumLines int) ([]goScriptPair, error) {
	entries, err := os.ReadDir(directory)
	if err != nil {
		return nil, fmt.Errorf("read scripts/: %w", err)
	}

	sources := make(map[string]string)
	tests := make(map[string]string)
	var failures []string
	for _, entry := range entries {
		if entry.IsDir() || strings.ToLower(filepath.Ext(entry.Name())) != ".go" {
			continue
		}
		path := filepath.Join(directory, entry.Name())
		information, statErr := entry.Info()
		if statErr != nil {
			failures = append(failures, fmt.Sprintf("inspect scripts/%s: %v", entry.Name(), statErr))
			continue
		}
		if !information.Mode().IsRegular() {
			failures = append(failures, fmt.Sprintf("scripts/%s must be a regular file", entry.Name()))
			continue
		}
		if lineCount, countErr := countFileLines(path); countErr != nil {
			failures = append(failures, fmt.Sprintf("read scripts/%s: %v", entry.Name(), countErr))
		} else if lineCount > maximumLines {
			failures = append(failures, fmt.Sprintf("scripts/%s has %d lines; the maximum is %d", entry.Name(), lineCount, maximumLines))
		}
		if headerErr := verifyGoHeader(path); headerErr != nil {
			failures = append(failures, fmt.Sprintf("scripts/%s: %v", entry.Name(), headerErr))
		}
		if packageErr := verifyMainPackage(path); packageErr != nil {
			failures = append(failures, fmt.Sprintf("scripts/%s: %v", entry.Name(), packageErr))
		}

		if strings.HasSuffix(entry.Name(), "_test.go") {
			stem := strings.TrimSuffix(entry.Name(), "_test.go")
			tests[stem] = entry.Name()
		} else {
			sources[strings.TrimSuffix(entry.Name(), filepath.Ext(entry.Name()))] = entry.Name()
		}
	}

	for stem, name := range sources {
		if _, found := tests[stem]; !found {
			failures = append(failures, fmt.Sprintf("scripts/%s has no paired scripts/%s_test.go", name, stem))
		}
	}
	for stem, name := range tests {
		if _, found := sources[stem]; !found {
			failures = append(failures, fmt.Sprintf("scripts/%s has no paired scripts/%s.go", name, stem))
		}
	}
	if len(failures) != 0 {
		sort.Strings(failures)
		return nil, errors.New("Go script inventory verification failed:\n - " + strings.Join(failures, "\n - "))
	}

	stems := make([]string, 0, len(sources))
	for stem := range sources {
		stems = append(stems, stem)
	}
	sort.Strings(stems)
	pairs := make([]goScriptPair, 0, len(stems))
	for _, stem := range stems {
		pairs = append(pairs, goScriptPair{
			name:       stem,
			sourcePath: filepath.Join("scripts", sources[stem]),
			testPath:   filepath.Join("scripts", tests[stem]),
		})
	}
	return pairs, nil
}

func countFileLines(path string) (int, error) {
	contents, err := os.ReadFile(path)
	if err != nil {
		return 0, err
	}
	if len(contents) == 0 {
		return 0, nil
	}
	lines := strings.Count(string(contents), "\n")
	if contents[len(contents)-1] != '\n' {
		lines++
	}
	return lines, nil
}

func verifyGoHeader(path string) error {
	contents, err := os.ReadFile(path)
	if err != nil {
		return err
	}
	lines := strings.SplitN(string(contents), "\n", 3)
	if len(lines) < 2 || !strings.HasPrefix(strings.TrimSuffix(lines[0], "\r"), "// SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>") || strings.TrimSuffix(lines[1], "\r") != goLicenseIdentifier {
		return errors.New("file must begin with the repository's Progmasoft SPDX copyright and license lines")
	}
	return nil
}

func verifyMainPackage(path string) error {
	parsed, err := parser.ParseFile(token.NewFileSet(), path, nil, parser.PackageClauseOnly)
	if err != nil {
		return fmt.Errorf("parse package declaration: %w", err)
	}
	if parsed.Name.Name != "main" {
		return fmt.Errorf("package is %q, want main", parsed.Name.Name)
	}
	return nil
}

func commandFailure(name string, output []byte, err error) error {
	return errors.New(commandFailureText(name, output, err))
}

func commandFailureText(name string, output []byte, err error) string {
	detail := strings.TrimSpace(string(output))
	if detail == "" {
		return fmt.Sprintf("%s failed: %v", name, err)
	}
	return fmt.Sprintf("%s failed: %v (%s)", name, err, detail)
}
