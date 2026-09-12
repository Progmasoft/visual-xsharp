// SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
// SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.1

// prebuild installs system tools used to develop Visual X#. Bazel, Cabal, and
// Gradle remain the owners of their build graphs after host bootstrap.
package main

import (
	"errors"
	"fmt"
	"os"
	"os/exec"
	"path/filepath"
	"runtime"
	"strings"
)

const prebuildUsage = `Visual X# development-host bootstrap

Usage:
  go run scripts/prebuild.go check
  go run scripts/prebuild.go install
  go run scripts/prebuild.go help

Commands:
  check    Report missing host tools without changing the machine.
  install  Install missing tools with winget on Windows or Homebrew on macOS.
  help     Show this help.

Supported hosts are Windows 10/11 and macOS Sequoia/Tahoe. The installed
toolchain includes LLVM, Bazelisk, GHCup/GHC/Cabal, Go, Temurin JDK 25, Git, and the
platform link resources. Visual X# uses ClangCL/LLD on Windows, not MSVC.`

type bootstrapHost int

const (
	bootstrapUnsupported bootstrapHost = iota
	bootstrapWindows
	bootstrapMacOS
)

type bootstrapRunner interface {
	Run(name string, arguments ...string) error
	Output(name string, arguments ...string) (string, error)
	LookPath(name string) (string, error)
}

type bootstrapSystemRunner struct{}

func (bootstrapSystemRunner) Run(name string, arguments ...string) error {
	command := exec.Command(name, arguments...)
	command.Stdin = os.Stdin
	command.Stdout = os.Stdout
	command.Stderr = os.Stderr
	return command.Run()
}

func (bootstrapSystemRunner) Output(name string, arguments ...string) (string, error) {
	command := exec.Command(name, arguments...)
	output, err := command.CombinedOutput()
	return strings.TrimSpace(string(output)), err
}

func (bootstrapSystemRunner) LookPath(name string) (string, error) {
	return exec.LookPath(name)
}

type toolRequirement struct {
	name            string
	executables     []string
	wingetPackageID string
	homebrewFormula string
}

var bootstrapTools = []toolRequirement{
	{name: "Go", executables: []string{"go"}, wingetPackageID: "GoLang.Go", homebrewFormula: "go"},
	{name: "Git", executables: []string{"git"}, wingetPackageID: "Git.Git", homebrewFormula: "git"},
	{name: "Bazelisk", executables: []string{"bazelisk"}, wingetPackageID: "Bazel.Bazelisk", homebrewFormula: "bazelisk"},
	{name: "LLVM", executables: []string{"llvm-config"}, wingetPackageID: "LLVM.LLVM", homebrewFormula: "llvm"},
	{name: "GHCup", executables: []string{"ghcup"}, homebrewFormula: "ghcup"},
	{name: "Temurin JDK 25", executables: []string{"java"}, wingetPackageID: "EclipseAdoptium.Temurin.25.JDK", homebrewFormula: "temurin@25"},
}

func main() {
	if err := runPrebuild(os.Args[1:], bootstrapSystemRunner{}); err != nil {
		fmt.Fprintf(os.Stderr, "\nerror: %v\n", err)
		os.Exit(1)
	}
}

func runPrebuild(arguments []string, runner bootstrapRunner) error {
	if len(arguments) == 0 || (len(arguments) == 1 && isPrebuildHelp(arguments[0])) {
		fmt.Println(prebuildUsage)
		return nil
	}
	if len(arguments) != 1 {
		return errors.New("prebuild accepts exactly one command: check or install")
	}
	host, err := detectBootstrapHost(runtime.GOOS)
	if err != nil {
		return err
	}
	switch strings.ToLower(arguments[0]) {
	case "check":
		return reportBootstrapState(host, runner)
	case "install":
		if err := installBootstrapTools(host, runner); err != nil {
			return err
		}
		return reportPostInstallState(host, runner)
	default:
		return fmt.Errorf("unknown prebuild command %q; choose check or install", arguments[0])
	}
}

func isPrebuildHelp(argument string) bool {
	switch strings.ToLower(argument) {
	case "help", "-help", "--help", "-h":
		return true
	default:
		return false
	}
}

func detectBootstrapHost(goos string) (bootstrapHost, error) {
	switch goos {
	case "windows":
		return bootstrapWindows, nil
	case "darwin":
		return bootstrapMacOS, nil
	default:
		return bootstrapUnsupported, fmt.Errorf("%s is not an official Visual X# development host", goos)
	}
}

func missingTools(host bootstrapHost, runner bootstrapRunner) []toolRequirement {
	missing := make([]toolRequirement, 0)
	for _, requirement := range bootstrapTools {
		if requirement.name == "Temurin JDK 25" {
			if !temurinJDK25Available(runner) {
				missing = append(missing, requirement)
			}
			continue
		}
		ready := true
		for _, executable := range requirementExecutables(host, requirement) {
			if !bootstrapToolAvailable(runner, executable) {
				ready = false
				break
			}
		}
		if !ready {
			missing = append(missing, requirement)
		}
	}
	return missing
}

func temurinJDK25Available(runner bootstrapRunner) bool {
	if _, err := runner.LookPath("java"); err != nil {
		return false
	}
	properties, err := runner.Output("java", "-XshowSettings:properties", "-version")
	if err != nil {
		return false
	}
	lower := strings.ToLower(properties)
	return strings.Contains(lower, "java.vendor = eclipse adoptium") &&
		(strings.Contains(lower, "java.version = 25.") || strings.Contains(lower, `openjdk version "25.`))
}

func bootstrapToolAvailable(runner bootstrapRunner, executable string) bool {
	if _, err := runner.LookPath(executable); err == nil {
		return true
	}
	if executable != "llvm-config" {
		return false
	}
	root := os.Getenv("LLVM_ROOT")
	if root == "" {
		return false
	}
	name := executable
	if runtime.GOOS == "windows" {
		name += ".exe"
	}
	information, err := os.Stat(filepath.Join(root, "bin", name))
	return err == nil && !information.IsDir()
}

func requirementExecutables(host bootstrapHost, requirement toolRequirement) []string {
	executables := append([]string(nil), requirement.executables...)
	if requirement.name != "LLVM" {
		return executables
	}
	if host == bootstrapWindows {
		return append(executables, "clang-cl", "lld-link")
	}
	return append(executables, "clang++")
}

func reportBootstrapState(host bootstrapHost, runner bootstrapRunner) error {
	fmt.Printf("Official host: %s\n", bootstrapHostName(host))
	missing := missingTools(host, runner)
	for _, requirement := range bootstrapTools {
		if containsRequirement(missing, requirement.name) {
			fmt.Printf("[missing] %s\n", requirement.name)
		} else {
			fmt.Printf("[ready]   %s\n", requirement.name)
		}
	}
	if host == bootstrapWindows {
		if windowsLinkResourcesPresent() {
			fmt.Println("[ready]   Windows SDK and MSVC CRT/C++ link libraries")
		} else {
			fmt.Println("[missing] Windows SDK or MSVC CRT/C++ link libraries")
			missing = append(missing, toolRequirement{name: "Windows link resources"})
		}
	} else if _, err := runner.Output("xcode-select", "-p"); err != nil {
		fmt.Println("[missing] Xcode Command Line Tools")
		missing = append(missing, toolRequirement{name: "Xcode Command Line Tools"})
	} else {
		fmt.Println("[ready]   Xcode Command Line Tools")
	}
	if _, err := runner.LookPath("ghc"); err != nil {
		fmt.Println("[missing] GHC selected by GHCup")
		missing = append(missing, toolRequirement{name: "GHC"})
	} else {
		fmt.Println("[ready]   GHC")
	}
	if _, err := runner.LookPath("cabal"); err != nil {
		fmt.Println("[missing] Cabal selected by GHCup")
		missing = append(missing, toolRequirement{name: "Cabal"})
	} else {
		fmt.Println("[ready]   Cabal")
	}
	if len(missing) != 0 {
		return errors.New("development host is incomplete; run 'go run scripts/prebuild.go install'")
	}
	fmt.Println("\nDevelopment host bootstrap is complete.")
	return nil
}

func reportPostInstallState(host bootstrapHost, runner bootstrapRunner) error {
	fmt.Println("\nInstallation commands completed.")
	if err := reportBootstrapState(host, runner); err != nil {
		fmt.Println("\nNew package-manager PATH entries may not be visible to this process yet.")
		fmt.Println("Open a new terminal, then run:")
		fmt.Println("  go run scripts/prebuild.go check")
		fmt.Println("  go run scripts/develop.go doctor")
		return errors.New("installation finished, but verification is incomplete in this process; open a new terminal and run prebuild check")
	}
	fmt.Println("Run 'go run scripts/develop.go doctor' before the first build.")
	return nil
}

func installBootstrapTools(host bootstrapHost, runner bootstrapRunner) error {
	missing := missingTools(host, runner)
	if host == bootstrapWindows {
		winget, err := runner.LookPath("winget")
		if err != nil {
			return errors.New("winget is required on Windows; install or repair App Installer first")
		}
		for _, requirement := range missing {
			if requirement.name == "GHCup" {
				if err := installGHCupWindows(runner); err != nil {
					return err
				}
				continue
			}
			if err := installWingetPackage(runner, winget, requirement.wingetPackageID); err != nil {
				return fmt.Errorf("cannot install %s: %w", requirement.name, err)
			}
		}
		if !windowsLinkResourcesPresent() {
			if err := installWindowsLinkResources(runner, winget); err != nil {
				return err
			}
		}
	} else {
		brew, err := runner.LookPath("brew")
		if err != nil {
			return errors.New("Homebrew is required on macOS; install it from https://brew.sh and rerun prebuild")
		}
		if _, err := runner.Output("xcode-select", "-p"); err != nil {
			fmt.Println("Requesting the Apple Command Line Tools installer...")
			if err := runner.Run("xcode-select", "--install"); err != nil {
				return fmt.Errorf("cannot request Xcode Command Line Tools: %w", err)
			}
		}
		for _, requirement := range missing {
			fmt.Printf("Installing %s with Homebrew...\n", requirement.name)
			arguments := homebrewInstallArguments(requirement)
			if err := runner.Run(brew, arguments...); err != nil {
				return fmt.Errorf("cannot install %s: %w", requirement.name, err)
			}
		}
	}
	return installHaskellTools(runner)
}

func homebrewInstallArguments(requirement toolRequirement) []string {
	if requirement.name == "Temurin JDK 25" {
		return []string{"install", "--cask", requirement.homebrewFormula}
	}
	return []string{"install", requirement.homebrewFormula}
}

func installWingetPackage(runner bootstrapRunner, winget string, packageID string) error {
	if packageID == "" {
		return errors.New("package has no winget identifier")
	}
	fmt.Printf("Installing %s with winget...\n", packageID)
	return runner.Run(winget, "install", "--id", packageID, "--exact", "--source", "winget",
		"--accept-package-agreements", "--accept-source-agreements", "--silent", "--disable-interactivity")
}

func installGHCupWindows(runner bootstrapRunner) error {
	// GHCup does not publish an official winget package. Its documented
	// non-interactive PowerShell bootstrap installs the manager into C:\ghcup;
	// GHCup itself then owns the selected GHC and Cabal versions.
	bootstrap := `$ErrorActionPreference='Stop';` +
		`Set-ExecutionPolicy Bypass -Scope Process -Force;` +
		`[Net.ServicePointManager]::SecurityProtocol=[Net.ServicePointManager]::SecurityProtocol -bor 3072;` +
		`$response=Invoke-WebRequest 'https://www.haskell.org/ghcup/sh/bootstrap-haskell.ps1' -UseBasicParsing;` +
		`& ([ScriptBlock]::Create($response.Content)) -Minimal -InBash -InstallDir 'C:\'`
	fmt.Println("Installing GHCup with the official Haskell bootstrap...")
	if err := runner.Run("powershell.exe", "-NoProfile", "-ExecutionPolicy", "Bypass", "-Command", bootstrap); err != nil {
		return fmt.Errorf("cannot install GHCup: %w", err)
	}
	return nil
}

func installWindowsLinkResources(runner bootstrapRunner, winget string) error {
	// ClangCL and LLD remain selected. The workload is used only because the SDK
	// import libraries and MSVC CRT/C++ link libraries are not a complete
	// standalone winget package.
	const packageID = "Microsoft.VisualStudio.2022.BuildTools"
	fmt.Printf("Installing Windows SDK/CRT link resources from %s...\n", packageID)
	arguments := []string{
		"install", "--id", packageID, "--exact", "--source", "winget",
		"--accept-package-agreements", "--accept-source-agreements", "--disable-interactivity",
		"--override", "--wait --passive --norestart --add Microsoft.VisualStudio.Workload.VCTools --includeRecommended",
	}
	if err := runner.Run(winget, arguments...); err != nil {
		return fmt.Errorf("winget could not install the Visual Studio Build Tools link-resource workload: %w", err)
	}
	return nil
}

func installHaskellTools(runner bootstrapRunner) error {
	ghcup, err := findGHCup(runner)
	if err != nil {
		fmt.Println("GHCup was installed but is not visible yet; open a new terminal and rerun prebuild install.")
		return nil
	}
	if _, err := runner.LookPath("ghc"); err != nil {
		fmt.Println("Installing and selecting GHCup's recommended GHC...")
		if err := runner.Run(ghcup, "install", "ghc", "recommended", "--set"); err != nil {
			return fmt.Errorf("cannot install GHC: %w", err)
		}
	}
	if _, err := runner.LookPath("cabal"); err != nil {
		fmt.Println("Installing and selecting Cabal...")
		if err := runner.Run(ghcup, "install", "cabal", "latest", "--set"); err != nil {
			return fmt.Errorf("cannot install Cabal: %w", err)
		}
	}
	return nil
}

func findGHCup(runner bootstrapRunner) (string, error) {
	if path, err := runner.LookPath("ghcup"); err == nil {
		return path, nil
	}
	candidates := []string{`C:\ghcup\bin\ghcup.exe`}
	if appData := os.Getenv("APPDATA"); appData != "" {
		candidates = append(candidates, filepath.Join(appData, "ghcup", "bin", "ghcup.exe"))
	}
	if userProfile := os.Getenv("USERPROFILE"); userProfile != "" {
		candidates = append(candidates, filepath.Join(userProfile, ".ghcup", "bin", "ghcup.exe"))
	}
	for _, candidate := range candidates {
		if information, err := os.Stat(candidate); err == nil && !information.IsDir() {
			return candidate, nil
		}
	}
	return "", errors.New("ghcup was not found")
}

func windowsLinkResourcesPresent() bool {
	programFilesX86 := os.Getenv("ProgramFiles(x86)")
	programFiles := os.Getenv("ProgramFiles")
	if programFilesX86 == "" {
		programFilesX86 = `C:\Program Files (x86)`
	}
	if programFiles == "" {
		programFiles = `C:\Program Files`
	}
	sdk := filepath.Join(programFilesX86, "Windows Kits", "10", "Lib", "*", "ucrt", "x64", "ucrt.lib")
	crt := filepath.Join(programFiles, "Microsoft Visual Studio", "*", "*", "VC", "Tools", "MSVC", "*", "lib", "x64", "libcmt.lib")
	return globHasFile(sdk) && globHasFile(crt)
}

func globHasFile(pattern string) bool {
	matches, err := filepath.Glob(pattern)
	if err != nil {
		return false
	}
	for _, match := range matches {
		if information, err := os.Stat(match); err == nil && !information.IsDir() {
			return true
		}
	}
	return false
}

func containsRequirement(requirements []toolRequirement, name string) bool {
	for _, requirement := range requirements {
		if requirement.name == name {
			return true
		}
	}
	return false
}

func bootstrapHostName(host bootstrapHost) string {
	switch host {
	case bootstrapWindows:
		return "Windows 10/11"
	case bootstrapMacOS:
		return "macOS Sequoia/Tahoe"
	default:
		return "unsupported"
	}
}
