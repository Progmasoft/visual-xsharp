// SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
// SPDX-License-Identifier: MPL-2.0

// variable system:

//
// Type annotations are required unless the declaration uses `:=`.
// Normal variables are mutable.
// Top-level variables are not supported.
//

fn main() {
    // mutable variables
    x: Int = 5;
    name: Str = "Alpha";
    enabled: Bool = true;
    letter: Char = 'A';
    data: Byte = 255;

    small: Short = 10;
    normal: Long = 20;
    large: Int = 30;
    huge: Integer = 40;

    usmall: UShort = 10;
    unormal: ULong = 20;
    ularge: UInt = 30;
    uhuge: UInteger = 40;

    single: SFloat = 2.5;
    floating: Float = 3.5;

    // Type-inferred bindings use := and always have an initializer.
    inferred_age := 26;
    inferred_pi := 3.14;
    inferred_letter := 'A';
    inferred_name := "Alpha";
    maybe: Optional<Int> = None;

    // reassignment
    x = 10;
    name = "Leitwolf";
    enabled = false;

    // val binding
    //
    // A val binding cannot be reassigned.
    // It does not make the referenced object deeply immutable.
    // Object, field, element and interior mutation are controlled by the
    // value's type, ownership and mutability rules.
    val version: Str = "1.0";
    val max_players: Int = 100;
    val is_ready: Bool = true;
    val inferred_version := "1.0";

    // compile-time constant
    //
    // A const value prefers compile-time evaluation.
    // If compile-time evaluation is not possible, initialization is lowered
    // to runtime evaluation.
    // After initialization, const is truly immutable.
    const BuildNumber: Int = 42;
    const AppName: Str = "Example";
    const DebugMode: Bool = false;
    const inferred_build_number := 42;

    // compile-time constant + static lifetime
    //
    // A static value must be compile-time evaluated and has static lifetime.
    // Static initialization never falls back to runtime evaluation.
    static CacheSize: Int = 1024;
    static ProductName: Str = "Example";
    static IsProduction: Bool = true;
}


// VALID
fn main() {
    x: Int = 5;
}


// INVALID
fn main() {
    x = 5;
}
// = is assignment, not a type-inferred declaration.


// INVALID
fn main() {
    missing := None;
}
// Optional<T> is never inferred. Write the Optional<T> annotation explicitly.


// VALID
fn main() {
    val x: Int = 5;
}


// INVALID
fn main() {
    val x: Int = 5;
    x = 10;
}
// Cannot reassign a val binding.


// VALID
fn main() {
    const Version: Int = 1;
}


// VALID
fn main() {
    const Version: Int = RuntimeValue();
}
// Valid for const if the runtime result can initialize a truly immutable value.


// VALID
fn main() {
    static BufferSize: Int = 4096;
}


// INVALID
fn main() {
    static BufferSize: Int = RuntimeValue();
}
// Static values must be compile-time constants and have static lifetime.


// INVALID
Version: Int = 1;
// Top-level variables are not allowed.
