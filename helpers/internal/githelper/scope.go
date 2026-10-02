// SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
// SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.1

package githelper

import (
	"fmt"
	"path"
	"sort"
	"strings"
)

// commitScope classifies what one commit touches. The class decides whether
// a configured co-author trailer applies.
type commitScope int

const (
	// scopeEmpty has no staged path.
	scopeEmpty commitScope = iota
	// scopeWithoutCode changes no programming-language source outside
	// helpers/: documentation, the Go helpers, configuration, data.
	scopeWithoutCode
	// scopeCode changes at least one programming-language source file
	// outside helpers/, alone or together with anything else.
	scopeCode
)

func (scope commitScope) String() string {
	switch scope {
	case scopeEmpty:
		return "nothing"
	case scopeWithoutCode:
		return "no code"
	default:
		return "code"
	}
}

// codeExtensions are the source files of programming languages. A file with
// one of these extensions is code wherever it lives, except the Go helpers.
var codeExtensions = map[string]struct{}{
	".c": {}, ".h": {}, ".cc": {}, ".cpp": {}, ".cxx": {}, ".hh": {}, ".hpp": {}, ".hxx": {}, ".inc": {}, ".ipp": {},
	".m": {}, ".mm": {}, ".hs": {}, ".lhs": {}, ".cs": {}, ".fs": {}, ".vb": {}, ".kt": {}, ".kts": {},
	".java": {}, ".groovy": {}, ".gradle": {}, ".scala": {}, ".ts": {}, ".tsx": {}, ".mts": {}, ".cts": {},
	".js": {}, ".jsx": {}, ".mjs": {}, ".cjs": {}, ".go": {}, ".rs": {}, ".swift": {}, ".py": {}, ".rb": {},
	".lua": {}, ".php": {}, ".vxs": {}, ".ll": {}, ".s": {}, ".asm": {}, ".sh": {}, ".bash": {}, ".ps1": {},
	".psm1": {}, ".bat": {}, ".cmd": {},
}

func isHelper(repositoryPath string) bool {
	return strings.HasPrefix(repositoryPath, "helpers/")
}

// isCode reports whether a repository path is programming-language source
// that counts as code for the co-author rule. Everything under helpers/ is
// exempt: the Go helpers are the one place where source may carry the
// trailer.
func isCode(repositoryPath string) bool {
	if isHelper(repositoryPath) {
		return false
	}
	_, known := codeExtensions[strings.ToLower(path.Ext(repositoryPath))]
	return known
}

// classify returns the scope of a set of staged paths: code as soon as one
// path is code, whatever else the commit contains.
func classify(paths []string) commitScope {
	if len(paths) == 0 {
		return scopeEmpty
	}
	for _, stagedPath := range paths {
		if isCode(stagedPath) {
			return scopeCode
		}
	}
	return scopeWithoutCode
}

// stagedPaths lists every path the index changes relative to HEAD, including
// both sides of a rename.
func stagedPaths(runner gitRunner) ([]string, error) {
	output, err := runner.Capture("diff", "--cached", "--name-only", "--no-renames", "-z")
	if err != nil {
		return nil, err
	}
	paths := splitNullSeparated(output)
	sort.Strings(paths)
	return paths, nil
}

// describeScope summarizes staged paths by top-level directory, so the scope
// of a commit is visible before it is created.
func describeScope(paths []string) string {
	counts := map[string]int{}
	for _, stagedPath := range paths {
		root, _, nested := strings.Cut(stagedPath, "/")
		if !nested {
			root = "(repository root)"
		}
		counts[root]++
	}
	roots := make([]string, 0, len(counts))
	for root := range counts {
		roots = append(roots, root)
	}
	sort.Strings(roots)
	var summary strings.Builder
	fmt.Fprintf(&summary, "%d staged path(s), %s:\n", len(paths), classify(paths))
	for _, root := range roots {
		fmt.Fprintf(&summary, "  %4d  %s\n", counts[root], root)
	}
	return summary.String()
}

const coAuthorTrailer = "Co-Authored-By: "

// composeMessage returns the commit message with the co-author trailer
// applied or withheld according to the scope rule: the trailer belongs on a
// commit that contains no code and never on one that does. A trailer for the
// same co-author that the caller already wrote is removed from a code commit
// and not duplicated on the others.
func composeMessage(message string, coAuthor string, scope commitScope) string {
	body := strings.TrimRight(strings.ReplaceAll(message, "\r\n", "\n"), "\n ")
	if coAuthor == "" {
		return body + "\n"
	}
	trailer := coAuthorTrailer + coAuthor
	lines := strings.Split(body, "\n")
	kept := lines[:0]
	for _, line := range lines {
		if !strings.EqualFold(strings.TrimSpace(line), trailer) {
			kept = append(kept, line)
		}
	}
	body = strings.TrimRight(strings.Join(kept, "\n"), "\n ")
	if scope != scopeWithoutCode {
		return body + "\n"
	}
	return body + "\n\n" + trailer + "\n"
}
