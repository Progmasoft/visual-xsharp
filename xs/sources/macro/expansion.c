/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 */

#include "internal.h"

#include <stdlib.h>

typedef struct
{
    const XsSyntaxNode **items;
    size_t count;
    size_t capacity;
} MacroList;

typedef struct
{
    XsText name;
    const XsSyntaxNode *argument;
    const XsSyntaxNode *call;
    size_t argument_index;
    size_t argument_count;
} Capture;

typedef struct
{
    Capture items[32];
    size_t count;
} CaptureSet;

static bool list_add(MacroList *list, const XsSyntaxNode *node)
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

static XsText macro_name(const XsSyntaxNode *macro)
{
    for(size_t i = 0; i < macro->child_count; ++i)
    {
        if(macro->children[i]->kind == XS_SYNTAX_IDENTIFIER)
            return macro->children[i]->text;
    }
    return (XsText){0};
}

static XsText call_name(const XsSyntaxNode *call)
{
    if(call->kind == XS_SYNTAX_STMT_MACRO_CALL)
    {
        for(size_t i = 0; i < call->child_count; ++i)
        {
            XsText name = call_name(call->children[i]);
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

static bool rule_supported(const XsSyntaxNode *rule)
{
    if(rule->child_count < 2)
        return false;
    const XsSyntaxNode *matcher = rule->children[0];
    for(size_t i = 0; i < matcher->child_count; ++i)
    {
        const XsSyntaxNode *element = matcher->children[i];
        if(element->kind == XS_SYNTAX_MACRO_MATCHER_REPETITION)
            return false;
        if(element->kind == XS_SYNTAX_MACRO_MATCHER_FRAGMENT)
        {
            if(element->child_count < 2 || !xs_macro_fragment_supported(element->children[1]->text))
                return false;
        }
    }
    const XsSyntaxNode *expansion = rule->children[1];
    for(size_t i = 0; i < expansion->child_count; ++i)
    {
        if(expansion->children[i]->kind == XS_SYNTAX_MACRO_EXPANSION_REPETITION)
            return false;
    }
    return true;
}

static const XsSyntaxNode *find_capture(const CaptureSet *captures, XsText name)
{
    for(size_t i = 0; i < captures->count; ++i)
    {
        if(xs_macro_text_equal(captures->items[i].name, name))
            return captures->items[i].argument;
    }
    return nullptr;
}

static bool add_capture(CaptureSet *captures, const XsSyntaxNode *fragment, const XsSyntaxNode *argument)
{
    if(fragment->child_count == 0 || captures->count == sizeof(captures->items) / sizeof(captures->items[0]))
        return false;
    captures->items[captures->count++] =
        (Capture){.name = fragment->children[0]->text, .argument = argument, .argument_count = 1};
    return true;
}

static bool add_capture_sequence(CaptureSet *captures, const XsSyntaxNode *fragment, const XsSyntaxNode *call,
                                 size_t argument_index, size_t argument_count)
{
    if(fragment->child_count == 0 || argument_count == 0 ||
       captures->count == sizeof(captures->items) / sizeof(captures->items[0]))
        return false;
    captures->items[captures->count++] = (Capture){.name = fragment->children[0]->text,
                                                   .argument = call->children[argument_index],
                                                   .call = call,
                                                   .argument_index = argument_index,
                                                   .argument_count = argument_count};
    return true;
}

static bool rule_matches(const XsSyntaxNode *rule, const XsSyntaxNode *call, CaptureSet *captures)
{
    const XsSyntaxNode *matcher = rule->children[0];
    size_t argument = 1;
    for(size_t i = 0; i < matcher->child_count; ++i)
    {
        if(argument >= call->child_count)
            return false;
        const XsSyntaxNode *element = matcher->children[i];
        if(element->kind == XS_SYNTAX_MACRO_MATCHER_TOKEN)
        {
            const XsSyntaxNode *value = call->children[argument++];
            if(!xs_macro_token_text_matches(element, value))
                return false;
        }
        if(element->kind == XS_SYNTAX_MACRO_MATCHER_FRAGMENT)
        {
            if(xs_macro_fragment_is_sequence(element))
            {
                if(i + 1 != matcher->child_count)
                    return false;
                return add_capture_sequence(captures, element, call, argument, call->child_count - argument);
            }
            const XsSyntaxNode *value = call->children[argument++];
            if(!xs_macro_fragment_matches(element, value) || !add_capture(captures, element, value))
                return false;
        }
    }
    return argument == call->child_count;
}

static bool plan_expansion(const XsSyntaxNode *rule, const CaptureSet *captures, XsMacroExpansionReport *report)
{
    const XsSyntaxNode *expansion = rule->children[1];
    for(size_t i = 0; i < expansion->child_count; ++i)
    {
        const XsSyntaxNode *element = expansion->children[i];
        if(element->kind == XS_SYNTAX_MACRO_EXPANSION_TOKEN)
        {
            ++report->output_tokens_planned;
            continue;
        }
        if(element->kind == XS_SYNTAX_MACRO_EXPANSION_VARIABLE && element->child_count != 0)
        {
            const XsSyntaxNode *argument = find_capture(captures, element->children[0]->text);
            if(argument == nullptr)
                return false;
            for(size_t capture = 0; capture < captures->count; ++capture)
            {
                if(xs_macro_text_equal(captures->items[capture].name, element->children[0]->text))
                {
                    report->output_tokens_planned += captures->items[capture].argument_count;
                    break;
                }
            }
            ++report->substitutions_planned;
        }
    }
    return true;
}

static const XsSyntaxNode *resolve_macro(const MacroList *visible, XsText name)
{
    for(size_t i = visible->count; i > 0; --i)
    {
        if(xs_macro_text_equal(macro_name(visible->items[i - 1]), name))
            return visible->items[i - 1];
    }
    return nullptr;
}

static bool prepare_node(const XsSyntaxNode *node, const MacroList *visible, XsDiagnostics *diagnostics,
                         XsMacroExpansionReport *report);

static bool prepare_scope(const XsSyntaxNode *scope, const MacroList *inherited, XsDiagnostics *diagnostics,
                          XsMacroExpansionReport *report)
{
    MacroList visible = {0};
    for(size_t i = 0; i < inherited->count; ++i)
    {
        if(!list_add(&visible, inherited->items[i]))
            return false;
    }
    for(size_t i = 0; i < scope->child_count; ++i)
    {
        if(scope->children[i]->kind == XS_SYNTAX_DECL_MACRO && !list_add(&visible, scope->children[i]))
            return false;
    }
    for(size_t i = 0; i < scope->child_count; ++i)
    {
        if(scope->children[i]->kind != XS_SYNTAX_DECL_MACRO &&
           !prepare_node(scope->children[i], &visible, diagnostics, report))
        {
            free(visible.items);
            return false;
        }
    }
    free(visible.items);
    return true;
}

static bool is_scope(XsSyntaxKind kind)
{
    return kind == XS_SYNTAX_FILE || kind == XS_SYNTAX_STMT_BLOCK || kind == XS_SYNTAX_DECL_CLASS ||
           kind == XS_SYNTAX_DECL_INTERFACE;
}

static bool prepare_call(const XsSyntaxNode *call, const MacroList *visible, XsDiagnostics *diagnostics,
                         XsMacroExpansionReport *report)
{
    (void)diagnostics;
    ++report->calls_seen;
    const XsSyntaxNode *macro = resolve_macro(visible, call_name(call));
    if(macro == nullptr)
        return true;
    ++report->calls_resolved;
    bool has_deferred_rule = false;
    size_t matches = 0;
    for(size_t i = 0; i < macro->child_count; ++i)
    {
        const XsSyntaxNode *rule = macro->children[i];
        if(rule->kind != XS_SYNTAX_MACRO_RULE || rule->child_count == 0)
            continue;
        if(!rule_supported(rule))
        {
            has_deferred_rule = true;
            continue;
        }
        CaptureSet captures = {0};
        if(rule_matches(rule, call, &captures) && plan_expansion(rule, &captures, report))
        {
            ++matches;
            continue;
        }
    }
    if(matches == 1)
    {
        ++report->calls_expandable;
        return true;
    }
    if(has_deferred_rule || matches > 1)
        ++report->calls_deferred;
    return true;
}

static bool prepare_node(const XsSyntaxNode *node, const MacroList *visible, XsDiagnostics *diagnostics,
                         XsMacroExpansionReport *report)
{
    if(node == nullptr)
        return true;
    if(is_scope(node->kind))
        return prepare_scope(node, visible, diagnostics, report);
    if(node->kind == XS_SYNTAX_EXPR_MACRO_CALL)
        return prepare_call(node, visible, diagnostics, report);
    for(size_t i = 0; i < node->child_count; ++i)
    {
        if(!prepare_node(node->children[i], visible, diagnostics, report))
            return false;
    }
    return true;
}

bool xs_macro_prepare_expansion(const XsSyntaxTree *tree, XsDiagnostics *diagnostics, XsMacroExpansionReport *report)
{
    if(tree == nullptr || tree->root == nullptr || diagnostics == nullptr || report == nullptr)
        return false;
    *report = (XsMacroExpansionReport){0};
    MacroList empty = {0};
    if(!prepare_scope(tree->root, &empty, diagnostics, report))
    {
        xs_diagnostics_add(diagnostics, XS_DIAGNOSTIC_ERROR, (XsSpan){0, 0},
                           "compiler ran out of memory while preparing macro expansion");
        return false;
    }
    return !xs_diagnostics_has_error(diagnostics);
}

static bool expansion_set_add(XsMacroExpansionSet *set, XsSpan call_span, XsMacroExpansion **out)
{
    if(set->count == set->capacity)
    {
        size_t capacity = set->capacity == 0 ? 8 : set->capacity * 2;
        XsMacroExpansion *items = realloc(set->items, capacity * sizeof(*items));
        if(items == nullptr)
        {
            set->allocation_failed = true;
            return false;
        }
        set->items = items;
        set->capacity = capacity;
    }
    XsMacroExpansion *expansion = &set->items[set->count++];
    *expansion = (XsMacroExpansion){.call_span = call_span};
    *out = expansion;
    return true;
}

static bool expansion_add_token(XsMacroExpansion *expansion, XsMacroExpansionToken token, XsMacroExpansionSet *set)
{
    if(expansion->token_count == expansion->token_capacity)
    {
        size_t capacity = expansion->token_capacity == 0 ? 4 : expansion->token_capacity * 2;
        XsMacroExpansionToken *tokens = realloc(expansion->tokens, capacity * sizeof(*tokens));
        if(tokens == nullptr)
        {
            set->allocation_failed = true;
            return false;
        }
        expansion->tokens = tokens;
        expansion->token_capacity = capacity;
    }
    expansion->tokens[expansion->token_count++] = token;
    return true;
}

static bool emit_rule_tokens(const XsSyntaxNode *rule, const CaptureSet *captures, XsMacroExpansion *expansion,
                             XsMacroExpansionSet *set)
{
    const XsSyntaxNode *body = rule->children[1];
    for(size_t i = 0; i < body->child_count; ++i)
    {
        const XsSyntaxNode *element = body->children[i];
        if(element->kind == XS_SYNTAX_MACRO_EXPANSION_TOKEN)
        {
            XsSpan span = {.start = element->span.start_offset, .end = element->span.end_offset};
            XsMacroExpansionToken token = {.kind = element->token_kind, .text = element->text, .source_span = span};
            if(!expansion_add_token(expansion, token, set))
                return false;
        }
        else if(element->kind == XS_SYNTAX_MACRO_EXPANSION_VARIABLE && element->child_count != 0)
        {
            const Capture *capture = nullptr;
            for(size_t index = 0; index < captures->count; ++index)
            {
                if(xs_macro_text_equal(captures->items[index].name, element->children[0]->text))
                {
                    capture = &captures->items[index];
                    break;
                }
            }
            if(capture == nullptr)
                return false;
            for(size_t offset = 0; offset < capture->argument_count; ++offset)
            {
                const XsSyntaxNode *argument = capture->call == nullptr
                                                   ? capture->argument
                                                   : capture->call->children[capture->argument_index + offset];
                XsSpan span = {.start = argument->span.start_offset, .end = argument->span.end_offset};
                XsMacroExpansionToken token = {.kind = argument->token_kind,
                                               .text = argument->text,
                                               .source_span = span,
                                               .from_substitution = true};
                if(!expansion_add_token(expansion, token, set))
                    return false;
            }
        }
    }
    return true;
}

static bool expand_node(const XsSyntaxNode *node, const MacroList *visible, XsMacroExpansionSet *set);

static bool expand_scope(const XsSyntaxNode *scope, const MacroList *inherited, XsMacroExpansionSet *set)
{
    MacroList visible = {0};
    for(size_t i = 0; i < inherited->count; ++i)
    {
        if(!list_add(&visible, inherited->items[i]))
            return false;
    }
    for(size_t i = 0; i < scope->child_count; ++i)
    {
        if(scope->children[i]->kind == XS_SYNTAX_DECL_MACRO && !list_add(&visible, scope->children[i]))
            return false;
    }
    for(size_t i = 0; i < scope->child_count; ++i)
    {
        if(scope->children[i]->kind != XS_SYNTAX_DECL_MACRO && !expand_node(scope->children[i], &visible, set))
        {
            free(visible.items);
            return false;
        }
    }
    free(visible.items);
    return true;
}

static bool expand_call(const XsSyntaxNode *call, const MacroList *visible, XsMacroExpansionSet *set)
{
    const XsSyntaxNode *macro = resolve_macro(visible, call_name(call));
    if(macro == nullptr)
        return true;
    bool success = true;
    bool matched = false;
    for(size_t i = 0; i < macro->child_count; ++i)
    {
        const XsSyntaxNode *rule = macro->children[i];
        if(rule->kind != XS_SYNTAX_MACRO_RULE || !rule_supported(rule))
            continue;
        CaptureSet captures = {0};
        if(!rule_matches(rule, call, &captures))
            continue;
        if(matched)
            return success;
        matched = true;
        XsSpan span = {.start = call->span.start_offset, .end = call->span.end_offset};
        XsMacroExpansion *expansion = nullptr;
        success =
            expansion_set_add(set, span, &expansion) && emit_rule_tokens(rule, &captures, expansion, set) && success;
    }
    return success;
}

static bool expand_node(const XsSyntaxNode *node, const MacroList *visible, XsMacroExpansionSet *set)
{
    if(node == nullptr)
        return true;
    if(is_scope(node->kind))
        return expand_scope(node, visible, set);
    if(node->kind == XS_SYNTAX_EXPR_MACRO_CALL)
        return expand_call(node, visible, set);
    for(size_t i = 0; i < node->child_count; ++i)
    {
        if(!expand_node(node->children[i], visible, set))
            return false;
    }
    return true;
}

void xs_macro_expansion_set_free(XsMacroExpansionSet *set)
{
    for(size_t i = 0; i < set->count; ++i)
        free(set->items[i].tokens);
    free(set->items);
    *set = (XsMacroExpansionSet){0};
}

bool xs_macro_expand_tokens(const XsSyntaxTree *tree, XsDiagnostics *diagnostics, XsMacroExpansionSet *set)
{
    if(tree == nullptr || tree->root == nullptr || diagnostics == nullptr || set == nullptr)
        return false;
    *set = (XsMacroExpansionSet){0};
    MacroList empty = {0};
    if(!expand_scope(tree->root, &empty, set))
    {
        xs_diagnostics_add(diagnostics, XS_DIAGNOSTIC_ERROR, (XsSpan){0, 0},
                           "compiler ran out of memory while expanding macro tokens");
        return false;
    }
    return !set->allocation_failed && !xs_diagnostics_has_error(diagnostics);
}
