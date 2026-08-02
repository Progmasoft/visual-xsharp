/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 */

#include "parser_internal.h"

#include <string.h>

static bool macro_fragment_kind_valid(SyntaxParser *parser, XsToken token)
{
    static const char *const kinds[] = {"expr", "ident",   "ty",   "path", "pat",      "stmt", "block",
                                        "item", "literal", "meta", "tt",   "lifetime", "vis"};
    for(size_t i = 0; i < sizeof(kinds) / sizeof(kinds[0]); ++i)
    {
        if(token_text_is(parser, token, kinds[i]))
            return true;
    }
    return false;
}

static bool macro_has_variable(const XsSyntaxNode *value, const XsSource *source, XsSpan text)
{
    if(value == nullptr)
        return false;
    if(value->kind == XS_SYNTAX_MACRO_MATCHER_FRAGMENT && value->child_count != 0 &&
       value->children[0]->text.length == text.end - text.start &&
       memcmp(value->children[0]->text.data, source->text + text.start, value->children[0]->text.length) == 0)
        return true;
    for(size_t i = 0; i < value->child_count; ++i)
    {
        if(macro_has_variable(value->children[i], source, text))
            return true;
    }
    return false;
}

static void parse_matcher_elements(SyntaxParser *parser, XsSyntaxNode *macro, XsSyntaxNode *parent, XsTokenKind closing)
{
    while(parser->current.kind != closing && parser->current.kind != XS_TOKEN_EOF)
    {
        size_t start = parser->current.span.start;
        if(accept(parser, XS_TOKEN_DOLLAR))
        {
            if(accept(parser, XS_TOKEN_LEFT_PAREN))
            {
                XsSyntaxNode *repetition =
                    node(parser, XS_SYNTAX_MACRO_MATCHER_REPETITION, (XsSpan){start, parser->previous.span.end});
                parse_matcher_elements(parser, macro, repetition, XS_TOKEN_RIGHT_PAREN);
                expect(parser, XS_TOKEN_RIGHT_PAREN, "expected ')' after macro matcher repetition");
                if(accept(parser, XS_TOKEN_COMMA))
                    repetition->flags |= XS_SYNTAX_FLAG_REPETITION_COMMA;
                if(accept(parser, XS_TOKEN_PLUS))
                    repetition->flags |= XS_SYNTAX_FLAG_REPETITION_ONE_OR_MORE;
                else
                    expect(parser, XS_TOKEN_STAR, "macro repetition must end with '*' or '+'");
                finish_node(parser, repetition, parser->previous.span.end);
                xs_syntax_node_add(parser->tree, parent, repetition);
                continue;
            }
            XsSyntaxNode *fragment =
                node(parser, XS_SYNTAX_MACRO_MATCHER_FRAGMENT, (XsSpan){start, parser->current.span.end});
            XsToken variable = parser->current;
            if(macro_has_variable(macro, parser->tree->source, variable.span))
                xs_diagnostics_add(parser->diagnostics, XS_DIAGNOSTIC_ERROR, variable.span,
                                   "macro variable name may be declared only once in a macro definition");
            xs_syntax_node_add(parser->tree, fragment, identifier(parser));
            expect(parser, XS_TOKEN_COLON, "expected ':' after macro variable name");
            XsToken kind = parser->current;
            if(kind.kind != XS_TOKEN_IDENTIFIER || !macro_fragment_kind_valid(parser, kind))
                xs_diagnostics_add(parser->diagnostics, XS_DIAGNOSTIC_ERROR, kind.span,
                                   "unsupported macro fragment specifier");
            xs_syntax_node_add(parser->tree, fragment, identifier(parser));
            finish_node(parser, fragment, parser->previous.span.end);
            xs_syntax_node_add(parser->tree, parent, fragment);
            continue;
        }
        XsToken token = parser->current;
        advance(parser);
        XsSyntaxNode *token_node = node(parser, XS_SYNTAX_MACRO_MATCHER_TOKEN, token.span);
        token_node->token_kind = token.kind;
        xs_syntax_node_add(parser->tree, parent, token_node);
    }
}

static void add_expansion_token(SyntaxParser *parser, XsSyntaxNode *parent, XsToken token)
{
    XsSyntaxNode *token_node = node(parser, XS_SYNTAX_MACRO_EXPANSION_TOKEN, token.span);
    token_node->token_kind = token.kind;
    xs_syntax_node_add(parser->tree, parent, token_node);
}

static XsTokenKind matching_closing_delimiter(XsTokenKind opening)
{
    switch(opening)
    {
    case XS_TOKEN_LEFT_BRACE:
        return XS_TOKEN_RIGHT_BRACE;
    case XS_TOKEN_LEFT_PAREN:
        return XS_TOKEN_RIGHT_PAREN;
    case XS_TOKEN_LEFT_BRACKET:
        return XS_TOKEN_RIGHT_BRACKET;
    default:
        return XS_TOKEN_EOF;
    }
}

static void parse_expansion_elements(SyntaxParser *parser, XsSyntaxNode *parent, XsTokenKind closing)
{
    while(parser->current.kind != closing && parser->current.kind != XS_TOKEN_EOF)
    {
        size_t start = parser->current.span.start;
        if(accept(parser, XS_TOKEN_DOLLAR))
        {
            if(accept(parser, XS_TOKEN_LEFT_PAREN))
            {
                XsSyntaxNode *repetition =
                    node(parser, XS_SYNTAX_MACRO_EXPANSION_REPETITION, (XsSpan){start, parser->previous.span.end});
                parse_expansion_elements(parser, repetition, XS_TOKEN_RIGHT_PAREN);
                expect(parser, XS_TOKEN_RIGHT_PAREN, "expected ')' after macro expansion repetition");
                if(accept(parser, XS_TOKEN_PLUS))
                    repetition->flags |= XS_SYNTAX_FLAG_REPETITION_ONE_OR_MORE;
                else
                    expect(parser, XS_TOKEN_STAR, "macro expansion repetition must end with '*' or '+'");
                finish_node(parser, repetition, parser->previous.span.end);
                xs_syntax_node_add(parser->tree, parent, repetition);
            }
            else
            {
                XsSyntaxNode *variable =
                    node(parser, XS_SYNTAX_MACRO_EXPANSION_VARIABLE, (XsSpan){start, parser->current.span.end});
                xs_syntax_node_add(parser->tree, variable, identifier(parser));
                finish_node(parser, variable, parser->previous.span.end);
                xs_syntax_node_add(parser->tree, parent, variable);
            }
            continue;
        }
        XsToken token = parser->current;
        XsTokenKind nested_closing = matching_closing_delimiter(token.kind);
        advance(parser);
        add_expansion_token(parser, parent, token);
        if(nested_closing != XS_TOKEN_EOF)
        {
            parse_expansion_elements(parser, parent, nested_closing);
            token = parser->current;
            if(expect(parser, nested_closing, "expected closing delimiter in macro expansion"))
                add_expansion_token(parser, parent, token);
        }
    }
}

XsSyntaxNode *parse_macro(SyntaxParser *parser, size_t start)
{
    XsSyntaxNode *macro = node(parser, XS_SYNTAX_DECL_MACRO, (XsSpan){start, parser->previous.span.end});
    expect(parser, XS_TOKEN_BANG, "expected '!' after macro_rules");
    xs_syntax_node_add(parser->tree, macro, identifier(parser));
    expect(parser, XS_TOKEN_LEFT_BRACE, "expected '{' before macro rules");
    while(parser->current.kind != XS_TOKEN_RIGHT_BRACE && parser->current.kind != XS_TOKEN_EOF)
    {
        size_t start_rule = parser->current.span.start;
        XsSyntaxNode *rule = node(parser, XS_SYNTAX_MACRO_RULE, (XsSpan){start_rule, start_rule});
        XsSyntaxNode *matcher = node(parser, XS_SYNTAX_MACRO_MATCHER, (XsSpan){start_rule, start_rule});
        xs_syntax_node_add(parser->tree, rule, matcher);
        xs_syntax_node_add(parser->tree, macro, rule);
        expect(parser, XS_TOKEN_LEFT_PAREN, "expected macro matcher");
        parse_matcher_elements(parser, macro, matcher, XS_TOKEN_RIGHT_PAREN);
        expect(parser, XS_TOKEN_RIGHT_PAREN, "expected ')' after macro matcher");
        finish_node(parser, matcher, parser->previous.span.end);
        expect(parser, XS_TOKEN_ARROW, "expected '->' after macro matcher");
        XsSyntaxNode *expansion =
            node(parser, XS_SYNTAX_MACRO_EXPANSION, (XsSpan){parser->current.span.start, parser->current.span.start});
        expect(parser, XS_TOKEN_LEFT_BRACE, "expected macro expansion");
        parse_expansion_elements(parser, expansion, XS_TOKEN_RIGHT_BRACE);
        expect(parser, XS_TOKEN_RIGHT_BRACE, "expected '}' after macro expansion");
        finish_node(parser, expansion, parser->previous.span.end);
        xs_syntax_node_add(parser->tree, rule, expansion);
        expect(parser, XS_TOKEN_SEMICOLON, "expected ';' after macro rule");
        finish_node(parser, rule, parser->previous.span.end);
    }
    expect(parser, XS_TOKEN_RIGHT_BRACE, "expected '}' after macro definition");
    finish_node(parser, macro, parser->previous.span.end);
    return macro;
}
