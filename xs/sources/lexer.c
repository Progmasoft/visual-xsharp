/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 */

#include "Visual/XSharp/lexer.h"

#include <ctype.h>

static bool at_end(const XsLexer *lexer)
{
    return lexer->cursor >= lexer->source->length;
}

static char peek(const XsLexer *lexer, size_t ahead)
{
    size_t offset = lexer->cursor + ahead;
    return offset < lexer->source->length ? lexer->source->text[offset] : '\0';
}

static bool starts_with(const XsLexer *lexer, const char *text, size_t length)
{
    if(lexer->cursor + length > lexer->source->length)
        return false;
    for(size_t i = 0; i < length; ++i)
    {
        if(lexer->source->text[lexer->cursor + i] != text[i])
            return false;
    }
    return true;
}

static XsToken token(XsTokenKind kind, size_t start, size_t end)
{
    return (XsToken){.kind = kind, .span = {.start = start, .end = end}};
}

static void error(XsLexer *lexer, size_t start, size_t end, const char *message)
{
    xs_diagnostics_add(lexer->diagnostics, XS_DIAGNOSTIC_ERROR, (XsSpan){start, end}, message);
}

static bool is_identifier_start(char character)
{
    unsigned char value = (unsigned char)character;
    return value == '_' || isalpha(value) != 0;
}

static bool is_identifier_continue(char character)
{
    unsigned char value = (unsigned char)character;
    return value == '_' || isalnum(value) != 0;
}

static void skip_line_comment(XsLexer *lexer)
{
    while(!at_end(lexer) && peek(lexer, 0) != '\n')
        ++lexer->cursor;
}

static void skip_trivia(XsLexer *lexer)
{
    for(;;)
    {
        while(!at_end(lexer) && isspace((unsigned char)peek(lexer, 0)) != 0)
            ++lexer->cursor;
        if(starts_with(lexer, "//", 2) && !starts_with(lexer, "///", 3) && !starts_with(lexer, "//!", 3))
        {
            skip_line_comment(lexer);
        }
        else
        {
            return;
        }
    }
}

static XsToken lex_doc_comment(XsLexer *lexer)
{
    size_t start = lexer->cursor;
    lexer->cursor += 3;
    skip_line_comment(lexer);
    return token(XS_TOKEN_DOC_COMMENT, start, lexer->cursor);
}

static XsToken lex_module_comment(XsLexer *lexer)
{
    size_t start = lexer->cursor;
    lexer->cursor += 3;
    skip_line_comment(lexer);
    return token(XS_TOKEN_MODULE_COMMENT, start, lexer->cursor);
}

static XsToken lex_identifier(XsLexer *lexer)
{
    size_t start = lexer->cursor++;
    while(is_identifier_continue(peek(lexer, 0)))
        ++lexer->cursor;
    XsTokenKind kind = xs_token_keyword(lexer->source->text + start, lexer->cursor - start);
    return token(kind, start, lexer->cursor);
}

static XsToken lex_number(XsLexer *lexer)
{
    size_t start = lexer->cursor;
    bool invalid = false;
    while(isdigit((unsigned char)peek(lexer, 0)) != 0 || peek(lexer, 0) == '\'')
    {
        if(peek(lexer, 0) == '\'' && (lexer->cursor == start || isdigit((unsigned char)peek(lexer, 1)) == 0))
            invalid = true;
        ++lexer->cursor;
    }
    XsTokenKind kind = XS_TOKEN_INTEGER;
    if(peek(lexer, 0) == '.' && isdigit((unsigned char)peek(lexer, 1)) != 0)
    {
        kind = XS_TOKEN_FLOAT;
        ++lexer->cursor;
        while(isdigit((unsigned char)peek(lexer, 0)) != 0 || peek(lexer, 0) == '\'')
        {
            if(peek(lexer, 0) == '\'' && isdigit((unsigned char)peek(lexer, 1)) == 0)
                invalid = true;
            ++lexer->cursor;
        }
    }
    if(peek(lexer, 0) == 'e' || peek(lexer, 0) == 'E')
    {
        kind = XS_TOKEN_FLOAT;
        ++lexer->cursor;
        if(peek(lexer, 0) == '+' || peek(lexer, 0) == '-')
            ++lexer->cursor;
        if(isdigit((unsigned char)peek(lexer, 0)) == 0)
            invalid = true;
        while(isdigit((unsigned char)peek(lexer, 0)) != 0 || peek(lexer, 0) == '\'')
        {
            if(peek(lexer, 0) == '\'' && isdigit((unsigned char)peek(lexer, 1)) == 0)
                invalid = true;
            ++lexer->cursor;
        }
    }
    if(peek(lexer, 0) == '\'' || peek(lexer, 0) == '_' || is_identifier_start(peek(lexer, 0)))
    {
        invalid = true;
        while(peek(lexer, 0) == '\'' || is_identifier_continue(peek(lexer, 0)))
            ++lexer->cursor;
    }
    if(invalid)
    {
        error(lexer, start, lexer->cursor, "invalid decimal numeric literal");
        return token(XS_TOKEN_ERROR, start, lexer->cursor);
    }
    return token(kind, start, lexer->cursor);
}

static bool is_hex(char character)
{
    return isxdigit((unsigned char)character) != 0;
}

static size_t scalar_utf8_length(const XsLexer *lexer)
{
    unsigned char first = (unsigned char)peek(lexer, 0);
    unsigned char second = (unsigned char)peek(lexer, 1);
    unsigned char third = (unsigned char)peek(lexer, 2);
    unsigned char fourth = (unsigned char)peek(lexer, 3);
    bool second_is_continuation = second >= 0x80U && second <= 0xbfU;
    bool third_is_continuation = third >= 0x80U && third <= 0xbfU;
    bool fourth_is_continuation = fourth >= 0x80U && fourth <= 0xbfU;
    if(first <= 0x7fU)
        return 1;
    if(first >= 0xc2U && first <= 0xdfU && second_is_continuation)
        return 2;
    if(first == 0xe0U && second >= 0xa0U && second <= 0xbfU && third_is_continuation)
        return 3;
    if(((first >= 0xe1U && first <= 0xecU) || (first >= 0xeeU && first <= 0xefU)) && second_is_continuation &&
       third_is_continuation)
        return 3;
    if(first == 0xedU && second >= 0x80U && second <= 0x9fU && third_is_continuation)
        return 3;
    if(first == 0xf0U && second >= 0x90U && second <= 0xbfU && third_is_continuation && fourth_is_continuation)
        return 4;
    if(first >= 0xf1U && first <= 0xf3U && second_is_continuation && third_is_continuation && fourth_is_continuation)
        return 4;
    if(first == 0xf4U && second >= 0x80U && second <= 0x8fU && third_is_continuation && fourth_is_continuation)
        return 4;
    return 0;
}

static unsigned hexadecimal_value(char digit)
{
    if(digit >= '0' && digit <= '9')
        return (unsigned)(digit - '0');
    if(digit >= 'a' && digit <= 'f')
        return (unsigned)(digit - 'a') + 10U;
    return (unsigned)(digit - 'A') + 10U;
}

static bool character_escape_is_scalar(const XsLexer *lexer, size_t slash)
{
    if(slash + 1U >= lexer->source->length || lexer->source->text[slash + 1U] != 'U')
        return true;
    unsigned value = 0;
    for(size_t index = slash + 2U; index < slash + 10U; ++index)
        value = (value << 4U) | hexadecimal_value(lexer->source->text[index]);
    return value <= 0x10ffffU && (value < 0xd800U || value > 0xdfffU);
}

static bool validate_escape(XsLexer *lexer, size_t slash)
{
    char escape = peek(lexer, 0);
    if(escape == '\'' || escape == '"' || escape == '\\' || escape == '0' || escape == 'a' || escape == 'b' ||
       escape == 'f' || escape == 'n' || escape == 'r' || escape == 't' || escape == 'v')
    {
        ++lexer->cursor;
        return true;
    }
    size_t digits = escape == 'u' ? 4 : escape == 'U' ? 8 : escape == 'x' ? 2 : 0;
    if(digits != 0)
    {
        ++lexer->cursor;
        for(size_t i = 0; i < digits; ++i)
        {
            if(!is_hex(peek(lexer, 0)))
            {
                error(lexer, slash, lexer->cursor, "invalid hexadecimal escape sequence");
                return false;
            }
            ++lexer->cursor;
        }
        return true;
    }
    if(!at_end(lexer))
        ++lexer->cursor;
    error(lexer, slash, lexer->cursor, "unknown escape sequence");
    return false;
}

static XsToken lex_string(XsLexer *lexer)
{
    size_t start = lexer->cursor;
    bool multiline = starts_with(lexer, "\"\"\"", 3);
    lexer->cursor += multiline ? 3 : 1;
    while(!at_end(lexer))
    {
        if(multiline && starts_with(lexer, "\"\"\"", 3))
        {
            lexer->cursor += 3;
            return token(XS_TOKEN_STRING, start, lexer->cursor);
        }
        if(!multiline && peek(lexer, 0) == '"')
        {
            ++lexer->cursor;
            return token(XS_TOKEN_STRING, start, lexer->cursor);
        }
        if(!multiline && peek(lexer, 0) == '\n')
            break;
        if(peek(lexer, 0) == '\\')
        {
            size_t slash = lexer->cursor++;
            (void)validate_escape(lexer, slash);
        }
        else
        {
            ++lexer->cursor;
        }
    }
    error(lexer, start, lexer->cursor, "unterminated string literal");
    return token(XS_TOKEN_ERROR, start, lexer->cursor);
}

static XsToken lex_character(XsLexer *lexer)
{
    size_t start = lexer->cursor++;
    if(is_identifier_start(peek(lexer, 0)))
    {
        size_t value_start = lexer->cursor;
        ++lexer->cursor;
        while(is_identifier_continue(peek(lexer, 0)))
            ++lexer->cursor;
        if(peek(lexer, 0) != '\'')
            return token(XS_TOKEN_LIFETIME, start, lexer->cursor);
        if(lexer->cursor - value_start == 1)
        {
            ++lexer->cursor;
            return token(XS_TOKEN_CHARACTER, start, lexer->cursor);
        }
        ++lexer->cursor;
        error(lexer, start, lexer->cursor, "character literal must contain one character");
        return token(XS_TOKEN_ERROR, start, lexer->cursor);
    }
    if(at_end(lexer) || peek(lexer, 0) == '\n' || peek(lexer, 0) == '\'')
    {
        error(lexer, start, lexer->cursor, "empty character literal");
        return token(XS_TOKEN_ERROR, start, lexer->cursor);
    }
    if(peek(lexer, 0) == '\\')
    {
        size_t slash = lexer->cursor++;
        if(!validate_escape(lexer, slash))
        {
            while(!at_end(lexer) && peek(lexer, 0) != '\n' && peek(lexer, 0) != '\'')
                ++lexer->cursor;
            if(peek(lexer, 0) == '\'')
                ++lexer->cursor;
            return token(XS_TOKEN_ERROR, start, lexer->cursor);
        }
        if(!character_escape_is_scalar(lexer, slash))
        {
            error(lexer, slash, lexer->cursor, "character escape must be one Unicode scalar value");
            while(!at_end(lexer) && peek(lexer, 0) != '\n' && peek(lexer, 0) != '\'')
                ++lexer->cursor;
            if(peek(lexer, 0) == '\'')
                ++lexer->cursor;
            return token(XS_TOKEN_ERROR, start, lexer->cursor);
        }
    }
    else
    {
        size_t length = scalar_utf8_length(lexer);
        if(length == 0)
        {
            ++lexer->cursor;
            error(lexer, start, lexer->cursor, "character literal must contain one Unicode scalar value");
        }
        else
        {
            lexer->cursor += length;
        }
    }
    if(peek(lexer, 0) != '\'')
    {
        while(!at_end(lexer) && peek(lexer, 0) != '\n' && peek(lexer, 0) != '\'')
            ++lexer->cursor;
        if(peek(lexer, 0) == '\'')
            ++lexer->cursor;
        error(lexer, start, lexer->cursor, "character literal must contain one character");
        return token(XS_TOKEN_ERROR, start, lexer->cursor);
    }
    ++lexer->cursor;
    return token(XS_TOKEN_CHARACTER, start, lexer->cursor);
}

static XsToken one_or_two(XsLexer *lexer, size_t start, char second, XsTokenKind one, XsTokenKind two)
{
    if(peek(lexer, 0) == second)
    {
        ++lexer->cursor;
        return token(two, start, lexer->cursor);
    }
    return token(one, start, lexer->cursor);
}

void xs_lexer_init(XsLexer *lexer, const XsSource *source, XsDiagnostics *diagnostics)
{
    *lexer = (XsLexer){.source = source, .diagnostics = diagnostics};
}

XsToken xs_lexer_next(XsLexer *lexer)
{
    skip_trivia(lexer);
    if(at_end(lexer))
        return token(XS_TOKEN_EOF, lexer->cursor, lexer->cursor);
    if(starts_with(lexer, "///", 3))
        return lex_doc_comment(lexer);
    if(starts_with(lexer, "//!", 3))
        return lex_module_comment(lexer);

    size_t start = lexer->cursor;
    char character = peek(lexer, 0);
    if(is_identifier_start(character))
        return lex_identifier(lexer);
    if(isdigit((unsigned char)character) != 0)
        return lex_number(lexer);
    if(character == '"')
        return lex_string(lexer);
    if(character == '\'')
        return lex_character(lexer);
    ++lexer->cursor;
    switch(character)
    {
    case '(':
        return token(XS_TOKEN_LEFT_PAREN, start, lexer->cursor);
    case ')':
        return token(XS_TOKEN_RIGHT_PAREN, start, lexer->cursor);
    case '{':
        return token(XS_TOKEN_LEFT_BRACE, start, lexer->cursor);
    case '}':
        return token(XS_TOKEN_RIGHT_BRACE, start, lexer->cursor);
    case '[':
        return token(XS_TOKEN_LEFT_BRACKET, start, lexer->cursor);
    case ']':
        return token(XS_TOKEN_RIGHT_BRACKET, start, lexer->cursor);
    case ',':
        return token(XS_TOKEN_COMMA, start, lexer->cursor);
    case ';':
        return token(XS_TOKEN_SEMICOLON, start, lexer->cursor);
    case ':':
        if(peek(lexer, 0) == ':')
        {
            ++lexer->cursor;
            return token(XS_TOKEN_DOUBLE_COLON, start, lexer->cursor);
        }
        if(peek(lexer, 0) == '=')
        {
            ++lexer->cursor;
            return token(XS_TOKEN_INFER_ASSIGN, start, lexer->cursor);
        }
        return token(XS_TOKEN_COLON, start, lexer->cursor);
    case '.':
        if(starts_with(lexer, "..", 2))
        {
            lexer->cursor += 2;
            return token(XS_TOKEN_ELLIPSIS, start, lexer->cursor);
        }
        return token(XS_TOKEN_DOT, start, lexer->cursor);
    case '=':
        return one_or_two(lexer, start, '=', XS_TOKEN_ASSIGN, XS_TOKEN_EQUAL);
    case '!':
        return one_or_two(lexer, start, '=', XS_TOKEN_BANG, XS_TOKEN_NOT_EQUAL);
    case '?':
        if(peek(lexer, 0) == '?')
        {
            ++lexer->cursor;
            if(peek(lexer, 0) == '=')
            {
                ++lexer->cursor;
                return token(XS_TOKEN_QUESTION_QUESTION_ASSIGN, start, lexer->cursor);
            }
            return token(XS_TOKEN_QUESTION_QUESTION, start, lexer->cursor);
        }
        if(peek(lexer, 0) == '.')
        {
            ++lexer->cursor;
            return token(XS_TOKEN_QUESTION_DOT, start, lexer->cursor);
        }
        return token(XS_TOKEN_QUESTION, start, lexer->cursor);
    case '>':
        if(peek(lexer, 0) == '>')
        {
            ++lexer->cursor;
            return token(XS_TOKEN_SHIFT_RIGHT, start, lexer->cursor);
        }
        return one_or_two(lexer, start, '=', XS_TOKEN_GREATER, XS_TOKEN_GREATER_EQUAL);
    case '<':
        if(peek(lexer, 0) == '<')
        {
            ++lexer->cursor;
            return token(XS_TOKEN_SHIFT_LEFT, start, lexer->cursor);
        }
        return one_or_two(lexer, start, '=', XS_TOKEN_LESS, XS_TOKEN_LESS_EQUAL);
    case '+':
        if(peek(lexer, 0) == '+')
            return one_or_two(lexer, start, '+', XS_TOKEN_PLUS, XS_TOKEN_PLUS_PLUS);
        return one_or_two(lexer, start, '=', XS_TOKEN_PLUS, XS_TOKEN_PLUS_ASSIGN);
    case '-':
        if(peek(lexer, 0) == '>')
            return one_or_two(lexer, start, '>', XS_TOKEN_MINUS, XS_TOKEN_ARROW);
        if(peek(lexer, 0) == '-')
            return one_or_two(lexer, start, '-', XS_TOKEN_MINUS, XS_TOKEN_MINUS_MINUS);
        return one_or_two(lexer, start, '=', XS_TOKEN_MINUS, XS_TOKEN_MINUS_ASSIGN);
    case '*':
        return one_or_two(lexer, start, '=', XS_TOKEN_STAR, XS_TOKEN_STAR_ASSIGN);
    case '/':
        return one_or_two(lexer, start, '=', XS_TOKEN_SLASH, XS_TOKEN_SLASH_ASSIGN);
    case '%':
        return one_or_two(lexer, start, '=', XS_TOKEN_PERCENT, XS_TOKEN_PERCENT_ASSIGN);
    case '&':
        if(peek(lexer, 0) == '&')
            return one_or_two(lexer, start, '&', XS_TOKEN_AMPERSAND, XS_TOKEN_LOGICAL_AND);
        return one_or_two(lexer, start, '=', XS_TOKEN_AMPERSAND, XS_TOKEN_AMPERSAND_ASSIGN);
    case '|':
        if(peek(lexer, 0) == '|')
            return one_or_two(lexer, start, '|', XS_TOKEN_PIPE, XS_TOKEN_LOGICAL_OR);
        return one_or_two(lexer, start, '=', XS_TOKEN_PIPE, XS_TOKEN_PIPE_ASSIGN);
    case '^':
        return one_or_two(lexer, start, '=', XS_TOKEN_CARET, XS_TOKEN_CARET_ASSIGN);
    case '#':
        return token(XS_TOKEN_HASH, start, lexer->cursor);
    case '@':
        return token(XS_TOKEN_AT, start, lexer->cursor);
    case '$':
        return token(XS_TOKEN_DOLLAR, start, lexer->cursor);
    default:
        error(lexer, start, lexer->cursor, "unrecognized character");
        return token(XS_TOKEN_ERROR, start, lexer->cursor);
    }
}
