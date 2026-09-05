// SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
// SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.0

package main

import (
	"errors"
	"reflect"
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
