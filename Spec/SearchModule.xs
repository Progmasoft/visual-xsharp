// SPDX-FileCopyrightText: 2026 Leitwolf <xs-lang.chess031@slmails.com>
// SPDX-License-Identifier: MPL-2.0

// Project module registry and lookup
//
// X# does not discover modules by scanning .xs text for a module declaration.
// Source text has no module keyword. xs-project evaluates the Kotlin project
// scripts and sends xs-compiler a resolved source/module registry.


// ============================================================
// Project inputs
// ============================================================

// xs.project.kts selects ordinary program sources and the pool of files that
// may become module members:
//
// source {
//   include("Sources")
//   exclude("Sources/Generated/**/*.xs")
// }
//
// module {
//   include("Modules")
// }
//
// source and module roots are searched recursively. Include roots cannot use
// globs. Absolute paths and `..` escapes are rejected. Source selection must
// resolve exactly one case-sensitive main.xs by default.


// ============================================================
// Membership inputs
// ============================================================

// xs.module.kts maps the module pool to logical names:
//
// module {
//   name("Network")
//   add("Modules/Network/client.xs")
//   add("Modules/Network/server.xs")
//
//   submodule {
//     name("http")
//     add("Modules/Network/Http/*.xs")
//   }
// }
//
// add(...) supports concrete paths and globs. Glob expansion is deterministic;
// the registry is sorted before it crosses the xs-project/xs-compiler process
// boundary.


// ============================================================
// Registry validation
// ============================================================

// Project evaluation rejects:
//
// - a module member not selected by module.include(...),
// - a selected module source with no xs.module.kts membership,
// - one physical file assigned to multiple module paths,
// - source/module selection overlap,
// - missing concrete paths,
// - glob patterns that match no files,
// - empty module or submodule names.
//
// Two or more files may belong to the same module. Their declarations share
// one module namespace and normal duplicate-declaration rules apply.


// ============================================================
// Compiler registry
// ============================================================

// The resolved registry conceptually contains:
//
// project source:
//   Sources/main.xs
//
// module source:
//   Network -> Modules/Network/client.xs
//   Network -> Modules/Network/server.xs
//   Network::http -> Modules/Network/Http/request.xs
//
// xs-compiler receives physical paths and optional logical module names. It
// does not execute Kotlin and does not repeat glob expansion.


// ============================================================
// Import lookup
// ============================================================

// Given:
//
// import Network;
// import Network::http;
//
// lookup proceeds against the project-provided registry:
//
// 1. Find the exact logical module path.
// 2. Make that path available for qualified name resolution.
// 3. Resolve public declarations from every file assigned to that path.
// 4. Record dependency edges for incremental compilation/codegen planning.
//
// Physical file names do not participate in logical lookup. Renaming a file
// without changing xs.module.kts does not rename the module.


// ============================================================
// Build tools
// ============================================================

// xs-project is required for xs.project.kts/xs.module.kts projects. It requires
// JRE 25 or newer and the external `kotlin` script runner. xs-compiler itself
// remains native and does not require a JVM.
//
// Direct `xs build -file main.xs` does not load xs.module.kts and therefore
// cannot resolve project module imports. Argument-free `xs build` invokes
// xs-project in the current project tree.
