<!--
SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
SPDX-License-Identifier: MPL-2.0
-->

# CLI contract

`/usr/bin/xs` is the single user-facing command. It owns native X# compilation and invokes its bundled project runtime to
evaluate Kotlin project files on JRE 25 or newer.

## Supported forms

```text
xs check
xs build
xs run
xs test
xs resolve
xs build -file Main.xs
xs run -file Main.xs
xs build --output hir -file Main.xs
xs build --output mir -file Main.xs
xs build --output xlil -file Main.xs
xs build --hir -file Main.xs
xs build --mir -file Main.xs
xs build --xlil -file Main.xs
xs build --warning all --werror true --verbose true
xs build --module ./Modules
xs --version
```

Argument-free `xs check`, `xs build`, `xs run`, and `xs test` use the bundled project runtime to discover and evaluate
`xs.project.kts` or the `xs.settings.kts` + `xs.build.kts` pair. The runtime returns source metadata and never parses or
compiles `.xs` files.

`--module <directory>` supplies a recursive module source root when the KTS project does not declare
`module { include(...) }`.

## One-shot compiler policy

The following options override compiler policy for one invocation and may be combined with an argument-free Kotlin
project build or `-file`:

- `--warning all|medium|low|none` selects warning volume. A warning declares the minimum volume at which it is active;
  `all` enables every warning, `medium` is the default, `low` keeps only the most important warnings, and `none` disables
  warnings.
- `--werror true|false` controls whether enabled warnings fail the compilation.
- `--verbose true|false` controls compiler progress output. When enabled, `xs` prints the effective policy and the
  ordered source registry entering the frontend.

The default policy is `warning=medium`, `werror=false`, and `verbose=true`. An explicit KTS project value is used for that
project, and a command-line value has final precedence for the current invocation.

For Kotlin projects these values override the evaluated `compiler {}` block without modifying either KTS file:

```text
xs build --warning low --werror false --verbose true
```

The compiler usage is:

```text
usage: xs <build|run> -file <Main.xs>
usage: xs <check|build|run|test>
       [--warning all|medium|low|none] [--werror true|false] [--verbose true|false]
usage: xs resolve
usage: xs test [-file <Test.xs>]
usage: xs build [--output hir|mir|xlil] -file <input>
usage: xs build [--hir|--mir|--xlil] -file <input>
usage: xs --version
```

## `xs --version`

`xs --version` prints the compiler version, such as `xs 0.2.4`.

## `xs resolve`

`xs resolve` discovers the current Kotlin project, reevaluates its dependency declarations, and atomically refreshes
`xs.lock.sqlite3`. The artifact is always a binary SQLite database; the command does not produce a readable lock-file
variant. The current format records exact declared coordinates. Remote package selection and download are not active yet.

## `xs check`

`xs check` does not produce objects. Current flow:

1. source input validation
2. X# parse/structural AST
3. macro validation and expansion preparation
4. HIR symbol/import/name/type resolution
5. early expression checks

## `xs build`

`xs build` will eventually run the full check, MIR, borrow checker, monomorphization, XLIL, backend, object, and link flow.
Today, plain `xs build -file <main.xs>` and argument-free `xs build` can produce a native
`.xse` only for the first
supported source slice:

```xs
fn add(left: Long, right: Long) -> Long { return left + right; }
fn main() -> Long { return add(2, 5); }
```

Project builds may place supported helper functions in separate selected source files. The compiler merges their expanded
AST packets, collects all program signatures before lowering bodies, and emits one verified XLIL/LLVM module. Kotlin
project sources are resolved before this compiler stage and put `main.<XS_EXTENSION>` first when it exists. Library-only
registries are valid project metadata, although a native `bin` build still requires a supported entry function.

The entry function must be top-level, named `main`, have no parameters, and return `Long`. Same-module helper functions may
take `Long`, `Int`, or `Bool` parameters and return `Long`, `Int`, or `Bool`. Supported bodies may contain explicit
`Long`/`Int`/`Bool` local bindings or
inferred `:=` local bindings with i32-compatible or bool-compatible initializers, simple assignments to mutable locals,
and then one return statement. A statement-level `if` may contain one simple assignment in each branch; the local may be
read after the branches merge. Supported `Long` assignments are `=`, `+=`, `-=`, `*=`, `/=`, `%=`, `&=`, `|=`, and
`^=`; a branch may contain multiple such assignments. `val`, `const`, and `static` locals can be initialized but not
reassigned. Supported conditional assignment blocks, including `else if` chains, may nest. A supported `while` has a Bool condition and one or more
supported assignment statements and `Long`/`Bool` local declarations in its body; it lowers through the same native
control-flow path. An unconditional `loop { ... }` uses that same CFG with a constant true condition. `break` and
`continue` target the innermost supported loop. Block-local bindings are unavailable
after their enclosing block; an inner scope may shadow an enclosing binding, which becomes visible again after scope exit.
The same supported
return-expression subset may be used by an early `return` inside a supported conditional or loop block. The
native slice also supports `for (name: Long = initializer; bool_condition; update)` with a required variable initializer
and an assignment, postfix `++`, or postfix `--` update. Fixed and runtime-sized arrays support `for (value in values)`,
including nested tuple binding patterns. Tuple destructuring declarations such as `(left, else, right) := value;` lower
to checked tuple projections; explicit tuple pattern annotations are also accepted. The
native slice also lowers statement-level `match (value)` over a supported `Long` or `Bool` selector. Arms use matching
literal patterns and must end with an `else` arm; each arm accepts the same supported block statements as `if`.
supported return expression subset is i32-range integer literals, local identifiers, direct same-module `Long` calls, unary
`-`, `+`, `-`, `*`, `/`, `%`, `&`, `|`, `^`, `<<`, `>>`, and one top-level `if (...) { expr; } else { expr; }` expression
whose condition is a bool literal, a `Bool` local, a direct same-module `Bool` helper call, unary `!`, or an i32 comparison,
including `==`, `!=`, `<`, `<=`, `>`, and `>=`. Local storage passes through MIR places, XLIL stack slots, and LLVM
stack operations before normal LLVM optimization. The compiler lowers that source `Long` slice to the direct native
process `i32` entry ABI. General source-level function body lowering is still incomplete.

The `--output hir|mir|xlil` spelling and the short `--hir`, `--mir`, and
`--xlil` spelling select the same intermediate output kind. The short spelling is currently valid only with `-file`.

## `xs test`

`xs test` evaluates the modern Kotlin project, selects its disjoint test registry, and parses and semantically validates
the production, module, and test sources as one program. Each top-level `#[Test] fn name()` is then compiled through the
normal HIR → MIR → XLIL → LLVM → object → link path into an isolated temporary `.xse` harness and executed. A test passes
when it exits successfully; `#[ShouldPanic]` reverses that expectation, and `#[Ignore]` skips execution. Temporary test
artifacts are removed after each case. A syntax, semantic, harness-compilation, or unexpected runtime failure makes the
command fail. `xs test -file <Test.xs>` uses the same path for one source file.

Test functions currently must be top-level, have a body, take no parameters, and use the default unit return type.

## `xs run`

For the supported native source subset, `xs run` and `xs run -file <main.xs>` perform the
same checked build as `xs build`, write `.ll`, `.o`, and `.xse`, then execute the generated `.xse`. The CLI returns the
native program's exit code unchanged. A compilation, linking, spawn, or wait failure returns a non-zero compiler error
instead. Program arguments and direct XHIR/XMIR/XLIL execution are not part of this first run slice.

## Intermediate outputs

- `.xhir`: human-readable XHIR text
- `.xmir`: human-readable MIR text
- `.xlil`: XLIL text registry

`.xhir`, `.xmir`, and `.xlil` are text formats. `.xhir` and `.xmir` are human-readable compiler intermediate dumps;
`.xlil` is a human-readable backend input registry. They will not be binary formats or opaque serialized compiler state.
Future `.xhir` and `.xmir` grammar changes must preserve direct human inspection and code-review friendliness. XHIR and
XMIR are not assembly-like formats; XHIR is a structured semantic tree/record dump, while XMIR is a structured
control-flow/analysis dump.

## Direct file builds

Recognized forms:

```text
xs build --output hir -file foo.xs
xs build --output mir -file foo.xs
xs build --output xlil -file foo.xs
xs build -file foo.xs
xs build --hir -file foo.xs
xs build --mir -file foo.xs
xs build --xlil -file foo.xlil
```

The direct file paths skip project manifests. Their final semantics depend on the selected intermediate kind:

- `hir` with `.xs`: parse/check a single `.xs` input and emit `.xhir`.
- `hir` with `.xhir`: parse the versioned XHIR program, type-check it, lower it through MIR/XLIL, and produce native
  artifacts for the supported model subset.
- `mir` with `.xs`: parse/check/lower a single `.xs` input and emit `.xmir`.
- `mir` with `.xmir`: parse the versioned XMIR program, structurally verify and borrow-check it, optimize it, lower it to
  verified XLIL, and produce native artifacts for the supported model subset.
- `xlil` with `.xs`: lower a single X# source file to `.xlil`.
- `xlil` with `.xlil`: parse and verify the `.xlil version N` registry, lower the supported subset to LLVM IR, run the
  configured LLVM verification/optimization pipeline, emit an object file, and link a native executable when the target is
  the local host.
- no output flag with `.xs`: check the source file and, for the first supported `main` slice, emit `.ll`, `.o`, and
  `.xse` beside the input.

For `.xs` input, all three output forms use the same checked compiler-core session as native compilation. They write beside
the source file, replacing `.xs` with `.xhir`, `.xmir`, or `.xlil`. A Kotlin project merges every selected source session
and writes the program output beside its selected entry source. XHIR and XMIR use one version header and explicit function
and program end records. XMIR additionally carries structured aggregate/fixed-array registry records needed to preserve
composite MIR types; XLIL remains the module registry consumed by the backend.
For direct `.xhir`, `.xmir`, and `.xlil` inputs, the CLI validates the complete supported program grammar and rejects
unsupported versions. XHIR and XMIR are lowered to an in-memory XLIL registry before entering the same backend boundary.
A supported XLIL module is parsed through the public XLIL C23 parser API, verified, lowered through the LLVM backend,
verified by LLVM, and passed through the configured optimization pipeline. It writes
`<input-stem>.ll`, `<input-stem>.o`, and a native executable named `<input-stem>.xse` alongside the input file. The direct
XLIL path currently uses the backend default `O0` pipeline; no CLI optimization flag is exposed yet.

Direct native XLIL builds require exactly one function definition with this platform entry signature:

```text
.func main : () -> i32
```

This is a direct XLIL build ABI, not the X# source-language entry-point rule. The command uses the Clang driver with LLD
for local Linux ELF linking. When the configured target triple differs from the host, it still writes the LLVM IR and object
file but stops before linking with a cross-linking diagnostic. Runtime and external-library linking are not configured yet;
an unresolved XLIL `.extern` symbol causes the linker to report a failed direct native build.

The `.xse` extension is the xs-project native executable artifact name. The first target format is Linux ELF; PE native
executables are planned after ELF support.
