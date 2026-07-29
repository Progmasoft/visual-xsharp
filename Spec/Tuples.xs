// SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
// SPDX-License-Identifier: MPL-2.0

// Tuple values are fixed-size, ordered, heterogeneous built-in values.
// Tuple types are structural; they are not nominal data declarations.

import stdio;


// Positional tuple

fn positional_person() -> (Str, Int, Bool)
{
  person: (Str, Int, Bool) = ("Leitwolf", 29, true);
  name: Str = person.0;
  age: Int = person.1;
  person.2 = false;
  return person;
}

// Positional member indices are compile-time tuple positions. They are not
// dynamic array indexing. An index outside the tuple arity is a compile error.


// Named tuple

fn named_person() -> (name: Str, age: Int, is_admin: Bool)
{
  person := (name: "Leitwolf", age: 29, is_admin: true);
  println!("{} {} {}", person.name, person.age, person.is_admin);
  return person;
}

// Named tuple labels are unique, ordered, and form part of the tuple type.
// Named and positional elements cannot be mixed in one tuple literal/type.
// Named tuples also retain positional access, although named access is
// canonical when a label exists.


// Unit and one-element tuples

fn tuple_forms()
{
  unit: () = ();
  one: (Int,) = (1,);
}

// Parentheses without a trailing comma group one value. `(value,)` is the
// one-element tuple form.


// Tuple patterns

fn classify_position(position: (Int, Int))
{
  match position
  {
    (0, 0) -> println!("origin"),
    (else, 0) -> println!("x-axis"),
    (0, else) -> println!("y-axis"),
    else -> println!("somewhere else"),
  }
}

// Tuple assignment follows ordinary binding and element mutability rules.
// `val` prevents rebinding the tuple variable; it does not independently make
// every referenced object deeply immutable.
