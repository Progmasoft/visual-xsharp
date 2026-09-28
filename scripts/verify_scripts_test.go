// SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
// SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.1

package main

import (
	"bytes"
	"errors"
	"os"
	"path/filepath"
	"strings"
	"testing"
)

func TestDiscoverGoScriptPairsReturnsSortedCommands(t *testing.T) {
	directory := makeGoScriptFixture(t, "zeta", "alpha")

	pairs, err := discoverGoScriptPairs(directory, 1500)
	if err != nil {
		t.Fatalf("discoverGoScriptPairs() returned error: %v", err)
	}
	if len(pairs) != 2 || pairs[0].name != "alpha" || pairs[1].name != "zeta" {
		t.Fatalf("discoverGoScriptPairs() = %#v, want alpha then zeta", pairs)
	}
	if pairs[0].sourcePath != filepath.Join("scripts", "alpha.go") || pairs[0].testPath != filepath.Join("scripts", "alpha_test.go") {
		t.Fatalf("paired paths = %#v, want paths rooted under scripts/", pairs[0])
	}
}

func TestDiscoverGoScriptPairsReportsMissingAndOrphanTests(t *testing.T) {
	directory := t.TempDir()
	writeGoFixtureFile(t, directory, "missing_test.go", goFixtureTest)
	writeGoFixtureFile(t, directory, "orphan_test.go", goFixtureTest)

	_, err := discoverGoScriptPairs(directory, 1500)
	if err == nil {
		t.Fatal("discoverGoScriptPairs() accepted an untested command and an orphan test")
	}
	for _, expected := range []string{
		"missing_test.go has no paired scripts/missing.go",
		"orphan_test.go has no paired scripts/orphan.go",
	} {
		if !strings.Contains(err.Error(), expected) {
			t.Errorf("error %q does not contain %q", err, expected)
		}
	}
}

func TestDiscoverGoScriptPairsEnforcesLineLimitLicenseAndPackage(t *testing.T) {
	directory := t.TempDir()
	writeGoFixtureFile(t, directory, "long.go", goFixtureSource+strings.Repeat("// extra\n", 3))
	writeGoFixtureFile(t, directory, "long_test.go", goFixtureTest)
	writeGoFixtureFile(t, directory, "header.go", "package main\n")
	writeGoFixtureFile(t, directory, "header_test.go", goFixtureTest)
	writeGoFixtureFile(t, directory, "package.go", strings.Replace(goFixtureSource, "package main", "package other", 1))
	writeGoFixtureFile(t, directory, "package_test.go", goFixtureTest)

	_, err := discoverGoScriptPairs(directory, 5)
	if err == nil {
		t.Fatal("discoverGoScriptPairs() accepted oversized, unlicensed, or non-main files")
	}
	for _, expected := range []string{
		"long.go has 9 lines; the maximum is 5",
		"header.go: file must begin with the repository's Progmasoft SPDX copyright and license lines",
		`package.go: package is "other", want main`,
	} {
		if !strings.Contains(err.Error(), expected) {
			t.Errorf("error %q does not contain %q", err, expected)
		}
	}
}

func TestRunScriptQualityExecutesFormatVetAndTest(t *testing.T) {
	root := t.TempDir()
	scriptsDirectory := filepath.Join(root, "scripts")
	if err := os.MkdirAll(scriptsDirectory, 0o700); err != nil {
		t.Fatal(err)
	}
	writeGoFixtureFile(t, scriptsDirectory, "alpha.go", goFixtureSource)
	writeGoFixtureFile(t, scriptsDirectory, "alpha_test.go", goFixtureTest)

	runner := &fakeGoQualityRunner{}
	var output bytes.Buffer
	var errorOutput bytes.Buffer
	if err := runScriptQuality([]string{"-Root", root}, &output, &errorOutput, runner); err != nil {
		t.Fatalf("runScriptQuality() returned error: %v (%s)", err, errorOutput.String())
	}
	if len(runner.calls) != 3 {
		t.Fatalf("quality gates made %d calls, want gofmt, go vet, and go test", len(runner.calls))
	}
	if runner.calls[0].command != "gofmt" || runner.calls[1].command != "go" || runner.calls[1].arguments[0] != "vet" || runner.calls[2].arguments[0] != "test" {
		t.Fatalf("quality-gate order = %#v, want gofmt then go vet then go test", runner.calls)
	}
	if !strings.Contains(output.String(), "Verified 1 Go commands and their unit tests.") {
		t.Fatalf("successful run summary missing: %q", output.String())
	}
}

func TestRunScriptQualitySurfacesVetAndTestDiagnostics(t *testing.T) {
	root := t.TempDir()
	scriptsDirectory := filepath.Join(root, "scripts")
	if err := os.MkdirAll(scriptsDirectory, 0o700); err != nil {
		t.Fatal(err)
	}
	writeGoFixtureFile(t, scriptsDirectory, "alpha.go", goFixtureSource)
	writeGoFixtureFile(t, scriptsDirectory, "alpha_test.go", goFixtureTest)
	runner := &fakeGoQualityRunner{failCommand: "test", failureOutput: []byte("assertion failed")}

	err := runScriptQuality([]string{"-Root", root}, &bytes.Buffer{}, &bytes.Buffer{}, runner)
	if err == nil || !strings.Contains(err.Error(), "go test alpha failed") || !strings.Contains(err.Error(), "assertion failed") {
		t.Fatalf("runScriptQuality() error = %v, want test diagnostics", err)
	}
}

func TestRunScriptQualityRejectsUnformattedSources(t *testing.T) {
	root := t.TempDir()
	scriptsDirectory := filepath.Join(root, "scripts")
	if err := os.MkdirAll(scriptsDirectory, 0o700); err != nil {
		t.Fatal(err)
	}
	writeGoFixtureFile(t, scriptsDirectory, "alpha.go", goFixtureSource)
	writeGoFixtureFile(t, scriptsDirectory, "alpha_test.go", goFixtureTest)
	runner := &fakeGoQualityRunner{formatOutput: []byte("scripts/alpha.go")}

	err := runScriptQuality([]string{"-Root", root}, &bytes.Buffer{}, &bytes.Buffer{}, runner)
	if err == nil || !strings.Contains(err.Error(), "files need formatting") {
		t.Fatalf("runScriptQuality() error = %v, want gofmt diagnostic", err)
	}
	if len(runner.calls) != 1 {
		t.Fatalf("unformatted files should fail before vet/test; got calls %#v", runner.calls)
	}
}

func TestRunScriptQualityHelpDoesNotInvokeTools(t *testing.T) {
	runner := &fakeGoQualityRunner{}
	var output bytes.Buffer
	if err := runScriptQuality([]string{"-Help"}, &output, &bytes.Buffer{}, runner); err != nil {
		t.Fatalf("help returned error: %v", err)
	}
	if len(runner.calls) != 0 || !strings.Contains(output.String(), "go vet") {
		t.Fatalf("help output/calls = %q / %d, want usage without tool execution", output.String(), len(runner.calls))
	}
}

const goFixtureSource = `// SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
// SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.1

package main

func value() int { return 1 }
`

func TestInternalPackagesParticipateInQualityGates(t *testing.T) {
	root := t.TempDir()
	directory := filepath.Join(root, "scripts", "internal", "development")
	if err := os.MkdirAll(directory, 0o700); err != nil {
		t.Fatal(err)
	}
	writeGoFixtureFile(t, filepath.Join(root, "scripts"), "alpha.go", goFixtureSource)
	writeGoFixtureFile(t, filepath.Join(root, "scripts"), "alpha_test.go", goFixtureTest)
	writeGoFixtureFile(t, directory, "commands.go", strings.Replace(goFixtureSource, "package main", "package development", 1))
	writeGoFixtureFile(t, directory, "commands_test.go", strings.Replace(goFixtureTest, "package main", "package development", 1))
	runner := &fakeGoQualityRunner{}
	if err := runScriptQuality([]string{"-Root", root}, &bytes.Buffer{}, &bytes.Buffer{}, runner); err != nil {
		t.Fatal(err)
	}
	if len(runner.calls) != 5 || runner.calls[3].arguments[1] != "./scripts/internal/development" || runner.calls[4].arguments[0] != "test" {
		t.Fatalf("internal package was not vetted and tested: %#v", runner.calls)
	}
	if err := os.Remove(filepath.Join(directory, "commands_test.go")); err != nil {
		t.Fatal(err)
	}
	if _, _, err := discoverInternalGoPackages(root); err == nil || !strings.Contains(err.Error(), "no unit tests") {
		t.Fatalf("untested internal package accepted: %v", err)
	}
}

const goFixtureTest = `// SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
// SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.1

package main

import "testing"

func TestValue(t *testing.T) { if value() != 1 { t.Fatal("wrong value") } }
`

type fakeGoQualityCall struct {
	root      string
	command   string
	arguments []string
}

type fakeGoQualityRunner struct {
	calls         []fakeGoQualityCall
	failCommand   string
	formatOutput  []byte
	failureOutput []byte
}

func (runner *fakeGoQualityRunner) Run(root string, command string, arguments ...string) ([]byte, error) {
	runner.calls = append(runner.calls, fakeGoQualityCall{
		root: root, command: command, arguments: append([]string(nil), arguments...),
	})
	if command == runner.failCommand || (command == "go" && len(arguments) > 0 && arguments[0] == runner.failCommand) {
		return runner.failureOutput, errors.New("command failed")
	}
	if command == "gofmt" {
		return runner.formatOutput, nil
	}
	return nil, nil
}

func makeGoScriptFixture(t *testing.T, names ...string) string {
	t.Helper()
	directory := t.TempDir()
	for _, name := range names {
		writeGoFixtureFile(t, directory, name+".go", goFixtureSource)
		writeGoFixtureFile(t, directory, name+"_test.go", goFixtureTest)
	}
	return directory
}

func writeGoFixtureFile(t *testing.T, directory string, name string, contents string) {
	t.Helper()
	if err := os.WriteFile(filepath.Join(directory, name), []byte(contents), 0o600); err != nil {
		t.Fatalf("write Go fixture %s: %v", name, err)
	}
}
