// SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
// SPDX-License-Identifier: MPL-2.0

// data system:

//
// Data types are lightweight nominal containers.
//
// Data declarations may appear:
//
// - At top-level
// - Inside classes
//
// Data types may contain fields, constructors, and methods.
// Constructors and methods may be overloaded by parameter type list.
// Operator overload declarations use the method form:
//
// fn operator +(right: Type) -> Type { }
//
// A duplicate constructor or method parameter type list is an error.
// Data types do not support destructors.
// A data type may inherit any number of data types, but no other type category.
//
// Data types support generics.
//
// Data values use ordinary field literals:
//
// TypeName { field: value }
//
// Fields are accessed with ordinary member access:
//
// value.field
//

// simple data

data User {
    name: Str
    age: Int
}


// overloaded constructors and methods

data Point {
    x: Int
    y: Int

    Point(x: Int, y: Int) {
        self.x = x;
        self.y = y;
    }

    Point(value: Int) {
        self.x = value;
        self.y = value;
    }

    fn translate(dx: Int, dy: Int) { }
    fn translate(offset: Point) { }

    fn operator +(right: Point) -> Point {
        return right;
    }
}

first: Point = new Point(10, 20);
second: Point = new Point(10);


// initialization

user: User = {
    name: "Alpha",
    age: 26
};


// field access

println!("{}", user.name);

println!("{}", user.age);


// nested data

class Program {

    data User {
        name: Str
        age: Int
    }

}


// generic data

data Pair<T, U> {
    first: T
    second: U
}

// multiple value-type bases

data Named {
    label: Str
}

data Tagged {
    tag: Int
}

data NamedPair<T, U> : Named, Pair<T, U>, Tagged {
    label: Str
}


// generic data initialization

pair: Pair<Str, Int> = {
    first: "Alpha",
    second: 26
};


// generic data access

println!("{}", pair.first);

println!("{}", pair.second);


// multiple fields

data Point {
    x: Int
    y: Int
}


point: Point = {
    x: 10,
    y: 20
};


println!("{}", point.x);

println!("{}", point.y);


// VALID

data User {
    name: Str
    age: Int
}


// VALID

data Pair<T, U> {
    first: T
    second: U
}


// VALID

class Program {

    data User {
        name: Str
    }

}


// VALID

user: User = {
    name: "Alpha",
    age: 26
};


// VALID

user.name

user.age


// INVALID

data User {

    Drop() {
    }

}


// Destructors are not allowed.


// summary

//
// Data types are nominal containers with fields, constructors, and methods.
// Constructors, methods, and operators may be overloaded by parameter type list.
// Data does not support destructors.
// A data type may have any number of data bases.
//
// Data supports generics.
//
// Data may be declared at top-level
// or inside classes.
//
// Data values use ordinary field literals:
//
// TypeName { field: value }
//
// Fields are accessed with ordinary member access:
//
// value.field
//
