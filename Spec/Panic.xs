// SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
// SPDX-License-Identifier: MPL-2.0

// Panic module:

//
// Panic exports assertion and panic macros.
//
// The compiler behaves as if `import panic;` existed. The following macros
// are available without an explicit import or through explicit `import panic`.
// They are not compiler built-ins:
//
// - assert!
// - assert_eq!
// - assert_ne!
// - debug_assert!
// - debug_assert_eq!
// - panic!
//
// Their source-level meanings follow Rust's macros with the same names.
//
// assert!(condition) panics when condition is false.
// assert_eq!(left, right) panics when left != right.
// assert_ne!(left, right) panics when left == right.
// debug_assert!(condition) is enabled in debug builds and may be disabled
// in optimized release builds.
// debug_assert_eq!(left, right) follows debug_assert! with equality.
// panic!() unconditionally starts panic propagation.
//
// Panic formatting follows the same placeholder-based formatting model as
// Stdio formatting. Runtime formatting and unwinding ABI are implementation
// details of the Panic runtime.


// conceptual macro expansion

// Panic macros expand during AST macro expansion, before HIR construction.
// Their semantic runtime boundary is std::panic::begin(FormatArguments), which
// never returns. Implementations may lower this boundary to equivalent MIR and
// runtime intrinsics.
//
// panic!()
//   -> std::panic::begin(format_args!("explicit panic"))
// panic!(template, values...)
//   -> std::panic::begin(format_args!(template, values...))
// assert!(condition)
//   -> evaluate condition once; when false, panic with the failed expression
// assert!(condition, template, values...)
//   -> evaluate condition once; when false, panic with the formatted message
// assert_eq!(left, right)
//   -> evaluate left then right exactly once; compare them with ==; when false,
//      panic with hygienically retained Debug views of both values
// assert_ne!(left, right)
//   -> evaluate left then right exactly once; compare them with !=; when false,
//      panic with hygienically retained Debug views of both values
// debug_assert!(...) and debug_assert_eq!(...)
//   -> use the corresponding assertion expansion when debug assertions are
//      enabled; otherwise expand to no runtime evaluation
//
// Assertion forms that accept a custom template pass it through format_args!.
// Compiler-generated temporary bindings are hygienic and preserve ownership:
// comparison and diagnostic formatting do not evaluate or move either operand
// a second time. Panic expansion produces a diverging expression, so control
// flow after std::panic::begin is unreachable.
//
// Each invocation must match exactly one exported Panic macro rule. These are
// normal imported macros even though the compiler supplies the implicit Panic
// import; format_args! in their expansions is a built-in macro. The built-in
// writer macros are separate and are not used by panic expansion directly.

// import panic; is optional.


fn validate_positive(value: Long) {
    assert!(value > 0);
}


fn validate_pair(left: Long, right: Long) {
    assert_eq!(left, right);
    assert_ne!(left, 0);
}


fn debug_validate(index: Long, length: Long) {
    debug_assert!(index >= 0);
    debug_assert_eq!(length, length);
}


fn stop_now() {
    panic!();
}


fn stop_with_message(name: Str) {
    panic!("invalid name: {}", name);
}


// `using namespace panic;` is invalid because Panic is macro-only. Macro names
// enter macro scope through the compiler-provided import and are never opened
// as ordinary namespace symbols.
