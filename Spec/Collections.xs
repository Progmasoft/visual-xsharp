// SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
// SPDX-License-Identifier: MPL-2.0

// Built-in collections:

//
// Collection types are part of the language type system. They are not aliases
// for nominal standard-library containers.
//
// - [T] is an array when it has no initializer.
// - [T] = [...] is a runtime-sized array initialized with those elements.
// - [T; N] is an explicitly sized fixed array.
// - [T] = {...} is a built-in set.
// - [K: V] is a built-in map.
// - ArrayList<T> is the built-in resizable sequence.
//
// There is no HashSet<T> type. The braces on a [T] initializer distinguish a
// set from an array. Collection member names use snake_case.
//

import stdio;

// ============================================================
// Arrays
// ============================================================

fn fixed_array_examples()
{
numbers: [Int] = [10, 20, 30];

// The effective type remains [Int]. Its current count is three; it is not
// silently refined to [Int; 3]. Runtime-sized arrays have fixed element count
// after construction. ArrayList<T> is used when the count must change.

repeated: [Int; 5];

// Numeric default construction produces [0, 0, 0, 0, 0]. Missing numeric
// initializer elements are filled with zero. Elements beyond an explicit
// fixed length are discarded.

trimmed: [Int; 2] = [1, 2, 3];


// Reading and changing elements

first_value: Int = numbers[0];
third_value: Int = numbers[2];

val mutable_elements: [Int] = [1, 2, 3];
mutable_elements[1] = 7;

// `val` prevents rebinding. Element mutability is a separate property of the
// collection value and element type.
//
// A calculated array index has type Int. An index outside 0..count causes a
// runtime bounds error before memory is accessed.

index: Int = 1;
selected: Int = numbers[index];


// Array properties

count: Int = numbers.count;
capacity: Int = numbers.capacity;
empty: Bool = numbers.is_empty;
start: Int = numbers.start_index;
end: Int = numbers.end_index;
first: Int = numbers.first;
last: Int = numbers.last;

// For an array, capacity equals count. start_index is zero and end_index
// is the position immediately after the last element; end_index is not a valid
// element index. first and last require a non-empty array.


// Array restrictions

// Operations that change element count are not available on [T] or [T; N].
// Use ArrayList<T> when insertion or removal is required.

}


// ============================================================
// Built-in sets
// ============================================================

fn set_examples()
{
users: [Str] = {"Leitwolf", "Helmut"};

users.append("Bob");
users.remove("Helmut");

has_bob: Bool = users.contains("Bob");
user_count: Int = users.count;
no_users: Bool = users.is_empty;

users.remove_all();

// Duplicate values collapse to one element according to the equality and hash
// semantics of T. Set iteration order is not insertion order and must not be
// used as stable program output.

}


// ============================================================
// Built-in maps
// ============================================================

fn map_examples()
{
ages: [Str: Optional<Int>] = [
    "Leitwolf": Some(26),
    "Helmut": Some(20),
];

ages["Bob"] = Some(25);

age: Optional<Int> = ages["Bob"];
has_bob: Bool = ages.contains("Bob");
age_count: Int = ages.count;
no_ages: Bool = ages.is_empty;

ages["Bob"] = None;
ages.remove("Helmut");

// Map lookup returns the declared V. A map whose lookup needs absence in its
// result should therefore use Optional<V> as the mapped type.

}


// ============================================================
// Resizable arrays
// ============================================================

fn array_list_examples()
{
fruits: ArrayList<Str> = ["Apple", "Banana"];

fruits.append("Orange");
fruits += ["Mango"];
fruits.append_contents(["Peach", "Pear"]);
fruits.insert("Cherry", 0);
fruits.insert_contents(["Lemon", "Lime"], 2);

fruits.remove(2);
fruits.remove_first();
fruits.remove_first(2);
fruits.remove_last();
fruits.remove_last(2);
fruits.remove_subrange(0, 2);
fruits.remove_all();

// remove_all(true) preserves allocated capacity.

fruits.remove_all(true);

}


// ============================================================
// Searching and transformations
// ============================================================

fn transformation_examples()
{
values: [Int] = [10, 20, 30, 20, 40];

has_twenty: Bool = values.contains(20);
first_twenty: Optional<Int> = values.first_index(20);
last_twenty: Optional<Int> = values.last_index(20);
first_large: Optional<Int> = values.first_where(fn(value) { value > 25 });
first_large_index: Optional<Int> = values.first_index_where(fn(value) { value > 25 });

doubled: [Int] = values.map(fn(value) { value * 2 });
large: [Int] = values.filter(fn(value) { value > 20 });
sum: Int = values.reduce(0, fn(total, value) { total + value });

parsed: [Int] = ["10", "invalid", "30"].compact_map(fn(value) {
    Int::parse(value)
});

ascending: [Int] = values.sorted();
descending: [Int] = values.sorted(fn(left, right) { left > right });

val sortable: [Int] = [4, 2, 5, 1, 3];
sortable.sort();
sortable.sort(fn(left, right) { left > right });

minimum: Optional<Int> = values.min();
maximum: Optional<Int> = values.max();
minimum_by: Optional<Int> = values.min_by(fn(left, right) { left < right });
maximum_by: Optional<Int> = values.max_by(fn(left, right) { left < right });

first_three: [Int] = values.prefix(3);
last_two: [Int] = values.suffix(2);
without_first: [Int] = values.drop_first();
without_first_two: [Int] = values.drop_first(2);
without_last: [Int] = values.drop_last();
without_last_two: [Int] = values.drop_last(2);
small_prefix: [Int] = values.prefix_while(fn(value) { value < 30 });
after_small_prefix: [Int] = values.drop_while(fn(value) { value < 30 });

joined: Str = values.reduce_into("", fn(result, value) {
    result += format!("{}", value);
});

values.for_each(fn(value) {
    println!("{}", value);
});

}


// ============================================================
// Iteration
// ============================================================

fn iteration_examples()
{
values: [Int] = [10, 20, 30];

for (value in values)
{
    println!("{}", value);
}

for (index in values.indices)
{
    println!("{} {}", index, values[index]);
}

for ((index, value) in values.enumerated())
{
    println!("{} {}", index, value);
}

}


// ============================================================
// Nested arrays
// ============================================================

fn nested_array_examples()
{
nested: [[Int]] = [
    [1, 2],
    [3, 4],
    [5],
];

nested_value: Int = nested[1][0];

flattened: [Int] = nested.flat_map(fn(group) { group });
}
