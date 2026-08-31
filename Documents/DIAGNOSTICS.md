<!-- SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com> -->
<!-- SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.0 -->

# Diagnostics and failure behavior

Diagnostics are a compiler interface. They must identify the stage that owns a failure, preserve source context when one
exists, and prevent invalid or stale artifacts from appearing successful.

## Output channels

`vxs` writes ordinary requested output to standard output and diagnostics to standard error. Help and version are parser
outcomes rendered by the driver; parsing itself does not print. This separation lets tests and embedding clients inspect a
typed parse result without redirecting global streams.

Compilation, project evaluation, tool discovery, malformed artifact, target-machine, and link failures return a nonzero
status. `vxs run` returns a build failure without launching anything, and after a successful build it propagates the native
program's exit status.

## Diagnostic ownership

| Failure class | Owning layer |
| --- | --- |
| unknown command or option | C++20 CLI parser |
| missing/duplicate/out-of-scope option | C++20 CLI parser |
| invalid typed option value | C++20 CLI parser |
| project discovery or DSL validation | Kotlin project evaluator |
| source-root containment or UTF-8 decoding | Haskell source loader |
| token spelling, escape, or literal structure | Haskell Lexer |
| grammar and precedence | Haskell Parser |
| duplicate lexical binding | Haskell Renamer |
| unresolved or ambiguous reference | Haskell Name Resolution |
| type, call, return, operator, or entry rule | Haskell Type Checker |
| malformed Core or CorePrep semantics | corresponding Core verifier |
| malformed Xpp or optimization result | Xpp verifier |
| malformed Xmm or optimization result | Xmm verifier |
| missing target layout or invalid LLVM module | LLVM backend |
| object/assembly emission failure | LLVM target machine boundary |
| executable link failure | C++20 LLD driver |
| missing Formatter or Linter installation | C++20 project-tool dispatch |

A later layer should not reinterpret an earlier layer's error. For example, an unresolved name is not reported as a missing
LLVM symbol, and a malformed integer token is not split into a valid prefix plus an unrelated identifier.

## CLI parse diagnostics

The CLI schema owns canonical spelling, command scope, arity, and value domains. Useful parse failures distinguish:

- unknown command;
- unknown option;
- option valid for another command but not this command;
- missing option value;
- duplicate option;
- invalid Boolean or enumerated value;
- unexpected positional argument;
- missing required positional package coordinate;
- invalid `-Build` and `-File` combination; and
- invalid process argument vector.

Option and value spelling is case-sensitive. The diagnostic should repeat the rejected spelling and, where bounded, the
accepted domain. It must not silently accept `--help`, lowercase a target-sensitive identifier, or reinterpret a misspelled
command as a file.

`-Help` is contextual. Global help lists commands and global syntax; `vxs build -Help` includes build-only `-Emit`; `vxs
check -Help` does not. Help is a successful outcome and does not start project evaluation or compilation.

## Source positions

Source diagnostics should retain:

- canonical project-relative file identity;
- one-based line and column for user display;
- a span covering the smallest relevant source form;
- the primary message;
- related declaration locations when ambiguity or duplication involves more than one site; and
- the stage/rule identity needed by Analyzer and Linter clients.

Byte offsets are an implementation detail and must not be presented as Unicode character indexes. Visual X# source is
decoded as UTF-8, while runtime `String` semantics use Unicode scalar values. Diagnostics must not confuse UTF-8 byte count,
UTF-16 code units, and scalar positions.

When recovery is possible, later messages should be suppressed if they are direct consequences of one missing delimiter or
malformed token. Recovery is for discovering independent errors, not maximizing the message count.

## Severity and warnings

Compiler warning policy is controlled by the resolved CLI/project settings:

```text
-Warnings all|medium|low|none
-Werror true|false
-Wexperimental true|false
-Wshadow true|false
-Wundef true|false
```

`-Werror` changes the build result of an emitted warning; it does not rewrite the diagnostic's semantic identity into an
unrelated error category. Experimental, shadowing, and undefined-name controls are explicit settings and follow the same
CLI-over-project precedence as other compiler settings.

Visual Linter has its own rule severity model. Compiler diagnostics and linter diagnostics can appear in one Analyzer
session, but their configuration and version lines remain independent.

## Artifact diagnostics

Public Core decoding is hostile-input parsing. Reader failures should state the violated contract without dumping arbitrary
document bytes. Relevant categories include:

- wrong magic or wire version;
- truncated field;
- document, text, collection, type-depth, or expression-depth limit exceeded;
- invalid UTF-32 scalar in a string value;
- duplicate or missing symbol;
- mismatched function/call signature;
- invalid branch target or missing terminator;
- unsupported payload in the current wire revision; and
- private CorePrep bytes supplied as public Core.

Xpp and Xmm have no connected public readers yet. Their verifier messages are still structured boundary failures for tests
and in-process clients.

## Safe output behavior

Failure must not make an old file look newly built.

- `check` writes no output artifact.
- A build replaces its selected output only after the producing stage succeeds.
- `run` records the exact artifact from the current invocation and never executes a pre-existing `.vxse` after failure.
- Binary emission removes temporary objects after success and failure.
- An ambiguous per-source output stem is rejected before either input overwrites the other.
- Project evaluation aborted by `panic` emits no partial plan and does not begin compilation.
- VXDC refuses to overwrite the binary lock database with a text dump.

When atomic replacement is available, writers should create a sibling temporary file, flush/close it, and replace the
destination only after validation. A failure message should mention both the requested destination and the underlying error,
without exposing credentials or unrelated environment contents.

## Tool discovery failures

Project `format` and `lint` commands require separately installed `vfmt` and `vlint`. If the executable is unavailable, the
message names the missing binary and the ViGet package used to install it. The compiler does not fall back to an embedded
formatter/linter or silently skip the command.

The private Haskell frontend is different: it is part of the compiler distribution and is resolved from the running
compiler's layout. A missing frontend is an installation/build-layout failure, not a suggestion to install a random
executable from `PATH`.

LLVM discovery failures belong to Bazel analysis or build configuration. Repository diagnostics should instruct the user to
set `LLVM_ROOT` or expose `llvm-config`; they must not recommend committing an absolute machine path.

## Analyzer and machine-readable use

The compiler pipeline keeps semantic rule decisions separate from final message construction. That lets Visual Analyzer and
Visual Linter reuse symbol, type, range, and control-flow facts without parsing human prose.

A stable machine-facing diagnostic needs:

- an owning subsystem;
- a stable diagnostic or rule identifier;
- severity;
- source range or artifact context;
- message arguments separate from the rendered sentence; and
- optional related locations and safe fixes.

The current CLI is text-oriented. A future structured protocol must be versioned rather than inferred from terminal output.
Until then, external tools should use the compiler libraries/frontends they own instead of scraping `vxs` wording.

## Security and privacy

Diagnostics may include source paths, package coordinates, target triples, and compiler tool paths needed to fix the error.
They must not print environment-variable values, OAuth secrets, registry tokens, mail passwords, signing keys, or complete
service responses containing credentials.

Malformed source and artifacts can contain control characters. Terminal rendering should escape or delimit untrusted
spellings so a diagnostic cannot forge another line or terminal control sequence.

## Diagnostic review checklist

Before merging a new failure path, verify:

1. the earliest owning stage reports it;
2. the message names the rejected thing and the expected contract;
3. a source span or artifact path is attached when meaningful;
4. error recovery does not generate a misleading cascade;
5. the command returns nonzero unless this is help/version or a warning allowed by policy;
6. no output or temporary artifact survives incorrectly;
7. tests assert the diagnostic category and essential context; and
8. secrets and arbitrary unescaped bytes are not printed.
