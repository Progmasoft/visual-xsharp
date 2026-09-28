// SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
// SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.1

package development

import (
	"io"
	"os"
	"os/exec"
	"runtime"
	"strings"
)

type commandRunner interface {
	Run(directory string, environment []string, name string, arguments ...string) error
	RunWithInput(directory string, environment []string, input string, name string, arguments ...string) (string, error)
	Output(name string, arguments ...string) (string, error)
	OutputIn(directory string, name string, arguments ...string) (string, error)
	LookPath(name string) (string, error)
}

type systemRunner struct {
	stdout io.Writer
	stderr io.Writer
}

func (runner systemRunner) Run(directory string, environment []string, name string, arguments ...string) error {
	command := exec.Command(name, arguments...)
	command.Dir = directory
	command.Env = mergedEnvironment(environment)
	command.Stdin = os.Stdin
	command.Stdout = runner.stdout
	command.Stderr = runner.stderr
	return command.Run()
}

// RunWithInput drives programs with deterministic stdin while keeping captured
// output available to the caller for smoke-test assertions.
func (runner systemRunner) RunWithInput(directory string, environment []string, input string, name string, arguments ...string) (string, error) {
	command := exec.Command(name, arguments...)
	command.Dir = directory
	command.Env = mergedEnvironment(environment)
	command.Stdin = strings.NewReader(input)
	output, err := command.CombinedOutput()
	return string(output), err
}

// mergedEnvironment replaces inherited keys rather than appending duplicate
// PATH entries whose precedence differs between host process APIs.
func mergedEnvironment(overrides []string) []string {
	return mergeEnvironment(os.Environ(), overrides, runtime.GOOS == "windows")
}

func mergeEnvironment(base []string, overrides []string, windows bool) []string {
	environment := append([]string(nil), base...)
	for _, override := range overrides {
		key, _, found := strings.Cut(override, "=")
		if !found || key == "" {
			environment = append(environment, override)
			continue
		}
		filtered := environment[:0]
		for _, entry := range environment {
			entryKey, _, hasValue := strings.Cut(entry, "=")
			matches := hasValue && entryKey == key
			if windows {
				matches = hasValue && strings.EqualFold(entryKey, key)
			}
			if !matches {
				filtered = append(filtered, entry)
			}
		}
		environment = append(filtered, override)
	}
	return environment
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
