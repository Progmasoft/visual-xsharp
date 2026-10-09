// SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
// SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.1

package development

import (
	"crypto/sha256"
	"errors"
	"fmt"
	"io"
	"os"
	"path/filepath"
	"runtime"
	"sort"
	"strings"
)

var bundleFiles = []string{
	"LICENSE.txt",
	"PATENTS",
	"LICENSES/AdditionRef-Progmasoft-Exception-1.1.txt",
	"LICENSES/AdditionRef-Progmasoft-Patent-Grant-1.1.txt",
}

func buildBundle(repository string, currentHost host, runner commandRunner, bazelArguments []string) error {
	// Build the complete native graph before staging. A bundle therefore cannot
	// be published from a checkout whose component-owned suites do not compile.
	if err := buildTargets(repository, runner, "", bazelArguments); err != nil {
		return err
	}

	compilerDirectory := filepath.Join(repository, "Compiler")
	frontend, err := locateFrontendLibrary(compilerDirectory)
	if err != nil {
		return fmt.Errorf("cannot locate the Haskell frontend shared library: %w", err)
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
	interactiveExecutable := "vxsi" + currentHost.executable
	frontendLibraryName := filepath.Base(frontend)
	nativeCompiler := filepath.Join(repository, "bazel-bin", "Compiler", "Cli", publicExecutable)
	nativeInteractive := filepath.Join(repository, "bazel-bin", "Interactive", interactiveExecutable)
	stagedFiles := []string{publicExecutable, interactiveExecutable, frontendLibraryName}
	if err := copyFile(nativeCompiler, filepath.Join(stagingDirectory, publicExecutable), 0o755); err != nil {
		return fmt.Errorf("cannot stage the public compiler driver: %w", err)
	}
	if err := copyFile(nativeInteractive, filepath.Join(stagingDirectory, interactiveExecutable), 0o755); err != nil {
		return fmt.Errorf("cannot stage the Visual X# Interactive executable: %w", err)
	}
	if err := copyFile(frontend, filepath.Join(stagingDirectory, frontendLibraryName), 0o755); err != nil {
		return fmt.Errorf("cannot stage the Haskell frontend shared library: %w", err)
	}
	// A program the bundled compiler links needs the runtime library beside
	// the compiler.
	if runtimeLibraryTarget != "" {
		if err := copyFile(builtRuntimeLibrary(repository), filepath.Join(stagingDirectory, runtimeLibraryName), 0o644); err != nil {
			return fmt.Errorf("cannot stage the runtime library: %w", err)
		}
		stagedFiles = append(stagedFiles, runtimeLibraryName)
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

func distributionPlatform(currentHost host, architecture string) (string, error) {
	platform := ""
	switch currentHost.kind {
	case hostWindows:
		platform = "windows"
	case hostMacOS:
		platform = "macos"
	case hostLinux:
		platform = "linux"
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
	interactive := filepath.Join(bundleDirectory, "vxsi"+currentHost.executable)
	pathEnvironment := []string{"PATH=" + bundleDirectory}
	// A same-named program in the working directory is deliberately wrong. The
	// public launcher must use PATH only, so this decoy can never shadow vxsi.
	decoy := filepath.Join(temporary, "vxsi"+currentHost.executable)
	if err := copyFile(compiler, decoy, 0o755); err != nil {
		return fmt.Errorf("cannot prepare the current-directory PATH decoy: %w", err)
	}
	fmt.Println("Smoke-testing the staged compiler version command...")
	output, err := runner.OutputIn(temporary, compiler, "version")
	if err != nil {
		return fmt.Errorf("staged compiler version smoke test failed: %w", err)
	}
	wantVersion := "vxs " + version
	if output != wantVersion {
		return fmt.Errorf("staged compiler reported %q; expected %q", output, wantVersion)
	}
	fmt.Println("Smoke-testing one-shot evaluation through vxs interactive and standalone vxsi...")
	output, err = runner.RunWithInput(temporary, pathEnvironment, "", compiler, "interactive", "-Eval", "5 + 5")
	if err != nil || !strings.HasPrefix(strings.TrimSpace(output), "10 : ") {
		return fmt.Errorf("vxs interactive -Eval failed or selected the current-directory decoy: %q (%v)", output, err)
	}
	output, err = runner.RunWithInput(temporary, pathEnvironment, "", interactive, "-Eval", "5 + 5")
	if err != nil || !strings.HasPrefix(strings.TrimSpace(output), "10 : ") {
		return fmt.Errorf("standalone vxsi -Eval smoke test failed: %q (%v)", output, err)
	}
	output, err = runner.RunWithInput(temporary, pathEnvironment, "", compiler, "interactive", "-Help")
	if err != nil || !strings.Contains(output, "Visual X# Interactive") || !strings.Contains(output, "Usage:") {
		return fmt.Errorf("vxs interactive -Help did not reach vxsi: %q (%v)", output, err)
	}
	output, err = runner.RunWithInput(temporary, pathEnvironment, "", interactive, "-Help")
	if err != nil || !strings.Contains(output, "Visual X# Interactive") || !strings.Contains(output, "Usage:") {
		return fmt.Errorf("standalone vxsi -Help smoke test failed: %q (%v)", output, err)
	}
	output, err = runner.RunWithInput(temporary, []string{"PATH="}, "", compiler, "interactive", "-Eval", "5 + 5")
	if err == nil || !strings.Contains(output, "vxsi") || !strings.Contains(output, "PATH") {
		return fmt.Errorf("vxs interactive should fail clearly when vxsi is absent from PATH: %q (%v)", output, err)
	}
	fmt.Println("Smoke-testing the persistent REPL session, history, reset, and error recovery...")
	replInput := strings.Repeat("x", 1024*1024+1) + "\n" +
		"5 + 5\nvxsiPrevious * 3\ntrue + 1\nvxsiPrevious + 1\n" +
		":type true\n:history\n:reset\nvxsiPrevious\n" +
		"7 + 8\n:history\n:help\n:quit\n"
	output, err = runner.RunWithInput(temporary, pathEnvironment, replInput, interactive)
	if err != nil {
		return fmt.Errorf("persistent REPL smoke test failed: %w\n%s", err, output)
	}
	for _, expected := range []string{"10 : ", "30 : ", "31 : ", "bool", "one Visual X# input line cannot exceed 1 MiB", "VXT0012", "session values, JIT modules, and history cleared", "VXN0001", "   1  7 + 8", "Type :help for commands"} {
		if !strings.Contains(output, expected) {
			return fmt.Errorf("persistent REPL output omitted %q:\n%s", expected, output)
		}
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
