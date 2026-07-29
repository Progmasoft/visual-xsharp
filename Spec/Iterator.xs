// SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
// SPDX-License-Identifier: MPL-2.0

// iterator system:

//
// Iterators are supported.
//
// Iterators are lazy.
//
// Iterators are zero-cost abstractions.
//
// Iterators use monomorphization.
//
// Type erasure does not exist.
//
// Boxing does not exist.
//
// Virtual dispatch does not exist.
//
// Hidden allocations are forbidden.
//
// Iterator chains must not create intermediate collections.
//
// Iterator usage must compile to code equivalent
// to an explicit loop.
//
// Iterator iteration moves yielded values by default.
//
// The source collection remains valid after iteration.
//
// Ownership rules apply normally during iteration.
//
// break and continue are supported inside iterator loops.
//



// array iteration

nums: [Int] = [1, 2, 3];

for (num in nums) {

    println!("{}", num);
}


// resizable array iteration

users: ArrayList<&Str> = [
    "Leitwolf",
    "Alpha",
];

for (user in users) {

    println!("{}", user);
}


// empty collection iteration

users: ArrayList<&Str> = [];

for (user in users) {

    println!("{}", user);
}

// loop body is never executed


// indexed iteration

users: ArrayList<&Str> = [
    "Leitwolf",
    "Alpha",
];

for ((index, user) in users.enumerated()) {

    println!("{}: {}", index, user);
}


// break

for (user in users) {

    if (user == "Alpha") {
        break;
    }
}


// continue

for (user in users) {

    if (user == "Leitwolf") {
        continue;
    }

    println!("{}", user);
}


// iterator filtering

users
    .iter()
    .filter(...);


// iterator mapping

users
    .iter()
    .map(...);


// iterator chaining

users
    .iter()
    .filter(...)
    .map(...);


// mutable iteration

for (user in &mut users) {

}


// collection remains valid

for (user in users) {

}

count: Int =
    users.count;


// VALID

for (item in users) {

}


// VALID

for ((index, item) in users.enumerated()) {

}


// VALID

users
    .iter()
    .filter(...)
    .map(...);


// VALID

for (item in &mut users) {

}


// performance guarantees

//
// Iterators are lazy.
//
// Iterators use monomorphization.
//
// Iterators are zero-cost abstractions.
//
// Iterator chains do not create
// intermediate collections.
//
// No hidden allocations.
//
// No boxing.
//
// No type erasure.
//
// No virtual dispatch.
//
