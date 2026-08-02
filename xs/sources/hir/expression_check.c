/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 */

#include "expression_check_internal.h"

#include "xs/hir/type_info.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct
{
    XsText name;
    const XsSyntaxNode *declaration;
    const XsSyntaxNode *type_node;
    bool immutable;
} LocalBinding;
typedef struct LocalScope LocalScope;
typedef struct CheckContext CheckContext;
typedef enum
{
    LITERAL_CONTEXT_INITIALIZER,
    LITERAL_CONTEXT_ASSIGNMENT,
    LITERAL_CONTEXT_RETURN,
} LiteralContext;

struct LocalScope
{
    LocalScope *parent;
    LocalBinding *bindings;
    size_t count;
    size_t capacity;
    bool allocation_failed;
};

struct CheckContext
{
    LocalScope *scope;
    const XsSyntaxNode *root;
    const XsHirPrimitiveInfo *return_type;
    const XsSyntaxNode *return_type_node;
    const XsSyntaxNode *active_property_name;
    bool inside_referential_op;
};

static const XsSyntaxNode *first_child_kind(const XsSyntaxNode *node, XsSyntaxKind kind)
{
    if(node == nullptr)
        return nullptr;
    for(size_t i = 0; i < node->child_count; ++i)
    {
        if(node->children[i]->kind == kind)
            return node->children[i];
    }
    return nullptr;
}

static bool text_equal(XsText left, XsText right)
{
    return left.length == right.length && memcmp(left.data, right.data, left.length) == 0;
}

static bool text_matches_cstr(XsText text, const char *value)
{
    size_t length = strlen(value);
    return text.length == length && memcmp(text.data, value, length) == 0;
}

static XsSpan node_span(const XsSyntaxNode *node)
{
    return (XsSpan){.start = node->span.start_offset, .end = node->span.end_offset};
}

static const XsSyntaxNode *single_path_identifier(const XsSyntaxNode *type)
{
    if(type == nullptr || type->kind != XS_SYNTAX_TYPE_NAMED)
        return nullptr;
    const XsSyntaxNode *path = first_child_kind(type, XS_SYNTAX_PATH);
    if(path == nullptr || path->child_count != 1 || path->children[0]->kind != XS_SYNTAX_IDENTIFIER)
        return nullptr;
    return path->children[0];
}

static const XsHirPrimitiveInfo *primitive_from_type(const XsSyntaxNode *type)
{
    const XsSyntaxNode *identifier = single_path_identifier(type);
    if(identifier == nullptr)
        return nullptr;
    return xs_hir_primitive_find(identifier->text.data, identifier->text.length);
}

static const XsSyntaxNode *type_named_path(const XsSyntaxNode *type)
{
    if(type == nullptr || type->kind != XS_SYNTAX_TYPE_NAMED)
        return nullptr;
    return first_child_kind(type, XS_SYNTAX_PATH);
}

static bool path_matches_qualified(const XsSyntaxNode *path, const char *first, const char *second, const char *third)
{
    if(path == nullptr || path->kind != XS_SYNTAX_PATH || path->child_count != 3)
        return false;
    return text_matches_cstr(path->children[0]->text, first) && text_matches_cstr(path->children[1]->text, second) &&
           text_matches_cstr(path->children[2]->text, third);
}

static bool path_matches_single(const XsSyntaxNode *path, const char *name)
{
    return path != nullptr && path->kind == XS_SYNTAX_PATH && path->child_count == 1 &&
           text_matches_cstr(path->children[0]->text, name);
}

static bool named_type_is_optional_base(const XsSyntaxNode *type)
{
    const XsSyntaxNode *path = type_named_path(type);
    return path_matches_single(path, "Optional") || path_matches_qualified(path, "std", "optional", "Optional");
}

static bool type_is_optional(const XsSyntaxNode *type)
{
    if(type == nullptr)
        return false;
    if((type->flags & XS_SYNTAX_FLAG_OPTIONAL_TYPE) != 0)
        return true;
    if(type->kind == XS_SYNTAX_TYPE_GENERIC && type->child_count >= 1)
        return named_type_is_optional_base(type->children[0]);
    return false;
}

static const char *literal_kind_name(XsTokenKind kind)
{
    switch(kind)
    {
    case XS_TOKEN_INTEGER:
        return "integer";
    case XS_TOKEN_FLOAT:
        return "float";
    case XS_TOKEN_STRING:
        return "string";
    case XS_TOKEN_CHARACTER:
        return "char";
    case XS_TOKEN_KW_TRUE:
    case XS_TOKEN_KW_FALSE:
        return "bool";
    case XS_TOKEN_KW_NONE:
        return "None";
    default:
        return "literal";
    }
}

static bool literal_matches_primitive(XsTokenKind literal, const XsHirPrimitiveInfo *primitive)
{
    if(primitive == nullptr)
        return true;
    if(literal == XS_TOKEN_KW_NONE)
        return false;
    if(literal == XS_TOKEN_INTEGER)
        return primitive->kind == XS_HIR_PRIMITIVE_BOOL ||
               (primitive->is_integer && primitive->kind != XS_HIR_PRIMITIVE_CHAR);
    if(literal == XS_TOKEN_FLOAT)
        return primitive->is_float;
    if(literal == XS_TOKEN_STRING)
        return false;
    if(literal == XS_TOKEN_CHARACTER)
        return primitive->kind == XS_HIR_PRIMITIVE_CHAR;
    if(literal == XS_TOKEN_KW_TRUE || literal == XS_TOKEN_KW_FALSE)
        return primitive->kind == XS_HIR_PRIMITIVE_BOOL;
    return true;
}

static bool report_literal_type_error(XsDiagnostics *diagnostics, const XsSyntaxNode *literal,
                                      const XsHirPrimitiveInfo *primitive)
{
    char message[256];
    snprintf(message, sizeof(message), "%s literal is not assignable to primitive type '%s'",
             literal_kind_name(literal->token_kind), primitive->name);
    return xs_diagnostics_add(diagnostics, XS_DIAGNOSTIC_ERROR, node_span(literal), message);
}

static bool report_assignment_literal_type_error(XsDiagnostics *diagnostics, const XsSyntaxNode *literal,
                                                 const XsHirPrimitiveInfo *primitive, const LocalBinding *binding)
{
    int length = binding->name.length > 128 ? 128 : (int)binding->name.length;
    char message[320];
    snprintf(message, sizeof(message), "%s literal is not assignable to local '%.*s' of primitive type '%s'",
             literal_kind_name(literal->token_kind), length, binding->name.data, primitive->name);
    return xs_diagnostics_add(diagnostics, XS_DIAGNOSTIC_ERROR, node_span(literal), message);
}

static bool report_binding_reassignment(XsDiagnostics *diagnostics, const XsSyntaxNode *target,
                                        const LocalBinding *binding)
{
    int length = binding->name.length > 128 ? 128 : (int)binding->name.length;
    char message[256];
    snprintf(message, sizeof(message), "cannot reassign binding '%.*s'", length, binding->name.data);
    return xs_diagnostics_add(diagnostics, XS_DIAGNOSTIC_ERROR, node_span(target), message);
}

static bool report_non_constant_initializer(XsDiagnostics *diagnostics, const XsSyntaxNode *initializer,
                                            const char *kind)
{
    char message[160];
    snprintf(message, sizeof(message), "%s initializer must be a compile-time constant", kind);
    return xs_diagnostics_add(diagnostics, XS_DIAGNOSTIC_ERROR, node_span(initializer), message);
}

static bool report_missing_constant_initializer(XsDiagnostics *diagnostics, const XsSyntaxNode *declaration,
                                                const char *kind)
{
    char message[160];
    snprintf(message, sizeof(message), "%s declaration requires a compile-time constant initializer", kind);
    return xs_diagnostics_add(diagnostics, XS_DIAGNOSTIC_ERROR, node_span(declaration), message);
}

static bool report_return_literal_type_error(XsDiagnostics *diagnostics, const XsSyntaxNode *literal,
                                             const XsHirPrimitiveInfo *primitive)
{
    char message[256];
    snprintf(message, sizeof(message), "%s literal is not assignable to function return type '%s'",
             literal_kind_name(literal->token_kind), primitive->name);
    return xs_diagnostics_add(diagnostics, XS_DIAGNOSTIC_ERROR, node_span(literal), message);
}

static bool report_context_literal_type_error(XsDiagnostics *diagnostics, const XsSyntaxNode *literal,
                                              const XsHirPrimitiveInfo *primitive, LiteralContext context,
                                              const LocalBinding *binding)
{
    if(context == LITERAL_CONTEXT_RETURN)
        return report_return_literal_type_error(diagnostics, literal, primitive);
    if(context == LITERAL_CONTEXT_ASSIGNMENT && binding != nullptr)
        return report_assignment_literal_type_error(diagnostics, literal, primitive, binding);
    return report_literal_type_error(diagnostics, literal, primitive);
}

static bool report_optional_type_error(XsDiagnostics *diagnostics, const XsSyntaxNode *expression)
{
    return xs_diagnostics_add(diagnostics, XS_DIAGNOSTIC_ERROR, node_span(expression),
                              "expression is not assignable to Optional<T>");
}

static bool report_missing_block_value(XsDiagnostics *diagnostics, const XsSyntaxNode *block,
                                       const XsHirPrimitiveInfo *primitive)
{
    char message[192];
    snprintf(message, sizeof(message), "block produces unit '()' but primitive type '%s' is required", primitive->name);
    return xs_diagnostics_add(diagnostics, XS_DIAGNOSTIC_ERROR, node_span(block), message);
}

static bool report_missing_optional_block_value(XsDiagnostics *diagnostics, const XsSyntaxNode *block)
{
    return xs_diagnostics_add(diagnostics, XS_DIAGNOSTIC_ERROR, node_span(block),
                              "block produces unit '()' but Optional<T> is required");
}

static bool expression_is_identifier_named(const XsSyntaxNode *expression, const char *name)
{
    if(expression == nullptr || expression->kind != XS_SYNTAX_EXPR_IDENTIFIER || expression->child_count != 1)
        return false;
    return expression->children[0]->kind == XS_SYNTAX_IDENTIFIER &&
           text_matches_cstr(expression->children[0]->text, name);
}

static bool path_is_std_optional_member(const XsSyntaxNode *path, const char *name)
{
    return path != nullptr && path->kind == XS_SYNTAX_PATH && path->child_count == 3 &&
           text_matches_cstr(path->children[0]->text, "std") &&
           text_matches_cstr(path->children[1]->text, "optional") && text_matches_cstr(path->children[2]->text, name);
}

static bool expression_is_path_named(const XsSyntaxNode *expression, const char *name)
{
    if(expression == nullptr || expression->kind != XS_SYNTAX_EXPR_IDENTIFIER || expression->child_count != 1)
        return false;
    return path_is_std_optional_member(expression->children[0], name);
}

static bool expression_is_std_optional_member(const XsSyntaxNode *expression, const char *name)
{
    if(expression_is_path_named(expression, name))
        return true;
    if(expression == nullptr || expression->kind != XS_SYNTAX_EXPR_MEMBER_ACCESS || expression->child_count != 2)
        return false;
    if(expression->children[1]->kind != XS_SYNTAX_IDENTIFIER || !text_matches_cstr(expression->children[1]->text, name))
        return false;
    const XsSyntaxNode *optional = expression->children[0];
    if(optional == nullptr || optional->kind != XS_SYNTAX_EXPR_MEMBER_ACCESS || optional->child_count != 2)
        return false;
    return expression_is_identifier_named(optional->children[0], "std") &&
           optional->children[1]->kind == XS_SYNTAX_IDENTIFIER &&
           text_matches_cstr(optional->children[1]->text, "optional");
}

static bool expression_is_optional_none(const XsSyntaxNode *expression)
{
    return (expression != nullptr && expression->kind == XS_SYNTAX_EXPR_LITERAL &&
            expression->token_kind == XS_TOKEN_KW_NONE) ||
           expression_is_identifier_named(expression, "nil") || expression_is_std_optional_member(expression, "None");
}

static bool expression_is_optional_some(const XsSyntaxNode *expression)
{
    if(expression == nullptr || expression->kind != XS_SYNTAX_EXPR_CALL || expression->child_count != 2)
        return false;
    const XsSyntaxNode *callee = expression->children[0];
    return expression_is_identifier_named(callee, "Some") || expression_is_std_optional_member(callee, "Some");
}

static bool expression_is_optional_some_callee(const XsSyntaxNode *expression)
{
    if(expression == nullptr || expression->kind != XS_SYNTAX_EXPR_CALL || expression->child_count == 0)
        return false;
    const XsSyntaxNode *callee = expression->children[0];
    return expression_is_identifier_named(callee, "Some") || expression_is_std_optional_member(callee, "Some");
}

static const XsSyntaxNode *expression_identifier(const XsSyntaxNode *expression)
{
    if(expression == nullptr || expression->kind != XS_SYNTAX_EXPR_IDENTIFIER || expression->child_count != 1)
        return nullptr;
    return expression->children[0]->kind == XS_SYNTAX_IDENTIFIER ? expression->children[0] : nullptr;
}

static bool report_optional_to_primitive_error(XsDiagnostics *diagnostics, const XsSyntaxNode *expression,
                                               const XsHirPrimitiveInfo *primitive)
{
    char message[256];
    snprintf(message, sizeof(message), "Optional value is not assignable to primitive type '%s'", primitive->name);
    return xs_diagnostics_add(diagnostics, XS_DIAGNOSTIC_ERROR, node_span(expression), message);
}

static const XsSyntaxNode *block_tail_expression(const XsSyntaxNode *block)
{
    if(block == nullptr || block->kind != XS_SYNTAX_STMT_BLOCK || block->child_count == 0)
        return nullptr;
    const XsSyntaxNode *tail = block->children[block->child_count - 1];
    if(tail->kind != XS_SYNTAX_STMT_EXPRESSION || tail->child_count == 0)
        return nullptr;
    if((tail->flags & XS_SYNTAX_FLAG_DISCARDED) != 0)
        return nullptr;
    return tail->children[0];
}

static bool check_expression_value_against_primitive(const XsSyntaxNode *expression,
                                                     const XsHirPrimitiveInfo *primitive, LiteralContext context,
                                                     const LocalBinding *binding, XsDiagnostics *diagnostics);

static bool check_expression_value_against_optional(const XsSyntaxNode *expression, XsDiagnostics *diagnostics);

static bool check_block_value_against_primitive(const XsSyntaxNode *block, const XsHirPrimitiveInfo *primitive,
                                                LiteralContext context, const LocalBinding *binding,
                                                XsDiagnostics *diagnostics)
{
    const XsSyntaxNode *tail = block_tail_expression(block);
    if(tail == nullptr && primitive != nullptr)
        return report_missing_block_value(diagnostics, block, primitive);
    return check_expression_value_against_primitive(tail, primitive, context, binding, diagnostics);
}

static bool check_match_arm_value_against_primitive(const XsSyntaxNode *arm, const XsHirPrimitiveInfo *primitive,
                                                    LiteralContext context, const LocalBinding *binding,
                                                    XsDiagnostics *diagnostics)
{
    if(arm == nullptr || arm->kind != XS_SYNTAX_MATCH_ARM || arm->child_count < 2)
        return true;
    return check_block_value_against_primitive(arm->children[1], primitive, context, binding, diagnostics);
}

static bool check_expression_value_against_primitive(const XsSyntaxNode *expression,
                                                     const XsHirPrimitiveInfo *primitive, LiteralContext context,
                                                     const LocalBinding *binding, XsDiagnostics *diagnostics)
{
    if(expression == nullptr || primitive == nullptr)
        return true;
    if(expression_is_optional_none(expression) || expression_is_optional_some_callee(expression))
        return report_optional_to_primitive_error(diagnostics, expression, primitive);
    if(expression->kind == XS_SYNTAX_EXPR_LITERAL)
    {
        if(literal_matches_primitive(expression->token_kind, primitive))
            return true;
        return report_context_literal_type_error(diagnostics, expression, primitive, context, binding);
    }
    if(expression->kind == XS_SYNTAX_EXPR_IF && expression->child_count >= 3)
    {
        bool then_ok =
            check_block_value_against_primitive(expression->children[1], primitive, context, binding, diagnostics);
        bool else_ok =
            check_block_value_against_primitive(expression->children[2], primitive, context, binding, diagnostics);
        return then_ok && else_ok;
    }
    if(expression->kind == XS_SYNTAX_EXPR_MATCH)
    {
        bool success = true;
        for(size_t i = 1; i < expression->child_count; ++i)
        {
            success = check_match_arm_value_against_primitive(expression->children[i], primitive, context, binding,
                                                              diagnostics) &&
                      success;
        }
        return success;
    }
    return true;
}

static bool check_block_value_against_optional(const XsSyntaxNode *block, XsDiagnostics *diagnostics)
{
    const XsSyntaxNode *tail = block_tail_expression(block);
    if(tail == nullptr)
        return report_missing_optional_block_value(diagnostics, block);
    return check_expression_value_against_optional(tail, diagnostics);
}

static bool check_match_arm_value_against_optional(const XsSyntaxNode *arm, XsDiagnostics *diagnostics)
{
    if(arm == nullptr || arm->kind != XS_SYNTAX_MATCH_ARM || arm->child_count < 2)
        return true;
    return check_block_value_against_optional(arm->children[1], diagnostics);
}

static bool check_expression_value_against_optional(const XsSyntaxNode *expression, XsDiagnostics *diagnostics)
{
    if(expression == nullptr)
        return true;
    if(expression_is_optional_none(expression) || expression_is_optional_some(expression))
        return true;
    if(expression_is_optional_some_callee(expression))
        return report_optional_type_error(diagnostics, expression);
    if(expression->kind == XS_SYNTAX_EXPR_LITERAL)
        return true;
    if(expression->kind == XS_SYNTAX_EXPR_IF && expression->child_count >= 3)
    {
        bool then_ok = check_block_value_against_optional(expression->children[1], diagnostics);
        bool else_ok = check_block_value_against_optional(expression->children[2], diagnostics);
        return then_ok && else_ok;
    }
    if(expression->kind == XS_SYNTAX_EXPR_MATCH)
    {
        bool success = true;
        for(size_t i = 1; i < expression->child_count; ++i)
            success = check_match_arm_value_against_optional(expression->children[i], diagnostics) && success;
        return success;
    }
    return true;
}

static const XsSyntaxNode *variable_initializer(const XsSyntaxNode *declaration)
{
    if(declaration == nullptr || declaration->kind != XS_SYNTAX_DECL_VARIABLE)
        return nullptr;
    if((declaration->flags & XS_SYNTAX_FLAG_INFERRED_TYPE) != 0)
        return declaration->child_count >= 2 ? declaration->children[1] : nullptr;
    return declaration->child_count >= 3 ? declaration->children[2] : nullptr;
}

static bool is_compile_time_constant_expression(const XsSyntaxNode *expression)
{
    if(expression == nullptr)
        return false;
    switch(expression->kind)
    {
    case XS_SYNTAX_EXPR_LITERAL:
        return true;
    case XS_SYNTAX_EXPR_UNARY:
    case XS_SYNTAX_EXPR_BINARY:
    case XS_SYNTAX_EXPR_TUPLE:
    case XS_SYNTAX_EXPR_ARRAY_LITERAL:
    case XS_SYNTAX_EXPR_OBJECT_LITERAL:
    {
        for(size_t i = 0; i < expression->child_count; ++i)
        {
            if(expression->children[i]->kind == XS_SYNTAX_TOKEN)
                continue;
            if(!is_compile_time_constant_expression(expression->children[i]))
                return false;
        }
        return true;
    }
    case XS_SYNTAX_EXPR_FIELD_SET:
        return expression->child_count >= 2 && is_compile_time_constant_expression(expression->children[1]);
    default:
        return false;
    }
}

static bool check_constant_initializer(const XsSyntaxNode *declaration, XsDiagnostics *diagnostics)
{
    uint32_t constant_flags = declaration->flags & (XS_SYNTAX_FLAG_CONSTANT | XS_SYNTAX_FLAG_STATIC_CONSTANT);
    if(constant_flags == 0)
        return true;
    const XsSyntaxNode *initializer = variable_initializer(declaration);
    const char *kind = (constant_flags & XS_SYNTAX_FLAG_STATIC_CONSTANT) != 0 ? "static" : "const";
    if(initializer == nullptr)
        return report_missing_constant_initializer(diagnostics, declaration, kind);
    if((constant_flags & XS_SYNTAX_FLAG_STATIC_CONSTANT) == 0)
        return true;
    if(is_compile_time_constant_expression(initializer))
        return true;
    return report_non_constant_initializer(diagnostics, initializer, kind);
}

static bool check_variable_initializer(const XsSyntaxNode *declaration, XsDiagnostics *diagnostics)
{
    if((declaration->flags & XS_SYNTAX_FLAG_INFERRED_TYPE) != 0)
        return check_constant_initializer(declaration, diagnostics);
    bool constant_ok = check_constant_initializer(declaration, diagnostics);
    if(declaration->child_count < 3)
        return constant_ok;
    const XsHirPrimitiveInfo *primitive = primitive_from_type(declaration->children[1]);
    const XsSyntaxNode *initializer = variable_initializer(declaration);
    if(type_is_optional(declaration->children[1]))
        return check_expression_value_against_optional(initializer, diagnostics) && constant_ok;
    if(xs_hir_type_is_borrowed_str(declaration->children[1]))
        return xs_hir_check_borrowed_str_value(initializer, diagnostics) && constant_ok;
    bool success = check_expression_value_against_primitive(initializer, primitive, LITERAL_CONTEXT_INITIALIZER,
                                                            nullptr, diagnostics);
    return constant_ok && success;
}
static const XsSyntaxNode *class_field_type(const XsSyntaxNode *field)
{
    return field != nullptr && field->kind == XS_SYNTAX_CLASS_FIELD && field->child_count >= 2 ? field->children[1]
                                                                                               : nullptr;
}
static const XsSyntaxNode *accessor_body(const XsSyntaxNode *accessor)
{
    if(accessor == nullptr)
        return nullptr;
    for(size_t i = 0; i < accessor->child_count; ++i)
    {
        if(accessor->children[i]->kind == XS_SYNTAX_STMT_BLOCK)
            return accessor->children[i];
    }
    return nullptr;
}
static bool report_duplicate_property_accessor(XsDiagnostics *diagnostics, const XsSyntaxNode *accessor)
{
    const char *name = accessor->token_kind == XS_TOKEN_KW_GETTER ? "getter" : "setter";
    char message[128];
    snprintf(message, sizeof(message), "property already has a %s accessor", name);
    return xs_diagnostics_add(diagnostics, XS_DIAGNOSTIC_ERROR, node_span(accessor), message);
}
static const XsSyntaxNode *declaration_identifier(const XsSyntaxNode *declaration)
{
    return declaration->child_count == 0 || declaration->children[0]->kind != XS_SYNTAX_IDENTIFIER
               ? nullptr
               : declaration->children[0];
}
static bool local_scope_reserve_one(LocalScope *scope)
{
    if(scope->count < scope->capacity)
        return true;
    size_t capacity = scope->capacity == 0 ? 8 : scope->capacity * 2;
    LocalBinding *bindings = realloc(scope->bindings, capacity * sizeof(*bindings));
    if(bindings == nullptr)
    {
        scope->allocation_failed = true;
        return false;
    }
    scope->bindings = bindings;
    scope->capacity = capacity;
    return true;
}
static bool local_scope_add(LocalScope *scope, const XsSyntaxNode *declaration)
{
    const XsSyntaxNode *identifier = declaration_identifier(declaration);
    if(scope == nullptr || identifier == nullptr)
        return true;
    if(!local_scope_reserve_one(scope))
        return false;
    bool immutable = (declaration->flags &
                      (XS_SYNTAX_FLAG_IMMUTABLE | XS_SYNTAX_FLAG_CONSTANT | XS_SYNTAX_FLAG_STATIC_CONSTANT)) != 0;
    const XsSyntaxNode *type_node = declaration->child_count >= 2 ? declaration->children[1] : nullptr;
    scope->bindings[scope->count++] = (LocalBinding){
        .name = identifier->text, .declaration = declaration, .type_node = type_node, .immutable = immutable};
    return true;
}
static bool local_scope_add_value(LocalScope *scope, const XsSyntaxNode *type_node)
{
    if(scope == nullptr)
        return true;
    if(!local_scope_reserve_one(scope))
        return false;
    static const char value_name[] = "value";
    scope->bindings[scope->count++] = (LocalBinding){
        .name = {.data = value_name, .length = sizeof(value_name) - 1},
        .declaration = nullptr,
        .type_node = type_node,
        .immutable = true,
    };
    return true;
}
static const LocalBinding *local_scope_find(const LocalScope *scope, XsText name)
{
    for(const LocalScope *current = scope; current != nullptr; current = current->parent)
    {
        for(size_t i = current->count; i > 0; --i)
        {
            if(text_equal(current->bindings[i - 1].name, name))
                return &current->bindings[i - 1];
        }
    }
    return nullptr;
}
static void local_scope_free(LocalScope *scope)
{
    free(scope->bindings);
    *scope = (LocalScope){0};
}
static bool has_child_kind(const XsSyntaxNode *node, XsSyntaxKind kind)
{
    if(node == nullptr)
        return false;
    for(size_t i = 0; i < node->child_count; ++i)
    {
        if(node->children[i]->kind == kind)
            return true;
    }
    return false;
}

static const XsHirPrimitiveInfo *function_return_primitive(const XsSyntaxNode *function)
{
    if(function == nullptr || (function->kind != XS_SYNTAX_DECL_FUNCTION && function->kind != XS_SYNTAX_EXPR_FUNCTION))
        return nullptr;
    for(size_t i = 0; i < function->child_count; ++i)
    {
        const XsSyntaxNode *child = function->children[i];
        if(child->kind == XS_SYNTAX_TYPE_NAMED && (child->flags & XS_SYNTAX_FLAG_RETURN_TYPE) != 0)
            return primitive_from_type(child);
    }
    return nullptr;
}

static const XsSyntaxNode *function_return_type(const XsSyntaxNode *function)
{
    if(function == nullptr || (function->kind != XS_SYNTAX_DECL_FUNCTION && function->kind != XS_SYNTAX_EXPR_FUNCTION))
        return nullptr;
    for(size_t i = 0; i < function->child_count; ++i)
    {
        const XsSyntaxNode *child = function->children[i];
        if((child->flags & XS_SYNTAX_FLAG_RETURN_TYPE) != 0)
            return child;
    }
    return nullptr;
}

static const XsSyntaxNode *function_named_in_tree(const XsSyntaxNode *node, XsText name)
{
    if(node == nullptr)
        return nullptr;
    if(node->kind == XS_SYNTAX_DECL_FUNCTION)
    {
        const XsSyntaxNode *identifier = first_child_kind(node, XS_SYNTAX_IDENTIFIER);
        if(identifier != nullptr && text_equal(identifier->text, name))
            return node;
    }
    for(size_t i = 0; i < node->child_count; ++i)
    {
        const XsSyntaxNode *match = function_named_in_tree(node->children[i], name);
        if(match != nullptr)
            return match;
    }
    return nullptr;
}

static const XsSyntaxNode *direct_call_return_type(const XsSyntaxNode *expression, const CheckContext *context)
{
    if(expression == nullptr || expression->kind != XS_SYNTAX_EXPR_CALL || expression->child_count == 0 ||
       context == nullptr || context->root == nullptr)
        return nullptr;
    const XsSyntaxNode *callee = expression_identifier(expression->children[0]);
    if(callee == nullptr)
        return nullptr;
    const XsSyntaxNode *function = function_named_in_tree(context->root, callee->text);
    return function_return_type(function);
}

static bool report_result_operand_error(XsDiagnostics *diagnostics, const XsSyntaxNode *node)
{
    return xs_diagnostics_add(diagnostics, XS_DIAGNOSTIC_ERROR, node_span(node),
                              "Result propagation operand must return Result<T, E>");
}

static const XsSyntaxNode *assignment_identifier_target(const XsSyntaxNode *node)
{
    if(node == nullptr || node->kind != XS_SYNTAX_EXPR_ASSIGNMENT || node->child_count < 1)
        return nullptr;
    const XsSyntaxNode *target = node->children[0];
    if(target->kind != XS_SYNTAX_EXPR_IDENTIFIER || target->child_count == 0 ||
       target->children[0]->kind != XS_SYNTAX_IDENTIFIER)
        return nullptr;
    return target->children[0];
}

static const XsSyntaxNode *assignment_value(const XsSyntaxNode *node)
{
    return node != nullptr && node->kind == XS_SYNTAX_EXPR_ASSIGNMENT && node->child_count >= 3 ? node->children[2]
                                                                                                : nullptr;
}

static bool check_return_statement(const XsSyntaxNode *node, const CheckContext *context, XsDiagnostics *diagnostics)
{
    if(node == nullptr || node->kind != XS_SYNTAX_STMT_RETURN || node->child_count == 0 ||
       (context->return_type == nullptr && !type_is_optional(context->return_type_node) &&
        !xs_hir_type_is_borrowed_str(context->return_type_node)))
        return true;
    const XsSyntaxNode *value = node->children[0];
    if(type_is_optional(context->return_type_node))
        return check_expression_value_against_optional(value, diagnostics);
    if(xs_hir_type_is_borrowed_str(context->return_type_node))
        return xs_hir_check_borrowed_str_value(value, diagnostics);
    return check_expression_value_against_primitive(value, context->return_type, LITERAL_CONTEXT_RETURN, nullptr,
                                                    diagnostics);
}

static bool check_assignment(const XsSyntaxNode *node, const CheckContext *context, XsDiagnostics *diagnostics)
{
    const XsSyntaxNode *identifier = assignment_identifier_target(node);
    if(identifier == nullptr)
        return true;
    const LocalBinding *binding = local_scope_find(context->scope, identifier->text);
    if(binding == nullptr)
        return true;
    bool success = true;
    const XsHirPrimitiveInfo *primitive = primitive_from_type(binding->type_node);
    const XsSyntaxNode *value = assignment_value(node);
    if(type_is_optional(binding->type_node))
        success = check_expression_value_against_optional(value, diagnostics) && success;
    else if(xs_hir_type_is_borrowed_str(binding->type_node))
        success = xs_hir_check_borrowed_str_value(value, diagnostics) && success;
    success =
        check_expression_value_against_primitive(value, primitive, LITERAL_CONTEXT_ASSIGNMENT, binding, diagnostics) &&
        success;
    if(binding->immutable)
        success = report_binding_reassignment(diagnostics, identifier, binding) && success;
    return success;
}

static bool check_result_propagation(const XsSyntaxNode *node, const CheckContext *context, XsDiagnostics *diagnostics)
{
    if(context != nullptr && xs_hir_type_returns_result(context->return_type_node))
    {
        const XsSyntaxNode *operand = node->child_count == 0 ? nullptr : node->children[0];
        const XsSyntaxNode *operand_return = direct_call_return_type(operand, context);
        if(operand_return != nullptr && !xs_hir_type_returns_result(operand_return))
            return report_result_operand_error(diagnostics, operand);
        return true;
    }
    xs_diagnostics_add(diagnostics, XS_DIAGNOSTIC_ERROR, node_span(node),
                       "Result propagation with '@' requires an enclosing function returning Result<T, E>");
    return false;
}

static bool member_access_is_active_property(const XsSyntaxNode *node, const CheckContext *context)
{
    if(node == nullptr || context == nullptr || context->active_property_name == nullptr ||
       (node->kind != XS_SYNTAX_EXPR_MEMBER_ACCESS && node->kind != XS_SYNTAX_EXPR_METHOD_CALL) ||
       node->child_count < 2)
        return false;
    const XsSyntaxNode *receiver = expression_identifier(node->children[0]);
    const XsSyntaxNode *member = node->children[1];
    return receiver != nullptr && member->kind == XS_SYNTAX_IDENTIFIER && text_matches_cstr(receiver->text, "self") &&
           text_equal(member->text, context->active_property_name->text);
}

static bool report_recursive_property_access(XsDiagnostics *diagnostics, const XsSyntaxNode *node)
{
    return xs_diagnostics_add(diagnostics, XS_DIAGNOSTIC_ERROR, node_span(node),
                              "property accessor recursively references itself; use an explicit backing field");
}

static bool node_breaks_referential_transparency(const XsSyntaxNode *node)
{
    if(node == nullptr)
        return false;
    switch(node->kind)
    {
    case XS_SYNTAX_DECL_EXTERN_BLOCK:
    case XS_SYNTAX_DECL_MACRO_CALL:
    case XS_SYNTAX_STMT_MACRO_CALL:
    case XS_SYNTAX_EXPR_ASSIGNMENT:
    case XS_SYNTAX_EXPR_CALL:
    case XS_SYNTAX_EXPR_METHOD_CALL:
    case XS_SYNTAX_EXPR_OPTIONAL_METHOD_CALL:
    case XS_SYNTAX_EXPR_RESULT_PROPAGATION:
    case XS_SYNTAX_EXPR_NEW:
    case XS_SYNTAX_EXPR_AWAIT:
    case XS_SYNTAX_EXPR_MUTABLE_BORROW:
    case XS_SYNTAX_EXPR_FIELD_SET:
        return true;
    default:
        return false;
    }
}

static bool check_referential_op_node(const XsSyntaxNode *node, const CheckContext *context, XsDiagnostics *diagnostics)
{
    if(context == nullptr || !context->inside_referential_op || !node_breaks_referential_transparency(node))
        return true;
    return xs_diagnostics_add(diagnostics, XS_DIAGNOSTIC_ERROR, node_span(node),
                              "op declarations must be referentially transparent; this operation is not allowed");
}

static bool check_node(const XsSyntaxNode *node, const XsMacroDeclarationExpansionSet *macro_declarations,
                       const XsMacroStatementExpansionSet *macro_statements, CheckContext context,
                       XsDiagnostics *diagnostics);

static bool check_children(const XsSyntaxNode *node, const XsMacroDeclarationExpansionSet *macro_declarations,
                           const XsMacroStatementExpansionSet *macro_statements, CheckContext context,
                           XsDiagnostics *diagnostics)
{
    bool success = true;
    for(size_t i = 0; i < node->child_count; ++i)
    {
        success = check_node(node->children[i], macro_declarations, macro_statements, context, diagnostics) && success;
    }
    return success;
}

static bool check_expanded_declaration_children(const XsSyntaxNode *node,
                                                const XsMacroDeclarationExpansionSet *macro_declarations,
                                                const XsMacroStatementExpansionSet *macro_statements,
                                                CheckContext context, XsDiagnostics *diagnostics)
{
    XsMacroExpandedDeclarationSet expanded = {0};
    if(!xs_macro_expand_child_declarations(node, macro_declarations, diagnostics, &expanded))
        return false;
    bool success = true;
    for(size_t i = 0; i < expanded.count; ++i)
        success =
            check_node(expanded.items[i].declaration, macro_declarations, macro_statements, context, diagnostics) &&
            success;
    xs_macro_expanded_declaration_set_free(&expanded);
    return success;
}

static bool check_expanded_statement_children(const XsSyntaxNode *node,
                                              const XsMacroDeclarationExpansionSet *macro_declarations,
                                              const XsMacroStatementExpansionSet *macro_statements,
                                              CheckContext context, XsDiagnostics *diagnostics)
{
    XsMacroExpandedStatementSet expanded = {0};
    if(!xs_macro_expand_child_statements(node, macro_statements, diagnostics, &expanded))
        return false;
    bool success = true;
    for(size_t i = 0; i < expanded.count; ++i)
        success = check_node(expanded.items[i].statement, macro_declarations, macro_statements, context, diagnostics) &&
                  success;
    xs_macro_expanded_statement_set_free(&expanded);
    return success;
}

static bool check_scoped_node(const XsSyntaxNode *node, const XsMacroDeclarationExpansionSet *macro_declarations,
                              const XsMacroStatementExpansionSet *macro_statements, CheckContext parent,
                              XsDiagnostics *diagnostics)
{
    LocalScope scope = {.parent = parent.scope};
    const XsHirPrimitiveInfo *return_type = function_return_primitive(node);
    const XsSyntaxNode *return_type_node = function_return_type(node);
    CheckContext context = {
        .scope = &scope,
        .root = parent.root,
        .return_type = return_type == nullptr ? parent.return_type : return_type,
        .return_type_node = return_type_node == nullptr ? parent.return_type_node : return_type_node,
        .active_property_name = parent.active_property_name,
        .inside_referential_op = parent.inside_referential_op,
    };
    if(node->kind == XS_SYNTAX_DECL_FUNCTION)
        context.inside_referential_op = (node->flags & XS_SYNTAX_FLAG_REFERENTIAL_TRANSPARENT) != 0;
    else if(node->kind == XS_SYNTAX_EXPR_FUNCTION)
        context.inside_referential_op = false;
    bool success = true;
    success = check_referential_op_node(node, &context, diagnostics) && success;
    if(macro_declarations != nullptr && has_child_kind(node, XS_SYNTAX_DECL_MACRO_CALL))
        success = check_expanded_declaration_children(node, macro_declarations, macro_statements, context, diagnostics);
    else if(macro_statements != nullptr && has_child_kind(node, XS_SYNTAX_STMT_MACRO_CALL))
        success = check_expanded_statement_children(node, macro_declarations, macro_statements, context, diagnostics);
    else
        success = check_children(node, macro_declarations, macro_statements, context, diagnostics);
    bool allocation_failed = scope.allocation_failed;
    local_scope_free(&scope);
    return success && !allocation_failed;
}

static bool check_property_field(const XsSyntaxNode *field, const XsMacroDeclarationExpansionSet *macro_declarations,
                                 const XsMacroStatementExpansionSet *macro_statements, CheckContext context,
                                 XsDiagnostics *diagnostics)
{
    const XsSyntaxNode *type_node = class_field_type(field);
    const XsSyntaxNode *name = declaration_identifier(field);
    const XsHirPrimitiveInfo *primitive = primitive_from_type(type_node);
    bool getter_seen = false;
    bool setter_seen = false;
    bool success = true;
    for(size_t i = 0; i < field->child_count; ++i)
    {
        const XsSyntaxNode *accessor = field->children[i];
        if(accessor->kind != XS_SYNTAX_PROPERTY_ACCESSOR)
            continue;
        bool is_getter = accessor->token_kind == XS_TOKEN_KW_GETTER;
        if((is_getter && getter_seen) || (!is_getter && setter_seen))
            success = report_duplicate_property_accessor(diagnostics, accessor) && success;
        getter_seen = getter_seen || is_getter;
        setter_seen = setter_seen || !is_getter;
        const XsSyntaxNode *body = accessor_body(accessor);
        if(body == nullptr)
            continue;
        LocalScope scope = {.parent = context.scope};
        CheckContext nested = {
            .scope = &scope,
            .root = context.root,
            .active_property_name = name,
            .inside_referential_op = context.inside_referential_op,
        };
        if(is_getter)
        {
            nested.return_type = primitive;
            nested.return_type_node = type_node;
        }
        else
            success = local_scope_add_value(&scope, type_node) && success;
        success = check_node(body, macro_declarations, macro_statements, nested, diagnostics) && success;
        bool allocation_failed = scope.allocation_failed;
        local_scope_free(&scope);
        success = success && !allocation_failed;
    }
    return success;
}

static bool check_node(const XsSyntaxNode *node, const XsMacroDeclarationExpansionSet *macro_declarations,
                       const XsMacroStatementExpansionSet *macro_statements, CheckContext context,
                       XsDiagnostics *diagnostics)
{
    if(node == nullptr)
        return true;
    bool success = true;
    if(node->kind == XS_SYNTAX_DECL_FUNCTION || node->kind == XS_SYNTAX_EXPR_FUNCTION ||
       node->kind == XS_SYNTAX_STMT_BLOCK)
        return check_scoped_node(node, macro_declarations, macro_statements, context, diagnostics);
    success = check_referential_op_node(node, &context, diagnostics) && success;
    if(node->kind == XS_SYNTAX_DECL_VARIABLE)
    {
        success = check_variable_initializer(node, diagnostics) && success;
        success = local_scope_add(context.scope, node) && success;
    }
    if(node->kind == XS_SYNTAX_EXPR_ASSIGNMENT)
        success = check_assignment(node, &context, diagnostics) && success;
    if(node->kind == XS_SYNTAX_EXPR_RESULT_PROPAGATION)
        success = check_result_propagation(node, &context, diagnostics) && success;
    if(node->kind == XS_SYNTAX_EXPR_CALL)
        success = xs_hir_check_result_constructor_call(node, xs_hir_type_returns_result(context.return_type_node),
                                                       diagnostics) &&
                  success;
    if(node->kind == XS_SYNTAX_STMT_RETURN)
        success = check_return_statement(node, &context, diagnostics) && success;
    if(member_access_is_active_property(node, &context))
        success = report_recursive_property_access(diagnostics, node) && success;
    if(node->kind == XS_SYNTAX_CLASS_FIELD)
        return check_property_field(node, macro_declarations, macro_statements, context, diagnostics) && success;
    if(macro_declarations != nullptr && has_child_kind(node, XS_SYNTAX_DECL_MACRO_CALL))
        return check_expanded_declaration_children(node, macro_declarations, macro_statements, context, diagnostics) &&
               success;
    if(macro_statements != nullptr && has_child_kind(node, XS_SYNTAX_STMT_MACRO_CALL))
        return check_expanded_statement_children(node, macro_declarations, macro_statements, context, diagnostics) &&
               success;
    return check_children(node, macro_declarations, macro_statements, context, diagnostics) && success;
}

bool xs_hir_check_expression_types_with_macros(const XsSyntaxTree *tree,
                                               const XsMacroDeclarationExpansionSet *macro_declarations,
                                               const XsMacroStatementExpansionSet *macro_statements,
                                               XsDiagnostics *diagnostics)
{
    if(tree == nullptr || tree->root == nullptr || diagnostics == nullptr)
        return false;
    return check_node(tree->root, macro_declarations, macro_statements, (CheckContext){.root = tree->root},
                      diagnostics) &&
           !xs_diagnostics_has_error(diagnostics);
}
