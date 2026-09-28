// SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
// SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.1

package development

import (
	"bytes"
	"strings"
	"testing"
)

func TestCommandTreeHelpDoesNotDiscoverTools(t *testing.T) {
	for _, args := range [][]string{{}, {"--help"}, {"build", "--help"}, {"fuzz-stress", "--help"}, {"help", "sanitize"}} {
		var out bytes.Buffer
		command := newCommand(fakeRunner{}, &out, &out)
		command.SetArgs(args)
		if err := command.Execute(); err != nil {
			t.Fatalf("%v: %v", args, err)
		}
		if !strings.Contains(out.String(), "Usage:") {
			t.Fatalf("missing help: %s", out.String())
		}
	}
}

func TestCommandTreeRejectsInvalidInputsBeforeExecution(t *testing.T) {
	for _, args := range [][]string{
		{"unknown"}, {"doctor", "extra"}, {"build", "--jobs=4"}, {"build", "--", "--config=private"},
		{"sanitize"}, {"sanitize", "address", "extra"}, {"version"}, {"clean", "extra"},
		{"fuzz-stress", "--unknown"}, {"fuzz-stress", "--asan=invalid"}, {"fuzz-stress", "extra"}, {"-Help"},
	} {
		command := newCommand(fakeRunner{}, &bytes.Buffer{}, &bytes.Buffer{})
		command.SetArgs(args)
		err := command.Execute()
		if err == nil {
			t.Fatalf("accepted %v", args)
		}
		if strings.Contains(err.Error(), "unexpected process") {
			t.Fatalf("executed invalid command %v: %v", args, err)
		}
	}
}
