// SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
// SPDX-License-Identifier: MPL-2.0

// operators system:

//
// Arithmetic Operators:
// +
// -
// *
// /
// %
//
//
// Comparison Operators:
// ==
// !=
// >
// <
// >=
// <=
//
// Logical Operators:
// &&
// ||
// !
//
// Bitwise Operators:
// &
// |
// ^
// <<
// >>
//
// Optional Operators:
// ?.
// ??
// ??=
// !
//
// Increment and Decrement Operators:
// ++value
// --value
// value++
// value--
//
// Prefix update expressions produce the updated value. Postfix update
// expressions produce the value from before the update. All four forms still
// update their mutable operand exactly once.
//

fn main() {
    a: Int = 10;
    b: Int = 20;

    // arithmetic
    sum: Int = a + b;
    difference: Int = b - a;
    product: Int = a * b;
    quotient: Int = b / a;
    remainder: Int = b % 3;
    positive: Int = +a;
    negative: Int = -a;
    inverted: Bool = !(a == b);

    // equality
    if (a == b) {
    }

    if (a != b) {
    }

    // comparison
    if (a > b) {
    }

    if (a < b) {
    }

    if (a >= b) {
    }

    if (a <= b) {
    }

    // logical
    if (a > 0 && b > 0) {
    }

    if (a > 0 || b > 0) {
    }

    if (!(a == b)) {
    }

    // bitwise
    result: Int = a & b;
    result = a | b;
    result = a ^ b;
    result = a << 2;
    result = b >> 1;

    // optional conditional access
    maybe_user: Optional<User> = None;
    maybe_name: Optional<Str> = maybe_user?.Name;

    // optional coalescing
    display_name: Str = maybe_name ?? "guest";

    // optional coalescing assignment
    maybe_name ??= Some("guest");

    // optional-forgiving unboxing
    forced_name: Str = maybe_name!;

    // prefix and postfix update values
    counter: Int = 5;
    before_increment: Int = counter++;
    after_increment: Int = ++counter;
    before_decrement: Int = counter--;
    after_decrement: Int = --counter;
}


// VALID
if (1 == 1) {
}


// VALID
if (1 != 2) {
}


// VALID
if (true && true) {
}


// VALID
if (true || false) {
}


// VALID
if (!false) {
}


// VALID
value: Int = 10 & 20;


// VALID
value: Int = 10 | 20;


// VALID
value: Int = 28 / 4;


// VALID
value: Int = 15 % 8;


// VALID
value: Int = 3 ^ 4;


// VALID
value: Int = (7 << 1) >> 1;


// VALID
name: Optional<Str> = None;
display: Str = name ?? "fallback";


// VALID
name ??= Some("fallback");


// VALID
forced: Str = name!;


// VALID
value: Int = 1;
old_value: Int = value++;
new_value: Int = ++value;
