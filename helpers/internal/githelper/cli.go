// SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
// SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.1

package githelper

import (
	"errors"
	"fmt"
	"io"
	"os"

	"github.com/spf13/cobra"
)

// coAuthorVariable names the environment variable that supplies the default
// co-author, so an identity never has to be written into a command line or
// into this source.
const coAuthorVariable = "VXS_GITHELPER_CO_AUTHOR"

const longDescription = `Guarded Git workflow for the Visual X# repositories.

The helper stages safely, commits, and pushes without ever rewriting remote
history. It keeps generated output and private agent notes out of the index,
refuses to commit on the default branch, and applies the co-author rule from
the staged paths instead of from memory.

Typical topic-branch flow:
  githelper start feature/name
  githelper update "Describe the change"
  githelper sync            # merge the default branch, then test
  githelper push            # publish the merge`

// newCommand owns parsing independently of Git, so an invalid invocation
// cannot stage, commit or push anything.
func newCommand(runner gitRunner, output, errorOutput io.Writer, environment func(string) string) *cobra.Command {
	root := &cobra.Command{
		Use:           "githelper",
		Short:         "Guarded Git workflow for the Visual X# repositories",
		Long:          longDescription,
		SilenceUsage:  true,
		SilenceErrors: true,
		Args:          cobra.NoArgs,
		RunE:          func(command *cobra.Command, _ []string) error { return command.Help() },
	}
	root.SetOut(output)
	root.SetErr(errorOutput)
	root.CompletionOptions.DisableDefaultCmd = true

	inRepository := func(action func(*cobra.Command, []string) error) func(*cobra.Command, []string) error {
		return func(command *cobra.Command, arguments []string) error {
			if err := requireWorkTree(runner); err != nil {
				return err
			}
			return action(command, arguments)
		}
	}

	options := updateOptions{}
	messageFile := ""
	updateCommand := &cobra.Command{
		Use:   "update [message]",
		Short: "Stage every change, commit, and push the current branch",
		Long: `Stage every change, commit, and push the current branch.

Generated output and files covered by an ignore rule are removed from the
index before and after staging. The commit message is the argument or the
content of --message-file. The push never forces.

When a co-author is configured, its Co-Authored-By trailer is added only to a
commit that changes documentation alone or helpers/ alone; a commit that
contains code never carries it.`,
		Args: cobra.MaximumNArgs(1),
		RunE: inRepository(func(_ *cobra.Command, arguments []string) error {
			message, err := resolveMessage(arguments, messageFile)
			if err != nil {
				return err
			}
			options.message = message
			if options.coAuthor == "" {
				options.coAuthor = environment(coAuthorVariable)
			}
			return update(runner, output, options)
		}),
	}
	updateCommand.Flags().StringVarP(&messageFile, "message-file", "F", "", "read the commit message from this file")
	updateCommand.Flags().StringVar(&options.coAuthor, "co-author", "", `co-author as "Name <address>"; default from `+coAuthorVariable)
	updateCommand.Flags().BoolVar(&options.dryRun, "dry-run", false, "show what would be committed and change nothing")
	updateCommand.Flags().BoolVar(&options.noPush, "no-push", false, "commit without pushing")
	updateCommand.Flags().BoolVar(&options.allowDefaultBranch, "allow-default-branch", false, "permit a commit on the default branch")

	root.AddCommand(
		updateCommand,
		&cobra.Command{
			Use:   "push",
			Short: "Push existing commits of the current branch without creating one",
			Args:  cobra.NoArgs,
			RunE:  inRepository(func(*cobra.Command, []string) error { return push(runner) }),
		},
		&cobra.Command{
			Use:   "sync",
			Short: "Merge the remote default branch into the current branch",
			Long: `Fetch and merge the remote default branch into the current branch.

The work tree must be clean. Nothing is pushed: test the merged tree, then
publish it with "githelper push".`,
			Args: cobra.NoArgs,
			RunE: inRepository(func(*cobra.Command, []string) error { return sync(runner, output) }),
		},
		&cobra.Command{
			Use:   "start <branch>",
			Short: "Create a topic branch from the remote default branch",
			Args:  cobra.ExactArgs(1),
			RunE: inRepository(func(_ *cobra.Command, arguments []string) error {
				return start(runner, output, arguments[0])
			}),
		},
		&cobra.Command{
			Use:     "status",
			Aliases: []string{"uncom"},
			Short:   "Show the branch, its upstream state, and uncommitted changes",
			Args:    cobra.NoArgs,
			RunE:    inRepository(func(*cobra.Command, []string) error { return status(runner, output) }),
		},
		&cobra.Command{
			Use:   "clean",
			Short: "Remove generated and ignored files from the index without committing",
			Args:  cobra.NoArgs,
			RunE:  inRepository(func(*cobra.Command, []string) error { return cleanIndex(runner) }),
		},
	)
	return root
}

// resolveMessage takes the commit message from exactly one source.
func resolveMessage(arguments []string, messageFile string) (string, error) {
	switch {
	case len(arguments) == 1 && messageFile != "":
		return "", exitError{code: 2, message: "error: give the commit message as an argument or with --message-file, not both"}
	case len(arguments) == 1:
		return arguments[0], nil
	case messageFile != "":
		content, err := os.ReadFile(messageFile)
		if err != nil {
			return "", failf("the commit message file could not be read: %v", err)
		}
		return string(content), nil
	default:
		return "", exitError{code: 2, message: "error: a commit message is required"}
	}
}

// Execute runs the helper with the given arguments and returns the process
// exit code.
func Execute(arguments []string, stdin io.Reader, stdout, stderr io.Writer) int {
	runner := processRunner{stdin: stdin, stdout: stdout, stderr: stderr}
	command := newCommand(runner, stdout, stderr, os.Getenv)
	command.SetArgs(arguments)
	if err := command.Execute(); err != nil {
		fmt.Fprintln(stderr, err)
		var failure exitError
		if errors.As(err, &failure) {
			return failure.code
		}
		return 2
	}
	return 0
}

// Main is the entry point of the githelper command.
func Main() {
	os.Exit(Execute(os.Args[1:], os.Stdin, os.Stdout, os.Stderr))
}
