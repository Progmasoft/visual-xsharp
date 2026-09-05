// SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
// SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.0

// develop is the human-facing entry point for common native compiler work.
// Bazel remains the only owner of the native build graph; this command merely
// detects the host, selects private diagnostic profiles, and runs native test
// programs consistently on Windows and macOS.
package main

import (
	"errors"
	"fmt"
	"io"
	"os"
	"os/exec"
	"path/filepath"
	"runtime"
	"strconv"
	"strings"
)

const usage = `Visual X# native developer command

Usage:
  go run scripts/develop.go doctor
  go run scripts/develop.go build [-- <Bazel options>]
  go run scripts/develop.go test [-- <Bazel options>]
  go run scripts/develop.go sanitize <address|undefined|thread> [-- <Bazel options>]
  go run scripts/develop.go clean

Commands:
  doctor    Explain whether this host has the required native toolchain.
  build     Build the compiler and every native contract suite.
  test      Build and execute every native contract suite.
  sanitize  Rebuild and execute the suites with a host-supported sanitizer.
  clean     Remove Bazel-owned generated output.

The supported hosts are Windows 10/11 and macOS 15 Sequoia or macOS 26
Tahoe. Platform selection is automatic. Bazel options after -- are an escape
hatch for diagnostics; ordinary development does not require --config.`

type hostKind int

const (
	hostUnsupported hostKind = iota
	hostWindows
	hostMacOS
)

type host struct {
	kind       hostKind
	name       string
	version    string
	executable string
}

type sanitizer struct {
	name        string
	config      string
	environment []string
}

type commandRunner interface {
	Run(directory string, environment []string, name string, arguments ...string) error
	Output(name string, arguments ...string) (string, error)
	LookPath(name string) (string, error)
}

type systemRunner struct {
	stdout io.Writer
	stderr io.Writer
}

var nativeTargets = []string{
	"//Compiler/Backend/LLVM/Tests:llvm_backend_tests",
	"//Compiler/Cli/Tests:cli_parser_tests",
	"//Compiler/Core/Tests:callable_contract_tests",
	"//Compiler/Core/Tests:core_pipeline_tests",
	"//Compiler/Driver/Tests:artifact_wire_tests",
	"//Compiler/Driver/Tests:closure_pipeline_tests",
	"//Compiler/Driver/Tests:scalar_pipeline_tests",
	"//Compiler/Runtime/AARC/Tests:aarc_runtime_tests",
}

var nativePrograms = []string{
	"Compiler/Backend/LLVM/Tests/llvm_backend_tests",
	"Compiler/Cli/Tests/cli_parser_tests",
	"Compiler/Core/Tests/callable_contract_tests",
	"Compiler/Core/Tests/core_pipeline_tests",
	"Compiler/Driver/Tests/artifact_wire_tests",
	"Compiler/Driver/Tests/closure_pipeline_tests",
	"Compiler/Driver/Tests/scalar_pipeline_tests",
	"Compiler/Runtime/AARC/Tests/aarc_runtime_tests",
}

func (runner systemRunner) Run(directory string, environment []string, name string, arguments ...string) error {
	command := exec.Command(name, arguments...)
	command.Dir = directory
	command.Env = append(os.Environ(), environment...)
	command.Stdin = os.Stdin
	command.Stdout = runner.stdout
	command.Stderr = runner.stderr
	return command.Run()
}

func (runner systemRunner) Output(name string, arguments ...string) (string, error) {
	command := exec.Command(name, arguments...)
	output, err := command.CombinedOutput()
	return strings.TrimSpace(string(output)), err
}

func (runner systemRunner) LookPath(name string) (string, error) {
	return exec.LookPath(name)
}

func main() {
	runner := systemRunner{stdout: os.Stdout, stderr: os.Stderr}
	if err := run(os.Args[1:], runner); err != nil {
		fmt.Fprintf(os.Stderr, "\nerror: %v\n", err)
		os.Exit(1)
	}
}

func run(arguments []string, runner commandRunner) error {
	if len(arguments) == 0 || isHelp(arguments[0]) {
		fmt.Println(usage)
		return nil
	}

	commandArguments, bazelArguments, err := splitArguments(arguments[1:])
	if err != nil {
		return err
	}

	repository, err := findRepositoryRoot()
	if err != nil {
		return err
	}
	currentHost, err := detectHost(runner)
	if err != nil {
		return err
	}

	switch strings.ToLower(arguments[0]) {
	case "doctor":
		if len(commandArguments) != 0 || len(bazelArguments) != 0 {
			return errors.New("doctor does not accept arguments")
		}
		return runDoctor(currentHost, runner)
	case "build":
		if len(commandArguments) != 0 {
			return errors.New("build accepts Bazel options only after --")
		}
		if err := requireBuildTools(currentHost, runner); err != nil {
			return err
		}
		return buildTargets(repository, runner, "", bazelArguments)
	case "test":
		if len(commandArguments) != 0 {
			return errors.New("test accepts Bazel options only after --")
		}
		if err := requireBuildTools(currentHost, runner); err != nil {
			return err
		}
		if err := buildTargets(repository, runner, "", bazelArguments); err != nil {
			return err
		}
		return runTests(repository, currentHost, runner, nil)
	case "sanitize":
		if len(commandArguments) != 1 {
			return errors.New("sanitize requires exactly one kind: address, undefined, or thread")
		}
		if err := requireBuildTools(currentHost, runner); err != nil {
			return err
		}
		selected, err := selectSanitizer(currentHost, commandArguments[0])
		if err != nil {
			return err
		}
		fmt.Printf("Sanitizer: %s\nHost: %s\n\n", selected.name, currentHost.name)
		if err := buildTargets(repository, runner, selected.config, bazelArguments); err != nil {
			return fmt.Errorf("%s sanitizer build failed: %w", selected.name, err)
		}
		selected.environment, err = sanitizerEnvironment(currentHost, selected, runner)
		if err != nil {
			return err
		}
		if err := runTests(repository, currentHost, runner, selected.environment); err != nil {
			return fmt.Errorf("%s sanitizer found a failure: %w", selected.name, err)
		}
		fmt.Printf("\n%s sanitizer completed without a reported violation.\n", selected.name)
		return nil
	case "clean":
		if len(commandArguments) != 0 || len(bazelArguments) != 0 {
			return errors.New("clean does not accept arguments")
		}
		bazel, err := findBazel(runner)
		if err != nil {
			return err
		}
		fmt.Println("Removing Bazel-owned generated output...")
		return runner.Run(repository, nil, bazel, "clean", "--expunge")
	default:
		return fmt.Errorf("unknown command %q; run with help to see the supported workflow", arguments[0])
	}
}

func sanitizerEnvironment(currentHost host, selected sanitizer, runner commandRunner) ([]string, error) {
	environment := append([]string(nil), selected.environment...)
	if currentHost.kind != hostWindows || selected.name != "AddressSanitizer" {
		return environment, nil
	}
	resourceDirectory, err := runner.Output("clang-cl", "/clang:-print-resource-dir")
	if err != nil || resourceDirectory == "" {
		return nil, errors.New("AddressSanitizer could not locate the Clang runtime directory")
	}
	runtimeDirectory := filepath.Join(resourceDirectory, "lib", "windows")
	runtimeLibrary := filepath.Join(runtimeDirectory, "clang_rt.asan_dynamic-x86_64.dll")
	if information, err := os.Stat(runtimeLibrary); err != nil || information.IsDir() {
		return nil, fmt.Errorf("AddressSanitizer runtime is missing: %s", runtimeLibrary)
	}
	// Instrumented executables use Clang's matching dynamic runtime. Scope the
	// PATH extension to child tests instead of mutating the developer's shell.
	environment = append(environment, "PATH="+runtimeDirectory+string(os.PathListSeparator)+os.Getenv("PATH"))
	return environment, nil
}

func isHelp(argument string) bool {
	switch strings.ToLower(argument) {
	case "help", "-help", "--help", "-h":
		return true
	default:
		return false
	}
}

func splitArguments(arguments []string) ([]string, []string, error) {
	for index, argument := range arguments {
		if argument != "--" {
			continue
		}
		for _, trailing := range arguments[index+1:] {
			if trailing == "--config" || strings.HasPrefix(trailing, "--config=") {
				return nil, nil, errors.New("--config is managed by the developer command; pass the desired sanitizer name instead")
			}
		}
		return arguments[:index], arguments[index+1:], nil
	}
	return arguments, nil, nil
}

func detectHost(runner commandRunner) (host, error) {
	switch runtime.GOOS {
	case "windows":
		version, _ := runner.Output("cmd", "/c", "ver")
		return host{kind: hostWindows, name: "Windows 10/11", version: version, executable: ".exe"}, nil
	case "darwin":
		version, err := runner.Output("sw_vers", "-productVersion")
		if err != nil {
			return host{}, fmt.Errorf("cannot determine the macOS version: %w", err)
		}
		majorText := strings.SplitN(version, ".", 2)[0]
		major, err := strconv.Atoi(majorText)
		if err != nil || (major != 15 && major != 26) {
			return host{}, fmt.Errorf("macOS %s is not an official host; use macOS 15 Sequoia or macOS 26 Tahoe", version)
		}
		name := "macOS 15 Sequoia"
		if major == 26 {
			name = "macOS 26 Tahoe"
		}
		return host{kind: hostMacOS, name: name, version: version}, nil
	default:
		return host{}, fmt.Errorf("%s is not an official development host; use Windows 10/11 or macOS Sequoia/Tahoe", runtime.GOOS)
	}
}

func selectSanitizer(currentHost host, requested string) (sanitizer, error) {
	name := strings.ToLower(requested)
	switch name {
	case "address", "asan":
		if currentHost.kind == hostWindows {
			return sanitizer{
				name:        "AddressSanitizer",
				config:      "asan-windows",
				environment: []string{"ASAN_OPTIONS=halt_on_error=1:strict_string_checks=1"},
			}, nil
		}
		return sanitizer{
			name:   "AddressSanitizer",
			config: "asan-macos",
			// Apple's AddressSanitizer runtime aborts when detect_leaks is set;
			// address, bounds, and use-after-free diagnostics remain enabled.
			environment: []string{"ASAN_OPTIONS=halt_on_error=1:strict_string_checks=1"},
		}, nil
	case "undefined", "ubsan":
		if currentHost.kind != hostMacOS {
			return sanitizer{}, errors.New("UndefinedBehaviorSanitizer is not exposed on Windows; use address, or run undefined on macOS")
		}
		return sanitizer{
			name:        "UndefinedBehaviorSanitizer",
			config:      "ubsan-macos",
			environment: []string{"UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1"},
		}, nil
	case "thread", "tsan":
		if currentHost.kind != hostMacOS {
			return sanitizer{}, errors.New("ThreadSanitizer is not exposed on Windows; run thread on macOS")
		}
		return sanitizer{
			name:        "ThreadSanitizer",
			config:      "tsan-macos",
			environment: []string{"TSAN_OPTIONS=halt_on_error=1"},
		}, nil
	default:
		return sanitizer{}, fmt.Errorf("unknown sanitizer %q; choose address, undefined, or thread", requested)
	}
}

func runDoctor(currentHost host, runner commandRunner) error {
	fmt.Printf("Official host: %s\n", currentHost.name)
	if currentHost.version != "" {
		fmt.Printf("Detected version: %s\n", currentHost.version)
	}

	required := []string{"llvm-config"}
	if currentHost.kind == hostWindows {
		required = append(required, "clang-cl", "lld-link")
	} else {
		required = append(required, "clang++", "xcrun")
	}
	missing := make([]string, 0)
	if _, err := findBazel(runner); err != nil {
		fmt.Println("[missing] bazelisk or bazel")
		missing = append(missing, "Bazelisk")
	} else {
		fmt.Println("[ready]   Bazel")
	}
	for _, tool := range required {
		if path, err := locateTool(runner, tool); err != nil {
			fmt.Printf("[missing] %s\n", tool)
			missing = append(missing, tool)
		} else {
			fmt.Printf("[ready]   %s (%s)\n", tool, path)
		}
	}
	if currentHost.kind == hostMacOS {
		if sdk, err := runner.Output("xcrun", "--show-sdk-path"); err == nil {
			fmt.Printf("[ready]   Apple SDK (%s)\n", sdk)
		} else {
			fmt.Println("[missing] Apple SDK; install the Xcode Command Line Tools")
			missing = append(missing, "Apple SDK")
		}
	}
	if len(missing) != 0 {
		return fmt.Errorf("toolchain is incomplete: %s", strings.Join(missing, ", "))
	}
	fmt.Println("\nNative toolchain discovery is ready.")
	return nil
}

func requireBuildTools(currentHost host, runner commandRunner) error {
	if _, err := findBazel(runner); err != nil {
		return err
	}
	tools := []string{"llvm-config"}
	if currentHost.kind == hostWindows {
		tools = append(tools, "clang-cl", "lld-link")
	} else {
		tools = append(tools, "clang++", "xcrun")
	}
	for _, tool := range tools {
		if _, err := locateTool(runner, tool); err != nil {
			return fmt.Errorf("required tool %q was not found; run doctor for the complete host report", tool)
		}
	}
	return nil
}

func locateTool(runner commandRunner, name string) (string, error) {
	if path, err := runner.LookPath(name); err == nil {
		return path, nil
	}
	// LLVM_ROOT is an established repository discovery input. Doctor must agree
	// with the Bazel repository rule instead of reporting a false negative when
	// the user intentionally keeps LLVM's bin directory off the global PATH.
	if name == "llvm-config" {
		if root := os.Getenv("LLVM_ROOT"); root != "" {
			executable := name
			if runtime.GOOS == "windows" {
				executable += ".exe"
			}
			candidate := filepath.Join(root, "bin", executable)
			if information, err := os.Stat(candidate); err == nil && !information.IsDir() {
				return candidate, nil
			}
		}
	}
	return "", errors.New("tool not found")
}

func findBazel(runner commandRunner) (string, error) {
	if path, err := runner.LookPath("bazelisk"); err == nil {
		return path, nil
	}
	if path, err := runner.LookPath("bazel"); err == nil {
		return path, nil
	}
	return "", errors.New("Bazelisk or Bazel was not found; install Bazelisk and run doctor again")
}

func buildTargets(repository string, runner commandRunner, config string, extra []string) error {
	bazel, err := findBazel(runner)
	if err != nil {
		return err
	}
	arguments := []string{"build"}
	if config != "" {
		arguments = append(arguments, "--config="+config)
	}
	arguments = append(arguments, "//Compiler/Cli:vxs")
	arguments = append(arguments, nativeTargets...)
	arguments = append(arguments, extra...)
	fmt.Printf("Building compiler and %d native suites...\n", len(nativeTargets))
	if err := runner.Run(repository, nil, bazel, arguments...); err != nil {
		return fmt.Errorf("Bazel build failed: %w", err)
	}
	return nil
}

func runTests(repository string, currentHost host, runner commandRunner, environment []string) error {
	for index, program := range nativePrograms {
		path := filepath.Join(repository, "bazel-bin", filepath.FromSlash(program)) + currentHost.executable
		fmt.Printf("[%d/%d] %s\n", index+1, len(nativePrograms), filepath.Base(program))
		if err := runner.Run(repository, environment, path); err != nil {
			return fmt.Errorf("native suite %s failed: %w", filepath.Base(program), err)
		}
	}
	fmt.Printf("\nAll %d native suites passed.\n", len(nativePrograms))
	return nil
}

func findRepositoryRoot() (string, error) {
	directory, err := os.Getwd()
	if err != nil {
		return "", fmt.Errorf("cannot read the current directory: %w", err)
	}
	for {
		if _, err := os.Stat(filepath.Join(directory, "MODULE.bazel")); err == nil {
			return directory, nil
		}
		parent := filepath.Dir(directory)
		if parent == directory {
			return "", errors.New("MODULE.bazel was not found; run this command inside the Visual X# checkout")
		}
		directory = parent
	}
}
