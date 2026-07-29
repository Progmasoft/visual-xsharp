// SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
// SPDX-License-Identifier: MPL-2.0

// Result module:

//
// Result provides explicit success/error values. Result<T, E> is an enum data
// family and both payload types are unrestricted. Postfix @ propagates E.
//

import fs, stdio;


// result data model

// The compiler provides the Result<T, E> enum data family with `Ok: T` and
// `Error: E` variants. The variant representation is class-backed. User source
// does not redeclare that standard enum data type.
// std::result::Error is the standard root error class and accepts a borrowed
// &Str message or an owned String message.
// Application error classes may derive from it nominally.

class ParseError : Error {
}

// Single-argument Result is valid only as Result<()>, which uses Error as its
// default error payload. Other success types must write both payloads:
// Result<Int, Error>, Result<(), Int>, and Result<Int, ()> are valid;
// Result<Int> is incomplete and invalid.
// Result is not the default return type. A function with no written return type
// returns unit. Other functions may declare another type. A body that uses postfix `@`, constructs
// `Ok(...)`, or constructs `Error(...)` must declare a Result return type.

// Standard generic enum data families may be inherited without specializing
// their parameters in the base list. The derived enum data remains a nominal
// subtype of Result and may use inherited Result variants and operations.
enum data MyResult : Result {
    Cached: Int,
}


// success construction

fn succeed() -> Result<()> {
    return Ok(());
}


// propagation

fn propagate() -> Result<()> {
    do_work()@;
    return Ok(());
}

fn do_work() -> Result<()> {
    return Ok(());
}

// The postfix @ operator propagates Error from the current function.
// On Ok(value), it evaluates to value.
// On Error(error), it returns that error from the enclosing function.


// explicit match without @

fn read_file_explicit(path: &Str) -> Result<String, Error> {
    file = match (std::fs::File::open(path)) {
        Ok(value) -> value,
        Error(error) -> return Error(error),
    };

    content: String = new String("");

    match (file.read_to_string(&mut content)) {
        Ok(else) -> {},
        Error(error) -> return Error(error),
    }

    return Ok(content);
}


// propagation with @

fn read_file(path: &Str) -> Result<String, Error> {
    file = std::fs::File::open(path)@;
    content: String = new String("");
    file.read_to_string(&mut content)@;
    return Ok(content);
}


// expect and unwrap

fn read_required_line() -> String {
    line: String = new String("");

    std::stdin()
        .read_line(&mut line)
        .expect("input could not be read");

    return line;
}

// expect(message) unwraps the success value or panics with message.
// unwrap() unwraps the success value or panics with the default Result panic.
