// SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
// SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.1

package development

import (
	"context"
	"fmt"
	"io"
	"os"
	"os/exec"
	"path/filepath"
	"runtime"
	"strconv"
	"strings"
	"time"
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
	command, finish := watchedCommand(name, arguments)
	defer finish(nil)
	command.Dir = directory
	command.Env = mergedEnvironment(environment)
	command.Stdin = os.Stdin
	command.Stdout = runner.stdout
	command.Stderr = runner.stderr
	return finish(command.Run())
}

// RunWithInput drives programs with deterministic stdin while keeping captured
// output available to the caller for smoke-test assertions.
func (runner systemRunner) RunWithInput(directory string, environment []string, input string, name string, arguments ...string) (string, error) {
	command, finish := watchedCommand(name, arguments)
	defer finish(nil)
	command.Dir = directory
	command.Env = mergedEnvironment(environment)
	command.Stdin = strings.NewReader(input)
	output, err := command.CombinedOutput()
	return string(output), finish(err)
}

// watchdogGrace bounds how long a cancelled program may hold its output pipes
// open after its process tree was terminated.
const watchdogGrace = 10 * time.Second

// watchedCommand bounds fuzz and smoke programs. libFuzzer's -timeout covers one
// input, but a deterministic smoke program or an HPC campaign has no in-process
// watchdog, and a miscompiled generated loop never returns. The deadline
// terminates the whole process tree rather than only the direct child, and the
// returned finish function reports expiry as a watchdog failure instead of the
// host's generic kill status. Build tools are not bounded here; they keep their
// CI job timeout.
func watchedCommand(name string, arguments []string) (*exec.Cmd, func(error) error) {
	return watchedCommandWithin(fuzzProcessSeconds(name, arguments), name, arguments)
}

func watchedCommandWithin(seconds int, name string, arguments []string) (*exec.Cmd, func(error) error) {
	if seconds == 0 {
		return exec.Command(name, arguments...), func(err error) error { return err }
	}
	ctx, cancel := context.WithTimeout(context.Background(), time.Duration(seconds)*time.Second)
	command := exec.CommandContext(ctx, name, arguments...)
	prepareProcessTree(command)
	command.Cancel = func() error { return terminateProcessTree(command) }
	command.WaitDelay = watchdogGrace
	return command, func(err error) error {
		expired := ctx.Err() == context.DeadlineExceeded
		cancel()
		if err != nil && expired {
			return fmt.Errorf("%s exceeded its %d-second process watchdog and its process tree was terminated: %w", filepath.Base(name), seconds, err)
		}
		return err
	}
}

func fuzzProcessSeconds(name string, arguments []string) int {
	binary := strings.TrimSuffix(filepath.Base(name), ".exe")
	if binary == "source_fuzz_smoke" || binary == "wire_fuzz_smoke" {
		return 90
	}
	if binary == "frontend-fuzz" && len(arguments) >= 2 {
		if seconds, err := strconv.Atoi(arguments[1]); err == nil && seconds >= 1 && seconds <= 3600 {
			return seconds + 300 // bounded corpus warmup and shutdown allowance
		}
	}
	if strings.HasSuffix(binary, "_fuzzer") {
		for _, argument := range arguments {
			if value, ok := strings.CutPrefix(argument, "-max_total_time="); ok {
				if seconds, err := strconv.Atoi(value); err == nil && seconds >= 1 && seconds <= 3600 {
					return seconds + 90
				}
			}
		}
	}
	return 0
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
