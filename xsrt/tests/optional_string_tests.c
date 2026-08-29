/*
 * SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
 * SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.0
 */

#include <Visual/C23/runtime.h>

#include <stdio.h>

static int failures = 0;

static void check(bool condition, const char *message)
{
    if(condition)
        return;
    fprintf(stderr, "check failed: %s\n", message);
    ++failures;
}

static void test_none(void)
{
    XsRuntimeOptionalStr value = xs_runtime_optional_str_none();
    XsRuntimeStrView view = {0};
    check(!xs_runtime_optional_str_is_some(&value), "None has no payload");
    check(xs_runtime_optional_str_borrow(&value, &view) == XS_RUNTIME_VALUE_IS_NONE, "None cannot be borrowed as Str");
    xs_runtime_optional_str_drop(&value);
}

static void test_some_and_clone(void)
{
    const uint16_t source[] = {0x004cU, 0x0065U, 0x0069U, 0x0074U, 0x0077U, 0x006fU, 0x006cU, 0x0066U};
    XsRuntimeOptionalStr value = xs_runtime_optional_str_none();
    check(xs_runtime_optional_str_some((XsRuntimeStrView){.units = source, .length = 8U}, &value) == XS_RUNTIME_OK,
          "Some copies a UTF-16 payload");
    check(xs_runtime_optional_str_is_some(&value), "Some has a payload");

    XsRuntimeStrView view = {0};
    check(xs_runtime_optional_str_borrow(&value, &view) == XS_RUNTIME_OK, "Some can be borrowed");
    check(view.length == 8U && view.units != source, "owned payload has independent storage");
    check(view.units[0] == 0x004cU && view.units[7] == 0x0066U, "owned payload preserves code units");

    XsRuntimeOptionalStr clone = xs_runtime_optional_str_none();
    check(xs_runtime_optional_str_clone(&value, &clone) == XS_RUNTIME_OK, "Some can be cloned");
    XsRuntimeStrView clone_view = {0};
    check(xs_runtime_optional_str_borrow(&clone, &clone_view) == XS_RUNTIME_OK, "clone can be borrowed");
    check(clone_view.units != view.units && clone_view.length == view.length, "clone is a deep copy");

    xs_runtime_optional_str_drop(&value);
    check(xs_runtime_optional_str_is_some(&clone), "clone survives original drop");
    xs_runtime_optional_str_drop(&clone);
}

static void test_empty_some(void)
{
    XsRuntimeOptionalStr value = xs_runtime_optional_str_none();
    check(xs_runtime_optional_str_some((XsRuntimeStrView){0}, &value) == XS_RUNTIME_OK, "empty Some is constructible");
    check(xs_runtime_optional_str_is_some(&value), "empty Some remains distinct from None");
    XsRuntimeStrView view = {0};
    check(xs_runtime_optional_str_borrow(&value, &view) == XS_RUNTIME_OK && view.length == 0U,
          "empty Some borrows as an empty Str");
    xs_runtime_optional_str_drop(&value);
}

int main(void)
{
    test_none();
    test_some_and_clone();
    test_empty_some();
    return failures == 0 ? 0 : 1;
}
