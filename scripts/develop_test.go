// SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
// SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.1

package main

import (
	"errors"
	"os"
	"path/filepath"
	"reflect"
	"strings"
	"testing"
)

func TestSplitArgumentsKeepsCommandAndBazelSurfacesSeparate(t *testing.T) {
	command, bazel, err := splitArguments([]string{"address", "--", "--jobs=4", "--nocache_test_results"})
	if err != nil {
		t.Fatal(err)
	}
	if !reflect.DeepEqual(command, []string{"address"}) {
		t.Fatalf("command arguments = %#v", command)
	}
	if !reflect.DeepEqual(bazel, []string{"--jobs=4", "--nocache_test_results"}) {
		t.Fatalf("Bazel arguments = %#v", bazel)
	}
}

func TestSplitArgumentsRejectsPrivateConfigurationEscape(t *testing.T) {
	for _, value := range []string{"--config", "--config=asan-windows"} {
		_, _, err := splitArguments([]string{"--", value})
		if err == nil {
			t.Fatalf("expected %q to be rejected", value)
		}
	}
}

func TestSelectAddressSanitizerUsesHostSpecificProfile(t *testing.T) {
	windows, err := selectSanitizer(host{kind: hostWindows}, "asan")
	if err != nil {
		t.Fatal(err)
	}
	if windows.config != "asan-windows" || windows.name != "AddressSanitizer" {
		t.Fatalf("unexpected Windows sanitizer: %#v", windows)
	}

	macOS, err := selectSanitizer(host{kind: hostMacOS}, "address")
	if err != nil {
		t.Fatal(err)
	}
	if macOS.config != "asan-macos" || macOS.name != "AddressSanitizer" {
		t.Fatalf("unexpected macOS sanitizer: %#v", macOS)
	}
	if !reflect.DeepEqual(macOS.environment, []string{"ASAN_OPTIONS=halt_on_error=1:strict_string_checks=1"}) {
		t.Fatalf("unexpected macOS AddressSanitizer environment: %#v", macOS.environment)
	}
}

func TestSelectSanitizerExplainsUnsupportedWindowsKinds(t *testing.T) {
	for _, kind := range []string{"undefined", "thread"} {
		_, err := selectSanitizer(host{kind: hostWindows}, kind)
		if err == nil {
			t.Fatalf("expected %s to be rejected on Windows", kind)
		}
	}
}

func TestSelectSanitizerRejectsUnknownName(t *testing.T) {
	_, err := selectSanitizer(host{kind: hostMacOS}, "memory")
	if err == nil {
		t.Fatal("expected an unknown sanitizer diagnostic")
	}
}

func TestHelpSpellingsAreAccepted(t *testing.T) {
	for _, spelling := range []string{"help", "-Help", "--help", "-h"} {
		if !isHelp(spelling) {
			t.Fatalf("expected %q to be a help spelling", spelling)
		}
	}
	if isHelp("doctor") {
		t.Fatal("doctor is not a help spelling")
	}
}

type fakeRunner struct {
	paths map[string]string
}

func (runner fakeRunner) Run(string, []string, string, ...string) error {
	return errors.New("unexpected process execution")
}

func (runner fakeRunner) Output(string, ...string) (string, error) {
	return "", errors.New("not available")
}

func (runner fakeRunner) OutputIn(string, string, ...string) (string, error) {
	return "", errors.New("not available")
}

func (runner fakeRunner) LookPath(name string) (string, error) {
	if path, ok := runner.paths[name]; ok {
		return path, nil
	}
	return "", errors.New("not found")
}

func TestFindBazelPrefersBazelisk(t *testing.T) {
	runner := fakeRunner{paths: map[string]string{
		"bazelisk": "preferred-bazelisk",
		"bazel":    "fallback-bazel",
	}}
	path, err := findBazel(runner)
	if err != nil {
		t.Fatal(err)
	}
	if path != "preferred-bazelisk" {
		t.Fatalf("selected %q", path)
	}
}

func TestFindBazelFallsBackToBazel(t *testing.T) {
	runner := fakeRunner{paths: map[string]string{"bazel": "fallback-bazel"}}
	path, err := findBazel(runner)
	if err != nil {
		t.Fatal(err)
	}
	if path != "fallback-bazel" {
		t.Fatalf("selected %q", path)
	}
}

func TestLocateToolUsesPathBeforeEnvironmentDiscovery(t *testing.T) {
	runner := fakeRunner{paths: map[string]string{"llvm-config": "path-llvm-config"}}
	path, err := locateTool(runner, "llvm-config")
	if err != nil {
		t.Fatal(err)
	}
	if path != "path-llvm-config" {
		t.Fatalf("selected %q", path)
	}
}

func TestParseModuleVersionReadsOnlyTheRootModuleDeclaration(t *testing.T) {
	contents := `module(
    name = "visual_xsharp",
    version = "0.3.6",
)
bazel_dep(name = "fmt", version = "12.1.0")`
	version, err := parseModuleVersion(contents)
	if err != nil {
		t.Fatal(err)
	}
	if version != "0.3.6" {
		t.Fatalf("version = %q", version)
	}
}

func TestParseModuleVersionRejectsMissingAndMalformedVersions(t *testing.T) {
	for _, contents := range []string{
		"module(\n    name = \"visual_xsharp\",\n)",
		"module(\n    version = \"0.3\",\n)",
		"module(\n    version = \"0.next.0\",\n)",
		"module(\n    version = \"0.3.6\",\n    version = \"0.3.7\",\n)",
	} {
		if _, err := parseModuleVersion(contents); err == nil {
			t.Fatalf("expected version input to be rejected:\n%s", contents)
		}
	}
}

func TestValidateSemanticVersionRejectsAmbiguousReleaseSpellings(t *testing.T) {
	for _, version := range []string{"", "0.3", "0.3.6.1", "0.03.6", "v0.3.6", "0.next.6"} {
		if err := validateSemanticVersion(version); err == nil {
			t.Fatalf("expected %q to be rejected", version)
		}
	}
	if err := validateSemanticVersion("0.3.6"); err != nil {
		t.Fatal(err)
	}
}

func TestCheckFileLineOnceDoesNotAcceptLongerVersionPrefix(t *testing.T) {
	path := filepath.Join(t.TempDir(), "metadata.txt")
	contents := "version: 0.3.60\n## 0.3.60 - future\n"
	if err := os.WriteFile(path, []byte(contents), 0o644); err != nil {
		t.Fatal(err)
	}
	if check := checkFileLineOnce(path, "version: 0.3.6", false, "exact"); check.ok {
		t.Fatal("exact release line accepted a longer version")
	}
	if check := checkFileLineOnce(path, "## 0.3.6 - ", true, "heading"); check.ok {
		t.Fatal("release heading accepted a longer version")
	}
}

func TestDistributionPlatformUsesStablePublicNames(t *testing.T) {
	tests := []struct {
		host         host
		architecture string
		want         string
	}{
		{host: host{kind: hostWindows}, architecture: "amd64", want: "windows-x86_64"},
		{host: host{kind: hostWindows}, architecture: "arm64", want: "windows-arm64"},
		{host: host{kind: hostMacOS}, architecture: "amd64", want: "macos-x86_64"},
		{host: host{kind: hostMacOS}, architecture: "arm64", want: "macos-arm64"},
	}
	for _, test := range tests {
		got, err := distributionPlatform(test.host, test.architecture)
		if err != nil {
			t.Fatal(err)
		}
		if got != test.want {
			t.Fatalf("platform = %q, want %q", got, test.want)
		}
	}
	if _, err := distributionPlatform(host{kind: hostWindows}, "386"); err == nil {
		t.Fatal("expected unsupported architecture to be rejected")
	}
}

func TestValidateBundleTargetRejectsTargetsOutsideExactDistributionRoot(t *testing.T) {
	repository := t.TempDir()
	unsafeTargets := []string{
		filepath.Join(repository, "dist"),
		filepath.Join(repository, "dist", "other-product-0.3.6"),
		filepath.Join(repository, "dist", "nested", "visual-xsharp-0.3.6-windows-x86_64"),
		filepath.Join(repository, "visual-xsharp-0.3.6-windows-x86_64"),
	}
	for _, target := range unsafeTargets {
		if err := validateBundleTarget(repository, target); err == nil {
			t.Fatalf("expected unsafe target %q to be rejected", target)
		}
	}

	safe := filepath.Join(repository, "dist", "visual-xsharp-0.3.6-windows-x86_64")
	if err := validateBundleTarget(repository, safe); err != nil {
		t.Fatal(err)
	}
	staging := filepath.Join(repository, "dist", ".staging")
	if err := os.MkdirAll(staging, 0o755); err != nil {
		t.Fatal(err)
	}
	if err := os.WriteFile(filepath.Join(staging, "verified.txt"), []byte("verified"), 0o644); err != nil {
		t.Fatal(err)
	}
	if err := publishBundleDirectory(repository, staging, safe); err != nil {
		t.Fatal(err)
	}
	contents, err := os.ReadFile(filepath.Join(safe, "verified.txt"))
	if err != nil || string(contents) != "verified" {
		t.Fatalf("verified staging content was not published: %q, %v", contents, err)
	}
}

func TestWriteBundleChecksumsIsSortedAndContentSensitive(t *testing.T) {
	bundle := t.TempDir()
	if err := os.MkdirAll(filepath.Join(bundle, "LICENSES"), 0o755); err != nil {
		t.Fatal(err)
	}
	if err := os.WriteFile(filepath.Join(bundle, "vxs.exe"), []byte("compiler"), 0o755); err != nil {
		t.Fatal(err)
	}
	license := filepath.Join(bundle, "LICENSES", "exception.txt")
	if err := os.WriteFile(license, []byte("license"), 0o644); err != nil {
		t.Fatal(err)
	}
	files := []string{"vxs.exe", "LICENSES/exception.txt"}
	if err := writeBundleChecksums(bundle, files); err != nil {
		t.Fatal(err)
	}
	first, err := os.ReadFile(filepath.Join(bundle, "SHA256SUMS"))
	if err != nil {
		t.Fatal(err)
	}
	lines := strings.Split(strings.TrimSpace(string(first)), "\n")
	if len(lines) != 2 || !strings.HasSuffix(lines[0], "  LICENSES/exception.txt") || !strings.HasSuffix(lines[1], "  vxs.exe") {
		t.Fatalf("checksums are not deterministically sorted: %q", string(first))
	}
	if err := os.WriteFile(license, []byte("changed license"), 0o644); err != nil {
		t.Fatal(err)
	}
	if err := writeBundleChecksums(bundle, files); err != nil {
		t.Fatal(err)
	}
	second, err := os.ReadFile(filepath.Join(bundle, "SHA256SUMS"))
	if err != nil {
		t.Fatal(err)
	}
	if string(first) == string(second) {
		t.Fatal("checksum output did not change with file contents")
	}
}

func TestCleanGeneratedPathsPreservesUnlistedDistribution(t *testing.T) {
	repository := t.TempDir()
	generated := filepath.Join(repository, "ProjectSystem", "build", "nested")
	if err := os.MkdirAll(generated, 0o755); err != nil {
		t.Fatal(err)
	}
	bundle := filepath.Join(repository, "dist", "visual-xsharp-0.3.6-windows-x86_64")
	if err := os.MkdirAll(bundle, 0o755); err != nil {
		t.Fatal(err)
	}
	if err := cleanGeneratedPaths(repository, []string{"ProjectSystem/build"}); err != nil {
		t.Fatal(err)
	}
	if _, err := os.Stat(filepath.Join(repository, "ProjectSystem", "build")); !os.IsNotExist(err) {
		t.Fatalf("generated build directory survived cleanup: %v", err)
	}
	if information, err := os.Stat(bundle); err != nil || !information.IsDir() {
		t.Fatalf("unlisted distribution was removed: %v", err)
	}
}

func TestCleanGeneratedPathsRejectsRepositoryEscape(t *testing.T) {
	repository := t.TempDir()
	if err := cleanGeneratedPaths(repository, []string{"../outside"}); err == nil {
		t.Fatal("cleanup accepted a path outside the repository")
	}
}
