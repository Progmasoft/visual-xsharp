// SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
// SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.1

package githelper

import (
	"fmt"
	"path"
	"sort"
	"strings"
)

// commitScope classifies what one commit touches. The classes decide whether
// a configured co-author trailer applies.
type commitScope int

const (
	// scopeEmpty has no staged path.
	scopeEmpty commitScope = iota
	// scopeDocumentation changes documentation files only.
	scopeDocumentation
	// scopeHelpers changes files under helpers/ only.
	scopeHelpers
	// scopeCode changes anything else, alone or mixed with the other classes.
	scopeCode
)

func (scope commitScope) String() string {
	switch scope {
	case scopeEmpty:
		return "nothing"
	case scopeDocumentation:
		return "documentation only"
	case scopeHelpers:
		return "helpers only"
	default:
		return "code"
	}
}

var documentationExtensions = map[string]struct{}{".md": {}, ".markdown": {}, ".rst": {}, ".adoc": {}}

// isDocumentation reports whether a repository path is prose documentation:
// anything under Documents/, or a documentation file format anywhere else.
// Language examples under Spec/ are normative source, not documentation.
func isDocumentation(repositoryPath string) bool {
	if strings.HasPrefix(repositoryPath, "Documents/") {
		return true
	}
	_, known := documentationExtensions[strings.ToLower(path.Ext(repositoryPath))]
	return known
}

func isHelper(repositoryPath string) bool {
	return strings.HasPrefix(repositoryPath, "helpers/")
}

// classify returns the scope of a set of staged paths. A commit that mixes
// documentation with helpers is neither class alone and counts as code, so a
// trailer is only ever added to a commit of exactly one of the two classes.
func classify(paths []string) commitScope {
	if len(paths) == 0 {
		return scopeEmpty
	}
	documentation, helpers := 0, 0
	for _, stagedPath := range paths {
		switch {
		case isHelper(stagedPath):
			helpers++
		case isDocumentation(stagedPath):
			documentation++
		default:
			return scopeCode
		}
	}
	switch {
	case helpers == len(paths):
		return scopeHelpers
	case documentation == len(paths):
		return scopeDocumentation
	default:
		return scopeCode
	}
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
// applied or withheld according to the scope rule: the trailer belongs only
// on a commit that changes documentation alone or helpers alone. A trailer
// for the same co-author that the caller already wrote is removed from a code
// commit and not duplicated on the others.
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
	if scope != scopeDocumentation && scope != scopeHelpers {
		return body + "\n"
	}
	return body + "\n\n" + trailer + "\n"
}
