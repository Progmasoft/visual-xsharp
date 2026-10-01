// SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
// SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.1

//go:build !windows

package development

import "syscall"

// processAlive reports whether a process with this identifier still exists.
func processAlive(pid int) bool {
	return syscall.Kill(pid, 0) == nil
}
