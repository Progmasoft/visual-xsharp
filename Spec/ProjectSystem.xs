// SPDX-FileCopyrightText: 2026 Leitwolf <xs-lang.chess031@slmails.com>
// SPDX-License-Identifier: MPL-2.0

// X# project system
//
// The primary project format is executable Kotlin script metadata. xs-project
// evaluates it with JRE 25 or newer and the external `kotlin` command. It does
// not parse or compile X# source. The native xs-compiler receives a resolved
// registry and owns every compiler stage from lexing through linking.


// ============================================================
// Combined project file
// ============================================================

// xs.project.kts:
//
// project("Example", "BETA", "0.1.0")
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
// compiler {
//   warnings("medium")
//   werror(false)
//   verbose(true)
// }
//
// The resolved ordinary source registry must contain exactly one case-sensitive
// main.xs. main.xs is ordered first and is the executable entry source.


// ============================================================
// Split project files
// ============================================================

// A project may replace xs.project.kts with both:
//
// - xs.settings.kts
// - xs.build.kts
//
// xs.settings.kts is evaluated first. xs.build.kts receives the complete state
// and may use any DSL section; there is no section-to-file ownership rule.
// xs.project.kts cannot coexist with the split pair.
//
// If xs.module.kts exists, it is evaluated after the combined project script or
// after the complete settings/build pair.
//
// A project may omit module { include("Modules") } only when each compiler
// invocation supplies --module ./Modules.


// ============================================================
// Source roots and exclusions
// ============================================================

// source.include, module.include, and test.include accept project-relative
// directory roots. Each root is searched recursively for the configured source
// extension. Include roots do not accept globs. source.exclude and test.exclude
// accept *, **, and ? globs. Absolute paths and parent-directory escapes are
// rejected.
//
// The default source extension is .xs. set("XS_EXTENSION", "xsharp") changes
// discovery to .xsharp and changes the required entry name to main.xsharp.
//
// source is the canonical block name. sources remains a compatibility DSL alias
// but is not used in canonical project files.


// ============================================================
// Module membership
// ============================================================

// Source text has no module keyword. xs.module.kts assigns selected module files
// to case-sensitive logical names:
//
// module {
//   name("MyModule")
//   members {
//     add("Modules/Example/*.xs")
//   }
//
//   submodule {
//     name("util")
//     add("Modules/Example/Utils/**/*.xs")
//   }
// }
//
// module {
//   name("FooModule")
//   add("Modules/Foo/math.xs")
// }
//
// add(...) accepts a concrete path or glob. members is optional: add directly
// under module has the same meaning. submodule is also optional.
//
// - MyModule direct declarations use MyModule::<item>.
// - util declarations use MyModule::util::<item>.
// - FooModule declarations use FooModule::<item>.
//
// `name("Math")` must be imported as `import Math;`. Module lookup is
// case-sensitive; `import math;` does not match.


// ============================================================
// Registry invariants
// ============================================================

// Project evaluation rejects:
//
// - zero or multiple ordinary main.xs files,
// - a source/module registry overlap,
// - an unassigned selected module source,
// - duplicate membership for one physical file,
// - a module member outside module.include selection,
// - missing concrete paths or empty glob matches,
// - empty project/module/submodule names.
//
// Registry order is deterministic. The process boundary includes the warning
// policy, ordinary source count, module member count, paths, and logical names.


// ============================================================
// Compiler options and variables
// ============================================================

// Defaults:
//
// warnings("medium")
// werror(false)
// verbose(true)
// set("XGC_ENABLED", false)
//
// Command-line warning/verbose/XGC options override one invocation without
// rewriting project scripts.
//
// set("TARGET", "x86_64-unknown-linux-gnu", "aarch64-unknown-linux-gnu")
// get("TARGET")
// getAll("TARGET")
// get("PROJECT")
//
// PROJECT is formatted as: name, channel, version.


// ============================================================
// Host conditions
// ============================================================

// Project scripts use ordinary Kotlin control flow with cfg values:
//
// if (cfg(OS == LINUX) && cfg(ARCH == X86_64)) {
//   set("NATIVE_TARGET", "x86_64-unknown-linux-gnu")
// } else if (cfg(FAMILY == BSD)) {
//   set("NATIVE_TARGET", "x86_64-unknown-freebsd")
// }
//
// BSD means FreeBSD, OpenBSD, or NetBSD. UNIX means Linux, macOS, or BSD.
// ReactOS has no OS == REACTOS value; it belongs to FAMILY == WINDOWS while
// OS == WINDOWS remains false for a ReactOS host.


// ============================================================
// Commands
// ============================================================

// xs build
// xs check
// xs run
//
// With no -file input these commands search for Kotlin project metadata
// and invoke xs-project. xs.module.kts cannot be used when xs-project is not
// installed or cannot execute Kotlin.
//
// xs build -file Sources/main.xs
//
// Direct source compilation does not load project module metadata.

// ============================================================
// Canonical layout
// ============================================================

// X# source directories canonically use PascalCase and .xs files canonically
// use snake_case. This convention is not mandatory and does not affect module
// identity; xs.module.kts is authoritative.
//
// Example:
//
// Example/
//   xs.project.kts
//   xs.module.kts
//   Sources/
//     main.xs
//   Modules/
//     Example/
//       math.xs
//       square.xs
//       Utils/
//         add.xs
//         multiply.xs
