/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 */

#ifndef XS_COMPILER_CORE_H
#define XS_COMPILER_CORE_H

#include "xs/syntax_ast.h"

#include <stdint.h>

#define XS_COMPILER_CORE_SYNTAX_ABI_VERSION 0U
#define XS_COMPILER_CORE_NO_NODE UINT64_MAX

typedef struct
{
    uint32_t kind;
    uint32_t token_kind;
    uint32_t visibility;
    uint32_t flags;
    uint64_t parent_index;
    uint64_t first_child;
    uint64_t child_count;
    uint64_t text_offset;
    uint64_t text_length;
    uint64_t file_id;
    uint64_t start_offset;
    uint64_t end_offset;
    uint64_t start_line;
    uint64_t start_column;
    uint64_t end_line;
    uint64_t end_column;
} XsCompilerCoreSyntaxNode;

typedef struct
{
    uint32_t abi_version;
    uint32_t reserved;
    uint64_t root_index;
    const XsCompilerCoreSyntaxNode *nodes;
    uint64_t node_count;
    const uint64_t *child_indices;
    uint64_t child_index_count;
    const uint8_t *text_bytes;
    uint64_t text_byte_count;
} XsCompilerCoreSyntaxPacket;

typedef enum
{
    XS_COMPILER_CORE_OK,
    XS_COMPILER_CORE_INVALID_ARGUMENT,
    XS_COMPILER_CORE_ALLOCATION_FAILED,
    XS_COMPILER_CORE_LIMIT_EXCEEDED,
} XsCompilerCoreStatus;

typedef enum
{
    XS_COMPILER_CORE_FFI_OK,
    XS_COMPILER_CORE_FFI_NULL_ARGUMENT,
    XS_COMPILER_CORE_FFI_INVALID_PACKET,
} XsCompilerCoreFfiStatus;

typedef struct XsCompilerCoreSyntaxStorage XsCompilerCoreSyntaxStorage;
typedef struct XsCompilerCoreSession XsCompilerCoreSession;
typedef struct XsCompilerCoreDirectIrSession XsCompilerCoreDirectIrSession;

typedef enum
{
    XS_COMPILER_CORE_TEST_IGNORED = 1U << 0,
    XS_COMPILER_CORE_TEST_SHOULD_PANIC = 1U << 1,
} XsCompilerCoreTestFlags;

XsCompilerCoreStatus xs_compiler_core_syntax_packet_create(const XsSyntaxTree *tree,
                                                           XsCompilerCoreSyntaxStorage **storage);
const XsCompilerCoreSyntaxPacket *xs_compiler_core_syntax_packet(const XsCompilerCoreSyntaxStorage *storage);
void xs_compiler_core_syntax_packet_free(XsCompilerCoreSyntaxStorage *storage);

XsCompilerCoreFfiStatus xslang_compiler_core_session_create(const XsCompilerCoreSyntaxPacket *packet,
                                                            XsCompilerCoreSession **session);
XsCompilerCoreFfiStatus xslang_compiler_core_session_create_in_module(const XsCompilerCoreSyntaxPacket *packet,
                                                                      const uint8_t *module_name,
                                                                      uint64_t module_name_length,
                                                                      XsCompilerCoreSession **session);
XsCompilerCoreFfiStatus xslang_compiler_core_session_merge(const XsCompilerCoreSession *const *sessions,
                                                           uint64_t session_count, XsCompilerCoreSession **merged);
uint64_t xslang_compiler_core_session_syntax_node_count(const XsCompilerCoreSession *session);
uint64_t xslang_compiler_core_session_function_count(const XsCompilerCoreSession *session);
uint64_t xslang_compiler_core_session_mir_function_count(const XsCompilerCoreSession *session);
uint64_t xslang_compiler_core_session_test_count(const XsCompilerCoreSession *session);
const uint8_t *xslang_compiler_core_session_test_name(const XsCompilerCoreSession *session, uint64_t index,
                                                      uint64_t *length);
uint32_t xslang_compiler_core_session_test_flags(const XsCompilerCoreSession *session, uint64_t index);
XsCompilerCoreFfiStatus xslang_compiler_core_test_harness_create(const XsCompilerCoreSession *session, uint64_t index,
                                                                 XsCompilerCoreSession **harness);
uint64_t xslang_compiler_core_session_diagnostic_count(const XsCompilerCoreSession *session);
const uint8_t *xslang_compiler_core_session_diagnostic_text(const XsCompilerCoreSession *session, uint64_t index,
                                                            uint64_t *length);
const uint8_t *xslang_compiler_core_session_xhir_text(const XsCompilerCoreSession *session, uint64_t *length);
const uint8_t *xslang_compiler_core_session_xmir_text(const XsCompilerCoreSession *session, uint64_t *length);
const uint8_t *xslang_compiler_core_session_xlil_text(const XsCompilerCoreSession *session, uint64_t *length);
void xslang_compiler_core_session_free(XsCompilerCoreSession *session);

XsCompilerCoreFfiStatus xslang_compiler_core_direct_xmir_create(const uint8_t *text, uint64_t length,
                                                                XsCompilerCoreDirectIrSession **session);
XsCompilerCoreFfiStatus xslang_compiler_core_direct_xhir_create(const uint8_t *text, uint64_t length,
                                                                XsCompilerCoreDirectIrSession **session);
uint64_t xslang_compiler_core_direct_ir_diagnostic_count(const XsCompilerCoreDirectIrSession *session);
const uint8_t *xslang_compiler_core_direct_ir_diagnostic_text(const XsCompilerCoreDirectIrSession *session,
                                                              uint64_t index, uint64_t *length);
const uint8_t *xslang_compiler_core_direct_ir_xlil_text(const XsCompilerCoreDirectIrSession *session, uint64_t *length);
void xslang_compiler_core_direct_ir_free(XsCompilerCoreDirectIrSession *session);

#endif
