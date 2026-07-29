// SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
// SPDX-License-Identifier: MPL-2.0

// ownership system:

//
// All values are move-by-default.
//
// Assignments move ownership.
// Function arguments move ownership.
// Return values move ownership.
//
// After a move, the source value becomes invalid.
//
// Borrowing follows the X# ownership rules below.
//
// Immutable borrow:
// &T
//
// Mutable borrow:
// &mut T
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
// Borrow checking is performed at compile time.
//
// Lifetime analysis is performed at compile time.
//
// None is a valid value for Optional<T>.
//
// Ownership rules apply to all values,
// including Optional<T> values containing None.
//

// move by assignment

user: User = new User();

user2: User = user;

// user moved into user2
// user is now invalid


// move by function call

fn consume(user: User) {
}

fn main() {
    user: User = new User();

    consume(user);

    // user is now invalid
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

borrow1: &User = &user;
borrow2: &User = &user;


// immutable borrow function

fn print(user: &User) {
}

fn main() {
    user: User = new User();

    print(&user);
    print(&user);
}


// mutable borrow

user: User = new User();

borrow: &mut User = &mut user;


// mutable borrow function

fn rename(user: &mut User) {
}

fn main() {
    user: User = new User();

    rename(&mut user);
}


// VALID

user: User = new User();

a: &User = &user;
b: &User = &user;


// VALID

user: User = new User();

a: &User = &user;
b: &User = &user;
c: &User = &user;


// VALID

user: User = new User();

a: &mut User = &mut user;


// VALID

user: User = new User();

rename(&mut user);


// VALID

user: Optional<User> = None;

if (user == None) {
}


// INVALID

user: User = new User();

user2: User = user;

user.get_name();

// user was moved


// INVALID

fn consume(user: User) {
}

fn main() {
    user: User = new User();

    consume(user);

    user.get_name();
}

// user was moved


// INVALID

user: User = new User();

a: &User = &user;
b: &mut User = &mut user;

// mutable and immutable borrows cannot coexist


// INVALID

user: User = new User();

a: &mut User = &mut user;
b: &User = &user;

// mutable and immutable borrows cannot coexist


// INVALID

user: User = new User();

a: &mut User = &mut user;
b: &mut User = &mut user;

// multiple mutable borrows


// INVALID

user: User = new User();

a: &User = &user;

user = new User();

// value is immutably borrowed


// INVALID

user: User = new User();

a: &mut User = &mut user;

user.get_name();

// value is mutably borrowed


// VALID

user: User = new User();

a: &mut User = &mut user;

// borrow ends here

user.get_name();


// lifetime example

fn print(user: &User) {
}

fn main() {
    user: User = new User();

    print(&user);
}


// lifetime checking is performed at compile time

fn get_user() -> &User {
    user: User = new User();

    return &user;
}

// INVALID
// returning a reference to a value that does not live Long enough


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
