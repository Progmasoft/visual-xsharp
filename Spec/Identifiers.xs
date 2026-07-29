// SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
// SPDX-License-Identifier: MPL-2.0

// Identifier and numeric-literal rules:

//
// This file defines:
//
// - Identifier character rules
// - Reserved-word restrictions
// - Unicode restrictions
// - Decimal Integer literals
// - Decimal floating-point literals
// - Scientific notation
// - Digit separators
// - Unicode and hexadecimal character escapes
//


// ============================================================
// Identifiers
// ============================================================

// Identifiers are case-sensitive.

// Naming convention:
//
// X# permits normal ASCII identifiers in every position, but public
// language and standard-library names use role-based casing.
//
// Canonical path shape:
//
//     PascalCase::PascalCase.snake_case.lowercase().camel_case()
//
// Standard-library examples:
//
//     scores["Alpha"]
//     std::stdin().read_line(&mut input)
//
// Segment roles:
//
// - std is the standard-library root.
// - PascalCase is used for public modules, namespaces, and nominal types.
// - Short public acronyms may stay fully uppercase, for example std::fs::
// - snake_case is used for registry-like families such as collection kinds.
// - lowercase is used for short free functions such as get().
// - camel_case is used for methods and longer function names such as read_line().
//
// This is a style and standard-library naming rule. It does not make
// differently cased identifiers equivalent.


// Valid first characters:
//
// - A-Z
// - a-z
// - _


// Valid continuation characters:
//
// - A-Z
// - a-z
// - 0-9
// - _


// Formal shape:
//
// [A-Za-z_][A-Za-z0-9_]*


// Valid identifiers:

user
user_name
UserName
_value
value2
A1
_privateValue


// Identifier casing is significant:

value
Value
VALUE

// These are three different identifiers.


// ============================================================
// Invalid identifier characters
// ============================================================

// Hyphen is not allowed inside identifiers.


// Invalid:

user-name
app-version
my-value


// Unicode characters are not allowed inside identifiers.


// Invalid:

number
user_name
value_name
name-with-dash
1prefixed_name


// An identifier cannot begin with a digit.


// Invalid:

1value
42name
9_user


// Digits may appear after the first character.


// Valid:

value1
user42
_x2


// ============================================================
// Reserved words
// ============================================================

// Words defined by the language cannot be used as identifiers.


// Invalid examples:

fn: Int = 1;
class: &Str = "A";
module: Bool = true;
import: Int = 2;
if: Int = 3;
else: Int = 4;
return: Int = 5;
match: Int = 6;
new: Int = 7;
None: Int = 8;


// This rule applies to every language keyword and reserved word.


// ============================================================
// Unicode usage
// ============================================================

// Unicode characters are not written directly inside identifiers.
//
// Unicode characters may be represented inside string and character
// contents through escape sequences.


// Four hexadecimal digits:

"\u_ffff"


// Eight hexadecimal digits:

"\U000FFFFF"


// Hexadecimal character escape:

"\x_ff"


// These are character escapes.
//
// They are not hexadecimal numeric literals.


// ============================================================
// Decimal Integer literals
// ============================================================

// Integer literals use decimal notation.


// Valid:

0
1
42
1000
1000000


// Hexadecimal, binary and octal numeric prefixes are not supported.


// Invalid:

0xFF
0XFF
0b1010
0B1010
0o77
0O77


// Such values may be represented as strings when needed:

hex_value: &Str = "FF";
binary_value: &Str = "1010";
octal_value: &Str = "77";


// ============================================================
// Decimal floating-point literals
// ============================================================

// Floating-point literals use decimal notation.


// Valid:

0.0
3.14
0.000001
42.5


// ============================================================
// Digit separators
// ============================================================

// Apostrophe is used as the digit separator.
//
// Separators do not change the numeric value or inferred literal type.


// Valid:

1'000
1'000'000
3.141'592
0.000'001
12'345.678'901


// The separator does not change the numeric value.


// Equivalent:

1000000
1'000'000


// Apostrophe must separate digits.


// Invalid:

'100
100'
1''000
1'
'1


// Underscore is not a numeric digit separator.


// Invalid:

1_000
3.141_592


// ============================================================
// Scientific notation
// ============================================================

// Scientific notation is supported.


// Valid:

1e6
1E6
3.14e2
3.14E2


// The exponent sign is optional.


// Valid:

1e6
1e+6
1e-6

1E6
1E+6
1E-6


// Digit separators may also appear in scientific notation.


// Valid:

1'000e3
3.141'592e-2
1.234'567E+8
1e1'000
1'000E+2'000


// Invalid separator placement:

1e'10
1e10'
1e''10
1'e10
1e+'10
1e-'10


// ============================================================
// Lexer interpretation
// ============================================================

// The lexer must distinguish:
//
// - identifiers
// - reserved words
// - decimal Integer literals
// - decimal floating-point literals
// - scientific-notation literals
// - string and character escape sequences


// Identifier recognition:
//
// 1. Read A-Z, a-z or _ as the first character.
// 2. Continue through A-Z, a-z, 0-9 or _.
// 3. Compare the completed identifier against the reserved-word set.
// 4. Emit a keyword token when reserved.
// 5. Otherwise emit an identifier token.


// Numeric-literal recognition:
//
// 1. Read decimal digits.
// 2. Permit apostrophes only between valid digits.
// 3. Permit one decimal point for a floating-point literal.
// 4. Permit e or E for scientific notation.
// 5. Permit one optional + or - immediately after e or E.
// 6. Require exponent digits after e or E.
// 7. Reject invalid apostrophe placement.
// 8. Remove apostrophe separators before numeric conversion.


// ============================================================
// Valid examples
// ============================================================

fn valid_identifiers() {
    value: Int = 42;
    value2: Int = 1'000;
    _privateValue: Int = 3;

    floating_value: Float = 3.141'592;
    small_value: Float = 0.000'001;
    scientific_value: Float = 1.234'567e-8;
}


// ============================================================
// Invalid examples
// ============================================================

fn invalid_identifiers() {
    user-name: Int = 1;
    number_name: Int = 2;
    1value: Int = 3;
    class: Int = 4;
}


fn invalid_numeric_literals() {
    first: Int = 1_000;
    second: Int = '100;
    third: Int = 100';
    fourth: Int = 1''000;
    fifth: Int = 0xFF;
    sixth: Float = 1e'10;
}


// ============================================================
// Summary
// ============================================================

//
// Identifier:
//
//     [A-Za-z_][A-Za-z0-9_]*
//
// Identifier properties:
//
//     ASCII only
//     Case-sensitive
//     No hyphen
//     No Unicode characters
//     Reserved words forbidden
//
// Numeric literals:
//
//     Decimal only
//     Integer and floating-point forms
//     Scientific notation supported
//     e and E supported
//     Optional exponent sign
//
// Digit separator:
//
//     '
//
// Escape forms:
//
//     \u_ffff
//     \U000FFFFF
//     \x_ff
//
