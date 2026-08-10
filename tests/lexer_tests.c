/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 */

#include "Visual/C23/diagnostic.hh"
#include "Visual/XSharp/lexer.h"

#include <stdio.h>
#include <string.h>

static int failures;

#define CHECK(condition)                                                                                               \
    do                                                                                                                 \
    {                                                                                                                  \
        if(!(condition))                                                                                               \
        {                                                                                                              \
            fprintf(stderr, "%s:%d: check failed: %s\n", __FILE__, __LINE__, #condition);                              \
            ++failures;                                                                                                \
        }                                                                                                              \
    } while(0)

#define CHECK_EQ(actual, expected)                                                                                     \
    do                                                                                                                 \
    {                                                                                                                  \
        const long long actual_value = (long long)(actual);                                                            \
        const long long expected_value = (long long)(expected);                                                        \
        if(actual_value != expected_value)                                                                             \
        {                                                                                                              \
            fprintf(stderr, "%s:%d: check failed: %s == %s; expected %lld, got %lld\n", __FILE__, __LINE__, #actual,   \
                    #expected, expected_value, actual_value);                                                          \
            ++failures;                                                                                                \
        }                                                                                                              \
    } while(0)

#define CHECK_TOKEN_EQ(actual_expression, expected_expression)                                                         \
    do                                                                                                                 \
    {                                                                                                                  \
        const XsTokenKind actual_value = (actual_expression);                                                          \
        const XsTokenKind expected_value = (expected_expression);                                                      \
        if(actual_value != expected_value)                                                                             \
        {                                                                                                              \
            fprintf(stderr, "%s:%d: expected token %s, got %s\n", __FILE__, __LINE__,                                  \
                    xs_token_kind_name(expected_value), xs_token_kind_name(actual_value));                             \
            ++failures;                                                                                                \
        }                                                                                                              \
    } while(0)

static void expect_tokens(const char *text, const XsTokenKind *expected, size_t count)
{
    XsSource source = {.path = "<test>", .text = text, .length = strlen(text)};
    XsDiagnostics diagnostics;
    XsLexer lexer;
    xs_diagnostics_init(&diagnostics);
    xs_lexer_init(&lexer, &source, &diagnostics);
    for(size_t i = 0; i < count; ++i)
    {
        XsToken actual = xs_lexer_next(&lexer);
        CHECK_TOKEN_EQ(actual.kind, expected[i]);
    }
    CHECK_EQ(lexer.cursor, source.length);
    CHECK(!xs_diagnostics_has_error(&diagnostics));
    xs_diagnostics_free(&diagnostics);
}

static void test_function(void)
{
    static const XsTokenKind expected[] = {
        XS_TOKEN_KW_FN,       XS_TOKEN_IDENTIFIER, XS_TOKEN_LEFT_PAREN, XS_TOKEN_IDENTIFIER, XS_TOKEN_COLON,
        XS_TOKEN_IDENTIFIER,  XS_TOKEN_COMMA,      XS_TOKEN_IDENTIFIER, XS_TOKEN_COLON,      XS_TOKEN_IDENTIFIER,
        XS_TOKEN_RIGHT_PAREN, XS_TOKEN_ARROW,      XS_TOKEN_IDENTIFIER, XS_TOKEN_LEFT_BRACE, XS_TOKEN_KW_RETURN,
        XS_TOKEN_IDENTIFIER,  XS_TOKEN_PLUS,       XS_TOKEN_IDENTIFIER, XS_TOKEN_SEMICOLON,  XS_TOKEN_RIGHT_BRACE,
        XS_TOKEN_EOF,
    };
    expect_tokens("fn Add(a: Int, b: Int) -> Int { return a + b; }", expected, sizeof(expected) / sizeof(expected[0]));
}

static void test_comments_and_strings(void)
{
    static const XsTokenKind expected[] = {XS_TOKEN_DOC_COMMENT, XS_TOKEN_MODULE_COMMENT, XS_TOKEN_STRING,
                                           XS_TOKEN_STRING,      XS_TOKEN_CHARACTER,      XS_TOKEN_CHARACTER,
                                           XS_TOKEN_EOF};
    expect_tokens("// ordinary\n//{ former block-comment spelling is an ordinary line comment\n/// docs\n//! "
                  "module\n\"A\\n\" \"\"\"multi\nline\"\"\" 'A' '\\n'",
                  expected, sizeof(expected) / sizeof(expected[0]));
    static const XsTokenKind next_line[] = {XS_TOKEN_KW_FN,       XS_TOKEN_IDENTIFIER, XS_TOKEN_LEFT_PAREN,
                                            XS_TOKEN_RIGHT_PAREN, XS_TOKEN_LEFT_BRACE, XS_TOKEN_RIGHT_BRACE,
                                            XS_TOKEN_EOF};
    expect_tokens("// a comment cannot continue onto the next physical line\nfn next() {}", next_line,
                  sizeof(next_line) / sizeof(next_line[0]));
    static const XsTokenKind block_delimiters[] = {XS_TOKEN_SLASH,      XS_TOKEN_STAR, XS_TOKEN_IDENTIFIER,
                                                   XS_TOKEN_IDENTIFIER, XS_TOKEN_STAR, XS_TOKEN_SLASH,
                                                   XS_TOKEN_EOF};
    expect_tokens("/* split\nline */", block_delimiters, sizeof(block_delimiters) / sizeof(block_delimiters[0]));
}

static void test_documented_operators(void)
{
    static const XsTokenKind expected[] = {
        XS_TOKEN_EQUAL,
        XS_TOKEN_NOT_EQUAL,
        XS_TOKEN_GREATER,
        XS_TOKEN_LESS,
        XS_TOKEN_GREATER_EQUAL,
        XS_TOKEN_LESS_EQUAL,
        XS_TOKEN_LOGICAL_AND,
        XS_TOKEN_LOGICAL_OR,
        XS_TOKEN_DOUBLE_COLON,
        XS_TOKEN_INFER_ASSIGN,
        XS_TOKEN_BANG,
        XS_TOKEN_AMPERSAND,
        XS_TOKEN_PIPE,
        XS_TOKEN_SHIFT_LEFT,
        XS_TOKEN_SHIFT_LEFT,
        XS_TOKEN_LESS,
        XS_TOKEN_SHIFT_RIGHT,
        XS_TOKEN_SHIFT_RIGHT,
        XS_TOKEN_GREATER,
        XS_TOKEN_CARET,
        XS_TOKEN_CARET_ASSIGN,
        XS_TOKEN_HASH,
        XS_TOKEN_ARROW,
        XS_TOKEN_ASSIGN,
        XS_TOKEN_GREATER,
        XS_TOKEN_QUESTION,
        XS_TOKEN_QUESTION_DOT,
        XS_TOKEN_QUESTION_QUESTION,
        XS_TOKEN_QUESTION_QUESTION_ASSIGN,
        XS_TOKEN_AT,
        XS_TOKEN_EOF,
    };
    expect_tokens("== != > < >= <= && || :: := ! & | << <<< >> >>> ^ ^= # -> => ? ?. ?? ?"
                  "?= @",
                  expected, sizeof(expected) / sizeof(expected[0]));
}

static void test_keywords(void)
{
    static const XsTokenKind expected[] = {
        XS_TOKEN_IDENTIFIER,   XS_TOKEN_KW_NAMESPACE, XS_TOKEN_KW_IMPORT, XS_TOKEN_KW_USING,  XS_TOKEN_KW_CLASS,
        XS_TOKEN_KW_INTERFACE, XS_TOKEN_KW_DATA,      XS_TOKEN_KW_ENUM,   XS_TOKEN_KW_ASYNC,  XS_TOKEN_KW_AWAIT,
        XS_TOKEN_KW_MOVE,      XS_TOKEN_KW_MUT,       XS_TOKEN_KW_OP,     XS_TOKEN_KW_EXTERN, XS_TOKEN_KW_NONE,
        XS_TOKEN_KW_VIRTUAL,   XS_TOKEN_KW_OVERRIDE,  XS_TOKEN_KW_SEALED, XS_TOKEN_KW_DO,     XS_TOKEN_KW_LOOP,
        XS_TOKEN_EOF,
    };
    expect_tokens(
        "module namespace import using class interface data enum async await move mut op extern None virtual override "
        "sealed do loop",
        expected, sizeof(expected) / sizeof(expected[0]));
    static const XsTokenKind removed_exception_words[] = {
        XS_TOKEN_IDENTIFIER, XS_TOKEN_IDENTIFIER, XS_TOKEN_IDENTIFIER,
        XS_TOKEN_IDENTIFIER, XS_TOKEN_IDENTIFIER, XS_TOKEN_EOF,
    };
    expect_tokens("try catch finally throw throws", removed_exception_words,
                  sizeof(removed_exception_words) / sizeof(removed_exception_words[0]));
}

static void test_identifiers_and_numbers(void)
{
    static const XsTokenKind expected[] = {
        XS_TOKEN_IDENTIFIER, XS_TOKEN_IDENTIFIER, XS_TOKEN_IDENTIFIER, XS_TOKEN_INTEGER,
        XS_TOKEN_FLOAT,      XS_TOKEN_FLOAT,      XS_TOKEN_FLOAT,      XS_TOKEN_EOF,
    };
    expect_tokens("userName _value value2 1'000 3.141'592 1e-6 1.234'567E+8", expected,
                  sizeof(expected) / sizeof(expected[0]));
}

static void test_lifetimes(void)
{
    static const XsTokenKind expected[] = {
        XS_TOKEN_AMPERSAND, XS_TOKEN_LIFETIME,  XS_TOKEN_IDENTIFIER, XS_TOKEN_AMPERSAND,
        XS_TOKEN_LIFETIME,  XS_TOKEN_KW_MUT,    XS_TOKEN_IDENTIFIER, XS_TOKEN_LIFETIME,
        XS_TOKEN_LIFETIME,  XS_TOKEN_CHARACTER, XS_TOKEN_EOF,
    };
    expect_tokens("&'a User &'b mut User 'static 'else 'A'", expected, sizeof(expected) / sizeof(expected[0]));
}

static void test_macro_tokens(void)
{
    static const XsTokenKind expected[] = {
        XS_TOKEN_KW_MACRO_RULES, XS_TOKEN_BANG,        XS_TOKEN_IDENTIFIER, XS_TOKEN_LEFT_BRACE, XS_TOKEN_LEFT_PAREN,
        XS_TOKEN_DOLLAR,         XS_TOKEN_IDENTIFIER,  XS_TOKEN_COLON,      XS_TOKEN_IDENTIFIER, XS_TOKEN_RIGHT_PAREN,
        XS_TOKEN_ARROW,          XS_TOKEN_LEFT_BRACE,  XS_TOKEN_DOLLAR,     XS_TOKEN_IDENTIFIER, XS_TOKEN_RIGHT_BRACE,
        XS_TOKEN_SEMICOLON,      XS_TOKEN_RIGHT_BRACE, XS_TOKEN_EOF,
    };
    expect_tokens("macro_rules! identity { ($value:expr) -> { $value }; }", expected,
                  sizeof(expected) / sizeof(expected[0]));
}

static void test_invalid_numbers(void)
{
    const char *texts[] = {"1_000", "100'", "1''000", "0xFF", "1e'10", "1value"};
    for(size_t i = 0; i < sizeof(texts) / sizeof(texts[0]); ++i)
    {
        XsSource source = {.path = "<test>", .text = texts[i], .length = strlen(texts[i])};
        XsDiagnostics diagnostics;
        XsLexer lexer;
        xs_diagnostics_init(&diagnostics);
        xs_lexer_init(&lexer, &source, &diagnostics);
        CHECK(xs_lexer_next(&lexer).kind == XS_TOKEN_ERROR);
        CHECK(xs_diagnostics_has_error(&diagnostics));
        xs_diagnostics_free(&diagnostics);
    }
}

static void test_invalid_characters(void)
{
    const char *texts[] = {"''", "'ab'", "'\n", "'\\q'", "'\\U00110000'", "'\\U0000d800'"};
    for(size_t i = 0; i < sizeof(texts) / sizeof(texts[0]); ++i)
    {
        XsSource source = {.path = "<test>", .text = texts[i], .length = strlen(texts[i])};
        XsDiagnostics diagnostics;
        XsLexer lexer;
        xs_diagnostics_init(&diagnostics);
        xs_lexer_init(&lexer, &source, &diagnostics);
        CHECK(xs_lexer_next(&lexer).kind == XS_TOKEN_ERROR);
        CHECK(xs_diagnostics_has_error(&diagnostics));
        xs_diagnostics_free(&diagnostics);
    }
}

static void test_utf32_character_literals(void)
{
    static const XsTokenKind expected[] = {XS_TOKEN_CHARACTER, XS_TOKEN_CHARACTER, XS_TOKEN_CHARACTER,
                                           XS_TOKEN_CHARACTER, XS_TOKEN_EOF};
    expect_tokens("'Ω' '\\u03a9' '😀' '\\U0001f600'", expected, sizeof(expected) / sizeof(expected[0]));
}

int main(void)
{
    test_function();
    test_comments_and_strings();
    test_documented_operators();
    test_keywords();
    test_identifiers_and_numbers();
    test_lifetimes();
    test_macro_tokens();
    test_invalid_numbers();
    test_invalid_characters();
    test_utf32_character_literals();
    return failures == 0 ? 0 : 1;
}
