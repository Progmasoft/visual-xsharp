// SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
// SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.1

package development

import (
	"bufio"
	"errors"
	"fmt"
	"os"
	"path/filepath"
	"strconv"
	"strings"
)

func readProjectVersion(repository string) (string, error) {
	contents, err := os.ReadFile(filepath.Join(repository, "MODULE.bazel"))
	if err != nil {
		return "", fmt.Errorf("cannot read MODULE.bazel: %w", err)
	}
	return parseModuleVersion(string(contents))
}

func checkReleaseMetadata(repository string, currentHost host, requested string, runner commandRunner) error {
	if err := validateReleaseVersion(requested); err != nil {
		return err
	}
	checks := make([]releaseCheck, 0, 5)
	moduleVersion, err := readProjectVersion(repository)
	checks = append(checks, releaseCheck{
		name:   "Bazel module version",
		ok:     err == nil && moduleVersion == requested,
		detail: exactVersionDetail(moduleVersion, requested, err),
	})
	checks = append(checks,
		checkFileLineOnce(filepath.Join(repository, "CHANGELOG.md"), "## "+requested+" - ", true, "CHANGELOG heading"),
		checkFileLineOnce(filepath.Join(repository, "Compiler", "Haskell", "Syntax", "visual-xsharp-syntax.cabal"), "version: "+requested, false, "Haskell syntax version"),
		checkFileLineOnce(filepath.Join(repository, "Compiler", "Haskell", "Frontend", "visual-xsharp-frontend.cabal"), "version: "+requested, false, "Haskell frontend version"),
		checkFileLineOnce(filepath.Join(repository, "Compiler", "Haskell", "Core", "visual-xsharp-core.cabal"), "version: "+requested, false, "Haskell Core version"),
		checkFileLineOnce(filepath.Join(repository, "Compiler", "Haskell", "Driver", "visual-xsharp-compiler.cabal"), "version: "+requested, false, "Haskell compiler version"),
		checkFileLineOnce(filepath.Join(repository, "Compiler", "Cli", "Arguments", "Options.cpp"), "#    define VXS_PROJECT_VERSION \""+requested+"\"", false, "native CLI fallback version"),
		checkFileLineOnce(filepath.Join(repository, "ProjectSystem", "build.gradle.kts"), "version = \""+requested+"\"", false, "Kotlin project runtime version"),
		checkFileLineOnce(filepath.Join(repository, "ProjectSystem", "Visual.XSharp.kts"), "version = \""+requested+"\"", false, "default compiler model version"),
	)

	compiler := filepath.Join(repository, "bazel-bin", "Compiler", "Cli", "vxs"+currentHost.executable)
	if information, statErr := os.Stat(compiler); statErr == nil && !information.IsDir() {
		output, outputErr := runner.OutputIn(repository, compiler, "version")
		want := "vxs " + requested
		checks = append(checks, releaseCheck{
			name:   "vxs version",
			ok:     outputErr == nil && output == want,
			detail: exactVersionDetail(output, want, outputErr),
		})
	} else {
		checks = append(checks, releaseCheck{
			name:   "vxs version",
			ok:     true,
			detail: "Bazel vxs executable is not built; runtime version check skipped",
		})
	}

	allValid := true
	for _, check := range checks {
		status := "ok"
		if !check.ok {
			status = "error"
			allValid = false
		}
		fmt.Printf("%s: %s - %s\n", status, check.name, check.detail)
	}
	if !allValid {
		return fmt.Errorf("release metadata does not consistently describe version %s", requested)
	}
	return nil
}

// validateReleaseVersion accepts the usual three-part release and the
// repository's optional fourth patch-revision component (for example 0.3.9.5).
// That four-part form is intentional release notation, not strict SemVer 2.0.
func validateReleaseVersion(version string) error {
	parts := strings.Split(version, ".")
	if len(parts) != 3 && len(parts) != 4 {
		return fmt.Errorf("version %q is not major.minor.patch[.revision]", version)
	}
	for _, part := range parts {
		if part == "" {
			return fmt.Errorf("version %q is not major.minor.patch[.revision]", version)
		}
		if len(part) > 1 && part[0] == '0' {
			return fmt.Errorf("version %q contains a leading zero", version)
		}
		if _, err := strconv.ParseUint(part, 10, 32); err != nil {
			return fmt.Errorf("version %q is not major.minor.patch[.revision]", version)
		}
	}
	return nil
}

func exactVersionDetail(actual string, expected string, err error) string {
	if err != nil {
		return err.Error()
	}
	if actual != expected {
		return fmt.Sprintf("expected %q, got %q", expected, actual)
	}
	return actual
}

func checkFileLineOnce(path string, expected string, prefix bool, name string) releaseCheck {
	contents, err := os.ReadFile(path)
	if err != nil {
		return releaseCheck{name: name, detail: err.Error()}
	}
	count := 0
	scanner := bufio.NewScanner(strings.NewReader(string(contents)))
	for scanner.Scan() {
		line := strings.TrimSpace(scanner.Text())
		matches := line == expected
		if prefix {
			matches = strings.HasPrefix(line, expected)
		}
		if matches {
			count++
		}
	}
	if err := scanner.Err(); err != nil {
		return releaseCheck{name: name, detail: err.Error()}
	}
	if count != 1 {
		return releaseCheck{
			name:   name,
			detail: fmt.Sprintf("%s should contain exactly one %q line; found %d", path, expected, count),
		}
	}
	return releaseCheck{name: name, ok: true, detail: path + " contains one expected line"}
}

func parseModuleVersion(contents string) (string, error) {
	scanner := bufio.NewScanner(strings.NewReader(contents))
	inModule := false
	version := ""
	for scanner.Scan() {
		line := strings.TrimSpace(scanner.Text())
		if !inModule {
			if line == "module(" {
				inModule = true
			}
			continue
		}
		if line == ")" {
			break
		}
		if !strings.HasPrefix(line, "version") {
			continue
		}
		key, value, found := strings.Cut(line, "=")
		if !found || strings.TrimSpace(key) != "version" || version != "" {
			return "", errors.New("MODULE.bazel contains an ambiguous module version")
		}
		value = strings.TrimSuffix(strings.TrimSpace(value), ",")
		unquoted, err := strconv.Unquote(value)
		if err != nil {
			return "", fmt.Errorf("MODULE.bazel has an invalid module version literal: %w", err)
		}
		version = unquoted
	}
	if err := scanner.Err(); err != nil {
		return "", fmt.Errorf("cannot scan MODULE.bazel: %w", err)
	}
	if version == "" {
		return "", errors.New("MODULE.bazel does not declare a module version")
	}
	if err := validateReleaseVersion(version); err != nil {
		return "", fmt.Errorf("invalid module %w", err)
	}
	return version, nil
}
