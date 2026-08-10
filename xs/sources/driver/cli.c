/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 */

#include "Visual/XSharp/driver.hh"

#include "compiler_core_native.h"
#include "native_artifact.h"
#include "options.h"
#include "project_driver.h"
#include "test_runner.h"

#include "Visual/XSharp/compiler_core.hh"
#include "Visual/C23/diagnostic.hh"
#include "Visual/XSharp/hir/cffi.h"
#include "Visual/XSharp/hir/expression_check.h"
#include "Visual/XSharp/hir/inheritance.h"
#include "Visual/XSharp/hir/module_registry.h"
#include "Visual/XSharp/hir/symbol_table.h"
#include "Visual/XSharp/hir/type_resolution.h"
#include "Visual/XSharp/macro.h"
#include "Visual/C23/source_include.h"
#include "Visual/XSharp/syntax_ast.hh"
#include "Visual/XSharp/syntax_parser.h"

#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef XS_PROJECT_VERSION
#    define XS_PROJECT_VERSION "0.2.5"
#endif

static char *copy_text(const char *text)
{
    size_t length = strlen(text);
    char *copy = malloc(length + 1);
    if(copy != nullptr)
        memcpy(copy, text, length + 1);
    return copy;
}

static char *read_file(const char *path, size_t *length);

static bool has_suffix(const char *text, const char *suffix)
{
    size_t text_length = strlen(text);
    size_t suffix_length = strlen(suffix);
    return text_length >= suffix_length && strcmp(text + text_length - suffix_length, suffix) == 0;
}

static char *rooted_path(const char *root, const char *source_path)
{
    bool absolute = source_path[0] == '/' || source_path[0] == '\\' ||
                    (isalpha((unsigned char)source_path[0]) && source_path[1] == ':' &&
                     (source_path[2] == '/' || source_path[2] == '\\'));
    if(absolute)
    {
        size_t length = strlen(source_path);
        char *result = malloc(length + 1);
        if(result != nullptr)
            memcpy(result, source_path, length + 1);
        return result;
    }
    size_t root_length = strlen(root);
    bool needs_separator = root_length != 0U && root[root_length - 1U] != '/';
    size_t source_length = strlen(source_path);
    char *result = malloc(root_length + (needs_separator ? 1U : 0U) + source_length + 1U);
    if(result == nullptr)
        return nullptr;
    memcpy(result, root, root_length);
    size_t offset = root_length;
    if(needs_separator)
        result[offset++] = '/';
    memcpy(result + offset, source_path, source_length + 1U);
    return result;
}

typedef struct
{
    char *path;
    char *module_name;
    char *text;
    XsSource source;
    XsDiagnostics diagnostics;
    XsSyntaxTree tree;
    XsMacroStatementExpansionSet macro_statements;
    XsMacroDeclarationExpansionSet macro_declarations;
    XsCompilerCoreSession *compiler_core;
    XsHirImportScope import;
    bool diagnostics_initialized;
    bool tree_initialized;
    bool macro_statements_initialized;
    bool macro_declarations_initialized;
    bool imports_initialized;
    bool hir_ready;
} CompilationUnit;

static void compilation_unit_free(CompilationUnit *unit)
{
    xslang_compiler_core_session_free(unit->compiler_core);
    if(unit->imports_initialized)
        xs_hir_import_scope_free(&unit->import);
    if(unit->macro_declarations_initialized)
        xs_macro_declaration_expansion_set_free(&unit->macro_declarations);
    if(unit->macro_statements_initialized)
        xs_macro_statement_expansion_set_free(&unit->macro_statements);
    if(unit->tree_initialized)
        xs_syntax_tree_free(&unit->tree);
    if(unit->diagnostics_initialized)
        xs_diagnostics_free(&unit->diagnostics);
    free(unit->text);
    free(unit->module_name);
    free(unit->path);
    *unit = (CompilationUnit){0};
}

static bool unit_path_exists(const CompilationUnit *units, size_t count, const char *path)
{
    for(size_t i = 0; i < count; ++i)
    {
        if(strcmp(units[i].path, path) == 0)
            return true;
    }
    return false;
}

static bool append_compilation_unit(CompilationUnit **units, size_t *count, size_t *capacity, char *path,
                                    const char *module_name)
{
    if(unit_path_exists(*units, *count, path))
    {
        free(path);
        return true;
    }
    if(*count == *capacity)
    {
        size_t new_capacity = *capacity == 0 ? 8 : *capacity * 2;
        CompilationUnit *grown = realloc(*units, new_capacity * sizeof(*grown));
        if(grown == nullptr)
        {
            free(path);
            return false;
        }
        *units = grown;
        *capacity = new_capacity;
    }
    char *module_copy = module_name == nullptr ? nullptr : copy_text(module_name);
    if(module_name != nullptr && module_copy == nullptr)
    {
        free(path);
        return false;
    }
    (*units)[(*count)++] = (CompilationUnit){.path = path, .module_name = module_copy};
    return true;
}

static bool import_compiler_core_syntax(CompilationUnit *unit)
{
    XsSpan root_span = {.start = unit->tree.root->span.start_offset, .end = unit->tree.root->span.end_offset};
    XsSyntaxTree expanded = {0};
    if(!xs_macro_materialize_expanded_tree(&unit->tree, &unit->macro_declarations, &unit->macro_statements,
                                           &unit->diagnostics, &expanded))
        return false;
    XsCompilerCoreSyntaxStorage *storage = nullptr;
    XsCompilerCoreStatus packet_status = xs_compiler_core_syntax_packet_create(&expanded, &storage);
    if(packet_status != XS_COMPILER_CORE_OK)
    {
        xs_syntax_tree_free(&expanded);
        return xs_diagnostics_add(&unit->diagnostics, XS_DIAGNOSTIC_ERROR, root_span,
                                  "compiler-core syntax packet could not be created") &&
               false;
    }
    const XsCompilerCoreSyntaxPacket *packet = xs_compiler_core_syntax_packet(storage);
    XsCompilerCoreFfiStatus import_status =
        unit->module_name == nullptr
            ? xslang_compiler_core_session_create(packet, &unit->compiler_core)
            : xslang_compiler_core_session_create_in_module(packet, (const uint8_t *)unit->module_name,
                                                            (uint64_t)strlen(unit->module_name), &unit->compiler_core);
    xs_compiler_core_syntax_packet_free(storage);
    xs_syntax_tree_free(&expanded);
    if(import_status == XS_COMPILER_CORE_FFI_OK)
        return true;
    return xs_diagnostics_add(&unit->diagnostics, XS_DIAGNOSTIC_ERROR, root_span,
                              "Rust compiler core rejected the expanded structural AST packet") &&
           false;
}

static bool parse_compilation_unit(CompilationUnit *unit, uint64_t file_id, XsHirSymbolTable *symbols,
                                   const XsCompilerSettings *settings)
{
    size_t length = 0;
    unit->text = read_file(unit->path, &length);
    if(unit->text == nullptr)
    {
        fprintf(stderr, "vxs: source file '%s' could not be read\n", unit->path);
        return false;
    }
    xs_diagnostics_init(&unit->diagnostics);
    xs_diagnostics_set_warning_policy(&unit->diagnostics, settings->warning_level, settings->warnings_as_errors);
    unit->diagnostics_initialized = true;
    unit->source =
        (XsSource){.path = unit->path, .module_name = unit->module_name, .text = unit->text, .length = length};
    xs_hir_import_scope_init(&unit->import);
    unit->imports_initialized = true;
    bool success = xs_syntax_parse(&unit->source, file_id, &unit->diagnostics, &unit->tree);
    unit->tree_initialized = true;
    XsIncludedSource included = {0};
    if(success)
        success = xs_source_expand_include_macros(&unit->tree, &unit->diagnostics, &included);
    if(success)
    {
        xs_syntax_tree_free(&unit->tree);
        free(unit->text);
        unit->text = included.text;
        included.text = nullptr;
        unit->source = (XsSource){
            .path = unit->path, .module_name = unit->module_name, .text = unit->text, .length = included.length};
        success = xs_syntax_parse(&unit->source, file_id, &unit->diagnostics, &unit->tree);
    }
    xs_included_source_free(&included);
    if(success)
        success = xs_macro_validate(&unit->tree, &unit->diagnostics);
    if(success)
    {
        XsMacroExpansionReport macro_report;
        success = xs_macro_prepare_expansion(&unit->tree, &unit->diagnostics, &macro_report);
    }
    if(success)
    {
        success = xs_macro_expand_statements(&unit->tree, &unit->diagnostics, &unit->macro_statements);
        unit->macro_statements_initialized = success;
    }
    if(success)
        success = xs_macro_expand_declarations(&unit->tree, &unit->diagnostics, &unit->macro_declarations);
    unit->macro_declarations_initialized = success;
    if(success)
        success = import_compiler_core_syntax(unit);
    if(success)
        success = xs_hir_collect_symbols_in_module_expanded(&unit->tree, &unit->macro_declarations, unit->module_name,
                                                            symbols, &unit->diagnostics);
    unit->hir_ready = success;
    return success;
}

static bool emit_requested_output(XsBuildOutput output, const XsCompilerCoreSession *session, const char *input_path,
                                  XsDiagnostics *diagnostics, XsSpan span)
{
    (void)session;
    (void)input_path;
    if(output == XS_BUILD_OUTPUT_NONE)
        return true;
    (void)xs_diagnostics_add(diagnostics, XS_DIAGNOSTIC_ERROR, span,
                             "requested emission stage is not connected to the renewed Core/Xpp/Xmm pipeline yet");
    return false;
}

static char *read_file(const char *path, size_t *length)
{
    FILE *file = fopen(path, "rb");
    if(file == nullptr)
        return nullptr;
    if(fseek(file, 0, SEEK_END) != 0)
    {
        fclose(file);
        return nullptr;
    }
    long size = ftell(file);
    if(size < 0 || fseek(file, 0, SEEK_SET) != 0)
    {
        fclose(file);
        return nullptr;
    }
    char *text = (char *)malloc((size_t)size + 1);
    if(text == nullptr)
    {
        fclose(file);
        return nullptr;
    }
    size_t read = fread(text, 1, (size_t)size, file);
    fclose(file);
    if(read != (size_t)size)
    {
        free(text);
        return nullptr;
    }
    text[read] = '\0';
    *length = read;
    return text;
}

static bool check_compilation_unit_semantics(CompilationUnit *unit, XsHirSymbolTable *symbols)
{
    if(!unit->hir_ready)
        return false;
    bool success = xs_hir_resolve_imports(&unit->tree, symbols, &unit->import, &unit->diagnostics);
    success = xs_hir_validate_cffi(&unit->tree, &unit->diagnostics) && success;
    success = xs_hir_validate_name_uses_with_macros(&unit->tree, &unit->macro_declarations, &unit->macro_statements,
                                                    symbols, &unit->import, &unit->diagnostics) &&
              success;
    success = xs_hir_resolve_types_with_macros(&unit->tree, &unit->macro_declarations, &unit->macro_statements, symbols,
                                               &unit->import, &unit->diagnostics) &&
              success;
    success = xs_hir_validate_inheritance(&unit->tree, symbols, &unit->import, &unit->diagnostics) && success;
    return xs_hir_check_expression_types_with_macros(&unit->tree, &unit->macro_declarations, &unit->macro_statements,
                                                     &unit->diagnostics) &&
           success;
}

static bool check_single_source_file(const char *path, XsBuildOutput output, bool build_native, bool run_tests,
                                     const XsCompilerSettings *settings)
{
    CompilationUnit unit = {.path = copy_text(path)};
    if(unit.path == nullptr)
    {
        fprintf(stderr, "vxs: out of memory while preparing source file '%s'\n", path);
        return false;
    }
    XsHirSymbolTable symbols;
    xs_hir_symbol_table_init(&symbols);
    bool success = parse_compilation_unit(&unit, 1, &symbols, settings);
    if(success)
        success = check_compilation_unit_semantics(&unit, &symbols);
    if(success && output != XS_BUILD_OUTPUT_NONE)
    {
        XsSpan span = {.start = unit.tree.root->span.start_offset, .end = unit.tree.root->span.end_offset};
        success = emit_requested_output(output, unit.compiler_core, path, &unit.diagnostics, span);
    }
    if(success && build_native)
    {
        if(xs_driver_compiler_core_native_available(unit.compiler_core))
        {
            XsSpan span = {.start = unit.tree.root->span.start_offset, .end = unit.tree.root->span.end_offset};
            success = xs_driver_build_compiler_core_native(path, unit.compiler_core, &unit.diagnostics, span);
        }
        else
        {
            XsSpan span = {.start = unit.tree.root->span.start_offset, .end = unit.tree.root->span.end_offset};
            if(!xs_driver_append_compiler_core_diagnostics(unit.compiler_core, &unit.diagnostics, span))
                (void)xs_diagnostics_add(
                    &unit.diagnostics, XS_DIAGNOSTIC_ERROR, span,
                    "Rust compiler core does not yet support this source body for native emission");
            success = false;
        }
    }
    if(success && run_tests)
        success = xs_driver_run_compiler_core_tests(unit.compiler_core) == 0;
    xs_diagnostics_print(&unit.diagnostics, &unit.source, stderr);
    xs_hir_symbol_table_free(&symbols);
    compilation_unit_free(&unit);
    return success;
}

static XsCompilerCoreSession *merge_compiler_core_sessions(CompilationUnit *units, size_t unit_count)
{
    const XsCompilerCoreSession **sessions = calloc(unit_count, sizeof(*sessions));
    if(sessions == nullptr)
        return nullptr;
    for(size_t i = 0; i < unit_count; ++i)
        sessions[i] = units[i].compiler_core;
    XsCompilerCoreSession *merged = nullptr;
    XsCompilerCoreFfiStatus status = xslang_compiler_core_session_merge(sessions, unit_count, &merged);
    free(sessions);
    return status == XS_COMPILER_CORE_FFI_OK ? merged : nullptr;
}

static size_t project_entry_unit_index(const CompilationUnit *units, size_t unit_count,
                                       const XsHirSymbolTable *symbols, const char *entry)
{
    if(entry == nullptr)
        return 0U;
    const XsHirSymbol *entry_symbol = xs_hir_symbol_table_find(symbols, entry);
    if(entry_symbol == nullptr || entry_symbol->kind != XS_HIR_SYMBOL_CLASS)
        return 0U;
    for(size_t i = 0; i < unit_count; ++i)
        if(units[i].tree.file_id == entry_symbol->span.file_id)
            return i;
    return 0U;
}

static bool check_project_sources(const char *root, const char *const *direct, size_t direct_count,
                                  XsBuildOutput output, bool build_native, bool run_tests,
                                  const XsCompilerSettings *settings, const char *entry, char **artifact_source)
{
    if(artifact_source != nullptr)
        *artifact_source = nullptr;
    if(build_native && direct_count == 0U)
    {
        fprintf(stderr, "vxs: project native build requires at least one selected source\n");
        return false;
    }

    XsModuleRegistry registry;
    XsModuleGraph graph;
    XsModuleIssues issues;
    xs_module_registry_init(&registry);
    xs_module_graph_init(&graph);
    xs_module_issues_init(&issues);
    bool success = xs_module_registry_discover(root, &registry, &issues);
    if(success)
        success = xs_module_graph_resolve(root, direct, direct_count, &registry, &graph, &issues);
    xs_module_issues_print(&issues);

    CompilationUnit *units = nullptr;
    size_t unit_count = 0;
    size_t unit_capacity = 0;
    for(size_t i = 0; i < direct_count; ++i)
    {
        char *path = rooted_path(root, direct[i]);
        if(path == nullptr || !append_compilation_unit(&units, &unit_count, &unit_capacity, path, nullptr))
            success = false;
    }
    for(size_t i = 0; i < graph.count; ++i)
    {
        bool already_direct = false;
        for(size_t direct_index = 0; direct_index < direct_count; ++direct_index)
            if(strcmp(graph.dependencies[i].imported_path, direct[direct_index]) == 0)
                already_direct = true;
        if(already_direct)
            continue;
        char *path = copy_text(graph.dependencies[i].imported_path);
        if(path == nullptr || !append_compilation_unit(&units, &unit_count, &unit_capacity, path, nullptr))
            success = false;
    }

    XsHirSymbolTable symbols;
    xs_hir_symbol_table_init(&symbols);
    uint64_t file_id = 1;
    for(size_t i = 0; i < unit_count; ++i)
    {
        success = parse_compilation_unit(&units[i], file_id++, &symbols, settings) && success;
    }
    const size_t primary = project_entry_unit_index(units, unit_count, &symbols, entry);
    if(success)
    {
        for(size_t i = 0; i < unit_count; ++i)
        {
            if(units[i].hir_ready)
            {
                success = check_compilation_unit_semantics(&units[i], &symbols) && success;
            }
        }
    }
    XsCompilerCoreSession *merged = nullptr;
    const XsCompilerCoreSession *program_session = nullptr;
    if(success && (output != XS_BUILD_OUTPUT_NONE || build_native || run_tests))
    {
        success = unit_count != 0;
        merged = success && unit_count > 1 ? merge_compiler_core_sessions(units, unit_count) : nullptr;
        program_session = unit_count == 1 ? units[0].compiler_core : merged;
        if(success && unit_count > 1 && merged == nullptr)
        {
            XsSpan span = {.start = units[0].tree.root->span.start_offset, .end = units[0].tree.root->span.end_offset};
            success = xs_diagnostics_add(&units[0].diagnostics, XS_DIAGNOSTIC_ERROR, span,
                                         "Rust compiler core could not merge the project source sessions") &&
                      false;
        }
    }
    if(success && output != XS_BUILD_OUTPUT_NONE)
    {
        XsSpan span = {.start = units[primary].tree.root->span.start_offset,
                       .end = units[primary].tree.root->span.end_offset};
        success = emit_requested_output(output, program_session, units[primary].path, &units[primary].diagnostics, span);
    }
    if(success && build_native)
    {
        if(xs_driver_compiler_core_native_available(program_session))
        {
            XsSpan span = {.start = units[primary].tree.root->span.start_offset,
                           .end = units[primary].tree.root->span.end_offset};
            success = xs_driver_build_compiler_core_native(units[primary].path, program_session,
                                                           &units[primary].diagnostics, span);
        }
        else
        {
            XsSpan span = {.start = units[primary].tree.root->span.start_offset,
                           .end = units[primary].tree.root->span.end_offset};
            if(!xs_driver_append_compiler_core_diagnostics(program_session, &units[primary].diagnostics, span))
                (void)xs_diagnostics_add(
                    &units[primary].diagnostics, XS_DIAGNOSTIC_ERROR, span,
                    "Rust compiler core does not yet support this project body for native emission");
            success = false;
        }
    }
    if(success && run_tests)
    {
        XsSpan span = {.start = units[primary].tree.root->span.start_offset,
                       .end = units[primary].tree.root->span.end_offset};
        if(xs_driver_append_compiler_core_diagnostics(program_session, &units[primary].diagnostics, span))
            success = false;
        else
            success = xs_driver_run_compiler_core_tests(program_session) == 0;
    }
    if(success && artifact_source != nullptr)
    {
        *artifact_source = copy_text(units[primary].path);
        success = *artifact_source != nullptr;
    }
    xslang_compiler_core_session_free(merged);
    for(size_t i = 0; i < unit_count; ++i)
        xs_diagnostics_print(&units[i].diagnostics, &units[i].source, stderr);

    xs_hir_symbol_table_free(&symbols);
    for(size_t i = 0; i < unit_count; ++i)
        compilation_unit_free(&units[i]);
    free(units);
    xs_module_issues_free(&issues);
    xs_module_graph_free(&graph);
    xs_module_registry_free(&registry);
    return success;
}

static int run_project_command(const XsCliOptions *options)
{
    XsResolvedProject resolved;
    bool testing = strcmp(options->command, "test") == 0;
    bool resolved_ok = testing ? xs_driver_resolve_project_tests(&resolved) : xs_driver_resolve_project(&resolved);
    if(!resolved_ok)
        return 1;
    xs_cli_apply_compiler_overrides(options, &resolved.settings);
    XsBuildOutput output = strcmp(options->command, "build") == 0
                               ? (options->output_override ? options->output : resolved.output)
                               : XS_BUILD_OUTPUT_BINARY;
    size_t selected_count = resolved.path_count + (testing ? resolved.test_path_count : 0U);
    const char **direct = selected_count == 0U ? nullptr : calloc(selected_count, sizeof(*direct));
    if(selected_count != 0U && direct == nullptr)
    {
        free(direct);
        xs_driver_free_project(&resolved);
        return 1;
    }
    size_t selected = 0;
    for(size_t i = 0; i < resolved.path_count; ++i)
    {
        direct[selected] = resolved.paths[i];
        ++selected;
    }
    if(testing)
    {
        for(size_t i = 0; i < resolved.test_path_count; ++i)
        {
            direct[selected] = resolved.test_paths[i];
            ++selected;
        }
    }
    bool build_native = (strcmp(options->command, "build") == 0 || strcmp(options->command, "run") == 0) &&
                        output == XS_BUILD_OUTPUT_BINARY;
    bool running = strcmp(options->command, "run") == 0;
    char *artifact_source = nullptr;
    bool success = check_project_sources(".", direct, selected_count, output, build_native, testing,
                                         &resolved.settings, resolved.entry, running ? &artifact_source : nullptr);
    if(success && testing && selected_count == 0U)
        fprintf(stderr, "vxs: test result: ok. 0 passed; 0 failed; 0 ignored\n");
    if(success && running)
    {
        int exit_code = xs_driver_run_native_artifact(artifact_source);
        free(artifact_source);
        free(direct);
        xs_driver_free_project(&resolved);
        return exit_code;
    }
    free(artifact_source);
    free(direct);
    xs_driver_free_project(&resolved);
    return success ? 0 : 1;
}

static int run_file_command(const XsCliOptions *options)
{
    if(options->input != XS_BUILD_INPUT_VXS)
    {
        fprintf(stderr, "vxs: selected -Build input is not connected to the renewed pipeline yet\n");
        return 1;
    }
    XsCompilerSettings settings = xs_cli_default_compiler_settings();
    xs_cli_apply_compiler_overrides(options, &settings);
    if(!has_suffix(options->file_path, ".vxs"))
    {
        fprintf(stderr, "vxs: Visual X# source file '%s' must use the '.vxs' extension\n", options->file_path);
        return 2;
    }
    bool success =
        check_single_source_file(options->file_path, options->output,
                                 (strcmp(options->command, "build") == 0 || strcmp(options->command, "run") == 0) &&
                                     options->output == XS_BUILD_OUTPUT_NONE,
                                 strcmp(options->command, "test") == 0, &settings);
    if(!success)
        return 1;
    return strcmp(options->command, "run") == 0 ? xs_driver_run_native_artifact(options->file_path) : 0;
}

int xs_driver_main(int argc, char **argv)
{
    XsCliOptions options = {0};
    XsCliParseResult parse_result = xs_cli_parse(argc, argv, &options);
    if(parse_result == XS_CLI_PARSE_EXIT)
        return 0;
    if(parse_result == XS_CLI_PARSE_ERROR)
        return 2;

    int exit_code = 0;
    if(strcmp(options.command, "resolve") == 0 || strcmp(options.command, "update") == 0)
        exit_code = xs_driver_refresh_lock() ? 0 : 1;
    else if(strcmp(options.command, "install") == 0 || strcmp(options.command, "viget") == 0)
    {
        fprintf(stderr, "vxs: %s requires the ViGet client, which is not linked into this compiler build yet\n",
                options.command);
        exit_code = 1;
    }
    else if(options.file_path != nullptr)
        exit_code = run_file_command(&options);
    else
        exit_code = run_project_command(&options);
    xs_cli_options_free(&options);
    return exit_code;
}
