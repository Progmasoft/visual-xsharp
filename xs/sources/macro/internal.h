/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 */

#ifndef XS_MACRO_INTERNAL_H
#define XS_MACRO_INTERNAL_H

#include "xs/macro.h"

bool xs_macro_text_equal(XsText left, XsText right);
bool xs_macro_token_text_matches(const XsSyntaxNode *matcher, const XsSyntaxNode *argument);
bool xs_macro_fragment_kind_is(const XsSyntaxNode *fragment, const char *kind);
bool xs_macro_fragment_supported(XsText kind);
bool xs_macro_fragment_is_sequence(const XsSyntaxNode *fragment);
bool xs_macro_fragment_matches(const XsSyntaxNode *fragment, const XsSyntaxNode *argument);
bool xs_macro_external_import_resolves(const XsSyntaxTree *tree, XsText name);

#endif
