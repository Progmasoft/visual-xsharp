<!-- SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com> -->
<!-- SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.0 -->

# Testing and verification

Visual X# crosses Haskell, C++20, Kotlin, LLVM, and tool-specific boundaries. No single test command validates every layer.
This guide maps a change to the smallest meaningful local gate and the integrated workflows that protect the repository.

## Test principles

- Test an invariant where it is owned, then test the next boundary that consumes it.
- Prefer semantic assertions over snapshots of unstable internal formatting.
- Give malformed artifacts dedicated negative tests; do not rely only on parser rejection.
- Keep unit tests deterministic and independent of network services.
- Never make a passing test depend on an old generated artifact in `build`, `bazel-*`, `dist-newstyle`, or Gradle output.
- A registered-but-disconnected CLI route needs a test for its explicit rejection.
- A public language design example does not count as implementation coverage until an owning compiler test exercises it.

## Fast preflight

Run these before expensive language-layer checks:

```powershell
git diff --check
rg -n "d[o]cs/" README.md Documents Spec Compiler ProjectSystem Analyzer Formatter Linter
```

The second command is a migration/stale-link example rather than a permanent assertion that the text `docs` can never
occur. External names such as `docs.rs` are unrelated and must not be rewritten.

For a source change, inspect the diff and verify that no generated or local-only directory entered the index:

```powershell
git status --short
git diff --stat
git diff --name-only
```

## Haskell compiler packages

The Haskell workspace lives under `Compiler/` and contains separate Syntax, Frontend, Core, and Driver packages. From that
directory run:

```powershell
cabal build all
cabal test all
Get-ChildItem Haskell -Filter *.cabal -Recurse | ForEach-Object {
  Push-Location $_.DirectoryName
  try { cabal check } finally { Pop-Location }
}
```

Use focused package tests during development, but finish a frontend/Core change with the complete workspace tests. Changes
to tokens, AST shapes, symbols, types, Core, CorePrep, or their codecs commonly cross more than one package even when the
compiler error appears in only one module.

### What Haskell tests should cover

| Layer | Primary assertions |
| --- | --- |
| Lexer | token kind, literal decoding, source span, one-error recovery |
| Parser | grammar shape, precedence, delimiter recovery, invalid-form diagnostics |
| Renamer | stable fresh identities, scope, duplicate declarations, shadowing |
| Name Resolution | reference binding, ambiguity, explicit captures, namespace merge |
| Type Checker | inferred/declared types, calls, returns, operators, range errors, entry signature |
| Desugarer | semantic preservation and removal of source-only forms |
| Core | verifier invariants, optimizer preservation, wire round trips and limits |
| CorePrep | atomization order, CFG construction, verifier failures, internal wire symmetry |
| Driver | source discovery, exclusions, UTF-8, containment, project entry selection |

Golden bytes are appropriate for a versioned wire format. Pair them with semantic round-trip tests so a reader and writer
cannot change together and accidentally hide an incompatible format change.

## Native compiler tests

Initialize recursive submodules and make the standalone LLVM development environment available before building:

```powershell
git submodule update --init --recursive
bazelisk build //Compiler/Cli:vxs
bazelisk build //Compiler/Cli/Tests:cli_parser_tests
.\bazel-bin\Compiler\Cli\Tests\cli_parser_tests.exe
```

The Catch2 binary is executed directly on Windows. This avoids adding a POSIX shell dependency only for Bazel's conventional
`cc_test` launcher.

Use Bazel target boundaries to keep iteration focused:

```powershell
bazelisk build //Compiler/Core:core
bazelisk build //Compiler/Codegen/Xpp:xpp
bazelisk build //Compiler/Codegen/Xmm:xmm
bazelisk build //Compiler/Backend/LLVM:llvm_backend
bazelisk build //Compiler/Driver:core_pipeline
bazelisk build //Compiler/Core/Tests:core_pipeline_tests
bazelisk build //Compiler/Driver/Tests:closure_pipeline_tests
bazelisk build //Compiler/Driver/Tests:scalar_pipeline_tests
bazelisk build //Compiler/Backend/LLVM/Tests:llvm_backend_tests
```

The exact label names are source-owned API. If a package is reorganized, update this guide and CI with the same change.

### Component-local ownership

Native tests do not live in a root `tests/` directory. The directory holding a
test identifies the contract that owns its maintenance:

| Test package | Contract |
| --- | --- |
| `Compiler/Cli/Tests` | command grammar, option scope, and output behavior |
| `Compiler/Core/Tests` | Core model, verification, artifacts, and golden wire |
| `Compiler/Driver/Tests` | connected CorePrep, Xpp, and Xmm stage behavior |
| `Compiler/Backend/LLVM/Tests` | LLVM IR and native artifact lowering |
| `Compiler/Package/Tests` | package archive implementation |
| `Compiler/Legacy/Tests` | retained compatibility code scheduled for removal |

Fixtures follow the same rule. A Core golden document is stored below the Core
test package, while multi-file `.vxs` projects used by the connected pipeline
belong to Driver tests. Retired textual-intermediate fixtures remain below the
Legacy boundary and are never an input promise for the current CLI.

When a test crosses several components, choose the narrowest owner of the
asserted contract. For example, scalar preservation from CorePrep through Xmm
belongs to Driver; the exact LLVM `sdiv` versus `udiv` choice belongs to the
LLVM backend. This prevents integration suites from becoming an unowned common
bucket.

Test targets use private default visibility. Production libraries may be test
dependencies, but production targets must never depend on a `Tests` package.
See [Test ownership](TEST-OWNERSHIP.md) for fixture, naming, and review rules.

After moving a suite, scan build labels, workflow commands, documentation,
REUSE annotations, compile-time fixture paths, and generated manifests for the
old location. A successful compiler build alone does not prove the move is
complete: a release-only workflow or license scanner may still carry the stale
path. Remove an empty root directory instead of preserving it as a future
fallback.

### What native tests should cover

- CLI command and option scope, arity, typed values, defaults, duplicates, and contextual help;
- bounded Core/CorePrep decoding and malformed-document rejection;
- equality of Haskell and C++20 wire expectations;
- Core and CorePrep symbol/type/control-flow verification;
- deterministic Core-to-CorePrep adaptation;
- Xpp and Xmm lowering, optimizer preservation, and independent verifier failures;
- LLVM module construction and rejection of values with no layout contract;
- object/assembly extension checks and target-machine errors;
- typed LLD invocation and stale-executable prevention; and
- temporary-file cleanup on success and every failure exit.

Tests that construct Xpp or Xmm directly must still pass through that IR's verifier. Direct construction is a reason for
more boundary verification, not an exemption.

## Kotlin project system

Run the project evaluator and DSL tests with the repository wrapper:

```powershell
.\ProjectSystem\gradlew.bat -p ProjectSystem test
```

Project-system tests should distinguish script parsing, immutable model construction, validation, lockfile persistence, and
native plan transport. Important cases include:

- project discovery from nested directories;
- required and all-or-none project metadata rules;
- namespace-qualified entry validation without path guessing;
- null versus explicit exclusion state;
- contained local plugin and package paths;
- exact hosted coordinates and duplicate declarations;
- case-sensitive identifiers, versions, targets, and test suite names;
- plugin descriptor/service/digest validation;
- deterministic SQLite lockfile contents; and
- `panic` producing no partial plan, lock update, or compilation start.

Do not test the evaluator by searching for `.vxs` files in Kotlin. Source discovery belongs to the Haskell compiler driver.

## Analyzer, Formatter, and Linter

Each ecosystem project has an independent workflow and release line. Their Haskell and Kotlin layers are tested separately
because a configuration snapshot can be correct while formatting or lint semantics are wrong.

Typical local gates are:

```powershell
Set-Location Analyzer
cabal build all
cabal test all
.\gradlew.bat test

Set-Location ..\Formatter
cabal build all
cabal test all
.\gradlew.bat test

Set-Location ..\Linter
cabal build all
cabal test all
.\gradlew.bat test
```

Run commands from the project they belong to; do not assume one Gradle or Cabal workspace owns all three tools. Evaluator
integration is intentionally separate from the typed DSL model and must not be implied by model-only tests.

Formatter tests need parser-gated idempotence: formatting valid input twice must produce the same bytes as formatting it
once. Dry-run must not write, in-place mode must replace only the selected source, and malformed input must not be silently
rewritten.

Linter tests should assert rule identity, severity, source range, explanation metadata, safe/unsafe fix classification, and
suppression behavior independently. A rule catalog entry is planned surface until an analysis implementation and focused
test exist.

Analyzer tests should keep syntax, semantic, and full analysis modes distinct and verify cancellation/stale-result behavior
when workspace support is connected.

## Formatting and static hygiene

Project-owned C and C++ sources use the checked-in `.clang-format` file and LLVM/Clang 23.1.0:

```powershell
$files = rg --files Compiler tests include -g '*.cpp' -g '*.hpp' -g '*.hh' -g '*.h'
clang-format --dry-run --Werror $files
```

Do not include `third_party/` or generated sources. Haskell formatting follows `fourmolu.yaml`; Kotlin formatting must respect
the project sources and generated-code exclusions.

Every implementation, test, build, configuration, and internal source file outside the documented exceptions must remain at
or below 1500 lines. A simple review aid is:

```powershell
$extensions = '*.hs','*.cpp','*.hpp','*.hh','*.h','*.kt','*.kts','*.rs','*.go','*.java'
Get-ChildItem Compiler,ProjectSystem,Analyzer,Formatter,Linter,tests,scripts -Recurse -File -Include $extensions |
  Where-Object { (Get-Content -LiteralPath $_.FullName).Count -gt 1500 } |
  Select-Object FullName
```

Treat any result as a decomposition task, not an invitation to minify the file.

## GitHub workflows

The repository currently separates six workflow ownership areas:

| Workflow | Responsibility |
| --- | --- |
| `native.yml` | Windows ClangCL/Bazel native graph and CLI contracts |
| `language-layers.yml` | Kotlin project system plus Haskell compiler layers |
| `haskell-coverage.yml` | Haskell coverage reporting |
| `analyzer.yml` | Visual Analyzer layers |
| `formatter.yml` | Visual Formatter layers |
| `linter.yml` | Visual Linter layers |

After pushing a normal compiler or repository-wide change, inspect every workflow triggered by the commit:

```powershell
gh run list --commit (git rev-parse HEAD)
gh run watch <run-id> --exit-status
```

A workflow that did not start is not green. Confirm its trigger/path filters before concluding that the change was covered.
When one workflow fails, inspect the failing job and reproduce its direct command locally; do not rerun blindly until a
transient external failure is established.

## Artifact-level smoke tests

For a connected source subset, a useful end-to-end matrix is:

```powershell
vxs check -File .\Sources\Main.vxs
vxs build -File .\Sources\Main.vxs -Emit core
vxs build -File .\Sources\Main.vxs -Emit llvmll
vxs build -File .\Sources\Main.vxs -Emit llvmbc
vxs build -File .\Sources\Main.vxs -Emit object
vxs build -File .\Sources\Main.vxs -Emit assembly
vxs build -File .\Sources\Main.vxs
vxs run -File .\Sources\Main.vxs
```

Assertions should include file extension, nonempty artifact, replacement of an older artifact, no output from `check`, no
temporary object after binary linking, and propagation of the executed process status from `run`.

## Documentation verification

For documentation-only changes:

1. confirm every relative Markdown link resolves;
2. scan for old directory and artifact names;
3. verify that examples use the accepted case-sensitive CLI spelling;
4. check that planned behavior is marked planned;
5. keep public prose in en-US English; and
6. run `git diff --check`.

Documentation is part of the interface. A command example that the parser rejects or an old registry hostname is a defect,
even when no compiler source changed.
