// SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
// SPDX-License-Identifier: MPL-2.0

// enum system:

//
// enum:
// - May be empty.
// - Cannot contain typed variants.
// - Has no numeric value.
// - Is not implicitly convertible to integers.
//
// enum data:
// - Must contain at least one typed variant.
// - May contain both typed and non-typed variants.
// - Each variant may contain at most one value.
// - Payload type may be any valid type.
// - Typed variants with the same name may be overloaded when their payload types differ.
// - Non-typed variants cannot be overloaded.
// - Regular enum variant names must be unique.
// - enum data may inherit any number of enum data types, but no other type category.
// - Standard generic enum data families may appear unspecialized in a base list.
// - A derived enum data remains usable as its base and inherits its variants and operations.
// - Optional<T> provides `Some: T` and `None`; Result<T, E> provides `Ok: T` and `Error: E`.
// - Regular enum cannot inherit.
//
// Visibility modifiers:
// - public
// - private
// - protected
// - internal
//
// Trailing commas are supported but not required.
//

// basic enum
enum Color {
    Red,
    Green,
    Blue,
}

// empty enum
enum Empty {
}

// enum usage
color: Color = Color::Red;


// data enum
enum data Token {
    Identifier: Str,
    Integer: Int,
    Plus,
}

// data enum usage
token: Token = Token::Identifier("hello");
number: Token = Token::Integer(42);
plus: Token = Token::Plus;


// payload type examples
class User {
}

enum data Value {
    Text: Str,
    Number: Int,
    Number: Long,
    User: User,
    Color: Color,
}

enum data TaggedValue {
    Tag: Int,
}

enum data ValueMetadata {
    Source: Str,
}

enum data RichValue : TaggedValue, ValueMetadata, Value {
    Number: Long,
}

enum data Option : Optional {
    Deferred: Str,
}

option: Optional<Str> = Option::Some("cached");

enum data MyResult : Result {
    Cached: Int,
}

result: Result<Int, Error> = MyResult::Ok(7);


// data enum overload selection
//
// A typed variant constructor is selected by its single argument type.
// The argument type must identify exactly one variant in the overload set.

value: Value = Value::Number(42);
large_value: Value = Value::Number(2'000'000'000);


// visibility
public enum Color {
    Red,
    Green,
    Blue,
}

internal enum Color {
    Red,
    Green,
    Blue,
}

public enum data Token {
    Identifier: Str,
    Integer: Int,
    Plus,
}

internal enum data Token {
    Identifier: Str,
    Integer: Int,
    Plus,
}


// trailing comma
enum Color {
    Red,
    Green,
    Blue,
}


// INVALID
enum DerivedColor : Color {
    Red,
}
// Regular enums cannot have base types.

enum Color {
    Red,
    Green,
    Blue
}


// pattern matching
fn parse(token: Token) {
    match (token) {
        Token::Identifier(value) -> {
        },
        Token::Integer(value) -> {
        },
        Token::Plus -> {
        },
        else -> {
        },
    }
}


// VALID
enum Color {
    Red,
    Green,
    Blue,
}


// VALID
enum Empty {
}


// VALID
enum data Token {
    Identifier: Str,
}


// VALID
enum data Token {
    Identifier: Str,
    Integer: Int,
    Plus,
    Minus,
}


// VALID
token: Token = Token::Identifier("hello");


// VALID
token: Token = Token::Integer(42);


// VALID
token: Token = Token::Plus;


// INVALID
enum Color {
    Red: Int,
}
// enum variants cannot contain types.


// INVALID
enum data Empty {
}
// enum data requires at least one typed variant.


// INVALID
enum data Token {
    Plus,
    Minus,
}
// enum data requires at least one typed variant.


// INVALID
enum data Token {
    Position: Int, Int,
}
// enum data variants can contain at most one value.


// INVALID
enum data Token {
    Position: (Int, Int),
}
// tuple payloads are not supported.


// INVALID
enum Color {
    Red,
    Red,
}
// regular enum variant names must be unique.


// INVALID
enum data Value {
    Number: Int,
    Number: Int,
}
// data enum overload payload types must differ.


// INVALID
enum data Value {
    None,
    None,
    Number: Int,
}
// non-typed data enum variants cannot be overloaded.


// INVALID
value: Int = Color::Red;
// enums cannot be converted to integers.


// INVALID
color: Color = 0;
// integers cannot be converted to enums.
