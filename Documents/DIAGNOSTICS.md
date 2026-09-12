<!-- SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com> -->
<!-- SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.1 -->

# Diagnostics and failure behavior

Diagnostics are a compiler interface. They must identify the stage that owns a failure, preserve source context when one
exists, and prevent invalid or stale artifacts from appearing successful.

## Structured tooling channel

Tools must consume the [VXDG structured diagnostic protocol](DIAGNOSTIC-PROTOCOL.md), not parse stderr. The versioned
side channel carries stage, severity, stable code, source range, named message arguments, related locations, and fix
descriptions. It remains private to trusted local compiler integrations and is not a user-selectable emit format.

The Haskell frontend converts its one-based source positions to the protocol's zero-based scalar coordinates. The C++
implementation validates and accumulates native-stage diagnostics using the same record model. Xide owns an independent
bounded Kotlin decoder, so compatibility is tested across all three implementation languages instead of being assumed.

### Accumulation semantics

Native compiler stages use `Visual::XSharp::Diagnostic::Collection` when more than one component contributes records.
The collection:

- validates a record before accepting it;
- preserves the first-emission order;
- coalesces only byte-identical records;
- counts errors and warnings without treating information or hints as failures;
- applies a configured record limit;
- merges a document transactionally;
- supports snapshot, transfer, clear, and reuse without retaining stale identities.

Transactional merge matters when one stage returns a batch. If any new record is invalid or would exceed capacity, none
of that batch becomes observable. An exact duplicate is successful but does not consume capacity or increment severity
counts. Messages with different arguments, locations, related context, or fixes remain distinct even when their codes
match.

### Side-channel lifecycle

The frontend writes a complete empty VXDG document after a successful check. On failure it writes the structured records
before printing the human-readable diagnostics and exiting. This makes the protocol independent of terminal wording and
ensures that an old failure file cannot survive as the apparent result of a new success.

If `VXS_DIAGNOSTICS_FILE` is absent, no protocol file is written. The variable is an implementation boundary passed by
the native driver or a trusted editor client; it is intentionally absent from public CLI help. A configured empty path,
an invalid diagnostic model, an encoding failure, or an I/O failure makes the frontend fail rather than silently leaving
the requesting tool without a trustworthy result.

Consumers use a fresh temporary path per process and remove it on every outcome. They must treat a missing file after a
normal process exit, malformed protocol bytes, unsupported versions, and oversized documents as integration failures.
Human stderr may be displayed as supplemental detail, but it must never be reparsed into synthetic diagnostic records.

### Document-version ownership

VXDG version 1 describes the disk snapshot compiled by `vxs`; it does not carry an editor document version. Xide records
the active version when it starts a check and publishes the resulting diagnostics only if that version remains current.
Edits made while the compiler runs therefore invalidate the result. Fixes require the same version guard before their
text edits can be applied.

Protocol columns count Unicode scalar values, whereas Kotlin strings use UTF-16 code units. An editor must map through
its line model and must not add a scalar column directly to a JVM string offset. This distinction is required for correct
highlighting after supplementary characters.

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

### Source parser commitments

The Haskell parser records token consumption while it recognizes a construct.
An alternative may run only if the previous branch consumed no tokens. After
`namespace`, `if`, `return`, or `else` has been recognized, errors belong to that
construct. Optional syntax therefore means absent syntax; it does not mean
silently ignoring an incomplete construct.

For example, `namespace Example class Program {}` reports the missing `;` at
`class`. It does not restart at `namespace` and report a missing class declaration.
Similarly, `if (true) {} else return;` reports the missing block at `return`.
The same rule preserves errors inside optional capture modes and closure bodies.

Identifier-led local declarations and expression statements share a prefix.
The implemented scalar-type grammar uses bounded token lookahead to select the
declaration path before parsing its initializer. A missing initializer or `;`
therefore remains a declaration error. Extending the type grammar requires
extending this discriminator alongside the new syntax.

### Token kinds and literal payloads

Grammar punctuation is recognized by both token kind and spelling. The lexer
decodes normal String content before the parser consumes it, so token text alone
cannot distinguish `"}"` from `}` or `"not"` from `not`.

String payloads never close blocks, introduce statements, or become operators.
The contextual capture modifiers `weak` and `unowned` are recognized only in
their capture-list context; their identifier tokens remain usable elsewhere.

### Comparison errors

The equality and relational precedence groups are non-associative, following
the examples in `Spec/Language/Operators.vxs`. `a < b < c` and `a == b == c`
produce `VXP0014` at the second operator. Parentheses establish a separate
expression level: `(a < b) == (b < c)` is syntactically valid and is subsequently
checked for type compatibility.

Use `a < b && b < c` for an ordered pair of comparisons. The parser does not
guess whether a chain was intended to mean a conjunction or nested comparison.

Type checking separates the expected result from the operands of comparisons
and logical operators. A `bool` return type does not turn the integers in
`1 == 2` into Boolean literals. Core verification accepts matching Boolean
equality operands, and constant folding compares their actual values. Numeric
equality never falls back to comparing truthiness.

### Supplied token streams

Embedding clients may supply a token list through `ParserInput`. An empty list
is an empty module, and a complete token list may omit the final EOF marker.
If a marker is present, it must be the final token. A suffix after EOF produces
`VXP0015`; it cannot be silently discarded as unparsed input.

When physical exhaustion occurs after a token, the parser retains a zero-width
span at that token's end. This preserves the source filename even for truncated
supplied streams. Lexer-produced EOF tokens retain their own source positions.

### Construct ranges

Function spans include access modifiers and the closing body delimiter. Branch
spans include the final closing brace, including empty branches and complete
`else if` chains. Callable spans include explicit capture brackets and their
body delimiter, including empty closures. Return statement spans include `;`.

These ranges support downstream diagnostics and editor selection without
reconstructing a construct's extent from its last nonempty child. The public
AST and token constructors remain unchanged by this cursor implementation.

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
