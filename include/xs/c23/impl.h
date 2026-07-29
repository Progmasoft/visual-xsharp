/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 */

#ifndef XS_C23_IMPL_H
#define XS_C23_IMPL_H

#include "xs/c23/trait.h"

#define XS_C23_DETAIL_IMPL_NAME_RAW(trait_name, type_name) xs_c23_impl_##trait_name##_for_##type_name
#define XS_C23_DETAIL_IMPL_NAME(trait_name, type_name) XS_C23_DETAIL_IMPL_NAME_RAW(trait_name, type_name)

/* Defines one translation-unit-local implementation of a trait for a type. */
#define XS_C23_IMPL_FOR(trait_name, type_name, ...)                                                                    \
  static const trait_name##VTable XS_C23_DETAIL_IMPL_NAME(trait_name, type_name) = {__VA_ARGS__}

/* Erases a typed object pointer into the selected trait implementation. */
#define XS_C23_AS_TRAIT(trait_name, type_name, object_pointer)                                                         \
  ((trait_name){.self = (object_pointer), .implementation = &XS_C23_DETAIL_IMPL_NAME(trait_name, type_name)})

#endif
