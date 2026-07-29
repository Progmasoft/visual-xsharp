// SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
// SPDX-License-Identifier: MPL-2.0

// generics system:

//
// Generics are supported for classes and functions.
//
// Generics use monomorphization.
//
// Each concrete type produces a separate specialization.
//
// Type erasure does not exist.
//
// Generics are zero-cost abstractions.
//
// Generics must not introduce hidden allocations.
//
// Generic parameters may be constrained by interfaces.
//
// Multiple generic parameters are supported.
//
// Multiple constraints are supported.
//
// Default generic parameters are not supported.
//


// generic class

class Box<T> {
    value: T;
}


// generic class usage

number_box: Box<Int>;

text_box: Box<Str>;


// generic class with multiple parameters

class Pair<T, U> {
    first: T;
    second: U;
}


pair: Pair<Str, Int>;


// generic function

fn print<T>(value: T) {
}


// generic function usage

Print<Int>(42);

Print<Str>("Hello");


// generic class constraint

interface Printable {
    fn print();
}

class Box<T: Printable> {
    value: T;
}


// generic function constraint

interface Runnable {
    fn run();
}

fn execute<T: Runnable>(value: T) {
    value.run();
}


// multiple constraints

interface Runnable {
    fn run();
}

interface Printable {
    fn print();
}

fn execute<T: Runnable, Printable>(value: T) {

    value.run();

    value.print();
}


// multiple generic parameters with constraints

class Storage<T: Printable, U: Runnable> {

    value: T;

    worker: U;
}


// monomorphization

class Box<T> {
    value: T;
}

int_box: Box<Int>;

str_box: Box<Str>;


// Compiler generates separate specializations.
//
// Example:
//
// Box_int
// Box_str


// generic collections

users: ArrayList<Str>;

scores: [Str: Int];


// generic enum data

enum data Result<T, E> {
    Ok: T,
    Error: E,
}


// generic return type

fn create_box<T>(value: T) -> Box<T> {

    box: Box<T>;

    box.value = value;

    return box;
}


// VALID

class Box<T> {
    value: T;
}


// VALID

class Pair<T, U> {
    first: T;
    second: U;
}


// VALID

fn print<T>(value: T) {
}


// VALID

fn execute<T: Runnable>(value: T) {
}


// VALID

fn execute<T: Runnable, Printable>(value: T) {
}


// VALID

class Storage<T: Printable> {
    value: T;
}


// INVALID

class Box<T = Str> {
}


// Default generic parameters are not supported.


// INVALID

fn print<T = Str>(value: T) {
}


// Default generic parameters are not supported.


// INVALID

class Storage<T: UnknownInterface> {
    value: T;
}


// Constraint type must exist.


// performance guarantees

//
// Generics use monomorphization.
//
// No type erasure.
//
// No hidden allocation.
//
// Zero-cost abstraction.
//
// Allocation behavior must remain visible
// or predictable.
//
