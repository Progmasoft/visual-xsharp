// SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
// SPDX-License-Identifier: MPL-2.0

// Attribute system:
//
// Attribute delimiters are compiler syntax:
//
// - #[...] applies to the following declaration/member.
// - #![...] applies to the enclosing file/module form.
//
// Attribute names are not compiler keywords. The compiler implicitly import
// the standard Attrs module and places its exported names in scope for
// attribute lookup.
// An explicit `import attrs;` declaration is optional and may be used for
// readability or aliasing, but it is not required.
// Therefore examples normally use the short scoped form:
//
// #[Deprecated("use NewMain")]
//
// The official X# attribute registry is the Attrs module under std.
//
// X# keeps Rust's attribute vocabulary as compatibility aliases, then adds
// stronger async and thread-oriented attributes for the X# pipeline.

#![NoMain]

#[Deprecated("use NewMain")]
fn old_main() {
}

#[MustUse("the returned status must be checked")]
fn make_status() -> Long {
    return 0;
}

#[Derive(Ord, Partial, Clone, Debug)]
data Version {
    major: Long,
    minor: Long,
}

#[Inline(Always)]
fn fast_add(left: Long, right: Long) -> Long {
    return left + right;
}

#[Test]
#[Ignore("example-only fixture")]
fn skipped_test() {
}

// `xs test` discovers top-level test functions after macro expansion. A test
// has a body, takes no parameters, and returns unit (the default return type).
// Each non-ignored test receives an isolated native harness. `ShouldPanic`
// passes only when that harness terminates unsuccessfully.

#[Test]
fn native_test() {
}

#[Test]
#[ShouldPanic]
fn expected_panic() {
    panic!("expected test panic");
}

#[ThreadSafe]
#[Send]
#[Sync]
data SharedCounter {
    value: Atomic<Long>,
}

#[AsyncEntry]
#[CancellationSafe]
fn run_async() -> Result<()> {
    return Ok(());
}

// Rust compatibility names exposed by the standard Attrs module:
//
// Conditional compilation:
// Cfg, CfgAttr.
//
// Testing:
// Test, Ignore, ShouldPanic.
//
// Derive:
// Derive, AutomaticallyDerived.
//
// Macro-related:
// MacroExport, MacroUse, ProcMacro, ProcMacroDerive, ProcMacroAttribute.
//
// Diagnostics and lints:
// Allow, Expect, Warn, Deny, Forbid, Deprecated, MustUse,
// Diagnostic::OnUnimplemented, Diagnostic::DoNotRecommend.
//
// ABI, linking, symbols, and FFI:
// Link, LinkName, LinkOrdinal, NoLink, Repr, CrateType, NoMain,
// ExportName, LinkSection, NoMangle, Used, CrateName.
//
// X# CFFI-oriented attributes:
// Abi, CallingConvention, LinkLibrary, Header, ImportLibrary,
// SymbolVisibility, Packed, Transparent, Align, Unsafe, Safe,
// NoUnwind, MayUnwind, Variadic, CFunctionPointer, ForeignThreadSafe,
// NoCallbackIntoRuntime, ThreadLocal, Opaque, DynamicLibrary, Ownership,
// ReturnsOwned, TakesOwnership, Borrows, OutParameter.
//
// Code generation:
// Inline, Cold, Naked, NoBuiltins, TargetFeature, TrackCaller,
// InstructionSet.
//
// Documentation and preludes:
// Doc, NoStd, NoImplicitPrelude, Path.
//
// Limits and runtime:
// RecursionLimit, TypeLengthLimit, PanicHandler, GlobalAllocator,
// WindowsSubsystem.
//
// Features, type system, and debugger:
// Feature, NonExhaustive, DebuggerVisualizer, CollapseDebuginfo.
//
// X# stronger async/thread attributes:
// AsyncEntry, AsyncRuntime, Awaitable, CancellationSafe, CancellationUnsafe,
// ThreadSafe, ThreadUnsafe, Send, NotSend, Sync, NotSync, MainThread,
// WorkerThread, ThreadLocal, LockFree, AtomicOnly.
