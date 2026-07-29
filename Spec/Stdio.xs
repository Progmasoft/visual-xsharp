// SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
// SPDX-License-Identifier: MPL-2.0

// Stdio module:

//
// Stdio provides formatted text output macros, the std::fmt formatting API,
// and standard stream functions.
// print!, println!, eprint!, eprintln!, and format! are exported Stdio macros.
// Their declarations carry #[MacroExport]. They and std::fmt::* are made
// available through `import stdio`; they are not prelude or compiler built-ins.
// Exported macros are invoked without qualification, for example println!.
// format_args! and format_args_nl! are compiler-special built-in macros. They
// do not participate in macro_rules! matching and cannot be redefined or
// shadowed. write! and writeln! are built-in writer macros. None of these
// built-ins require Stdio to be imported.
//
// Stdio does not provide filesystem operations.
// Filesystem operations belong to the FS module.
//
// Stdio does not provide string interpolation.
// Formatted output uses placeholder-based formatting.
//

import stdio, fs;


// standard output macros

fn print_without_newline() {
    print!("Hello");
}

fn print_with_newline() {
    println!("Hello");
}

fn print_newline_only() {
    println!();
}


// standard error macros

fn error_without_newline() {
    eprint!("error");
}

fn error_with_newline() {
    eprintln!("error");
}

fn error_newline_only() {
    eprintln!();
}

// writer macros

fn write_to_stream() {
    write!(std::stdout(), "Hello");
}

fn write_line_to_stream() {
    writeln!(std::stdout(), "{} is {}", "Alpha", 26);
}

fn write_newline_only_to_stream() {
    writeln!(std::stdout());
}

// formatting

fn format_values() {
    user: &Str = "Alpha";
    age: Int = 26;

    println!("{} is {}", user, age);
    println!("{:?}", user);
    println!("{:#?}", user);
    println!("{:_>12}", user);
    println!("{:08x}", age);
}

// print!, eprint!, and format! require a Str format template as the first
// argument.
//
// println! and eprintln! either take no arguments or use the same format
// template form as print!/eprint!. With no arguments they write exactly one
// newline. With arguments they append exactly one newline after the formatted
// text.
//
// write! requires a destination expression followed by a Str format template.
// writeln! either takes only a destination expression or a destination followed
// by the same format template form. With only a destination it writes exactly
// one newline. With a template it appends exactly one newline after the
// formatted text.
//
// "{}" formats one value with the Display formatter.
// "{:?}" formats one value with the Debug formatter.
// "{:#?}" formats one value with the pretty Debug formatter.
// "{{" emits "{".
// "}}" emits "}".
//
// Placeholder count must match the number of following arguments.


// conceptual macro expansion

// Stdio macros and built-in writer macros expand during AST macro expansion,
// before HIR construction. Their source grammar and expansion structure match
// Rust 1.57 exactly. X# maps `$crate` to its standard runtime registry:
//
// print!(arguments...)
//   -> std::_print(format_args!(arguments...))
// println!()
//   -> print!("\n")
// println!(arguments...)
//   -> std::_print(format_args_nl!(arguments...))
// eprint!(arguments...)
//   -> std::_eprint(format_args!(arguments...))
// eprintln!()
//   -> eprint!("\n")
// eprintln!(arguments...)
//   -> std::_eprint(format_args_nl!(arguments...))
// write!(destination, arguments...)
//   -> destination.write_fmt(format_args!(arguments...))
// writeln!(destination[,])
//   -> write!(destination, "\n")
// writeln!(destination, arguments...)
//   -> destination.write_fmt(format_args_nl!(arguments...))
// format!(arguments...)
//   -> std::fmt::format(format_args!(arguments...))
//
// format_args_nl! is the compiler-special newline variant of format_args!. std::_print
// and std::_eprint are internal output boundaries. std::fmt::format
// materializes an owned UTF-32 String and belongs to the imported Stdio
// surface.
//
// A destination expression is evaluated exactly once. Format arguments are
// evaluated exactly once from left to right after the destination, and before
// the runtime write begins. The template and argument grammar are validated at compile time.
// A macro invocation must still match exactly one rule.


// format! returns String and does not write to a stream.
// Built-in format_args! returns the formatting argument value used by output
// and writer macros and does not write to a stream. format_args_nl! returns the
// same value with one trailing newline. Built-in write! and writeln! accept any
// compatible destination without importing Stdio.

fn build_message() {
    user1: &Str = "Alpha";
    user2: &Str = "Leitwolf";

    users: String = format!("{} {}", user1, user2);
    format_args!("{} {}", user1, user2);
    debug_users := format_args!("{:#?}", users);
    aligned: String = format!("{:^16}", user1);
    hex_age: String = format!("{:#x}", 26);
}

fn write_report_line(name: &Str, score: Int) {
    writeln!(std::stdout(), "{:<16} {:>4}", name, score);
}

fn write_debug_report<T>(value: T) {
    write!(std::stderr(), "{:#?}", value);
    writeln!(std::stderr());
}


// standard stream functions

fn standard_handles() {
    std::fs::write(std::stdout(), "stdout text\n");
    std::fs::write(std::stderr(), "stderr text\n");

    text: String = std::fs::read_to_str(std::stdin());
    println!("{}", text);
}

// std::stdout() and std::stderr() return standard output and error stream handles.
// std::stdin() returns the standard input stream handle.
// Raw reading and writing through these handles is provided by std::fs::
// Stdio macros use std::stdout() and std::stderr() internally.


// line input

fn read_line() {
    input: Optional<String> = Some(new String(""));

    std::stdin()
        .read_line(&mut input)
        .expect("input could not be read");

    println!("Input: {}", input.trim());
}

fn read_number() {
    input: Optional<String> = Some(new String(""));

    std::stdin()
        .read_line(&mut input)
        .expect("input could not be read");

    number: Int = input.trim().parse().expect("expected a valid number");
    println!("Number: {}", number);
}

fn read_many_numbers() {
    input: Optional<String> = Some(new String(""));

    std::stdin()
        .read_line(&mut input)
        .unwrap();

    numbers: ArrayList<Int> = input
        .split_whitespace()
        .map(fn(value) {
            return value.parse().expect("invalid number");
        });

    println!("Input: {:?}", numbers);
}


// invalid examples

fn invalid_non_string_template() {
    println!(10);
}

// INVALID: println! expects a Str format template as its first argument.


fn invalid_missing_placeholder() {
    println!("value", 10);
}

// INVALID: one argument is supplied but the template has no placeholder.


fn invalid_missing_argument() {
    println!("{}",);
}

// INVALID: the template has one placeholder but no value argument.


fn invalid_empty_print() {
    print!();
}

// INVALID: print! has no newline-only form.


fn invalid_empty_format_args() {
    format_args!();
}

// INVALID: built-in format_args! requires a Str format template.
