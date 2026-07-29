// SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
// SPDX-License-Identifier: MPL-2.0

// class system:

//
// Class declarations use braces.
// Parentheses are not used in class declarations.
//
// Fields are supported.
// Field initializers are optional.
// Properties use getter and setter accessors.
//
// Constructors are supported.
// Constructor name must match the class name.
//
// A class may contain overloaded constructors.
// Constructor overload resolution follows the same arity/type selection model as function overload resolution.
//
// Methods are supported.
// Static methods are supported.
// Static fields are supported.
//
// Methods with the same name are overloaded.
// Method merging is not supported.
// Two methods with the same name and the same parameter type list are a duplicate declaration error.
//
// Inheritance and interface implementation use one `:` base list.
// A class may inherit from multiple classes and implement multiple interfaces.
// Interfaces may inherit any number of interfaces, but no other type category.
// Base-list order is significant and is preserved in HIR for layout and dispatch planning.
// Base entries may carry public, protected, private, internal, and virtual inheritance modifiers.
// Omitted base access is internal.
// `extends` and `implements` are not separate declaration forms.
//
// Dispatch modifiers:
// - virtual introduces an overridable class method.
// - override verifies that a method replaces compatible inherited virtual slots; `virtual override` is valid.
// - sealed class prevents further inheritance.
// - sealed override prevents further overriding of that method slot.
//
// Visibility modifiers:
// - public
// - private
// - protected
// - internal
//
// Visibility rules:
// - public: visible from any module that can name the type/member.
// - private: visible only inside the declaring type or source scope.
// - protected: visible inside the declaring type and derived classes.
// - internal: visible inside the current project/package assembly boundary.
// - omitted: internal for declarations, members, and base entries.
//
// Class construction uses `new Type(...)`.
// Method calls use the dot operator.
//
// incomplete classes are supported.
//

// basic class
class User {
}

// fields
class User {
    name: String;
    age: Int;

    enabled: Bool = true;
}

// properties
public class Person {
    public Name: String {
        getter;
        setter;
    }

    public Age: Int {
        getter;
        setter;
    }
}

class User {
    private _age: Int;

    name: String {
        getter;
        setter;
    }

    age: Int {
        getter {
            return self._age;
        }

        setter {
            self._age = value;
        }
    }
}

// constructor
class User {
    name: String;

    User(name: String) {
        self.name = name;
    }
}

// object creation
class User {
    User() {
    }
}

fn main() {
    user: User = new User();
}

// methods
class User {
    fn get_name() -> &Str {
        return "Alpha";
    }
}

// method call
fn main() {
    user: User = new User();

    user.get_name();
}

// static field
class Math {
    static Pi: Float = 3.141592653589793;
}

// static method
class Math {
    static fn add(a: Int, b: Int) -> Int {
        return a + b;
    }
}

// inheritance
class Animal {
}

class Dog : Animal {
}

class Audited {
    virtual fn audit() {
    }
}

class WorkingDog : public Animal, public virtual Audited {
    override fn audit() {
    }
}

// interface
interface Runnable {
    fn run();
}

// interface implementation
class Program : Runnable {
    fn run() {
    }
}

// multiple interface implementation
interface Runnable {
    fn run();
}

interface Closeable {
    fn close();
}

class Program : Runnable, Closeable {
    fn run() {
    }

    fn close() {
    }
}

// visibility
class User {
    public name: String;

    private password: String;

    protected token: String;

    internal id: Int;
}

// incomplete class
incomplete class Animal {
    incomplete fn speak();
}

// method overloading
class Math {
    fn add(a: Int) -> Int {
        return a;
    }

    fn add(a: Int, b: Int) -> Int {
        return a + b;
    }
}

// These overloads stay distinct; X# does not merge method bodies.

// VALID
class User {
}


// VALID
class User {
    name: String;
}


// VALID
class User {
    name: String = new String("Alpha");
}


// VALID
class User {
    User(name: String) {
    }
}


// VALID
class User {
    User() {
    }

    User(name: String) {
    }
}
// Constructors may be overloaded by parameter type list.


// VALID
class Math {
    static Counter: Int = 0;
}


// VALID
class Program : Runnable, Closeable {
}

sealed class FinalProgram : Program, Runnable, Closeable {
}


// INVALID
class User() {
}
// Parentheses are not allowed in class declarations.
