// SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
// SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.1

package development

import (
	"errors"
	"fmt"
	"github.com/Progmasoft/visual-xsharp/helpers/internal/repository"
	"os"
	"strings"
)

// Main runs the developer command with the host process streams.
func Main() {
	runner := systemRunner{stdout: os.Stdout, stderr: os.Stderr}
	if err := run(os.Args[1:], runner); err != nil {
		fmt.Fprintf(os.Stderr, "\nerror: %v\n", err)
		os.Exit(1)
	}
}

func executeWorkflow(arguments []string, runner commandRunner) error {
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
	case "incremental-clean-build", "cold-clean-build":
		if len(commandArguments) != 0 || len(bazelArguments) != 0 {
			return fmt.Errorf("%s does not accept arguments", strings.ToLower(arguments[0]))
		}
		return runCleanBuild(repository, currentHost, runner, strings.EqualFold(arguments[0], "cold-clean-build"))
	case "benchmark":
		if len(commandArguments) != 0 {
			return errors.New("benchmark accepts Bazel options only after --")
		}
		if err := requireBuildTools(currentHost, runner); err != nil {
			return err
		}
		return runBenchmarks(repository, currentHost, runner, bazelArguments)
	case "bundle":
		if len(commandArguments) != 0 {
			return errors.New("bundle accepts Bazel options only after --")
		}
		if err := requireBuildTools(currentHost, runner); err != nil {
			return err
		}
		return buildBundle(repository, currentHost, runner, bazelArguments)
	case "fuzz":
		if len(commandArguments) != 0 || len(bazelArguments) != 0 {
			return errors.New("fuzz does not accept arguments")
		}
		if err := requireBuildTools(currentHost, runner); err != nil {
			return err
		}
		return runFuzzCampaign(repository, currentHost, runner, false, false)
	case "fuzz-stress":
		if len(bazelArguments) != 0 || (len(commandArguments) != 0 && !(len(commandArguments) == 1 && strings.EqualFold(commandArguments[0], "--asan"))) {
			return errors.New("fuzz-stress accepts only the optional --asan flag")
		}
		if err := requireBuildTools(currentHost, runner); err != nil {
			return err
		}
		return runFuzzCampaign(repository, currentHost, runner, true, len(commandArguments) == 1)
	case "version":
		if len(commandArguments) != 1 || len(bazelArguments) != 0 {
			return errors.New("version requires exactly one major.minor.patch[.revision] argument")
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

func isHelp(argument string) bool {
	return argument == "--help" || argument == "-h"
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

func findRepositoryRoot() (string, error) {
	directory, err := os.Getwd()
	if err != nil {
		return "", fmt.Errorf("cannot read the current directory: %w", err)
	}
	return repository.FindRoot(directory)
}
