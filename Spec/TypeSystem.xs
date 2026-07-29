// SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
// SPDX-License-Identifier: MPL-2.0

// X# type system baseline.

// Primitive type names are nominal language types.
// Even when two user-defined types have the same fields, type identity is
// based on the declared name.
// Str is an unsized immutable sequence of Unicode code points in the native
// UTF-32 representation. &Str is an explicit borrowed view; Str is never
// borrowed implicitly. String is a distinct heap-owned growable UTF-32 value.
// A string literal has type &Str, including when its type is inferred.

value_str: &Str = "text";
inferred_str := "Leitwolf";
value_bool: Bool = true;
numeric_false: Bool = 0;
numeric_true: Bool = 42;
inferred_number := 42; // Int, not Bool.

// Bool has a u8 runtime representation. In an explicit Bool context, an
// integer literal is normalized like C truth: zero is false and every nonzero
// value is true. Without that context, an integer literal keeps its normal
// default type, Int.

value_byte: Byte = 255;
value_s_byte: SByte = -1;
value_char: Char = 'A'; // Unicode scalar value, represented as u32.

value_short: Short = -32'000;
value_long: Long = 2'000'000'000;
value_int: Int = 9'000'000'000;
value_integer: Integer = 170'000'000'000'000'000'000;

value_u_short: UShort = 65'535;
value_u_long: ULong = 4'000'000'000;
value_u_int: UInt = 18'000'000'000;
value_u_integer: UInteger = 340'000'000'000'000'000'000;

value_s_float: SFloat = 1.0; // f16
value_l_float: LFloat = 1.0; // f32, "Long Float"
value_float: Float = 1.0;    // f64
value_double: Double = 1.0;  // f128

// Primitive widths are fixed by X# and do not follow the host pointer width.
// X# defines no x86-32-specific primitive or native-width alias; current
// native targets are x86-64 and ARM64.

// Optional<T> is resolved as if the compiler had inserted
// `import optional; using namespace std::optional;` and brought
// std::optional::Optional<T> into scope as Optional<T>.
// Optional<T> is compiler-provided enum data with `Some: T` and payload-free
// `None` variants. The compiler makes both constructors available.
// Users may import Optional explicitly, but normal source files do not need to.
// T? is canonical sugar for Optional<T>. A direct non-nil assignment in an
// Optional context is wrapped in Some(...) implicitly; nil becomes None.

age: Int? = 26;
missing_age: Int? = nil;
explicit_age: Optional<Int> = 26;
explicit_missing_age: Optional<Int> = nil;

// Borrowed and owned optional strings remain distinct.
borrowed_name: Optional<&Str> = "Leitwolf";
owned_name: Optional<String> = new String("Leitwolf");

// Result is also special. `import result;` is optional; the compiler behaves
// as if `using namespace std::result;` existed for Result<()>, Result<T, E>, Ok(...), and
// Error(...). Result<T, E> is enum data and both payload types are unrestricted.
// The only single-argument form is Result<()>, whose error payload defaults to Error.
// Most other std::* modules still require qualified names or
// explicit using declarations.

status: Result<Int, Error> = Ok(0);

empty_canonical: Optional<&Str> = None;
canonical_name: Optional<&Str> = Some("Leitwolf");
short_name: Optional<&Str> = canonical_name;

display: Str = name ?? "guest";
name ??= Some("guest");

// Automatic unboxing from Optional<T> to T may fail. New code models that as
// Error values through Result.

unboxed_name: Str = name;
forced_name: Str = name!;

user: Optional<User> = None;
city: Optional<Str> = user?.Address?.City;

fn normalize_optional_name(value: Optional<Str>) -> Result<Str, Error> {
    if (value == None) {
        return Error(new Error("name is missing"));
    }

    return Ok(value!);
}
