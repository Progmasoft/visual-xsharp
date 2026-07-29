// SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
// SPDX-License-Identifier: MPL-2.0

// Macro system:

//
// The macro system provides AST-based declarative macros.
//
// Macro expansion runs after AST creation and before HIR generation.
//
// Macros are defined with macro_rules!.
//
// Macros:
//
// - Match syntax fragments.
// - Expand into AST nodes.
// - Follow lexical scope rules.
// - May be declared at top level or inside another scope.
// - May be called before their textual declaration.
// - May call other macros when no recursion cycle is created.
// - Are not hygienic.
//

// include!() is a built-in source inclusion macro.
//
// It is recognized after the enclosing source has a structural AST.
// Its included text is then reparsed and materialized at the macro call site.
//
// It is not a macro_rules! declaration and is not resolved through
// the AST macro scope system.
//
// include!() accepts only a local relative file path string.
// The included file contributes declarations at the call site.
//
// Valid:
//
// include!("foo.xs");
// include!("./foo.xs");
// include!("../foo.xs");
//
// Invalid:
//
// include!();
// include!("/tmp/foo.xs");
//
// include!() cannot include packages, modules or libraries.
// It is only for local source files.
//
// format_args!() is a compiler-special built-in formatting macro.
// It validates the Rust 1.57 formatting grammar, which is the exact X# format
// syntax contract, and returns the formatting argument value used by
// writer/output macros. It is not resolved through Stdio.
// format_args_nl!() is also compiler-special and applies the same grammar while
// appending one newline to the formatted argument sequence.
// Neither macro participates in macro_rules! lookup or matcher expansion, and
// neither name can be declared or shadowed by a macro_rules! declaration.

import stdio;

// print!, println!, eprint!, eprintln!, and format! are normal macros exported
// by Stdio. They are not compiler built-ins.
//
// write! and writeln! are built-in writer macros. They are available without
// importing Stdio.
//
// assert!, assert_eq!, assert_ne!, debug_assert!, debug_assert_eq!, and
// panic! are normal macros exported by Panic. They are not compiler
// built-ins.
//
// include!, format_args!, format_args_nl!, write!, and writeln! are the
// currently specified built-in macros.
// The conceptual AST expansions and evaluation-order contracts of Stdio and
// Panic macros are specified in Stdio.xs and Panic.xs respectively.


// ============================================================
// Basic macro
// ============================================================

macro_rules! say {
    ($value:expr) -> {
        println!("{}", $value);
    };
}

say!("hello");


// Equivalent expansion:

fn expanded_main() {
    println!("{}", "Alpha");
}


// ============================================================
// Macro definition
// ============================================================

// General form:
//
// macro_rules! macro_name {
//     (matcher) -> {
//         expansion
//     };
// }


// macro_rules!
//
// - Begins a macro definition.
// - Must be followed by the macro name.
// - The macro name follows normal identifier rules.
// - snake_case is the canonical style for macro names, but it is not a hard
//   validity requirement.
// - Each matcher is written inside parentheses.
// - Each expansion is written inside braces.
// - Every macro rule must end with semicolon.
// - The final rule must also end with semicolon.


// Valid:

macro_rules! empty {
    () -> {
    };
}


// Invalid because the rule does not end with semicolon:

macro_rules! invalid_rule {
    () -> {
    }
}


// ============================================================
// Macro calls
// ============================================================

// Macro calls use ! followed by parentheses.

say!("Alpha");


// Only parentheses are valid for macro calls.


// Invalid:

say!["Alpha"];
say!{"Alpha"};


// A macro call used as a statement follows ordinary statement
// semicolon rules.


// ============================================================
// Multiple rules
// ============================================================

macro_rules! output {
    () -> {
        println!("");
    };

    ($value:expr) -> {
        println!("{}", $value);
    };
}


// A macro may contain multiple matcher rules.
//
// If no rule matches:
//
// - A compile-time error is produced.


// If multiple rules match:
//
// - A compile-time error is produced.
//
// A macro call must match exactly one rule.


// Example:

macro_rules! describe {
    ($expression:expr) -> {
        println!("expression");
    };

    ($identifier:ident) -> {
        println!("identifier");
    };
}

fn invalid_multiple_matching_rules() {
    describe!(value);
}


// INVALID: value can match both expr and ident.


// ============================================================
// Fragment specifiers
// ============================================================

// Supported macro fragment specifiers:
//
// - expr
// - ident
// - ty
// - path
// - pat
// - stmt
// - block
// - item
// - literal
// - meta
// - tt
// - lifetime
// - vis


// Expression:

macro_rules! expression_macro {
    ($value:expr) -> {
        print!("{}", $value);
    };
}


// Identifier:

macro_rules! identifier_macro {
    ($name:ident) -> {
        $name: Int = 42;
    };
}


// Type:

macro_rules! type_macro {
    ($type_value:ty) -> {
        value: $type_value;
    };
}


// Path:

macro_rules! path_macro {
    ($module_path:path) -> {
        $module_path;
    };
}


// Pattern:

macro_rules! pattern_macro {
    ($pattern_value:pat) -> {
        match (value) {
            $pattern_value -> {
            },
        }
    };
}


// Statement:

macro_rules! statement_macro {
    ($statement_value:stmt) -> {
        $statement_value
    };
}


// Block:

macro_rules! block_macro {
    ($block_value:block) -> {
        $block_value
    };
}


// Item:

macro_rules! item_macro {
    ($item_value:item) -> {
        $item_value
    };
}


// Literal:

macro_rules! literal_macro {
    ($literal_value:literal) -> {
        print!("{}", $literal_value);
    };
}


// Metadata:

macro_rules! metadata_macro {
    ($metadata_value:meta) -> {
        $metadata_value
    };
}


// Token tree:

macro_rules! token_tree_macro {
    ($token_value:tt) -> {
        $token_value
    };
}


// Lifetime:

macro_rules! lifetime_macro {
    ($lifetime_value:lifetime) -> {
        $lifetime_value
    };
}


// Visibility:

macro_rules! visibility_macro {
    ($visibility_value:vis) -> {
        $visibility_value fn generated() {
        }
    };
}


// ============================================================
// Repetition
// ============================================================

// Zero-or-more repetition:

// $(
//     ...
// )*


// One-or-more repetition:

// $(
//     ...
// )+


// Optional repetition with ? is not supported.


// Zero or more expressions:

macro_rules! print_many {
    ($(
        $value:expr
    )*) -> {
        $(
            println!("{}", $value);
        )*
    };
}


// One or more expressions:

macro_rules! print_at_least_one {
    ($(
        $value:expr
    )+) -> {
        $(
            println!("{}", $value);
        )*
    };
}


// ============================================================
// Repetition separators
// ============================================================

// Comma is the only supported repetition separator.


// Zero or more comma-separated expressions:

macro_rules! comma_separated {
    ($(
        $value:expr
    ),*) -> {
        $(
            println!("{}", $value);
        )*
    };
}


// One or more comma-separated expressions:

macro_rules! comma_separated_required {
    ($(
        $value:expr
    ),+) -> {
        $(
            println!("{}", $value);
        )*
    };
}


// Valid:

fn repetition_calls() {
    comma_separated!();
    comma_separated!(1);
    comma_separated!(1, 2, 3);

    comma_separated_required!(1);
    comma_separated_required!(1, 2, 3);
}


// The following separators are not supported:
//
// - semicolon
// - vertical bar
// - ->
// - any token other than comma


// ============================================================
// Macro variables
// ============================================================

// Macro variables begin with $.

macro_rules! duplicate_output {
    ($value:expr) -> {
        print!("{}{}", $value, $value);
    };
}


// A captured macro variable may be used multiple times in the
// expansion body.


// A macro variable name may be declared only once in the entire
// macro_rules! definition.


// Invalid: duplicate variable in one rule.

macro_rules! invalid_duplicate_in_rule {
    ($value:expr, $value:expr) -> {
    };
}


// Invalid: duplicate variable across different rules.

macro_rules! invalid_duplicate_across_rules {
    ($value:expr) -> {
    };

    ($value:ident) -> {
    };
}


// Invalid: duplicate variable inside and outside repetition.

macro_rules! invalid_duplicate_with_repetition {
    (
        $value:expr,
        $(
            $value:expr
        ),*
    ) -> {
    };
}


// Valid: every matcher variable has a unique name.

macro_rules! unique_variables {
    ($expression_value:expr) -> {
    };

    ($identifier_value:ident) -> {
    };
}


// ============================================================
// Scope
// ============================================================

// A macro may be declared at top level.

macro_rules! global_macro {
    () -> {
        println!("global");
    };
}


// A top-level macro is global.

// Macros are not prelude entries. `#[MacroExport]` places a top-level macro in
// its module's exported macro registry. Importing that module makes the macro
// available by its unqualified invocation name; qualified macro calls do not
// exist.

#[MacroExport]
macro_rules! exported_notice {
    () -> {
        println!("exported");
    };
}


// A macro may also be declared inside a scope.

fn local_macro_scope() {
    macro_rules! local_macro {
        () -> {
            println!("local");
        };
    }

    local_macro!();
}


// A scoped macro is available only in its declaring scope.


// Nested scopes may use a macro from an enclosing scope.

fn outer_scope() {
    macro_rules! outer_macro {
        () -> {
            println!("outer");
        };
    }

    {
        outer_macro!();
    }
}


// A macro declared inside a scope is not available outside that
// scope.


// ============================================================
// Declaration order
// ============================================================

// A macro may be called before its textual declaration in the
// same scope.

fn call_before_definition() {
    local_macro!();

    macro_rules! local_macro {
        () -> {
            println!("local");
        };
    }
}


// The compiler collects macro definitions in the relevant scope
// before expansion.


// ============================================================
// Generated declarations
// ============================================================

// Declarations produced by a macro become active in the scope in
// which the macro is called.

macro_rules! create_value {
    () -> {
        generated_value: Int = 42;
    };
}

fn generated_variable() {
    create_value!();

    print!("{}", generated_value);
}


// Generated declarations may be used before the textual macro call
// because macro expansion is completed before later semantic stages.

macro_rules! create_type {
    () -> {
        class GeneratedUser {
        }
    };
}

fn generated_type_before_call() {
    user: GeneratedUser = new GeneratedUser();

    create_type!();
}


// Generated declarations may include:
//
// - variables
// - functions
// - classes
// - enums
// - data declarations
// - other valid items


// ============================================================
// Macro hygiene
// ============================================================

// Macros are not hygienic.
//
// Identifiers created by an expansion are inserted directly into
// the call scope.


// Name conflicts produce compile-time errors.

macro_rules! create_conflicting_value {
    () -> {
        value: Int = 42;
    };
}

fn invalid_generated_name_conflict() {
    value: Int = 1;

    create_conflicting_value!();
}


// The expanded declaration conflicts with the existing value
// declaration in the same scope.


// ============================================================
// Macro-to-macro calls
// ============================================================

// A macro may call another macro.

macro_rules! inner_macro {
    ($value:expr) -> {
        println!("{}", $value);
    };
}

macro_rules! outer_macro {
    ($outer_value:expr) -> {
        inner_macro!($outer_value);
    };
}

fn nested_macro_call() {
    outer_macro!("Alpha");
}


// Macro call chains are allowed when they do not create a cycle.


// ============================================================
// Recursion
// ============================================================

// Direct macro recursion is forbidden.

macro_rules! invalid_direct_recursion {
    () -> {
        invalid_direct_recursion!();
    };
}


// Indirect macro recursion is also forbidden.

macro_rules! invalid_first {
    () -> {
        invalid_second!();
    };
}

macro_rules! invalid_second {
    () -> {
        invalid_first!();
    };
}


// The compiler constructs a macro call graph.
//
// Any direct or indirect cycle in the macro call graph is a
// compile-time error.


// ============================================================
// Expansion stage
// ============================================================

// Compiler flow:
//
// .xs source code
//     ↓
// Source include expansion
//     ↓
// Lexing + Parsing
//     ↓
// AST
//     ↓
// Macro expansion
//     ↓
// HIR


// Macro expansion operates directly on the AST.
//
// Expansion results are AST nodes.
//
// HIR generation receives the fully expanded AST.


// Macro expansion must complete before:
//
// - HIR generation
// - type checking
// - MIR generation
// - borrow checking
// - code generation


// ============================================================
// Declarative macro fallback rules
// ============================================================

// Macro behavior not otherwise defined by this file follows the declarative
// matching rules below.
//
// Explicit X# rules in this file always take precedence.
// In particular, X# keeps these differences:
//
// - Calls may appear before textual declaration in the same scope.
// - A call must match exactly one rule.
// - Macros are not hygienic.
// - Only comma is a repetition separator.
// - Optional `?` repetition is not supported.


// Matching proceeds one token at a time without arbitrary lookahead.
// A locally ambiguous matcher is a compile-time error.


// Delimited token trees preserve balanced (), [] and {} nesting.


// A captured fragment is forwarded as an opaque syntax fragment.
// A later macro may match it through the same fragment category or tt;
// it may not inspect the fragment's internal tokens unless tt was used.


// A metavariable used in an expansion must appear at the same repetition
// nesting depth and nesting order as in its matcher.


// Every expansion repetition must contain at least one metavariable.
// When one repetition uses multiple metavariables, their matched lengths
// must be equal at that repetition depth.


// Fragment follow restrictions use the Rust declarative-macro rules,
// limited to tokens and fragments that exist in X#:
//
// - expr and stmt may be followed by ->, comma or semicolon.
// - pat may be followed by ->, comma, =, if or in.
// - path and ty may be followed by ->, comma, =, |, semicolon,
//   colon, >, >>, [, {, as or a block fragment.
// - vis may be followed by comma, an identifier, a token that begins
//   a type, or an ident, ty or path fragment.
// - Other fragment kinds have no additional follow restriction.


// ============================================================
// Invalid unmatched call
// ============================================================

macro_rules! only_literal {
    ($value:literal) -> {
        print!("{}", $value);
    };
}

fn invalid_unmatched_macro_call() {
    only_literal!(first + second);
}

// If the argument does not match any rule, compilation fails.


// ============================================================
// Summary
// ============================================================

//
// Definition:
//
//     macro_rules! name {
//         (matcher) -> {
//             expansion
//         };
//     }
//
// Call:
//
//     name!(arguments);
//
// Call delimiters:
//
//     () only
//
// Fragment specifiers:
//
//     expr
//     ident
//     ty
//     path
//     pat
//     stmt
//     block
//     item
//     literal
//     meta
//     tt
//     lifetime
//     vis
//
// Repetition:
//
//     $(...)*
//     $(...)+
//     $(...),*
//     $(...),+
//
// Separator:
//
//     comma only
//
// Scope:
//
//     top-level macro -> global
//     scoped macro    -> declaring scope only
//
// Rule matching:
//
//     no match         -> compile-time error
//     one match        -> expand that rule
//     multiple matches -> compile-time error
//
// Recursion:
//
//     direct recursion   -> forbidden
//     indirect recursion -> forbidden
//
// Hygiene:
//
//     macros are not hygienic
//     generated names enter the call scope
//
// Built-in macro:
//
//     include!
//
// Module-exported macros:
//
//     Built-in macros include include!, format_args!, format_args_nl!, write!,
//     and writeln!
//     Stdio exports print!, println!, eprint!, eprintln!, and format!
//     Panic exports assert!, assert_eq!, assert_ne!, debug_assert!,
//     debug_assert_eq!, panic!
//
