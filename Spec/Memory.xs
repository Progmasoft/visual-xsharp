// SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
// SPDX-License-Identifier: MPL-2.0

// memory system:

//
// Memory management is based on:
//
// - Ownership
// - Move Semantics
// - Borrow Checking
// - Lifetime Analysis
// - RAII
// - Arena Allocation
//
// The current LLVM backend uses this ownership-oriented model. Managed memory
// for the future XPLR target is an X Platform runtime concern rather than an
// alternate X# source-language mode.
//
// All values are move-by-default.
//
// Assignments move ownership.
// Function arguments move ownership.
// Return values move ownership.
//
// After a move, the source value becomes invalid.
//
// Ownership is verified at compile time.
//
// Borrow checking is verified at compile time.
//
// Lifetime analysis is verified at compile time.
//
// Allocation must be visible or predictable.
//
// Hidden allocations are forbidden.
//
// Zero-cost abstractions are required.
//
// Iterators, generics and closures must not introduce
// runtime allocations unless explicitly documented.
//
// Small Short-lived values should remain on the stack.
//
// Escape analysis may move values when necessary.
//
// Resource destruction is deterministic.
//
// Destructors execute in LIFO order.
//
// None is a valid value for Optional<T>.
//

// ownership

user: User = new User();

user2: User = user;

// user moved
// user invalid


// move by function call

fn consume(user: User) {
}

fn main() {
    user: User = new User();

    consume(user);

    // user invalid
}


// move by return

fn create_user() -> User {
    user: User = new User();

    return user;
}

fn main() {
    user: User = create_user();
}


// immutable borrow

user: User = new User();

a: &User = &user;
b: &User = &user;


// mutable borrow

user: User = new User();

a: &mut User = &mut user;


// borrow rules

//
// At any given time, a value may have:
//
// - Exactly one mutable borrow (&mut T)
//
// or
//
// - One or more immutable borrows (&T)
//
// Mutable and immutable borrows cannot coexist.
//


// VALID

user: User = new User();

a: &User = &user;
b: &User = &user;


// VALID

user: User = new User();

a: &mut User = &mut user;


// INVALID

user: User = new User();

a: &mut User = &mut user;
b: &mut User = &mut user;


// INVALID

user: User = new User();

a: &User = &user;
b: &mut User = &mut user;


// INVALID

user: User = new User();

a: &mut User = &mut user;

user.get_name();


// lifetime example

fn print(user: &User) {
}

fn main() {
    user: User = new User();

    print(&user);
}


// INVALID

fn get_user() -> &User {
    user: User = new User();

    return &user;
}

// reference escapes its lifetime


// destructors

class File {
    File::Drop() {
        println!("File closed");
    }
}


// destructor execution

fn main() {
    file: File = new File();

} // File::Drop() runs here


// moved values

file: File = new File();

file2: File = file;

// file invalid
// only file2 owns the resource


// LIFO destruction order

fn main() {
    a: File = new File();
    b: File = new File();
    c: File = new File();
}

// destruction order:
//
// c.drop()
// b.drop()
// a.drop()


// None values

name: Optional<Str> = None;

enabled: Optional<Bool> = None;

user: Optional<User> = None;


// VALID

if (name == None) {
}


// VALID

if (enabled == None) {
}


// INVALID

if (enabled) {
}

// condition cannot be None


// stack allocation

point: Point = new Point(10, 20);

// compiler may keep this value on the stack


// escape analysis

fn create_point() -> Point {
    point: Point = new Point(10, 20);

    return point;
}

// compiler performs escape analysis


// zero-cost abstractions

iter: Iterator<Int> = numbers.iterator();


// no hidden allocation

// VALID
// allocation is explicit

users: ArrayList<User> = [];


// INVALID DESIGN

result: String = a + b;

// if allocation occurs here,
// it must be visible or warned by the compiler


// performance principles

//
// The current LLVM mode has no garbage collection.
//
// No Hidden Allocation.
//
// Deterministic Destruction.
//
// Predictable Resource Release.
//
// Ownership Verified At Compile Time.
//
// Borrow Checking Verified At Compile Time.
//
// Lifetime Analysis Verified At Compile Time.
//
// Zero-Cost Abstractions.
//
// Allocation Must Be Visible Or Predictable.
//
