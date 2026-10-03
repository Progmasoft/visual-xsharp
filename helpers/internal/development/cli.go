// SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
// SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.1

package development

import (
	"fmt"
	"io"
	"os"

	"github.com/spf13/cobra"
)

// newCommand owns parsing independently of tool discovery and execution. Invalid
// invocations therefore cannot trigger downloads, builds, or generated cleanup.
func newCommand(runner commandRunner, output, errorOutput io.Writer) *cobra.Command {
	root := &cobra.Command{
		Use:           "develop",
		Short:         "Visual X# native developer command",
		Long:          "Visual X# native developer command\n\nBuild, verify, package, and diagnose the native toolchain.",
		SilenceUsage:  true,
		SilenceErrors: true,
		Args:          cobra.NoArgs,
		RunE:          func(cmd *cobra.Command, args []string) error { return cmd.Help() },
	}
	root.SetOut(output)
	root.SetErr(errorOutput)
	root.CompletionOptions.DisableDefaultCmd = true
	type commandSpec struct {
		name, description string
		positionals       int
		forward           bool
	}
	specs := []commandSpec{
		{"doctor", "Explain whether this host has the required native toolchain.", 0, false},
		{"build", "Build the compiler and native contract suites.", 0, true},
		{"benchmark", "Run native and Haskell compiler benchmarks.", 0, true},
		{"bundle", "Stage, checksum, and smoke-test a host distribution.", 0, true},
		{"test", "Build and execute every native contract suite.", 0, true},
		{"sanitize", "Run address, undefined, address-undefined, or thread sanitizer suites.", 1, true},
		{"tidy", "Run clang-tidy over every first-party C++ translation unit.", 0, true},
		{"version", "Validate major.minor.patch[.revision] release metadata.", 1, false},
		{"fuzz", "Run bounded ASan/UBSan libFuzzer campaigns and the Haskell HPC campaign.", 0, false},
		{"fuzz-thread", "Run the threaded fuzz targets under ThreadSanitizer (macOS/Linux).", 0, false},
		{"incremental-clean-build", "Rebuild while preserving downloaded dependencies.", 0, false},
		{"cold-clean-build", "Expunge Bazel state and rebuild the compiler.", 0, false},
		{"clean", "Remove generated Bazel, Cabal, and Gradle output.", 0, false},
	}
	for _, spec := range specs {
		use := spec.name
		if spec.positionals != 0 {
			use += " <value>"
		}
		if spec.forward {
			use += " [-- <Bazel options>]"
		}
		child := &cobra.Command{Use: use, Short: spec.description, DisableFlagParsing: spec.forward}
		child.Args = func(cmd *cobra.Command, args []string) error {
			if spec.forward && len(args) == 1 && isHelp(args[0]) {
				return nil
			}
			positionals, trailing, err := splitArguments(args)
			if err != nil {
				return err
			}
			if !spec.forward && len(trailing) != 0 {
				return fmt.Errorf("%s does not accept Bazel options", spec.name)
			}
			if len(positionals) != spec.positionals {
				return fmt.Errorf("%s requires %d positional arguments; Bazel options belong after --", spec.name, spec.positionals)
			}
			return nil
		}
		child.RunE = func(cmd *cobra.Command, args []string) error {
			if spec.forward && len(args) == 1 && isHelp(args[0]) {
				return cmd.Help()
			}
			return executeWorkflow(append([]string{spec.name}, args...), runner)
		}
		root.AddCommand(child)
	}
	var asan bool
	stress := &cobra.Command{
		Use:   "fuzz-stress",
		Short: "Run long per-target fuzz campaigns with ASan and UBSan.",
		Args:  cobra.NoArgs,
		RunE: func(cmd *cobra.Command, args []string) error {
			arguments := []string{"fuzz-stress"}
			if asan {
				arguments = append(arguments, "--asan")
			}
			return executeWorkflow(arguments, runner)
		},
	}
	stress.Flags().BoolVar(&asan, "asan", false, "compatibility flag; ASan and UBSan are always enabled")
	root.AddCommand(stress)
	return root
}

func run(arguments []string, runner commandRunner) error {
	command := newCommand(runner, os.Stdout, os.Stderr)
	command.SetArgs(arguments)
	return command.Execute()
}
