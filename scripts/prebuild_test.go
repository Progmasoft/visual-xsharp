// SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
// SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.1

package main

import (
	"errors"
	"reflect"
	"strings"
	"testing"
)

type bootstrapFakeRunner struct {
	paths       map[string]string
	invocations [][]string
	javaDetails string
}

func (runner *bootstrapFakeRunner) Run(name string, arguments ...string) error {
	runner.invocations = append(runner.invocations, append([]string{name}, arguments...))
	return nil
}

func (runner *bootstrapFakeRunner) Output(name string, arguments ...string) (string, error) {
	if name == "java" && runner.javaDetails != "" {
		return runner.javaDetails, nil
	}
	return "", errors.New("not available")
}

func (runner *bootstrapFakeRunner) LookPath(name string) (string, error) {
	if path, ok := runner.paths[name]; ok {
		return path, nil
	}
	return "", errors.New("not found")
}

func TestDetectBootstrapHostAcceptsOnlyOfficialFamilies(t *testing.T) {
	for goos, want := range map[string]bootstrapHost{"windows": bootstrapWindows, "darwin": bootstrapMacOS} {
		got, err := detectBootstrapHost(goos)
		if err != nil {
			t.Fatal(err)
		}
		if got != want {
			t.Fatalf("host for %s = %v, want %v", goos, got, want)
		}
	}
	if _, err := detectBootstrapHost("linux"); err == nil {
		t.Fatal("Linux unexpectedly became an official bootstrap host")
	}
}

func TestLLVMRequirementIsHostSpecific(t *testing.T) {
	llvm := toolRequirement{name: "LLVM", executables: []string{"llvm-config"}}
	windows := requirementExecutables(bootstrapWindows, llvm)
	if !reflect.DeepEqual(windows, []string{"llvm-config", "clang-cl", "lld-link"}) {
		t.Fatalf("Windows LLVM tools = %#v", windows)
	}
	macOS := requirementExecutables(bootstrapMacOS, llvm)
	if !reflect.DeepEqual(macOS, []string{"llvm-config", "clang++"}) {
		t.Fatalf("macOS LLVM tools = %#v", macOS)
	}
}

func TestMissingToolsRequiresCompleteLLVMAndTemurinVendor(t *testing.T) {
	runner := &bootstrapFakeRunner{
		paths: map[string]string{
			"go": "go", "git": "git", "bazelisk": "bazelisk", "llvm-config": "llvm-config",
			"clang-cl": "clang-cl", "ghcup": "ghcup", "java": "java",
		},
		javaDetails: "java.vendor = Eclipse Adoptium\njava.version = 25.0.4",
	}
	missing := missingTools(bootstrapWindows, runner)
	if len(missing) != 1 || missing[0].name != "LLVM" {
		t.Fatalf("missing groups = %#v", missing)
	}
	runner.paths["lld-link"] = "lld-link"
	runner.javaDetails = "java.vendor = Another Vendor\njava.version = 25.0.4"
	missing = missingTools(bootstrapWindows, runner)
	if len(missing) != 1 || missing[0].name != "Temurin JDK 25" {
		t.Fatalf("non-Temurin JDK was accepted: %#v", missing)
	}
}

func TestWingetInstallUsesExactNonInteractivePackageIdentity(t *testing.T) {
	runner := &bootstrapFakeRunner{}
	if err := installWingetPackage(runner, "winget.exe", "LLVM.LLVM"); err != nil {
		t.Fatal(err)
	}
	want := []string{
		"winget.exe", "install", "--id", "LLVM.LLVM", "--exact", "--source", "winget",
		"--accept-package-agreements", "--accept-source-agreements", "--silent", "--disable-interactivity",
	}
	if !reflect.DeepEqual(runner.invocations, [][]string{want}) {
		t.Fatalf("winget invocation = %#v", runner.invocations)
	}
}

func TestWindowsGHCupUsesOfficialHTTPSBootstrap(t *testing.T) {
	runner := &bootstrapFakeRunner{}
	if err := installGHCupWindows(runner); err != nil {
		t.Fatal(err)
	}
	if len(runner.invocations) != 1 {
		t.Fatalf("GHCup invocations = %#v", runner.invocations)
	}
	invocation := strings.Join(runner.invocations[0], " ")
	if !strings.Contains(invocation, "https://www.haskell.org/ghcup/sh/bootstrap-haskell.ps1") ||
		!strings.Contains(invocation, "-Minimal -InBash") {
		t.Fatalf("unexpected GHCup bootstrap: %s", invocation)
	}
}

func TestWindowsLinkResourcesUseCurrentWingetBuildToolsIdentity(t *testing.T) {
	runner := &bootstrapFakeRunner{}
	if err := installWindowsLinkResources(runner, "winget.exe"); err != nil {
		t.Fatal(err)
	}
	if len(runner.invocations) != 1 {
		t.Fatalf("invocation count = %d, want 1", len(runner.invocations))
	}
	invocation := strings.Join(runner.invocations[0], " ")
	if !strings.Contains(invocation, "Microsoft.VisualStudio.2022.BuildTools") ||
		!strings.Contains(invocation, "Microsoft.VisualStudio.Workload.VCTools") {
		t.Fatalf("link-resource invocation = %q", invocation)
	}
}

func TestHomebrewUsesCaskOnlyForTemurin(t *testing.T) {
	temurin := toolRequirement{name: "Temurin JDK 25", homebrewFormula: "temurin@25"}
	if got := homebrewInstallArguments(temurin); !reflect.DeepEqual(got, []string{"install", "--cask", "temurin@25"}) {
		t.Fatalf("Temurin arguments = %#v", got)
	}
	llvm := toolRequirement{name: "LLVM", homebrewFormula: "llvm"}
	if got := homebrewInstallArguments(llvm); !reflect.DeepEqual(got, []string{"install", "llvm"}) {
		t.Fatalf("LLVM arguments = %#v", got)
	}
}
