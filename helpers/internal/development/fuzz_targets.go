// SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
// SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.1

package development

// Labels, corpus identities and limits share one inventory so a component-local
// harness cannot silently disappear from the nightly run after a directory move.
type fuzzTarget struct {
	label, binary, corpus, maxLength, rssLimit string
	// frontend targets load the Haskell shared library beside the executable.
	frontend bool
	// threaded targets start their own worker threads. Only these gain evidence
	// from a ThreadSanitizer campaign; the others execute one thread per input.
	threaded bool
}

func nativeFuzzTargets() []fuzzTarget {
	return []fuzzTarget{
		{"//Compiler/Fuzzing:wire_fuzzer", "wire_fuzzer", "wire", "16384", "768", false, false},
		{"//Compiler/Fuzzing:lexer_fuzzer", "lexer_fuzzer", "lexer", "65536", "1024", true, false},
		{"//Compiler/Fuzzing:parser_fuzzer", "parser_fuzzer", "parser", "65536", "1536", true, false},
		{"//Compiler/Fuzzing:source_llvm_fuzzer", "source_llvm_fuzzer", "source", "65536", "4096", true, false},
		// The generator reads at most 33 bytes: 31 expression selectors at depth
		// four, then one shape and one trip-count byte. Bound mutations close to
		// that semantic input instead of evolving unused tails.
		{"//Compiler/Fuzzing:differential_fuzzer", "differential_fuzzer", "differential", "64", "4096", true, false},
		{"//Compiler/Cli/Fuzzing:cli_fuzzer", "cli_fuzzer", "cli", "16384", "768", false, false},
		{"//Compiler/ProjectSystem/Bridge/Fuzzing:project_fuzzer", "project_fuzzer", "project", "65536", "768", false, false},
		{"//Interactive/Fuzzing:repl_fuzzer", "repl_fuzzer", "repl", "64", "4096", true, false},
		{"//Compiler/Runtime/AARC/Fuzzing:ownership_fuzzer", "ownership_fuzzer", "ownership", "64", "768", false, true},
	}
}

// threadFuzzTargets selects the harnesses whose oracle depends on interleaving.
func threadFuzzTargets() []fuzzTarget {
	var targets []fuzzTarget
	for _, target := range nativeFuzzTargets() {
		if target.threaded {
			targets = append(targets, target)
		}
	}
	return targets
}
