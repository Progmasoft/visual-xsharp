<!-- SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com> -->
<!-- SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.0 -->

# Ecosystem tools

Visual Analyzer, Visual Formatter, and Visual Linter are separate products with separate versions, build layers, and CI
workflows. The compiler coordinates project source policy, but it does not embed these tools or force them to share the
compiler release number.

## Product map

| Product | Primary implementation | Public executable | Configuration |
| --- | --- | --- | --- |
| Visual Analyzer | Haskell LSP plus editor hosts | no standalone user binary | `Visual.Analyzer.kts` |
| Visual Formatter | Haskell formatter | `vfmt` | `Visual.Formatter.kts` |
| Visual Linter | Haskell semantic/source linter | `vlint` | `Visual.Linter.kts` |

Each product also contains a Kotlin Script DSL model. All three configuration hosts require JRE 25 and Kotlin Script Runner;
the runtime is not embedded into the Haskell tool.

The Kotlin projects currently provide typed scopes, defaults, validation, and immutable configuration snapshots. Script
evaluator integration is a separate layer. A checked-in DSL type or test must not be described as proof that end-to-end
script evaluation is connected.

## Shared project source policy

`vxs format` and `vxs lint` operate on the complete project. They evaluate `Visual.XSharp.kts`, obtain the same main source
roots and exclusions used by the compiler, and ask the Haskell source-discovery layer for the deterministic `.vxs` list.
They do not independently search for files in Kotlin.

```text
vxs format
vxs lint
```

`format` requires `vfmt`; `lint` requires `vlint`. If the corresponding configuration file is absent from the project root,
the installed tool uses its defaults. When present, configuration is selected for that tool only:

```text
Visual.Formatter.kts
Visual.Linter.kts
Visual.Analyzer.kts
```

The compiler project file does not grow formatter or linter rule blocks. Tool versioning and configuration remain owned by
the tool.

## Visual Analyzer

Visual Analyzer is an LSP implementation rather than a separate command-line program. Its Haskell layer can request syntax,
semantic, or full frontend analysis. Editor-specific hosts connect that service to their platforms.

The intended configuration surface includes:

```kotlin
version = "latest"
analysisMode = AnalysisMode.FULL

diagnostics {
  compiler = true
  linter = true
  onChange = true
  onSave = true
}

inlayHints = true
formatting = true

workspace {
  indexDependencies = true
}

performance {
  workerThreads = 0
}
```

`workerThreads = 0` means automatic selection. Compiler and linter diagnostic controls are independent switches: Analyzer
coordinates them but does not merge their version or rule configuration.

The repository can contain IntelliJ Platform and VS Code host code without making either editor the language architecture.
Editor hosts should be thin protocol/UI adapters; Lexer through CorePrep remains in the Haskell language layer.

## Visual Formatter

Visual Formatter provides `vfmt`.

```text
vfmt File.vxs
vfmt -In-Place File.vxs
vfmt -Dry-Run File.vxs
vfmt -Help
```

Default mode writes formatted source to standard output. `-In-Place` replaces the selected source. `-Dry-Run` reports
whether formatting would change the file without writing formatted output or modifying the source. `-Help` describes the
standalone tool, not `vxs` compiler options.

Formatter configuration includes version selection, column and indentation widths, tab behavior, brace style, parenthesis
and punctuation spacing, line endings, final newline policy, sorting, input/output encoding, and byte-order-mark emission.
The current configuration model defaults to UTF-8 input/output and no BOM.

Formatting must be parser-gated and idempotent. The formatter may normalize physical source layout, but it must not invent
language forms, change name identity, turn an invalid source into a different valid program, or apply C-family `switch/case`
rules to a language without those constructs.

Install the package globally through ViGet:

```text
vxs install -Global Progmasoft.VisualFormatter
```

## Visual Linter

Visual Linter provides `vlint`. Its standalone control surface includes:

```text
vlint File.vxs
vlint -Fix File.vxs
vlint -List-Checks
vlint -Help
```

`-List-Checks` lists the installed linter's checks without compiling a project. `-Fix` applies fixes classified as applicable
by the configured safety policy; a diagnostic suggestion is not automatically a safe rewrite.

The linter configuration begins with global diagnostic behavior:

- product version;
- default severity;
- maximum diagnostic count;
- source/severity/rule ordering;
- rule IDs and explanation links;
- safe and unsafe fix application;
- generated/inactive/dependency/whole-program analysis policy;
- unknown and deprecated rule policy;
- suppression reporting; and
- baseline behavior.

Rules are grouped by language ownership rather than one flat list. Current catalog categories include naming, imports,
visibility, declarations, modifiers, overrides, members, bindings, functions, constructors, destructors, entry points,
expressions, control flow, exceptions, optionals, ownership, closures, generators, iteration, collections, BLINQ, text,
comments, attributes, operators, unsafe operations, FFI, assembly, directives, performance, concurrency, I/O, command
execution, databases, public API, maintenance, and formatting hygiene.

A key in the typed catalog is not automatically an implemented analysis. A linter rule becomes connected when it has a
semantic/source implementation, stable rule identity, severity behavior, focused tests, and—when relevant—safe/unsafe fix
classification.

Install the package globally through ViGet:

```text
vxs install -Global Progmasoft.VisualLinter
```

## Configuration independence

Formatter and Linter configurations are not sections of `Visual.XSharp.kts`. Their defaulting rules are:

1. use the tool configuration file in the project root when it exists;
2. otherwise use the installed tool's default configuration; and
3. apply command-line controls of the standalone tool to the current invocation.

Tool configuration versions do not inherit the compiler version. This is necessary because formatting stability, linter
rule evolution, Analyzer protocol features, and compiler language support can ship on different schedules.

## Distribution and registry paths

ViGet is the one hosted package registry. Visual X# packages and Kotlin DSL plugins occupy distinct catalogs:

```text
https://viget.progmasoft.com/<Publisher>/<Name>/
https://viget.progmasoft.com/dslplugins/<Publisher>/<Name>/
```

The first path contains Visual X#-authored `.vipkg` packages. The second contains Kotlin DSL plugin JARs. A DSL plugin is not
a `.vipkg`, and a Visual X# library is not loaded as JVM build logic.

Publisher identity is the Progmasoft Account name. Account management and dashboards belong under the account service, not
under a fake registry dashboard path:

```text
https://account.progmasoft.com/<Account>/dashboard
```

ViGet coordinates are case-sensitive. Local project declarations use explicit contained paths and do not create another
repository:

```kotlin
plugins {
  plugin("local") {
    path = "plugin.jar"
  }
}

dependencies {
  dependency("local") {
    path = "dependency.vipkg"
  }
}
```

Remote coordinate layout is fixed by ViGet. Project files do not configure a Gradle-like repository list or generate an
alternative DSL-plugin repository coordinate.

## Build and workflow ownership

Each ecosystem root is a small multi-language project:

```text
Analyzer/    Cabal + Gradle + editor-host build metadata
Formatter/   Cabal + Gradle
Linter/      Cabal + Gradle
```

Cabal owns Haskell implementation/tests. Gradle owns Kotlin DSL models/tests. Editor host package systems own only their host
integration. GitHub workflows are split into `analyzer.yml`, `formatter.yml`, and `linter.yml` so a tool failure is visible
without coupling all releases to the compiler workflow.

## Boundary rules

- Do not move language parsing or type checking into editor code.
- Do not make `vxs` silently format or lint during ordinary compilation.
- Do not make Formatter/Linter installation a compiler build dependency.
- Do not use filesystem layout as namespace identity.
- Do not call an unevaluated Kotlin model an active configuration evaluator.
- Do not treat Visual Analyzer as a standalone binary.
- Do not publish DSL plugin JARs in the `.vipkg` catalog.
- Do not add user-configurable remote repository lists; ViGet is the hosted registry.
- Do not share release numbers merely because components live in one source repository.
