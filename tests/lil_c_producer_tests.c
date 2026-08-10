/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 */

#include "Visual/XSharp/lil.hh"

#include <stdio.h>
#include <string.h>

static int failures;

#define CHECK(condition)                                                                                               \
    do                                                                                                                 \
    {                                                                                                                  \
        if(!(condition))                                                                                               \
        {                                                                                                              \
            fprintf(stderr, "%s:%d: check failed: %s\n", __FILE__, __LINE__, #condition);                              \
            ++failures;                                                                                                \
        }                                                                                                              \
    } while(0)

static bool add_registry(XsLilModule *module, XsLilType *pair, XsLilType *fixed, XsLilType *dynamic, XsLilError *error)
{
    XsLilType i32 = xs_lil_scalar_type(XS_LIL_TYPE_I32);
    const XsLilType pair_fields[] = {i32, i32};
    if(xs_lil_module_add_aggregate_type(module, "Pair", pair_fields, 2, pair, error) != XS_LIL_OK ||
       xs_lil_module_add_array_type(module, i32, 2, fixed, error) != XS_LIL_OK ||
       xs_lil_module_add_dynamic_array_type(module, i32, dynamic, error) != XS_LIL_OK)
        return false;

    const XsLilType add_parameters[] = {i32, i32};
    XsLilType str = xs_lil_scalar_type(XS_LIL_TYPE_STR);
    return xs_lil_module_add_function(module, "add", i32, add_parameters, 2, error) == XS_LIL_OK &&
           xs_lil_module_add_function(module, "sink", xs_lil_scalar_type(XS_LIL_TYPE_VOID), &str, 1, error) ==
               XS_LIL_OK;
}

static bool add_definitions(XsLilModule *module, XsLilError *error)
{
    XsLilFunction *function = nullptr;
    XsLilType i32 = xs_lil_scalar_type(XS_LIL_TYPE_I32);
    if(xs_lil_module_add_function_definition(module, "main", i32, nullptr, 0, &function, error) != XS_LIL_OK ||
       xs_lil_module_add_function_definition(module, "trap", xs_lil_scalar_type(XS_LIL_TYPE_VOID), nullptr, 0,
                                             &function, error) != XS_LIL_OK)
        return false;
    XsLilBlock *trap = nullptr;
    return xs_lil_function_append_block(function, "entry", &trap, error) == XS_LIL_OK &&
           xs_lil_block_set_panic(trap, error) == XS_LIL_OK;
}

static bool build_main(XsLilModule *module, XsLilType pair, XsLilType fixed, XsLilType dynamic, XsLilError *error)
{
    XsLilFunction *main_function = xs_lil_module_function_named_mut(module, "main");
    XsLilBlock *entry = nullptr;
    XsLilBlock *accepted = nullptr;
    XsLilBlock *rejected = nullptr;
    if(main_function == nullptr || xs_lil_function_append_block(main_function, "entry", &entry, error) != XS_LIL_OK ||
       xs_lil_function_append_block(main_function, "accepted", &accepted, error) != XS_LIL_OK ||
       xs_lil_function_append_block(main_function, "rejected", &rejected, error) != XS_LIL_OK)
        return false;

    XsLilSlotId slot = XS_LIL_INVALID_SLOT_ID;
    XsLilValueId two = XS_LIL_INVALID_VALUE_ID;
    XsLilValueId three = XS_LIL_INVALID_VALUE_ID;
    XsLilValueId sum = XS_LIL_INVALID_VALUE_ID;
    XsLilValueId loaded = XS_LIL_INVALID_VALUE_ID;
    XsLilValueId condition = XS_LIL_INVALID_VALUE_ID;
    XsLilType i32 = xs_lil_scalar_type(XS_LIL_TYPE_I32);
    if(xs_lil_function_add_slot(main_function, i32, &slot, error) != XS_LIL_OK ||
       xs_lil_block_add_const_i32(entry, 2, &two, error) != XS_LIL_OK ||
       xs_lil_block_add_const_i32(entry, 3, &three, error) != XS_LIL_OK)
        return false;
    const XsLilValueId actual_call_arguments[] = {two, three};
    if(xs_lil_block_add_call(entry, "add", i32, actual_call_arguments, 2, &sum, error) != XS_LIL_OK ||
       xs_lil_block_add_store(entry, slot, sum, error) != XS_LIL_OK ||
       xs_lil_block_add_load(entry, slot, &loaded, error) != XS_LIL_OK ||
       xs_lil_block_add_const_bool(entry, true, &condition, error) != XS_LIL_OK)
        return false;

    static const uint32_t message[] = {0x004FU, 0x004BU};
    XsLilValueId string = XS_LIL_INVALID_VALUE_ID;
    if(xs_lil_block_add_const_str(entry, XS_LIL_UTF32_LE, message, 2, &string, error) != XS_LIL_OK ||
       xs_lil_block_add_void_call(entry, "sink", &string, 1, error) != XS_LIL_OK)
        return false;

    XsLilValueId float_left = XS_LIL_INVALID_VALUE_ID;
    XsLilValueId float_right = XS_LIL_INVALID_VALUE_ID;
    XsLilValueId float_sum = XS_LIL_INVALID_VALUE_ID;
    XsLilValueId float_equal = XS_LIL_INVALID_VALUE_ID;
    XsLilType f32 = xs_lil_scalar_type(XS_LIL_TYPE_F32);
    if(xs_lil_block_add_const_f32_bits(entry, UINT32_C(0x3f800000), &float_left, error) != XS_LIL_OK ||
       xs_lil_block_add_const_f32_bits(entry, UINT32_C(0x40000000), &float_right, error) != XS_LIL_OK ||
       xs_lil_block_binary_float(entry, XS_LIL_FLOAT_ADD, f32, float_left, float_right, &float_sum, error) !=
           XS_LIL_OK ||
       xs_lil_block_compare_float(entry, XS_LIL_FLOAT_EQ, f32, float_sum, float_right, &float_equal, error) !=
           XS_LIL_OK)
        return false;

    const XsLilValueId elements[] = {two, three};
    XsLilValueId pair_value = XS_LIL_INVALID_VALUE_ID;
    XsLilValueId fixed_value = XS_LIL_INVALID_VALUE_ID;
    XsLilValueId dynamic_value = XS_LIL_INVALID_VALUE_ID;
    XsLilValueId index = XS_LIL_INVALID_VALUE_ID;
    XsLilValueId extracted = XS_LIL_INVALID_VALUE_ID;
    XsLilValueId element = XS_LIL_INVALID_VALUE_ID;
    XsLilValueId replaced = XS_LIL_INVALID_VALUE_ID;
    XsLilValueId length = XS_LIL_INVALID_VALUE_ID;
    if(xs_lil_block_add_aggregate(entry, pair, elements, 2, &pair_value, error) != XS_LIL_OK ||
       xs_lil_block_add_extract(entry, pair_value, 0, i32, &extracted, error) != XS_LIL_OK ||
       xs_lil_block_add_array(entry, fixed, elements, 2, &fixed_value, error) != XS_LIL_OK ||
       xs_lil_block_add_array(entry, dynamic, elements, 2, &dynamic_value, error) != XS_LIL_OK ||
       xs_lil_block_add_const_i64(entry, 1, &index, error) != XS_LIL_OK ||
       xs_lil_block_add_array_set(entry, dynamic_value, index, loaded, &replaced, error) != XS_LIL_OK ||
       xs_lil_block_add_array_get(entry, replaced, index, i32, &element, error) != XS_LIL_OK ||
       xs_lil_block_add_array_length(entry, replaced, &length, error) != XS_LIL_OK ||
       xs_lil_block_set_branch_if(entry, condition, xs_lil_block_id(accepted), xs_lil_block_id(rejected), error) !=
           XS_LIL_OK)
        return false;
    (void)fixed_value;
    (void)element;
    (void)length;
    return xs_lil_block_set_return_value(accepted, extracted, error) == XS_LIL_OK &&
           xs_lil_block_set_return_value(rejected, sum, error) == XS_LIL_OK;
}

static void test_complete_public_producer_round_trip(void)
{
    XsLilError error = {0};
    XsLilModule *module = nullptr;
    XsLilType pair = {0};
    XsLilType fixed = {0};
    XsLilType dynamic = {0};
    CHECK(xs_lil_module_create("ThirdParty", &module, &error) == XS_LIL_OK);
    CHECK(xs_lil_module_text_version(module) == XS_LIL_TEXT_VERSION);
    CHECK(add_registry(module, &pair, &fixed, &dynamic, &error));
    CHECK(add_definitions(module, &error));
    CHECK(build_main(module, pair, fixed, dynamic, &error));
    CHECK(xs_lil_module_type_is_valid(module, pair));
    CHECK(xs_lil_module_type_is_valid(module, fixed));
    CHECK(xs_lil_module_verify(module, &error) == XS_LIL_OK);

    XsLilText text = {0};
    CHECK(xs_lil_module_emit_text(module, &text, &error) == XS_LIL_OK);
    CHECK(text.data != nullptr);
    if(text.data != nullptr)
    {
        CHECK(text.length == strlen(text.data));
        CHECK(strstr(text.data, ".xlil version 1\n") != nullptr);
        CHECK(strstr(text.data, "%r") != nullptr);
    }

    XsLilModule *parsed = nullptr;
    CHECK(xs_lil_module_parse_text("third_party.xlil", text.data, text.length, &parsed, &error) == XS_LIL_OK);
    CHECK(xs_lil_module_verify(parsed, &error) == XS_LIL_OK);
    const XsLilFunction *main_function = xs_lil_module_function_named(parsed, "main");
    CHECK(main_function != nullptr);
    CHECK(xs_lil_function_parameter_value(main_function, 0) == XS_LIL_INVALID_VALUE_ID);
    const XsLilBlock *entry = xs_lil_function_block_named(main_function, "entry");
    CHECK(entry != nullptr);
    CHECK(xs_lil_block_instruction_has_result(entry, 0));
    CHECK(xs_lil_block_instruction_result_type(entry, 0).kind == XS_LIL_TYPE_I32);
    CHECK(!xs_lil_block_instruction_has_result(entry, 3));

    xs_lil_module_destroy(parsed);
    xs_lil_text_destroy(&text);
    CHECK(text.data == nullptr);
    CHECK(text.length == 0);
    xs_lil_module_destroy(module);
}

static void test_emit_rejects_unverified_module(void)
{
    XsLilError error = {0};
    XsLilModule *module = nullptr;
    XsLilFunction *function = nullptr;
    XsLilBlock *entry = nullptr;
    XsLilText text = {0};
    CHECK(xs_lil_module_create("Invalid", &module, &error) == XS_LIL_OK);
    CHECK(xs_lil_module_add_function_definition(module, "missing", xs_lil_scalar_type(XS_LIL_TYPE_VOID), nullptr, 0,
                                                &function, &error) == XS_LIL_OK);
    CHECK(xs_lil_function_append_block(function, "entry", &entry, &error) == XS_LIL_OK);
    CHECK(xs_lil_module_emit_text(module, &text, &error) == XS_LIL_INVALID_ARGUMENT);
    CHECK(text.data == nullptr);
    xs_lil_module_destroy(module);
}

static void test_builder_and_stable_handles(void)
{
    XsLilError error = {0};
    XsLilModule *module = nullptr;
    XsLilFunction *main_function = nullptr;
    XsLilBuilder *builder = nullptr;
    XsLilType i32 = xs_lil_scalar_type(XS_LIL_TYPE_I32);
    const XsLilType parameters[] = {i32, i32};
    CHECK(xs_lil_c_api_version() == XS_LIL_C_API_VERSION);
    CHECK(xs_lil_module_create("Builder", &module, &error) == XS_LIL_OK);
    CHECK(xs_lil_module_add_function(module, "add", i32, parameters, 2, &error) == XS_LIL_OK);
    CHECK(xs_lil_module_add_function_definition(module, "main", i32, nullptr, 0, &main_function, &error) == XS_LIL_OK);
    CHECK(xs_lil_builder_create(module, &builder, &error) == XS_LIL_OK);
    CHECK(xs_lil_builder_append_block(builder, main_function, "entry", nullptr, &error) == XS_LIL_OK);

    for(size_t index = 0; index < 20; ++index)
    {
        char name[32] = {0};
        snprintf(name, sizeof(name), "filler_%zu", index);
        CHECK(xs_lil_module_add_function(module, name, i32, nullptr, 0, &error) == XS_LIL_OK);
    }

    XsLilValueId left = XS_LIL_INVALID_VALUE_ID;
    XsLilValueId right = XS_LIL_INVALID_VALUE_ID;
    XsLilValueId sum = XS_LIL_INVALID_VALUE_ID;
    XsLilValueId call = XS_LIL_INVALID_VALUE_ID;
    CHECK(xs_lil_builder_const_i32(builder, 20, &left, &error) == XS_LIL_OK);
    CHECK(xs_lil_builder_const_i32(builder, 22, &right, &error) == XS_LIL_OK);
    CHECK(xs_lil_builder_binary_integer(builder, XS_LIL_INTEGER_ADD, left, right, &sum, &error) == XS_LIL_OK);
    const XsLilValueId arguments[] = {sum, right};
    CHECK(xs_lil_builder_call(builder, "add", arguments, 2, &call, &error) == XS_LIL_OK);
    CHECK(xs_lil_builder_return_value(builder, call, &error) == XS_LIL_OK);
    CHECK(xs_lil_module_verify(module, &error) == XS_LIL_OK);

    xs_lil_builder_destroy(builder);
    xs_lil_module_destroy(module);
}

static void test_ffi_owned_helpers(void)
{
    XsLilError *error = xs_lil_error_create();
    XsLilText *text = xs_lil_text_create();
    CHECK(error != nullptr);
    CHECK(text != nullptr);
    CHECK(xs_lil_error_status(error) == XS_LIL_OK);
    CHECK(strcmp(xs_lil_status_name(XS_LIL_IO_ERROR), "io_error") == 0);
    CHECK(xs_lil_text_data(text) == nullptr);
    CHECK(xs_lil_text_length(text) == 0);
    CHECK(xs_lil_type_kind(xs_lil_scalar_type(XS_LIL_TYPE_I64)) == XS_LIL_TYPE_I64);
    CHECK(xs_lil_type_registry_id(xs_lil_aggregate_type(7)) == 7);
    xs_lil_text_delete(text);
    xs_lil_error_destroy(error);
}

int main(void)
{
    test_complete_public_producer_round_trip();
    test_emit_rejects_unverified_module();
    test_builder_and_stable_handles();
    test_ffi_owned_helpers();
    return failures == 0 ? 0 : 1;
}
