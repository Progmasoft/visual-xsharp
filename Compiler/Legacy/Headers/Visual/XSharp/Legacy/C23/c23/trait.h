/*
 * SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
 * SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.0
 *
 */

#ifndef XS_C23_TRAIT_H
#define XS_C23_TRAIT_H

/*
 * Declares a named, object-safe C23 trait. Method declarations receive the
 * erased object pointer as their first parameter.
 *
 * XS_C23_TRAIT(Renderable, int (*render)(void *self, int width););
 */
#define XS_C23_TRAIT(name, ...)               \
    typedef struct name##VTable name##VTable; \
    typedef struct name name;                 \
    struct name##VTable                       \
    {                                         \
        __VA_ARGS__                           \
    };                                        \
    struct name                               \
    {                                         \
        void *self;                           \
        const name##VTable *implementation;   \
    }

/* Calls one method through a trait object. */
#define XS_C23_TRAIT_CALL(object, method, ...) \
    ((object).implementation->method((object).self __VA_OPT__(, ) __VA_ARGS__))

#endif
