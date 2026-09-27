// SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
// SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.1

// optional_packages installs or checks optional native toolchains used for
// examples and comparative benchmarks. It deliberately does not change the
// repository's core compiler requirements.
package main

import (
	"errors"
	"fmt"
	"os"
	"os/exec"
	"path/filepath"
	"regexp"
	"runtime"
	"strings"
)

const optionalPackagesUsage = `Optional Visual X# toolchains

Usage:
  go run scripts/optional_packages.go install
  go run scripts/optional_packages.go check

Commands:
  install  Install the .NET 10 SDK and GNU Fortran, plus rustup components.
  check    Check the .NET SDK, GNU Fortran, rustup, rustc, and rust-std.

Windows uses WinGet defaults. macOS uses Homebrew. Package managers choose
their recommended installation scope. Rustup may manage components only for
an already-installed toolchain; this command never installs a toolchain.`

type optionalPackage struct {
	name                string
	executable          string
	wingetID            string
	homebrewFormula     string
	versionArgs         []string
	versionPattern      *regexp.Regexp
	versionNote         string
	requiredExecutables []string
	requiredComponents  []*regexp.Regexp
	homebrewCask        bool
}

var optionalPackages = []optionalPackage{
	{
		name: "Microsoft .NET 10 SDK", executable: "dotnet",
		wingetID: "Microsoft.DotNet.SDK.10", homebrewFormula: "dotnet@10",
		versionArgs:    []string{"--list-sdks"},
		versionPattern: regexp.MustCompile(`(?m)^10\.[0-9]+\.[0-9]+`),
		versionNote:    "SDK major version 10",
	},
	{
		name: "GNU Fortran", executable: "gfortran",
		wingetID:        "BrechtSanders.WinLibs.POSIX.UCRT",
		homebrewFormula: "gcc",
		versionArgs:     []string{"--version"},
		versionPattern:  regexp.MustCompile(`(?i)GNU Fortran.*(14\.[1-9]|14\.[1-9][0-9]+|1[5-9]\.[0-9]+|[2-9][0-9]\.[0-9]+)`),
		versionNote:     "GNU Fortran 14.1+ with Fortran 2023 mode",
	},
	{
		name: "rustup, rustc, and rust-std", executable: "rustup",
		wingetID: "Rustlang.Rustup", homebrewFormula: "rustup",
		versionArgs:         []string{"--version"},
		versionPattern:      regexp.MustCompile(`(?i)^rustup\s+[0-9]+\.[0-9]+\.[0-9]+`),
		versionNote:         "rustup manager with rustc and host rust-std installed on the active toolchain",
		requiredExecutables: []string{"rustc"},
		requiredComponents: []*regexp.Regexp{
			regexp.MustCompile(`(?m)^rustc(?:-[A-Za-z0-9_-]+)?$`),
			regexp.MustCompile(`(?m)^rust-std(?:-[A-Za-z0-9_-]+)?$`),
		},
	},
}

type packageRunner interface {
	lookPaths(string) []string
	output(string, ...string) (string, error)
	run(string, ...string) error
}

type systemPackageRunner struct{}

func (systemPackageRunner) lookPaths(name string) []string {
	paths := make([]string, 0, 4)
	seen := make(map[string]struct{})
	appendPath := func(path string) {
		key := strings.ToLower(filepath.Clean(path))
		if _, exists := seen[key]; !exists {
			seen[key] = struct{}{}
			paths = append(paths, path)
		}
	}
	if path, err := exec.LookPath(name); err == nil {
		appendPath(path)
	}
	if runtime.GOOS == "windows" && (name == "rustup" || name == "rustc") {
		if home, err := os.UserHomeDir(); err == nil {
			appendPath(filepath.Join(home, ".cargo", "bin", name+".exe"))
		}
	}
	if runtime.GOOS == "darwin" && name == "rustup" {
		if brew, err := exec.LookPath("brew"); err == nil {
			prefix, outputErr := exec.Command(brew, "--prefix", "rustup").Output()
			if outputErr == nil {
				candidate := filepath.Join(strings.TrimSpace(string(prefix)), "bin", "rustup")
				if information, statErr := os.Stat(candidate); statErr == nil && !information.IsDir() {
					appendPath(candidate)
				}
			}
		}
	}

	// Search every PATH directory instead of trusting LookPath's first hit.
	// Older system compilers can precede newer package-manager installs.
	extensions := []string{""}
	if runtime.GOOS == "windows" && filepath.Ext(name) == "" {
		extensions = extensions[:0]
		for _, extension := range strings.Split(os.Getenv("PATHEXT"), ";") {
			if extension != "" {
				extensions = append(extensions, strings.ToLower(extension))
			}
		}
		if len(extensions) == 0 {
			extensions = []string{".exe", ".cmd", ".bat"}
		}
	}
	for _, directory := range filepath.SplitList(os.Getenv("PATH")) {
		if strings.TrimSpace(directory) == "" {
			continue
		}
		for _, extension := range extensions {
			candidate := filepath.Join(directory, name+extension)
			info, err := os.Stat(candidate)
			if err == nil && !info.IsDir() {
				appendPath(candidate)
			}
		}
	}
	return paths
}

func (systemPackageRunner) output(name string, args ...string) (string, error) {
	command := exec.Command(name, args...)
	output, err := command.CombinedOutput()
	return strings.TrimSpace(string(output)), err
}

func (systemPackageRunner) run(name string, args ...string) error {
	command := exec.Command(name, args...)
	command.Stdin = os.Stdin
	command.Stdout = os.Stdout
	command.Stderr = os.Stderr
	return command.Run()
}

func main() {
	if err := runOptionalPackages(os.Args[1:], systemPackageRunner{}); err != nil {
		fmt.Fprintf(os.Stderr, "error: %v\n", err)
		os.Exit(1)
	}
}

func runOptionalPackages(args []string, runner packageRunner) error {
	if len(args) != 1 {
		fmt.Fprintln(os.Stderr, optionalPackagesUsage)
		return errors.New("expected exactly one command: install or check")
	}

	switch args[0] {
	case "help", "-Help", "--help":
		fmt.Println(optionalPackagesUsage)
		return nil
	case "check":
		return checkOptionalPackages(runner)
	case "install":
		return installOptionalPackages(runner)
	default:
		fmt.Fprintln(os.Stderr, optionalPackagesUsage)
		return fmt.Errorf("unknown command %q", args[0])
	}
}

func checkOptionalPackages(runner packageRunner) error {
	missing := make([]string, 0, len(optionalPackages))
	for _, item := range optionalPackages {
		path, version, ready := findReadyTool(runner, item)
		if !ready && len(runner.lookPaths(item.executable)) == 0 {
			location := item.executable + " not found on PATH"
			fmt.Printf("MISSING  %s (%s)\n", item.name, location)
			missing = append(missing, item.name)
			continue
		}
		if !ready {
			fmt.Printf("MISSING  %s (found %s, but %s was not detected)\n", item.name, path, item.versionNote)
			missing = append(missing, item.name)
			continue
		}
		fmt.Printf("READY    %s: %s [%s]\n", item.name, firstMatchingLine(version, item.versionPattern), path)
	}
	if len(missing) != 0 {
		return fmt.Errorf("%d optional toolchain(s) missing; run `go run scripts/optional_packages.go install`", len(missing))
	}
	return nil
}

func findReadyTool(runner packageRunner, item optionalPackage) (string, string, bool) {
	paths := runner.lookPaths(item.executable)
	for _, path := range paths {
		if !requiredToolsPresent(runner, item) {
			continue
		}
		if len(item.requiredComponents) != 0 {
			components, err := runner.output(path, "component", "list", "--installed")
			if err != nil || !allPatternsMatch(components, item.requiredComponents) {
				continue
			}
		}
		version, err := runner.output(path, item.versionArgs...)
		if err == nil && item.versionPattern.MatchString(version) {
			return path, version, true
		}
	}
	if len(paths) != 0 {
		return paths[0], "", false
	}
	return "", "", false
}

func requiredToolsPresent(runner packageRunner, item optionalPackage) bool {
	for _, executable := range item.requiredExecutables {
		if len(runner.lookPaths(executable)) == 0 {
			return false
		}
	}
	return true
}

func allPatternsMatch(value string, patterns []*regexp.Regexp) bool {
	for _, pattern := range patterns {
		if !pattern.MatchString(value) {
			return false
		}
	}
	return true
}

func firstMatchingLine(output string, pattern *regexp.Regexp) string {
	for _, line := range strings.Split(output, "\n") {
		if pattern.MatchString(line) {
			return strings.TrimSpace(line)
		}
	}
	return "version available"
}

func installOptionalPackages(runner packageRunner) error {
	if runtime.GOOS != "windows" && runtime.GOOS != "darwin" {
		return fmt.Errorf("unsupported operating system %q; supported hosts are Windows and macOS", runtime.GOOS)
	}

	var failures []string
	for _, item := range optionalPackages {
		if _, _, ready := findReadyTool(runner, item); ready {
			fmt.Printf("SKIP     %s already available\n", item.name)
			continue
		}
		if len(item.requiredComponents) != 0 {
			if err := installRustComponents(runner, item); err != nil {
				failures = append(failures, fmt.Sprintf("%s: %v", item.name, err))
			} else {
				fmt.Printf("INSTALLED %s components on the selected toolchain\n", item.name)
			}
			continue
		}
		if installed := packageAlreadyInstalled(runner, item, runtime.GOOS); installed {
			fmt.Printf("SKIP     %s package is already installed; leaving it unchanged\n", item.name)
			continue
		}
		if err := installOne(runner, item); err != nil {
			failures = append(failures, fmt.Sprintf("%s: %v", item.name, err))
		} else {
			fmt.Printf("INSTALLED %s\n", item.name)
		}
	}
	if len(failures) != 0 {
		return fmt.Errorf("some installations failed:\n- %s", strings.Join(failures, "\n- "))
	}
	fmt.Println("Installation commands completed. Open a new terminal, then run `go run scripts/optional_packages.go check`.")
	return nil
}

func packageAlreadyInstalled(runner packageRunner, item optionalPackage, goos string) bool {
	switch goos {
	case "windows":
		if item.wingetID == "" || len(runner.lookPaths("winget")) == 0 {
			return false
		}
		listing, err := runner.output("winget", "list", "--id", item.wingetID, "--exact", "--source", "winget", "--disable-interactivity")
		return err == nil && strings.Contains(strings.ToLower(listing), strings.ToLower(item.wingetID))
	case "darwin":
		if item.homebrewFormula == "" || len(runner.lookPaths("brew")) == 0 {
			return false
		}
		arguments := []string{"list", "--formula", item.homebrewFormula}
		if item.homebrewCask {
			arguments[1] = "--cask"
		}
		_, err := runner.output("brew", arguments...)
		return err == nil
	default:
		return false
	}
}

func installRustComponents(runner packageRunner, item optionalPackage) error {
	paths := runner.lookPaths("rustup")
	if len(paths) == 0 {
		if packageAlreadyInstalled(runner, item, runtime.GOOS) {
			return errors.New("the rustup package is already installed but its command is not visible; repair PATH instead of reinstalling it")
		}
		if runtime.GOOS == "windows" {
			if len(runner.lookPaths("winget")) == 0 {
				return errors.New("winget is required to install the rustup manager")
			}
			// The explicit no-toolchain option is essential: installing the
			// manager must not silently choose stable or another channel.
			if err := runner.run("winget", rustupManagerWingetArguments(item.wingetID)...); err != nil {
				return fmt.Errorf("cannot install rustup without selecting a toolchain: %w", err)
			}
		} else {
			if len(runner.lookPaths("brew")) == 0 {
				return errors.New("Homebrew is required to install rustup")
			}
			if err := runner.run("brew", "install", item.homebrewFormula); err != nil {
				return fmt.Errorf("cannot install the rustup manager: %w", err)
			}
		}
		paths = runner.lookPaths("rustup")
		if len(paths) == 0 && runtime.GOOS == "windows" {
			if home, err := os.UserHomeDir(); err == nil {
				candidate := filepath.Join(home, ".cargo", "bin", "rustup.exe")
				if information, statErr := os.Stat(candidate); statErr == nil && !information.IsDir() {
					paths = append(paths, candidate)
				}
			}
		}
		if len(paths) == 0 {
			return errors.New("rustup was installed but is not visible to this process; open a new terminal and rerun install")
		}
	}

	if _, err := runner.output(paths[0], "show", "active-toolchain"); err != nil {
		return errors.New("rustup is installed, but there is no active toolchain; choose or install one yourself, then rerun install (this script never installs toolchains)")
	}
	return runner.run(paths[0], "component", "add", "rustc", "rust-std")
}

func rustupManagerWingetArguments(packageID string) []string {
	return []string{
		"install", "--id", packageID, "--exact", "--source", "winget",
		"--accept-source-agreements", "--accept-package-agreements",
		"--override", "-y --default-toolchain none",
	}
}

func installOne(runner packageRunner, item optionalPackage) error {
	if runtime.GOOS == "windows" {
		if len(runner.lookPaths("winget")) == 0 {
			return errors.New("winget is required on Windows; install App Installer from Microsoft Store, then retry")
		}
		args := []string{
			"install", "--id", item.wingetID, "--exact",
			"--source", "winget", "--accept-source-agreements",
			"--accept-package-agreements",
		}
		return runner.run("winget", args...)
	}
	if len(runner.lookPaths("brew")) == 0 {
		return errors.New("Homebrew is required on macOS; install it for your user, then retry")
	}
	return runner.run("brew", "install", item.homebrewFormula)
}
