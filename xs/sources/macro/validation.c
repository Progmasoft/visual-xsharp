/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 */

#include "internal.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

typedef struct
{
    const XsSyntaxNode **items;
    size_t count;
    size_t capacity;
} NodeList;

static bool node_list_add(NodeList *list, const XsSyntaxNode *node)
{
    if(list->count == list->capacity)
    {
        size_t capacity = list->capacity == 0 ? 8 : list->capacity * 2;
        const XsSyntaxNode **items = realloc(list->items, capacity * sizeof(*items));
        if(items == nullptr)
            return false;
        list->items = items;
        list->capacity = capacity;
    }
    list->items[list->count++] = node;
    return true;
}

static void collect_kind(const XsSyntaxNode *node, XsSyntaxKind kind, NodeList *nodes)
{
    if(node == nullptr)
        return;
    if(node->kind == kind)
        node_list_add(nodes, node);
    for(size_t i = 0; i < node->child_count; ++i)
        collect_kind(node->children[i], kind, nodes);
}

static XsText macro_name(const XsSyntaxNode *macro)
{
    for(size_t i = 0; i < macro->child_count; ++i)
    {
        if(macro->children[i]->kind == XS_SYNTAX_IDENTIFIER)
            return macro->children[i]->text;
    }
    return (XsText){0};
}

static bool attribute_name_is(const XsSyntaxNode *attribute, const char *name)
{
    if(attribute == nullptr || attribute->kind != XS_SYNTAX_ATTRIBUTE || attribute->child_count == 0)
        return false;
    const XsSyntaxNode *path = attribute->children[0];
    if(path->kind != XS_SYNTAX_PATH || path->child_count != 1)
        return false;
    const XsSyntaxNode *segment = path->children[0];
    size_t length = strlen(name);
    return segment->kind == XS_SYNTAX_IDENTIFIER && segment->text.length == length &&
           memcmp(segment->text.data, name, length) == 0;
}

static bool macro_has_attribute(const XsSyntaxNode *macro, const char *name)
{
    for(size_t index = 0; index < macro->child_count; ++index)
    {
        const XsSyntaxNode *list = macro->children[index];
        if(list->kind != XS_SYNTAX_ATTRIBUTE_LIST)
            continue;
        for(size_t attribute = 0; attribute < list->child_count; ++attribute)
        {
            if(attribute_name_is(list->children[attribute], name))
                return true;
        }
    }
    return false;
}

static void validate_export_placement(const XsSyntaxNode *node, bool module_scope, XsDiagnostics *diagnostics)
{
    if(node == nullptr)
        return;
    for(size_t index = 0; index < node->child_count; ++index)
    {
        const XsSyntaxNode *child = node->children[index];
        if(child->kind == XS_SYNTAX_DECL_MACRO && macro_has_attribute(child, "MacroExport") && !module_scope)
        {
            XsSpan span = {.start = child->span.start_offset, .end = child->span.end_offset};
            xs_diagnostics_add(diagnostics, XS_DIAGNOSTIC_ERROR, span,
                               "MacroExport is only valid on macros declared at module or namespace scope");
        }
        bool child_module_scope = child->kind == XS_SYNTAX_DECL_NAMESPACE;
        validate_export_placement(child, child_module_scope, diagnostics);
    }
}

static bool matcher_variable_depth(const XsSyntaxNode *node, XsText name, size_t depth, size_t *result)
{
    if(node == nullptr)
        return false;
    if(node->kind == XS_SYNTAX_MACRO_MATCHER_FRAGMENT && node->child_count != 0 &&
       xs_macro_text_equal(node->children[0]->text, name))
    {
        *result = depth;
        return true;
    }
    size_t child_depth = depth + (node->kind == XS_SYNTAX_MACRO_MATCHER_REPETITION ? 1 : 0);
    for(size_t i = 0; i < node->child_count; ++i)
    {
        if(matcher_variable_depth(node->children[i], name, child_depth, result))
            return true;
    }
    return false;
}

static void validate_expansion_variables(const XsSyntaxNode *node, const XsSyntaxNode *matcher,
                                         XsDiagnostics *diagnostics, size_t depth, bool *contains_variable)
{
    if(node == nullptr)
        return;
    if(node->kind == XS_SYNTAX_MACRO_EXPANSION_VARIABLE && node->child_count != 0)
    {
        *contains_variable = true;
        size_t matcher_depth = 0;
        XsSpan span = {.start = node->span.start_offset, .end = node->span.end_offset};
        if(!matcher_variable_depth(matcher, node->children[0]->text, 0, &matcher_depth))
        {
            xs_diagnostics_add(diagnostics, XS_DIAGNOSTIC_ERROR, span,
                               "macro expansion variable is not captured by this rule matcher");
        }
        else if(matcher_depth != depth)
        {
            xs_diagnostics_add(diagnostics, XS_DIAGNOSTIC_ERROR, span,
                               "macro variable must keep its matcher repetition nesting depth during expansion");
        }
    }
    if(node->kind == XS_SYNTAX_MACRO_EXPANSION_REPETITION)
    {
        bool repetition_has_variable = false;
        for(size_t i = 0; i < node->child_count; ++i)
            validate_expansion_variables(node->children[i], matcher, diagnostics, depth + 1, &repetition_has_variable);
        if(!repetition_has_variable)
        {
            XsSpan span = {.start = node->span.start_offset, .end = node->span.end_offset};
            xs_diagnostics_add(diagnostics, XS_DIAGNOSTIC_ERROR, span,
                               "macro expansion repetition must contain at least one captured variable");
        }
        *contains_variable = *contains_variable || repetition_has_variable;
        return;
    }
    for(size_t i = 0; i < node->child_count; ++i)
        validate_expansion_variables(node->children[i], matcher, diagnostics, depth, contains_variable);
}

static void validate_macro_rules(const XsSyntaxNode *macro, XsDiagnostics *diagnostics)
{
    for(size_t i = 0; i < macro->child_count; ++i)
    {
        const XsSyntaxNode *rule = macro->children[i];
        if(rule->kind != XS_SYNTAX_MACRO_RULE || rule->child_count < 2)
            continue;
        bool contains_variable = false;
        validate_expansion_variables(rule->children[1], rule->children[0], diagnostics, 0, &contains_variable);
    }
}

static bool expansion_calls_name(const XsSyntaxNode *node, XsText name)
{
    if(node == nullptr)
        return false;
    for(size_t i = 0; i + 1 < node->child_count; ++i)
    {
        const XsSyntaxNode *first = node->children[i];
        const XsSyntaxNode *second = node->children[i + 1];
        if(first->kind == XS_SYNTAX_MACRO_EXPANSION_TOKEN && first->token_kind == XS_TOKEN_IDENTIFIER &&
           xs_macro_text_equal(first->text, name) && second->kind == XS_SYNTAX_MACRO_EXPANSION_TOKEN &&
           second->token_kind == XS_TOKEN_BANG)
            return true;
    }
    for(size_t i = 0; i < node->child_count; ++i)
    {
        if(expansion_calls_name(node->children[i], name))
            return true;
    }
    return false;
}

static bool macro_reaches(const NodeList *macros, size_t from, size_t target, bool *visiting)
{
    if(visiting[from])
        return false;
    visiting[from] = true;
    for(size_t i = 0; i < macros->count; ++i)
    {
        if(!expansion_calls_name(macros->items[from], macro_name(macros->items[i])))
            continue;
        if(i == target || macro_reaches(macros, i, target, visiting))
        {
            visiting[from] = false;
            return true;
        }
    }
    visiting[from] = false;
    return false;
}

static void validate_recursion(const NodeList *macros, XsDiagnostics *diagnostics)
{
    if(macros->count == 0)
        return;
    bool *visiting = calloc(macros->count, sizeof(*visiting));
    if(visiting == nullptr)
    {
        xs_diagnostics_add(diagnostics, XS_DIAGNOSTIC_ERROR, (XsSpan){0, 0},
                           "compiler ran out of memory while validating macro call graph");
        return;
    }
    for(size_t i = 0; i < macros->count; ++i)
    {
        memset(visiting, 0, macros->count * sizeof(*visiting));
        if(macro_reaches(macros, i, i, visiting))
        {
            XsSpan span = {.start = macros->items[i]->span.start_offset, .end = macros->items[i]->span.end_offset};
            xs_diagnostics_add(diagnostics, XS_DIAGNOSTIC_ERROR, span,
                               "direct or indirect macro recursion is not allowed");
        }
    }
    free(visiting);
}

static bool is_macro_scope(XsSyntaxKind kind)
{
    return kind == XS_SYNTAX_FILE || kind == XS_SYNTAX_STMT_BLOCK || kind == XS_SYNTAX_DECL_CLASS ||
           kind == XS_SYNTAX_DECL_INTERFACE;
}

static XsText macro_call_name(const XsSyntaxNode *call)
{
    if(call == nullptr)
        return (XsText){0};
    if(call->kind == XS_SYNTAX_STMT_MACRO_CALL)
    {
        for(size_t i = 0; i < call->child_count; ++i)
        {
            XsText name = macro_call_name(call->children[i]);
            if(name.data != nullptr)
                return name;
        }
        return (XsText){0};
    }
    if(call->kind != XS_SYNTAX_EXPR_MACRO_CALL)
        return (XsText){0};
    for(size_t i = 0; i < call->child_count; ++i)
    {
        if(call->children[i]->kind == XS_SYNTAX_IDENTIFIER)
            return call->children[i]->text;
    }
    return (XsText){0};
}

static const XsSyntaxNode *resolve_macro(const NodeList *visible, XsText name)
{
    for(size_t i = visible->count; i > 0; --i)
    {
        if(xs_macro_text_equal(macro_name(visible->items[i - 1]), name))
            return visible->items[i - 1];
    }
    return nullptr;
}

static bool text_is_cstr(XsText text, const char *value)
{
    size_t length = strlen(value);
    return text.length == length && memcmp(text.data, value, length) == 0;
}

static bool is_stdio_macro(XsText name)
{
    return text_is_cstr(name, "print") || text_is_cstr(name, "println") || text_is_cstr(name, "eprint") ||
           text_is_cstr(name, "eprintln") || text_is_cstr(name, "format");
}

static bool is_builtin_macro(XsText name)
{
    return text_is_cstr(name, "include") || text_is_cstr(name, "format_args") || text_is_cstr(name, "format_args_nl") ||
           text_is_cstr(name, "write") || text_is_cstr(name, "writeln");
}

static bool is_format_args_macro(XsText name)
{
    return text_is_cstr(name, "format_args") || text_is_cstr(name, "format_args_nl");
}

static bool stdio_macro_allows_empty(XsText name)
{
    return text_is_cstr(name, "println") || text_is_cstr(name, "eprintln");
}

static bool is_writer_macro(XsText name)
{
    return text_is_cstr(name, "write") || text_is_cstr(name, "writeln");
}

static bool writer_macro_allows_destination_only(XsText name)
{
    return text_is_cstr(name, "writeln");
}

static bool token_changes_depth(XsTokenKind kind, int *depth)
{
    if(kind == XS_TOKEN_LEFT_PAREN || kind == XS_TOKEN_LEFT_BRACE || kind == XS_TOKEN_LEFT_BRACKET)
    {
        ++*depth;
        return true;
    }
    if(kind == XS_TOKEN_RIGHT_PAREN || kind == XS_TOKEN_RIGHT_BRACE || kind == XS_TOKEN_RIGHT_BRACKET)
    {
        if(*depth > 0)
            --*depth;
        return true;
    }
    return false;
}

static size_t count_stdio_format_arguments_after(const XsSyntaxNode *call, size_t comma_index)
{
    if(call->child_count <= comma_index || call->children[comma_index]->token_kind != XS_TOKEN_COMMA)
        return 0;
    size_t count = 0;
    bool saw_argument = false;
    int depth = 0;
    for(size_t index = comma_index + 1; index < call->child_count; ++index)
    {
        XsTokenKind kind = call->children[index]->token_kind;
        if(token_changes_depth(kind, &depth))
        {
            if(!saw_argument)
            {
                saw_argument = true;
                ++count;
            }
            continue;
        }
        if(depth == 0 && kind == XS_TOKEN_COMMA)
        {
            saw_argument = false;
            continue;
        }
        if(!saw_argument)
        {
            saw_argument = true;
            ++count;
        }
    }
    return count;
}

static size_t count_stdio_format_arguments(const XsSyntaxNode *call)
{
    return count_stdio_format_arguments_after(call, 2);
}

static bool find_top_level_comma(const XsSyntaxNode *call, size_t start, size_t *comma)
{
    int depth = 0;
    for(size_t index = start; index < call->child_count; ++index)
    {
        XsTokenKind kind = call->children[index]->token_kind;
        if(token_changes_depth(kind, &depth))
            continue;
        if(depth == 0 && kind == XS_TOKEN_COMMA)
        {
            *comma = index;
            return true;
        }
    }
    return false;
}

static bool is_format_identifier_start(char character)
{
    unsigned char value = (unsigned char)character;
    return character == '_' || isalpha(value) != 0;
}

static bool is_format_identifier_continue(char character)
{
    unsigned char value = (unsigned char)character;
    return character == '_' || isalnum(value) != 0;
}

static bool is_format_type(char character)
{
    return character == 'x' || character == 'X' || character == 'o' || character == 'b' || character == 'p' ||
           character == 'e' || character == 'E';
}

static bool parse_format_count(XsText literal, size_t end, size_t *index, bool precision)
{
    if(*index >= end)
        return false;
    if(precision && literal.data[*index] == '*')
    {
        ++*index;
        return true;
    }
    if(isdigit((unsigned char)literal.data[*index]) != 0)
    {
        while(*index < end && isdigit((unsigned char)literal.data[*index]) != 0)
            ++*index;
        if(*index < end && literal.data[*index] == '$')
            ++*index;
        return true;
    }
    if(is_format_identifier_start(literal.data[*index]))
    {
        while(*index < end && is_format_identifier_continue(literal.data[*index]))
            ++*index;
        if(*index < end && literal.data[*index] == '$')
        {
            ++*index;
            return true;
        }
        return false;
    }
    return false;
}

static bool format_count_starts(XsText literal, size_t index, size_t end)
{
    if(index >= end)
        return false;
    if(isdigit((unsigned char)literal.data[index]) != 0)
        return true;
    if(!is_format_identifier_start(literal.data[index]))
        return false;
    while(index < end && is_format_identifier_continue(literal.data[index]))
        ++index;
    return index < end && literal.data[index] == '$';
}

static bool validate_stdio_format_spec(XsText literal, size_t start, size_t end)
{
    size_t index = start;
    if(index >= end)
        return true;
    if(index + 1 < end &&
       (literal.data[index + 1] == '<' || literal.data[index + 1] == '^' || literal.data[index + 1] == '>'))
        index += 2;
    else if(literal.data[index] == '<' || literal.data[index] == '^' || literal.data[index] == '>')
        ++index;
    if(index < end && (literal.data[index] == '+' || literal.data[index] == '-'))
        ++index;
    if(index < end && literal.data[index] == '#')
        ++index;
    if(index < end && literal.data[index] == '0')
        ++index;
    if(format_count_starts(literal, index, end))
    {
        if(!parse_format_count(literal, end, &index, false))
            return false;
    }
    if(index < end && literal.data[index] == '.')
    {
        ++index;
        if(!parse_format_count(literal, end, &index, true))
            return false;
    }
    if(index >= end)
        return true;
    if(literal.data[index] == '?')
        return index + 1 == end;
    if((literal.data[index] == 'x' || literal.data[index] == 'X') && index + 1 < end && literal.data[index + 1] == '?')
        return index + 2 == end;
    if(is_format_type(literal.data[index]))
        return index + 1 == end;
    return false;
}

static bool validate_stdio_placeholder(XsText literal, size_t start, size_t end)
{
    size_t index = start;
    if(index < end && isdigit((unsigned char)literal.data[index]) != 0)
    {
        while(index < end && isdigit((unsigned char)literal.data[index]) != 0)
            ++index;
    }
    else if(index < end && is_format_identifier_start(literal.data[index]))
    {
        while(index < end && is_format_identifier_continue(literal.data[index]))
            ++index;
    }
    if(index == end)
        return true;
    if(literal.data[index] != ':')
        return false;
    return validate_stdio_format_spec(literal, index + 1, end);
}

static bool count_stdio_placeholders(XsText literal, size_t *count)
{
    *count = 0;
    if(literal.length < 2)
        return false;
    for(size_t index = 1; index + 1 < literal.length; ++index)
    {
        char current = literal.data[index];
        char next = index + 1 < literal.length ? literal.data[index + 1] : '\0';
        if(current == '{')
        {
            if(next == '{')
            {
                ++index;
                continue;
            }
            ++*count;
            size_t placeholder_start = index + 1;
            while(index + 1 < literal.length && literal.data[index] != '}')
                ++index;
            if(index + 1 >= literal.length)
                return false;
            if(!validate_stdio_placeholder(literal, placeholder_start, index))
                return false;
            continue;
        }
        if(current == '}')
        {
            if(next == '}')
            {
                ++index;
                continue;
            }
            return false;
        }
    }
    return true;
}

static void validate_formatting_macro_call(const XsSyntaxNode *call, XsText name, XsDiagnostics *diagnostics)
{
    if(!is_stdio_macro(name) && !is_format_args_macro(name) && !is_writer_macro(name))
        return;
    XsSpan span = {.start = call->span.start_offset, .end = call->span.end_offset};
    if(is_writer_macro(name))
    {
        if(call->child_count == 1)
        {
            xs_diagnostics_add(diagnostics, XS_DIAGNOSTIC_ERROR, span, "writer macro requires a destination argument");
            return;
        }
        size_t destination_comma = 0;
        if(!find_top_level_comma(call, 1, &destination_comma))
        {
            if(!writer_macro_allows_destination_only(name))
                xs_diagnostics_add(diagnostics, XS_DIAGNOSTIC_ERROR, span,
                                   "writer macro requires a format string argument");
            return;
        }
        size_t format_index = destination_comma + 1;
        if(call->child_count <= format_index || call->children[format_index]->token_kind != XS_TOKEN_STRING)
        {
            xs_diagnostics_add(diagnostics, XS_DIAGNOSTIC_ERROR, span,
                               "writer macro format argument must be a string literal");
            return;
        }
        size_t format_comma = format_index + 1;
        if(call->child_count > format_comma && call->children[format_comma]->token_kind != XS_TOKEN_COMMA)
        {
            xs_diagnostics_add(diagnostics, XS_DIAGNOSTIC_ERROR, span,
                               "writer macro format string must be followed by ',' before arguments");
            return;
        }
        size_t placeholders = 0;
        if(!count_stdio_placeholders(call->children[format_index]->text, &placeholders))
        {
            xs_diagnostics_add(diagnostics, XS_DIAGNOSTIC_ERROR, span, "writer macro format string is malformed");
            return;
        }
        size_t arguments = count_stdio_format_arguments_after(call, format_comma);
        if(placeholders != arguments)
            xs_diagnostics_add(diagnostics, XS_DIAGNOSTIC_ERROR, span,
                               "writer macro placeholder count must match the number of format arguments");
        return;
    }
    if(is_format_args_macro(name) && call->child_count == 1)
    {
        xs_diagnostics_add(diagnostics, XS_DIAGNOSTIC_ERROR, span,
                           "format argument macro requires a format string argument");
        return;
    }
    if(call->child_count == 1)
    {
        if(!stdio_macro_allows_empty(name))
            xs_diagnostics_add(diagnostics, XS_DIAGNOSTIC_ERROR, span, "stdio macro requires a format string argument");
        return;
    }
    const XsSyntaxNode *format = call->children[1];
    if(format->token_kind != XS_TOKEN_STRING)
    {
        xs_diagnostics_add(diagnostics, XS_DIAGNOSTIC_ERROR, span,
                           "stdio macro first argument must be a string literal");
        return;
    }
    if(call->child_count > 2 && call->children[2]->token_kind != XS_TOKEN_COMMA)
    {
        xs_diagnostics_add(diagnostics, XS_DIAGNOSTIC_ERROR, span,
                           "stdio macro format string must be followed by ',' before arguments");
        return;
    }
    size_t placeholders = 0;
    if(!count_stdio_placeholders(format->text, &placeholders))
    {
        xs_diagnostics_add(diagnostics, XS_DIAGNOSTIC_ERROR, span, "stdio macro format string is malformed");
        return;
    }
    size_t arguments = count_stdio_format_arguments(call);
    if(placeholders != arguments)
        xs_diagnostics_add(diagnostics, XS_DIAGNOSTIC_ERROR, span,
                           "stdio macro placeholder count must match the number of format arguments");
}

typedef enum
{
    MATCH_NO,
    MATCH_YES,
    MATCH_DEFERRED,
    MATCH_AMBIGUOUS,
} MatchStatus;

static MatchStatus match_rule_arguments(const XsSyntaxNode *rule, const XsSyntaxNode *call)
{
    if(rule == nullptr || rule->kind != XS_SYNTAX_MACRO_RULE || rule->child_count == 0)
        return MATCH_NO;
    const XsSyntaxNode *matcher = rule->children[0];
    size_t argument_index = 1;
    for(size_t i = 0; i < matcher->child_count; ++i)
    {
        const XsSyntaxNode *element = matcher->children[i];
        if(element->kind == XS_SYNTAX_MACRO_MATCHER_REPETITION)
            return MATCH_DEFERRED;
        if(argument_index >= call->child_count)
            return MATCH_NO;
        if(element->kind == XS_SYNTAX_MACRO_MATCHER_TOKEN)
        {
            const XsSyntaxNode *argument = call->children[argument_index++];
            if(!xs_macro_token_text_matches(element, argument))
                return MATCH_NO;
            continue;
        }
        if(element->kind == XS_SYNTAX_MACRO_MATCHER_FRAGMENT)
        {
            if(element->child_count < 2)
                return MATCH_NO;
            if(xs_macro_fragment_is_sequence(element))
                return i + 1 == matcher->child_count && argument_index < call->child_count ? MATCH_YES : MATCH_NO;
            if(!xs_macro_fragment_supported(element->children[1]->text))
                return MATCH_DEFERRED;
            const XsSyntaxNode *argument = call->children[argument_index++];
            if(!xs_macro_fragment_matches(element, argument))
                return MATCH_NO;
            continue;
        }
        return MATCH_NO;
    }
    return argument_index == call->child_count ? MATCH_YES : MATCH_NO;
}

static MatchStatus macro_matches_call(const XsSyntaxNode *macro, const XsSyntaxNode *call)
{
    MatchStatus result = MATCH_NO;
    size_t matches = 0;
    for(size_t i = 0; i < macro->child_count; ++i)
    {
        MatchStatus status = match_rule_arguments(macro->children[i], call);
        if(status == MATCH_YES)
        {
            ++matches;
            result = MATCH_YES;
            continue;
        }
        if(status == MATCH_DEFERRED)
            result = MATCH_DEFERRED;
    }
    return matches > 1 ? MATCH_AMBIGUOUS : result;
}

static void validate_scope_calls(const XsSyntaxTree *tree, const XsSyntaxNode *scope, const NodeList *inherited,
                                 XsDiagnostics *diagnostics);

static void validate_node_calls(const XsSyntaxTree *tree, const XsSyntaxNode *node, const NodeList *visible,
                                XsDiagnostics *diagnostics)
{
    if(node == nullptr)
        return;
    if(is_macro_scope(node->kind))
    {
        validate_scope_calls(tree, node, visible, diagnostics);
        return;
    }
    if(node->kind == XS_SYNTAX_EXPR_MACRO_CALL)
    {
        XsText name = macro_call_name(node);
        XsSpan span = {.start = node->span.start_offset, .end = node->span.end_offset};
        if(is_format_args_macro(name))
        {
            validate_formatting_macro_call(node, name, diagnostics);
            return;
        }
        const XsSyntaxNode *macro = resolve_macro(visible, name);
        if(macro == nullptr && !is_builtin_macro(name) && !xs_macro_external_import_resolves(tree, name))
        {
            xs_diagnostics_add(diagnostics, XS_DIAGNOSTIC_ERROR, span, "macro call does not resolve in this scope");
            return;
        }
        if(macro == nullptr)
        {
            validate_formatting_macro_call(node, name, diagnostics);
            return;
        }
        MatchStatus status = macro_matches_call(macro, node);
        if(status == MATCH_NO)
            xs_diagnostics_add(diagnostics, XS_DIAGNOSTIC_ERROR, span, "macro call does not match any rule");
        else if(status == MATCH_AMBIGUOUS)
            xs_diagnostics_add(diagnostics, XS_DIAGNOSTIC_ERROR, span, "macro call matches more than one rule");
        return;
    }
    for(size_t i = 0; i < node->child_count; ++i)
        validate_node_calls(tree, node->children[i], visible, diagnostics);
}

static void validate_scope_calls(const XsSyntaxTree *tree, const XsSyntaxNode *scope, const NodeList *inherited,
                                 XsDiagnostics *diagnostics)
{
    NodeList visible = {0};
    for(size_t i = 0; i < inherited->count; ++i)
        node_list_add(&visible, inherited->items[i]);
    for(size_t i = 0; i < scope->child_count; ++i)
    {
        if(scope->children[i]->kind == XS_SYNTAX_DECL_MACRO)
            node_list_add(&visible, scope->children[i]);
    }
    for(size_t i = 0; i < scope->child_count; ++i)
    {
        if(scope->children[i]->kind != XS_SYNTAX_DECL_MACRO)
            validate_node_calls(tree, scope->children[i], &visible, diagnostics);
    }
    free(visible.items);
}

static void validate_scope_recursion(const XsSyntaxNode *scope, XsDiagnostics *diagnostics)
{
    NodeList macros = {0};
    for(size_t i = 0; i < scope->child_count; ++i)
    {
        if(scope->children[i]->kind == XS_SYNTAX_DECL_MACRO)
            node_list_add(&macros, scope->children[i]);
    }
    validate_recursion(&macros, diagnostics);
    free(macros.items);
    for(size_t i = 0; i < scope->child_count; ++i)
    {
        if(is_macro_scope(scope->children[i]->kind))
            validate_scope_recursion(scope->children[i], diagnostics);
        else
        {
            for(size_t j = 0; j < scope->children[i]->child_count; ++j)
            {
                if(is_macro_scope(scope->children[i]->children[j]->kind))
                    validate_scope_recursion(scope->children[i]->children[j], diagnostics);
            }
        }
    }
}

bool xs_macro_validate(const XsSyntaxTree *tree, XsDiagnostics *diagnostics)
{
    if(tree == nullptr || tree->root == nullptr || diagnostics == nullptr)
        return false;
    NodeList macros = {0};
    collect_kind(tree->root, XS_SYNTAX_DECL_MACRO, &macros);
    for(size_t i = 0; i < macros.count; ++i)
    {
        XsText name = macro_name(macros.items[i]);
        if(is_format_args_macro(name))
        {
            XsSpan span = {.start = macros.items[i]->span.start_offset, .end = macros.items[i]->span.end_offset};
            xs_diagnostics_add(diagnostics, XS_DIAGNOSTIC_ERROR, span,
                               "compiler-special formatting macro cannot be declared with macro_rules");
            continue;
        }
        validate_macro_rules(macros.items[i], diagnostics);
    }
    validate_export_placement(tree->root, true, diagnostics);
    validate_scope_recursion(tree->root, diagnostics);
    NodeList empty = {0};
    validate_scope_calls(tree, tree->root, &empty, diagnostics);
    free(macros.items);
    return !xs_diagnostics_has_error(diagnostics);
}
