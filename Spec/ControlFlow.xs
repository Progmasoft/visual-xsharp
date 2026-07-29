// SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
// SPDX-License-Identifier: MPL-2.0

// control flow system:

//
// if/else may be used as a statement or as an expression.
// match may be used as a statement or as an expression.
//
// Supported statements:
// - if
// - else if
// - else
// - for
// - while
// - do/while
// - break
// - continue
// - match
// - else discard
//
// if statement syntax may omit `else`.
// if expression syntax requires `else` and ends with `};` when used
// as a full statement.
//
// `else: expression;` evaluates and explicitly discards an expression value.
// It is equivalent in intent to Rust's discard-binding pattern, but X# spells
// that default/discard position as `else`.
//
// match statement syntax ends with `}`.
// match expression syntax ends with `};` when used as a full statement,
// for example in `return match (...) { ... };`.
//

fn main() {
    x: Int = 5;

    if (x > 0) {
        x = 10;
    }

    if (x > 0) {
        x = 10;
    }
    else {
        x = 0;
    }

    if (x > 0) {
        x = 10;
    }
    else if (x < 0) {
        x = -10;
    }
    else {
        x = 0;
    }

    for (i: Int = 0; i < 10; i++) {
        if (i == 5) {
            break;
        }
    }

    for (i: Int = 0; i < 10; i++) {
        if (i == 5) {
            continue;
        }
    }

    while (x < 10) {
        x = x + 1;
    }

    do {
        x = x + 1;
    } while (x < 10);

    match (x) {
        0 -> {
            x = 1;
        },
        1 -> {
            x = 2;
        },
        else -> {
            x = 0;
        },
    }
}


// explicit discard

fn initialize() {
    else: a = new();
}


// VALID
fn test_if(x: Int) {
    if (x > 0) {
        x = 1;
    }
}


// VALID
fn test_else_if(x: Int) {
    if (x > 0) {
        x = 1;
    }
    else if (x < 0) {
        x = -1;
    }
    else {
        x = 0;
    }
}


// VALID
fn test_if_expression(x: Int) -> Int {
    return if (x > 0) {
        1
    }
    else {
        0
    };
}


// match expression

fn http_status_text(status: Int) -> Str {
    return match (status) {
        200 -> {
            "ok"
        },
        404 -> {
            "not found"
        },
        500 -> {
            "server error"
        },
        else -> {
            "unknown"
        },
    };
}


// if expression used in a local binding

fn select_port(debug: Bool) -> Int {
    port: Int = if (debug) {
        8080
    }
    else {
        80
    };

    return port;
}


// explicit discard for initialization-like side effects

fn warm_cache(cache: Cache) {
    for (key: Str in cache.keys()) {
        else: cache.load(key);
    }
}
// if expression branches must produce a value compatible with the
// surrounding expression type unless the branch diverges.


// VALID
fn test_for() {
    for (i: Int = 0; i < 10; i++) {
        continue;
    }
}


// VALID
fn test_while() {
    x: Int = 0;

    while (x < 10) {
        x = x + 1;
    }
}


// VALID
// `loop` is an unconditional loop. `break` exits it and `continue` starts the
// next iteration.
fn test_loop() {
    x: Int = 0;

    loop {
        x += 1;
        if (x == 10) {
            break;
        }
    }
}


// VALID
// A do/while body executes before its condition is tested. The construct is
// syntax sugar for an unconditional loop with a conditional break at its tail.
fn test_do_while() {
    x: Int = 10;

    do {
        x = x + 1;
    } while (x < 10);
}


// VALID
fn test_match(x: Int) {
    match (x) {
        0 -> {
            x = 10;
        },
        1 -> {
            x = 20;
        },
        else -> {
            x = 0;
        },
    }
}


// VALID
fn test_match_expression(x: Int) -> Int {
    return match (x) {
        0 -> {
            10
        },
        else -> {
            0
        },
    };
}
// match expression arms must produce a value compatible with the
// surrounding expression type unless the arm diverges.


// INVALID
fn invalid_break() {
    break;
}
// break can only be used inside loops.


// INVALID
fn invalid_continue() {
    continue;
}
// continue can only be used inside loops.


// INVALID
fn invalid_if_expression(x: Int) -> Int {
    return if (x > 0) {
        1
    };
}
// if expression requires else.
