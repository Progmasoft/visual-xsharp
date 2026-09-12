// SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
// SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.1

// develop is the human-facing entry point for common native compiler work.
// Bazel remains the only owner of the native build graph; this command merely
// detects the host, selects private diagnostic profiles, and runs native test
// programs consistently on Windows and macOS.
package main

import (
	"bufio"
	"crypto/sha256"
	"errors"
	"fmt"
	"io"
	"os"
	"os/exec"
	"path/filepath"
	"runtime"
	"sort"
	"strconv"
	"strings"
)

const usage = `Visual X# native developer command

Usage:
  go run scripts/develop.go doctor
  go run scripts/develop.go build [-- <Bazel options>]
  go run scripts/develop.go bundle [-- <Bazel options>]
  go run scripts/develop.go version <major.minor.patch>
  go run scripts/develop.go test [-- <Bazel options>]
  go run scripts/develop.go sanitize <address|undefined|thread> [-- <Bazel options>]
  go run scripts/develop.go clean

Commands:
  doctor    Explain whether this host has the required native toolchain.
  build     Build the compiler and every native contract suite.
  bundle    Build, stage, checksum, and smoke-test a host distribution.
  version   Validate release metadata and an available vxs binary.
  test      Build and execute every native contract suite.
  sanitize  Rebuild and execute the suites with a host-supported sanitizer.
  clean     Remove Bazel, Cabal, and Gradle generated build output.

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

type releaseCheck struct {
	name   string
	ok     bool
	detail string
}

type commandRunner interface {
	Run(directory string, environment []string, name string, arguments ...string) error
	Output(name string, arguments ...string) (string, error)
	OutputIn(directory string, name string, arguments ...string) (string, error)
	LookPath(name string) (string, error)
}

type systemRunner struct {
	stdout io.Writer
	stderr io.Writer
}

var nativeTargets = []string{
	"//Compiler/Analysis/Tests:definite_initialization_tests",
	"//Compiler/Backend/LLVM/Tests:llvm_backend_tests",
	"//Compiler/Cli/Tests:cli_parser_tests",
	"//Compiler/Core/Tests:callable_contract_tests",
	"//Compiler/Core/Tests:core_pipeline_tests",
	"//Compiler/Diagnostic/Tests:diagnostic_protocol_tests",
	"//Compiler/Driver/Tests:artifact_wire_tests",
	"//Compiler/Driver/Tests:closure_pipeline_tests",
	"//Compiler/Driver/Tests:scalar_pipeline_tests",
	"//Compiler/Codegen/Xmm/Tests:xmm_verifier_tests",
	"//Compiler/Codegen/Xpp/Tests:xpp_verifier_tests",
	"//Compiler/Runtime/AARC/Tests:aarc_runtime_tests",
}

var nativePrograms = []string{
	"Compiler/Analysis/Tests/definite_initialization_tests",
	"Compiler/Backend/LLVM/Tests/llvm_backend_tests",
	"Compiler/Cli/Tests/cli_parser_tests",
	"Compiler/Core/Tests/callable_contract_tests",
	"Compiler/Core/Tests/core_pipeline_tests",
	"Compiler/Diagnostic/Tests/diagnostic_protocol_tests",
	"Compiler/Driver/Tests/artifact_wire_tests",
	"Compiler/Driver/Tests/closure_pipeline_tests",
	"Compiler/Driver/Tests/scalar_pipeline_tests",
	"Compiler/Codegen/Xmm/Tests/xmm_verifier_tests",
	"Compiler/Codegen/Xpp/Tests/xpp_verifier_tests",
	"Compiler/Runtime/AARC/Tests/aarc_runtime_tests",
}

// bundleFiles is deliberately explicit. A release must not accidentally absorb
// a stale license draft merely because it appeared under LICENSES/.
var bundleFiles = []string{
	"LICENSE.txt",
	"PATENTS",
	"LICENSES/AdditionRef-Progmasoft-Exception-1.1.txt",
	"LICENSES/AdditionRef-Progmasoft-Patent-Grant-1.1.txt",
}

var generatedBuildPaths = []string{
	"Compiler/dist-newstyle",
	"ProjectSystem/.gradle",
	"ProjectSystem/build",
	"Analyzer/.gradle",
	"Analyzer/build",
	"Formatter/.gradle",
	"Formatter/build",
	"Linter/.gradle",
	"Linter/build",
	"Compiler/Driver/Tests/Fixtures/Source/haskell_frontend/Main.vxse",
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
	return runner.OutputIn("", name, arguments...)
}

func (runner systemRunner) OutputIn(directory string, name string, arguments ...string) (string, error) {
	command := exec.Command(name, arguments...)
	command.Dir = directory
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
	case "bundle":
		if len(commandArguments) != 0 {
			return errors.New("bundle accepts Bazel options only after --")
		}
		if err := requireBuildTools(currentHost, runner); err != nil {
			return err
		}
		return buildBundle(repository, currentHost, runner, bazelArguments)
	case "version":
		if len(commandArguments) != 1 || len(bazelArguments) != 0 {
			return errors.New("version requires exactly one major.minor.patch argument")
		}
		return checkReleaseMetadata(repository, currentHost, commandArguments[0], runner)
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
		if err := runner.Run(repository, nil, bazel, "clean", "--expunge"); err != nil {
			return err
		}
		return cleanGeneratedBuildPaths(repository)
	default:
		return fmt.Errorf("unknown command %q; run with help to see the supported workflow", arguments[0])
	}
}

func cleanGeneratedBuildPaths(repository string) error {
	return cleanGeneratedPaths(repository, generatedBuildPaths)
}

func cleanGeneratedPaths(repository string, relativePaths []string) error {
	root, err := filepath.Abs(repository)
	if err != nil {
		return fmt.Errorf("cannot resolve repository root for cleanup: %w", err)
	}
	for _, relative := range relativePaths {
		target, err := filepath.Abs(filepath.Join(root, filepath.FromSlash(relative)))
		if err != nil {
			return fmt.Errorf("cannot resolve generated path %s: %w", relative, err)
		}
		within, err := filepath.Rel(root, target)
		if err != nil || within == "." || within == ".." || strings.HasPrefix(within, ".."+string(os.PathSeparator)) {
			return fmt.Errorf("refusing to remove unsafe generated path %q", target)
		}
		if err := os.RemoveAll(target); err != nil {
			return fmt.Errorf("cannot remove generated path %s: %w", relative, err)
		}
	}
	return nil
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

func buildBundle(repository string, currentHost host, runner commandRunner, bazelArguments []string) error {
	// Build the complete native graph before staging. A bundle therefore cannot
	// be published from a checkout whose component-owned suites do not compile.
	if err := buildTargets(repository, runner, "", bazelArguments); err != nil {
		return err
	}

	cabal, err := runner.LookPath("cabal")
	if err != nil {
		return errors.New("required tool \"cabal\" was not found; install GHCup's Cabal tool before creating a bundle")
	}
	compilerDirectory := filepath.Join(repository, "Compiler")
	fmt.Println("Building the private Haskell frontend companion...")
	if err := runner.Run(compilerDirectory, nil, cabal, "build", "exe:vxs-frontend"); err != nil {
		return fmt.Errorf("Haskell frontend build failed: %w", err)
	}
	frontend, err := runner.OutputIn(compilerDirectory, cabal, "list-bin", "exe:vxs-frontend")
	if err != nil {
		return fmt.Errorf("cannot locate the Haskell frontend executable: %w", err)
	}
	frontend = strings.TrimSpace(frontend)
	if frontend == "" {
		return errors.New("Cabal returned an empty path for vxs-frontend")
	}

	version, err := readProjectVersion(repository)
	if err != nil {
		return err
	}
	platform, err := distributionPlatform(currentHost, runtime.GOARCH)
	if err != nil {
		return err
	}
	bundleName := fmt.Sprintf("visual-xsharp-%s-%s", version, platform)
	bundleDirectory := filepath.Join(repository, "dist", bundleName)
	if err := validateBundleTarget(repository, bundleDirectory); err != nil {
		return err
	}
	distributionRoot := filepath.Join(repository, "dist")
	if err := os.MkdirAll(distributionRoot, 0o755); err != nil {
		return fmt.Errorf("cannot create the distribution root: %w", err)
	}
	stagingDirectory, err := os.MkdirTemp(distributionRoot, "."+bundleName+"-staging-")
	if err != nil {
		return fmt.Errorf("cannot create the bundle staging directory: %w", err)
	}
	defer func() {
		if stagingDirectory != "" {
			_ = os.RemoveAll(stagingDirectory)
		}
	}()

	publicExecutable := "vxs" + currentHost.executable
	privateExecutable := "vxs-frontend" + currentHost.executable
	nativeCompiler := filepath.Join(repository, "bazel-bin", "Compiler", "Cli", publicExecutable)
	stagedFiles := []string{publicExecutable, privateExecutable}
	if err := copyFile(nativeCompiler, filepath.Join(stagingDirectory, publicExecutable), 0o755); err != nil {
		return fmt.Errorf("cannot stage the public compiler driver: %w", err)
	}
	if err := copyFile(frontend, filepath.Join(stagingDirectory, privateExecutable), 0o755); err != nil {
		return fmt.Errorf("cannot stage the private frontend companion: %w", err)
	}
	for _, relative := range bundleFiles {
		if err := copyFile(filepath.Join(repository, filepath.FromSlash(relative)), filepath.Join(stagingDirectory, filepath.FromSlash(relative)), 0o644); err != nil {
			return fmt.Errorf("cannot stage %s: %w", relative, err)
		}
		stagedFiles = append(stagedFiles, relative)
	}
	if err := writeBundleChecksums(stagingDirectory, stagedFiles); err != nil {
		return err
	}
	if err := smokeTestBundle(repository, stagingDirectory, currentHost, version, runner); err != nil {
		return err
	}
	if err := publishBundleDirectory(repository, stagingDirectory, bundleDirectory); err != nil {
		return err
	}
	stagingDirectory = ""

	fmt.Printf("\nBundle ready: %s\n", bundleDirectory)
	fmt.Printf("Checksums: %s\n", filepath.Join(bundleDirectory, "SHA256SUMS"))
	return nil
}

func readProjectVersion(repository string) (string, error) {
	contents, err := os.ReadFile(filepath.Join(repository, "MODULE.bazel"))
	if err != nil {
		return "", fmt.Errorf("cannot read MODULE.bazel: %w", err)
	}
	return parseModuleVersion(string(contents))
}

func checkReleaseMetadata(repository string, currentHost host, requested string, runner commandRunner) error {
	if err := validateSemanticVersion(requested); err != nil {
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
		checkFileLineOnce(filepath.Join(repository, "Compiler", "Cli", "Arguments", "Options.cpp"), "#    define XS_PROJECT_VERSION \""+requested+"\"", false, "native CLI fallback version"),
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

func validateSemanticVersion(version string) error {
	parts := strings.Split(version, ".")
	if len(parts) != 3 {
		return fmt.Errorf("version %q is not major.minor.patch", version)
	}
	for _, part := range parts {
		if part == "" {
			return fmt.Errorf("version %q is not major.minor.patch", version)
		}
		if len(part) > 1 && part[0] == '0' {
			return fmt.Errorf("version %q contains a leading zero", version)
		}
		if _, err := strconv.ParseUint(part, 10, 32); err != nil {
			return fmt.Errorf("version %q is not major.minor.patch", version)
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
	if err := validateSemanticVersion(version); err != nil {
		return "", fmt.Errorf("invalid module %w", err)
	}
	return version, nil
}

func distributionPlatform(currentHost host, architecture string) (string, error) {
	platform := ""
	switch currentHost.kind {
	case hostWindows:
		platform = "windows"
	case hostMacOS:
		platform = "macos"
	default:
		return "", errors.New("cannot create a distribution for an unsupported host")
	}
	switch architecture {
	case "amd64":
		architecture = "x86_64"
	case "arm64":
	default:
		return "", fmt.Errorf("cannot create a distribution for architecture %q", architecture)
	}
	return platform + "-" + architecture, nil
}

func validateBundleTarget(repository string, bundleDirectory string) error {
	distributionRoot, err := filepath.Abs(filepath.Join(repository, "dist"))
	if err != nil {
		return fmt.Errorf("cannot resolve the distribution root: %w", err)
	}
	target, err := filepath.Abs(bundleDirectory)
	if err != nil {
		return fmt.Errorf("cannot resolve the bundle target: %w", err)
	}
	if filepath.Dir(target) != distributionRoot || !strings.HasPrefix(filepath.Base(target), "visual-xsharp-") {
		return fmt.Errorf("refusing to replace unsafe bundle target %q", target)
	}
	return nil
}

func publishBundleDirectory(repository string, stagingDirectory string, bundleDirectory string) error {
	if err := validateBundleTarget(repository, bundleDirectory); err != nil {
		return err
	}
	target, err := filepath.Abs(bundleDirectory)
	if err != nil {
		return fmt.Errorf("cannot resolve the bundle target: %w", err)
	}
	if err := os.RemoveAll(target); err != nil {
		return fmt.Errorf("cannot replace the previous bundle: %w", err)
	}
	if err := os.Rename(stagingDirectory, target); err != nil {
		return fmt.Errorf("cannot publish the verified bundle: %w", err)
	}
	return nil
}

func copyFile(source string, destination string, mode os.FileMode) error {
	input, err := os.Open(source)
	if err != nil {
		return err
	}
	defer input.Close()
	if err := os.MkdirAll(filepath.Dir(destination), 0o755); err != nil {
		return err
	}
	output, err := os.OpenFile(destination, os.O_CREATE|os.O_TRUNC|os.O_WRONLY, mode)
	if err != nil {
		return err
	}
	_, copyErr := io.Copy(output, input)
	closeErr := output.Close()
	if copyErr != nil {
		return copyErr
	}
	return closeErr
}

func writeBundleChecksums(bundleDirectory string, relativeFiles []string) error {
	files := append([]string(nil), relativeFiles...)
	sort.Strings(files)
	var checksums strings.Builder
	for _, relative := range files {
		path := filepath.Join(bundleDirectory, filepath.FromSlash(relative))
		input, err := os.Open(path)
		if err != nil {
			return fmt.Errorf("cannot checksum %s: %w", relative, err)
		}
		digest := sha256.New()
		_, copyErr := io.Copy(digest, input)
		closeErr := input.Close()
		if copyErr != nil {
			return fmt.Errorf("cannot checksum %s: %w", relative, copyErr)
		}
		if closeErr != nil {
			return fmt.Errorf("cannot close %s after checksumming: %w", relative, closeErr)
		}
		fmt.Fprintf(&checksums, "%x  %s\n", digest.Sum(nil), filepath.ToSlash(relative))
	}
	if err := os.WriteFile(filepath.Join(bundleDirectory, "SHA256SUMS"), []byte(checksums.String()), 0o644); err != nil {
		return fmt.Errorf("cannot write SHA256SUMS: %w", err)
	}
	return nil
}

func smokeTestBundle(repository string, bundleDirectory string, currentHost host, version string, runner commandRunner) error {
	temporary, err := os.MkdirTemp("", "visual-xsharp-bundle-smoke-")
	if err != nil {
		return fmt.Errorf("cannot create the bundle smoke-test directory: %w", err)
	}
	defer os.RemoveAll(temporary)

	fixture := filepath.Join(repository, "Compiler", "Driver", "Tests", "Fixtures", "Source", "haskell_frontend", "Main.vxs")
	source := filepath.Join(temporary, "Main.vxs")
	if err := copyFile(fixture, source, 0o644); err != nil {
		return fmt.Errorf("cannot stage the smoke-test source: %w", err)
	}
	compiler := filepath.Join(bundleDirectory, "vxs"+currentHost.executable)
	fmt.Println("Smoke-testing the staged compiler version command...")
	output, err := runner.OutputIn(temporary, compiler, "version")
	if err != nil {
		return fmt.Errorf("staged compiler version smoke test failed: %w", err)
	}
	wantVersion := "vxs " + version
	if output != wantVersion {
		return fmt.Errorf("staged compiler reported %q; expected %q", output, wantVersion)
	}
	fmt.Println("Smoke-testing source-to-native compilation through the staged frontend...")
	if err := runner.Run(temporary, nil, compiler, "build", "-File", source); err != nil {
		return fmt.Errorf("staged source-to-native compilation failed: %w", err)
	}
	executable := filepath.Join(temporary, "Main.vxse")
	information, err := os.Stat(executable)
	if err != nil || information.IsDir() {
		return fmt.Errorf("staged compiler did not produce the expected native executable %s", executable)
	}
	if err := runner.Run(temporary, nil, executable); err != nil {
		return fmt.Errorf("generated native executable smoke test failed: %w", err)
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
