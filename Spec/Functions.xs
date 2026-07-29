// SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
// SPDX-License-Identifier: MPL-2.0

// function and file structure:

//
// Statements that require a terminator use semicolons. A block's final
// expression omits the semicolon and becomes the block value.
// Top-level execution is not supported.
// Top-level functions are allowed.
//

import stdio;

fn add(a: Int, b: Int) -> Int {
  return a + b;
}

// -> declares the return type.
// If -> is omitted, the function returns unit `()`.
// Use `-> ()` when the unit return type must be written explicitly.

class Program {
  fn test() {
  }
}

// INVALID EXAMPLE:
// Top-level execution is not supported. This snippet is intentionally invalid
// and is not part of the valid declarations above.
fn add(a: Int, b: Int) -> Int {
  return a + b;
}

add(1, 2);

// VALID: top-level functions are allowed.
fn main() {
}

// VALID: omitted -> means unit.
fn test() {
}

// VALID: explicit unit return.
fn test_explicit_unit() -> () {
}


// named and inferred helper examples

fn clamp(value: Int, minimum: Int, maximum: Int) -> Int {
  if (value < minimum) {
    return minimum;
  }

  if (value > maximum) {
    return maximum;
  }

  return value;
}

fn describe_user(name: &Str, age: Int) -> String {
  return format!("{} ({})", name, age);
}

fn try_parse_id(text: &Str) -> Result<Int, Error> {
  parsed: Int = Int::parse(text);
  return Ok(parsed);
}


// lambdas infer parameter and return types from context

fn lambda_example() {
  add_values: fn(Int, Int) -> Int = fn(a, b) { a + b };
  println!("{}", add_values(2, 3));
}
