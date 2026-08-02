/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 */

#include "xs/parser.h"

static void advance(XsParser *parser)
{
    parser->previous = parser->current;
    do
    {
        parser->current = xs_lexer_next(&parser->lexer);
    } while(parser->current.kind == XS_TOKEN_DOC_COMMENT || parser->current.kind == XS_TOKEN_MODULE_COMMENT);
}

static bool accept(XsParser *parser, XsTokenKind kind)
{
    if(parser->current.kind != kind)
        return false;
    advance(parser);
    return true;
}

static bool expect(XsParser *parser, XsTokenKind kind, const char *message)
{
    if(accept(parser, kind))
        return true;
    xs_diagnostics_add(parser->diagnostics, XS_DIAGNOSTIC_ERROR, parser->current.span, message);
    return false;
}

static XsVisibility parse_visibility(XsParser *parser)
{
    if(accept(parser, XS_TOKEN_KW_PUBLIC))
        return XS_VISIBILITY_PUBLIC;
    if(accept(parser, XS_TOKEN_KW_PRIVATE))
        return XS_VISIBILITY_PRIVATE;
    if(accept(parser, XS_TOKEN_KW_PROTECTED))
        return XS_VISIBILITY_PROTECTED;
    if(accept(parser, XS_TOKEN_KW_INTERNAL))
        return XS_VISIBILITY_INTERNAL;
    return XS_VISIBILITY_DEFAULT;
}

static bool skip_balanced(XsParser *parser, XsTokenKind open, XsTokenKind close, XsSpan *span)
{
    if(parser->current.kind != open)
        return false;
    size_t start = parser->current.span.start;
    size_t depth = 0;
    do
    {
        if(parser->current.kind == open)
            ++depth;
        else if(parser->current.kind == close)
            --depth;
        advance(parser);
        if(parser->previous.kind == XS_TOKEN_EOF || parser->previous.kind == XS_TOKEN_ERROR)
            break;
    } while(depth != 0);
    if(depth != 0)
    {
        xs_diagnostics_add(parser->diagnostics, XS_DIAGNOSTIC_ERROR, (XsSpan){start, parser->previous.span.end},
                           "unterminated delimited construct");
        return false;
    }
    *span = (XsSpan){start, parser->previous.span.end};
    return true;
}

static bool skip_until_semicolon(XsParser *parser, XsSpan *span)
{
    size_t start = parser->current.span.start;
    while(parser->current.kind != XS_TOKEN_SEMICOLON && parser->current.kind != XS_TOKEN_EOF)
    {
        XsSpan ignored;
        if(parser->current.kind == XS_TOKEN_LEFT_PAREN)
        {
            if(!skip_balanced(parser, XS_TOKEN_LEFT_PAREN, XS_TOKEN_RIGHT_PAREN, &ignored))
                return false;
        }
        else if(parser->current.kind == XS_TOKEN_LEFT_BRACKET)
        {
            if(!skip_balanced(parser, XS_TOKEN_LEFT_BRACKET, XS_TOKEN_RIGHT_BRACKET, &ignored))
                return false;
        }
        else
        {
            advance(parser);
        }
    }
    if(!expect(parser, XS_TOKEN_SEMICOLON, "expected ';'"))
        return false;
    *span = (XsSpan){start, parser->previous.span.end};
    return true;
}

static bool parse_named_block(XsParser *parser, XsAst *ast, XsAstItemKind kind, XsVisibility visibility,
                              bool incomplete, bool data_enum, size_t start)
{
    if(!expect(parser, XS_TOKEN_IDENTIFIER, "expected declaration name"))
        return false;
    XsSpan name = parser->previous.span;
    if(parser->current.kind == XS_TOKEN_LEFT_PAREN)
    {
        xs_diagnostics_add(parser->diagnostics, XS_DIAGNOSTIC_ERROR, parser->current.span,
                           "parentheses are not allowed in this declaration");
        return false;
    }
    if(accept(parser, XS_TOKEN_LESS))
    {
        size_t depth = 1;
        while(depth != 0 && parser->current.kind != XS_TOKEN_EOF)
        {
            if(accept(parser, XS_TOKEN_LESS))
                ++depth;
            else if(accept(parser, XS_TOKEN_GREATER))
                --depth;
            else
                advance(parser);
        }
        if(depth != 0)
        {
            xs_diagnostics_add(parser->diagnostics, XS_DIAGNOSTIC_ERROR, name, "unterminated generic parameter list");
            return false;
        }
    }
    XsSpan body;
    if(!skip_balanced(parser, XS_TOKEN_LEFT_BRACE, XS_TOKEN_RIGHT_BRACE, &body))
    {
        xs_diagnostics_add(parser->diagnostics, XS_DIAGNOSTIC_ERROR, parser->current.span, "expected declaration body");
        return false;
    }
    XsAstItem item = {.span = {start, body.end},
                      .name = name,
                      .body = body,
                      .visibility = visibility,
                      .is_incomplete = incomplete,
                      .is_data_enum = data_enum};
    return xs_ast_push(ast, (XsAstNode){.kind = kind, .item = item});
}

static bool parse_function(XsParser *parser, XsAst *ast, XsVisibility visibility, bool is_async, bool incomplete,
                           size_t start)
{
    if(!expect(parser, XS_TOKEN_IDENTIFIER, "expected function name"))
        return false;
    XsSpan name = parser->previous.span;
    if(accept(parser, XS_TOKEN_LESS))
    {
        size_t depth = 1;
        while(depth != 0 && parser->current.kind != XS_TOKEN_EOF)
        {
            if(accept(parser, XS_TOKEN_LESS))
                ++depth;
            else if(accept(parser, XS_TOKEN_GREATER))
                --depth;
            else
                advance(parser);
        }
    }
    XsSpan parameters;
    if(!skip_balanced(parser, XS_TOKEN_LEFT_PAREN, XS_TOKEN_RIGHT_PAREN, &parameters))
    {
        xs_diagnostics_add(parser->diagnostics, XS_DIAGNOSTIC_ERROR, parser->current.span,
                           "expected function parameter list");
        return false;
    }
    while(parser->current.kind != XS_TOKEN_LEFT_BRACE && parser->current.kind != XS_TOKEN_SEMICOLON &&
          parser->current.kind != XS_TOKEN_EOF)
        advance(parser);

    XsSpan body = {parser->current.span.start, parser->current.span.start};
    if(accept(parser, XS_TOKEN_SEMICOLON))
    {
        body = parser->previous.span;
    }
    else if(!skip_balanced(parser, XS_TOKEN_LEFT_BRACE, XS_TOKEN_RIGHT_BRACE, &body))
    {
        xs_diagnostics_add(parser->diagnostics, XS_DIAGNOSTIC_ERROR, parser->current.span,
                           "expected function body or ';'");
        return false;
    }
    XsAstItem item = {.span = {start, body.end},
                      .name = name,
                      .body = body,
                      .visibility = visibility,
                      .is_async = is_async,
                      .is_incomplete = incomplete};
    return xs_ast_push(ast, (XsAstNode){.kind = XS_AST_FUNCTION, .item = item});
}

static bool parse_macro(XsParser *parser, XsAst *ast, size_t start)
{
    if(!expect(parser, XS_TOKEN_BANG, "expected '!' after macro_rules"))
        return false;
    if(!expect(parser, XS_TOKEN_IDENTIFIER, "expected macro name"))
        return false;
    XsSpan name = parser->previous.span;
    if(!expect(parser, XS_TOKEN_LEFT_BRACE, "expected '{' before macro rules"))
        return false;
    size_t body_start = parser->previous.span.start;

    while(parser->current.kind != XS_TOKEN_RIGHT_BRACE && parser->current.kind != XS_TOKEN_EOF)
    {
        XsSpan matcher;
        XsSpan expansion;
        if(!skip_balanced(parser, XS_TOKEN_LEFT_PAREN, XS_TOKEN_RIGHT_PAREN, &matcher))
        {
            xs_diagnostics_add(parser->diagnostics, XS_DIAGNOSTIC_ERROR, parser->current.span,
                               "expected macro matcher in parentheses");
            return false;
        }
        if(!expect(parser, XS_TOKEN_ARROW, "expected '->' after macro matcher"))
            return false;
        if(!skip_balanced(parser, XS_TOKEN_LEFT_BRACE, XS_TOKEN_RIGHT_BRACE, &expansion))
        {
            xs_diagnostics_add(parser->diagnostics, XS_DIAGNOSTIC_ERROR, parser->current.span,
                               "expected macro expansion in braces");
            return false;
        }
        if(!expect(parser, XS_TOKEN_SEMICOLON, "expected ';' after macro rule"))
            return false;
    }
    if(!expect(parser, XS_TOKEN_RIGHT_BRACE, "expected '}' after macro definition"))
        return false;

    XsSpan body = {.start = body_start, .end = parser->previous.span.end};
    XsAstItem item = {.span = {start, body.end}, .name = name, .body = body};
    return xs_ast_push(ast, (XsAstNode){.kind = XS_AST_MACRO, .item = item});
}

static bool parse_item(XsParser *parser, XsAst *ast)
{
    size_t start = parser->current.span.start;
    XsVisibility visibility = parse_visibility(parser);
    bool incomplete = accept(parser, XS_TOKEN_KW_INCOMPLETE);
    bool is_async = accept(parser, XS_TOKEN_KW_ASYNC);

    if(accept(parser, XS_TOKEN_KW_NAMESPACE))
    {
        if(!expect(parser, XS_TOKEN_IDENTIFIER, "expected namespace name"))
            return false;
        XsSpan name = parser->previous.span;
        if(!expect(parser, XS_TOKEN_SEMICOLON, "expected ';' after namespace declaration"))
            return false;
        XsAstItem item = {.span = {start, parser->previous.span.end}, .name = name, .visibility = visibility};
        return xs_ast_push(ast, (XsAstNode){.kind = XS_AST_NAMESPACE, .item = item});
    }
    if(accept(parser, XS_TOKEN_KW_IMPORT) || accept(parser, XS_TOKEN_KW_USING))
    {
        XsSpan import_span;
        if(!skip_until_semicolon(parser, &import_span))
            return false;
        XsAstItem item = {.span = {start, import_span.end}, .body = import_span, .visibility = visibility};
        return xs_ast_push(ast, (XsAstNode){.kind = XS_AST_IMPORT, .item = item});
    }
    if(accept(parser, XS_TOKEN_KW_FN) || accept(parser, XS_TOKEN_KW_OP))
        return parse_function(parser, ast, visibility, is_async, incomplete, start);
    if(accept(parser, XS_TOKEN_KW_MACRO_RULES))
        return parse_macro(parser, ast, start);
    if(accept(parser, XS_TOKEN_KW_CLASS))
        return parse_named_block(parser, ast, XS_AST_CLASS, visibility, incomplete, false, start);
    if(accept(parser, XS_TOKEN_KW_INTERFACE))
        return parse_named_block(parser, ast, XS_AST_INTERFACE, visibility, incomplete, false, start);
    if(accept(parser, XS_TOKEN_KW_DATA))
        return parse_named_block(parser, ast, XS_AST_DATA, visibility, incomplete, false, start);
    if(accept(parser, XS_TOKEN_KW_ENUM))
    {
        bool data_enum = accept(parser, XS_TOKEN_KW_DATA);
        return parse_named_block(parser, ast, XS_AST_ENUM, visibility, incomplete, data_enum, start);
    }

    xs_diagnostics_add(parser->diagnostics, XS_DIAGNOSTIC_ERROR, parser->current.span,
                       "top-level execution is not allowed; expected a declaration");
    return false;
}

static void synchronize(XsParser *parser)
{
    while(parser->current.kind != XS_TOKEN_EOF)
    {
        switch(parser->current.kind)
        {
        case XS_TOKEN_KW_NAMESPACE:
        case XS_TOKEN_KW_IMPORT:
        case XS_TOKEN_KW_USING:
        case XS_TOKEN_KW_FN:
        case XS_TOKEN_KW_OP:
        case XS_TOKEN_KW_CLASS:
        case XS_TOKEN_KW_INTERFACE:
        case XS_TOKEN_KW_DATA:
        case XS_TOKEN_KW_ENUM:
        case XS_TOKEN_KW_MACRO_RULES:
            return;
        default:
            advance(parser);
        }
    }
}

void xs_parser_init(XsParser *parser, const XsSource *source, XsDiagnostics *diagnostics)
{
    *parser = (XsParser){.diagnostics = diagnostics};
    xs_lexer_init(&parser->lexer, source, diagnostics);
    parser->current = xs_lexer_next(&parser->lexer);
}

bool xs_parser_parse(XsParser *parser, XsAst *ast)
{
    while(parser->current.kind != XS_TOKEN_EOF)
    {
        size_t failed_at = parser->current.span.start;
        if(!parse_item(parser, ast))
        {
            synchronize(parser);
            if(parser->current.kind != XS_TOKEN_EOF && parser->current.span.start == failed_at)
                advance(parser);
        }
    }
    if(ast->allocation_failed)
        xs_diagnostics_add(parser->diagnostics, XS_DIAGNOSTIC_ERROR, parser->current.span,
                           "compiler ran out of memory while building the AST");
    return !xs_diagnostics_has_error(parser->diagnostics);
}
