<!-- SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com> -->
<!-- SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.0 -->

# Visual X# documentation

This directory explains the repository as it exists today. It is intentionally separate from `Spec/`: the specification
records language design, while these documents describe compiler architecture, supported workflows, implementation
coverage, and contributor contracts.

Visual X# is experimental. A documented design is not automatically an implemented feature. Every document uses the
following vocabulary:

- **connected** means the behavior is reachable through a supported command and covered by the owning test layer;
- **implemented** means code and focused tests exist, although a public command may not expose it yet;
- **registered** means a public spelling or model value is reserved but its execution route is intentionally rejected;
- **planned** means the design has direction but must not be relied on by programs; and
- **legacy** means retained code is outside the production pipeline and receives no new feature work.

## Start here

| Goal | Document |
| --- | --- |
| Understand the whole repository | [Architecture](ARCHITECTURE.md) and [repository layout](MONOREPO.md) |
| Follow one source file through the compiler | [Compiler pipeline](COMPILER-PIPELINE.md) |
| Understand the typed Core contract | [Core IR](CORE-IR.md) and [Core optimization](CORE-OPTIMIZER.md) |
| Check what works now | [Implementation status](IMPLEMENTATION.md) |
| Build a development checkout | [Building](BUILDING.md) |
| Run the correct verification layers | [Testing](TESTING.md) |
| Place tests and fixtures | [Test ownership](TEST-OWNERSHIP.md) |
| Use `vxs` | [CLI](CLI.md) |
| Write `Visual.XSharp.kts` | [Project files](PROJECT_FILES.md) |
| Understand diagnostics and exit behavior | [Diagnostics](DIAGNOSTICS.md) |
| Use Analyzer, Formatter, or Linter | [Ecosystem tools](ECOSYSTEM.md) |
| Work on LLVM lowering | [LLVM backend](LLVM-BACKEND.md) |
| Work on callable literals | [Closure pipeline](CLOSURE-PIPELINE.md) |
| Work on scalar types and literals | [Numeric types](NUMERIC-TYPES.md) |
| Trace scalars through native lowering | [Scalar pipeline](SCALAR-PIPELINE.md) |
| Maintain compiler wire contracts | [Artifact wire](ARTIFACT-WIRE.md) |
| Read or change language examples | [Specification guide](SPECIFICATION.md) |
| Prepare a change | [Contributing](CONTRIBUTING.md) |
| See intended sequencing | [Roadmap](ROADMAP.md) |

## Architecture set

- [Architecture](ARCHITECTURE.md) defines stage ownership and the major process boundaries.
- [Compiler pipeline](COMPILER-PIPELINE.md) follows discovery, parsing, semantic analysis, verified IRs, native lowering,
  emission, and linking in execution order.
- [Core IR](CORE-IR.md) defines the typed tree, symbol, verifier, closure, and wire contracts.
- [Core optimization](CORE-OPTIMIZER.md) documents fixed-point passes, effects, liveness, metrics, and reporting.
- [Implementation status](IMPLEMENTATION.md) distinguishes connected, partial, registered, planned, and legacy surfaces.
- [LLVM backend](LLVM-BACKEND.md) defines the Xmm-to-LLVM contract, supported values, verification, and artifact ownership.
- [Closure pipeline](CLOSURE-PIPELINE.md) explains capture analysis and the currently deliberate LLVM rejection boundary.
- [Numeric types](NUMERIC-TYPES.md) records fixed scalar widths and the gap between frontend semantics and the current Core
  transport.
- [Scalar pipeline](SCALAR-PIPELINE.md) follows fixed-width values through verification, wire v3, Xpp, Xmm, and LLVM.
- [Artifact wire](ARTIFACT-WIRE.md) defines the bounded internal Core and CorePrep transport contracts.

## User and project set

- [CLI](CLI.md) is the command and option reference. It also explains precedence and intentionally disconnected commands.
- [Project files](PROJECT_FILES.md) documents discovery, source policy, entry selection, compiler settings, plugins,
  dependencies, publishing metadata, test suites, and the SQLite lockfile.
- [Diagnostics](DIAGNOSTICS.md) describes error ownership, output streams, source positions, artifact safety, and exit status.
- [Ecosystem tools](ECOSYSTEM.md) separates the compiler from Visual Analyzer, Visual Formatter, and Visual Linter.

## Developer set

- [Building](BUILDING.md) contains prerequisites, environment discovery, component builds, and troubleshooting.
- [Testing](TESTING.md) maps changes to local gates and GitHub workflows.
- [Test ownership](TEST-OWNERSHIP.md) assigns suites, fixtures, and Bazel targets to their compiler components.
- [Repository layout](MONOREPO.md) explains ownership boundaries and allowed dependency direction.
- [Contributing](CONTRIBUTING.md) records code style, file naming, decomposition, generated-file hygiene, and update flow.

## Specification relationship

The public `Spec/` tree is the authoritative language-design catalog. Its files contain independent valid and invalid
examples; they are not one buildable application. The compiler tests are authoritative for current implementation coverage.
When those sources differ, do not rewrite a design example merely to make an incomplete compiler accept it. Instead, state
the implementation gap here and add a focused compiler test when the feature is implemented.

## Documentation maintenance

Documentation changes follow the same ownership rules as code changes:

1. Verify claims against source, tests, and build targets rather than historical names.
2. State whether a route is connected, implemented, registered, planned, or legacy.
3. Use repository-relative links and the canonical `Documents/` directory spelling.
4. Keep machine-specific paths, credentials, hostnames used only for local development, and internal planning notes out of
   public files.
5. Use en-US English and correct spelling, including names that originated as typos.
6. Do not describe CorePrep as a public artifact: it is an internal adapter and transport only.
7. Do not revive retired public intermediate-representation names or the removed C lexer/parser route.
8. Run a stale-link scan and `git diff --check` after moving or adding documents.

The documentation should be useful without knowledge of repository history. Historical decisions belong in the changelog;
the documents in this directory describe the current model and explicitly marked future work.
