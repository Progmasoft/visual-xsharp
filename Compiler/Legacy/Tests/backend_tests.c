/*
 * SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
 * SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.0
 */

#include "Visual/XSharp/Legacy/backend/linker.h"
#include "Visual/XSharp/Legacy/backend/llvm_backend.h"

#include <llvm-c/Core.h>

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

static bool file_contains(const char *path, const char *needle)
{
    FILE *file = fopen(path, "rb");
    if(file == nullptr)
        return false;
    char buffer[4096] = {0};
    size_t read = fread(buffer, 1, sizeof(buffer) - 1U, file);
    fclose(file);
    buffer[read] = '\0';
    return strstr(buffer, needle) != nullptr;
}

int main(int argc, char **argv)
{
    if(argc != 3)
    {
        fprintf(stderr, "backend test requires object path and linker path\n");
        return 2;
    }

    XsBackendError error = {0};
    XsLlvmBackend *backend = nullptr;
    XsLlvmBackendConfig config = {
        .optimization = XS_LLVM_OPT_NONE,
        .verify_modules = true,
    };
    CHECK(xs_llvm_backend_create(&config, &backend, &error) == XS_BACKEND_OK);
    CHECK(backend != nullptr);
    CHECK(xs_llvm_backend_context(backend) != nullptr);
    CHECK(xs_llvm_backend_target_triple(backend) != nullptr && xs_llvm_backend_target_triple(backend)[0] != '\0');
    CHECK(xs_llvm_backend_data_layout(backend) != nullptr && xs_llvm_backend_data_layout(backend)[0] != '\0');

    XsLlvmCodegenUnit *first = nullptr;
    XsLlvmCodegenUnit *second = nullptr;
    CHECK(xs_llvm_codegen_unit_create(backend, "first", &first, &error) == XS_BACKEND_OK);
    CHECK(xs_llvm_codegen_unit_create(backend, "second", &second, &error) == XS_BACKEND_OK);
    CHECK(first != nullptr && second != nullptr);
    CHECK(xs_llvm_codegen_unit_module(first) != xs_llvm_codegen_unit_module(second));

    LLVMTypeRef type = nullptr;
    CHECK(xs_llvm_primitive_type(backend, XS_PRIMITIVE_BOOL, &type, &error) == XS_BACKEND_OK);
    CHECK(LLVMGetIntTypeWidth(type) == 8);
    CHECK(xs_llvm_primitive_type(backend, XS_PRIMITIVE_BYTE, &type, &error) == XS_BACKEND_OK);
    CHECK(LLVMGetIntTypeWidth(type) == 8);
    CHECK(xs_llvm_primitive_type(backend, XS_PRIMITIVE_SBYTE, &type, &error) == XS_BACKEND_OK);
    CHECK(LLVMGetIntTypeWidth(type) == 8);
    CHECK(xs_llvm_primitive_type(backend, XS_PRIMITIVE_INT, &type, &error) == XS_BACKEND_OK);
    CHECK(LLVMGetIntTypeWidth(type) == 64);
    CHECK(xs_llvm_primitive_type(backend, XS_PRIMITIVE_INTEGER, &type, &error) == XS_BACKEND_OK);
    CHECK(LLVMGetIntTypeWidth(type) == 128);
    CHECK(xs_llvm_primitive_type(backend, XS_PRIMITIVE_CHAR, &type, &error) == XS_BACKEND_OK);
    CHECK(LLVMGetIntTypeWidth(type) == 32);
    CHECK(xs_llvm_primitive_type(backend, XS_PRIMITIVE_SFLOAT, &type, &error) == XS_BACKEND_OK);
    CHECK(LLVMGetTypeKind(type) == LLVMHalfTypeKind);
    CHECK(xs_llvm_primitive_type(backend, XS_PRIMITIVE_STR, &type, &error) == XS_BACKEND_OK);
    CHECK(LLVMGetTypeKind(type) == LLVMStructTypeKind && LLVMCountStructElementTypes(type) == 2);
    CHECK(xs_llvm_lil_type(backend, (XsLilType){.kind = XS_LIL_TYPE_BOOL}, &type, &error) == XS_BACKEND_OK);
    CHECK(LLVMGetIntTypeWidth(type) == 8);
    CHECK(xs_llvm_lil_type(backend, (XsLilType){.kind = XS_LIL_TYPE_I64}, &type, &error) == XS_BACKEND_OK);
    CHECK(LLVMGetIntTypeWidth(type) == 64);
    CHECK(xs_llvm_lil_type(backend, (XsLilType){.kind = XS_LIL_TYPE_STR}, &type, &error) == XS_BACKEND_OK);
    CHECK(LLVMGetTypeKind(type) == LLVMStructTypeKind && LLVMCountStructElementTypes(type) == 2);
    CHECK(xs_llvm_lil_type(backend, (XsLilType){.kind = XS_LIL_TYPE_F16}, &type, &error) == XS_BACKEND_DEFERRED);

    XsLilModule *array_module = nullptr;
    CHECK(xs_lil_module_create("ArrayLowering", &array_module, nullptr) == XS_LIL_OK);
    XsLilType array_type = {0};
    CHECK(xs_lil_module_add_array_type(array_module, (XsLilType){.kind = XS_LIL_TYPE_I32}, 3, &array_type, nullptr) ==
          XS_LIL_OK);
    const XsLilType array_parameters[] = {
        {.kind = XS_LIL_TYPE_I32},
        {.kind = XS_LIL_TYPE_I32},
        {.kind = XS_LIL_TYPE_I32},
    };
    XsLilFunction *array_function = nullptr;
    CHECK(xs_lil_module_add_function_definition(array_module, "Second", (XsLilType){.kind = XS_LIL_TYPE_I32},
                                                array_parameters, 3, &array_function, nullptr) == XS_LIL_OK);
    XsLilBlock *array_entry = nullptr;
    CHECK(xs_lil_function_append_block(array_function, "entry", &array_entry, nullptr) == XS_LIL_OK);
    const XsLilValueId array_fields[] = {0, 1, 2};
    XsLilValueId array_value = 0;
    XsLilValueId array_element = 0;
    CHECK(xs_lil_block_add_array(array_entry, array_type, array_fields, 3, &array_value, nullptr) == XS_LIL_OK);
    CHECK(xs_lil_block_add_extract(array_entry, array_value, 1, (XsLilType){.kind = XS_LIL_TYPE_I32}, &array_element,
                                   nullptr) == XS_LIL_OK);
    CHECK(xs_lil_block_set_return_value(array_entry, array_element, nullptr) == XS_LIL_OK);
    CHECK(xs_lil_module_verify(array_module, nullptr) == XS_LIL_OK);
    CHECK(xs_llvm_register_lil_types(second, array_module, &error) == XS_BACKEND_OK);
    LLVMValueRef array_llvm_function = nullptr;
    CHECK(xs_llvm_declare_lil_function(second, "Second", (XsLilType){.kind = XS_LIL_TYPE_I32}, array_parameters, 3,
                                       &array_llvm_function, &error) == XS_BACKEND_OK);
    CHECK(xs_llvm_lower_lil_function_body(second, array_function, &error) == XS_BACKEND_OK);

    const XsPrimitiveType parameters[] = {XS_PRIMITIVE_INT, XS_PRIMITIVE_INT};
    XsFunctionSignature signature = {
        .name = "Add",
        .return_type = XS_PRIMITIVE_INT,
        .parameter_types = parameters,
        .parameter_count = 2,
    };
    LLVMValueRef function = nullptr;
    CHECK(xs_llvm_declare_function(first, &signature, &function, &error) == XS_BACKEND_OK);
    CHECK(function != nullptr);
    CHECK(LLVMCountParamTypes(LLVMGlobalGetValueType(function)) == 2);
    CHECK(LLVMCountBasicBlocks(function) == 0);
    const XsLilType import_parameters[] = {{.kind = XS_LIL_TYPE_I64}};
    CHECK(xs_llvm_declare_lil_function(first, "Import", (XsLilType){.kind = XS_LIL_TYPE_I64}, import_parameters, 1,
                                       &function, &error) == XS_BACKEND_OK);
    CHECK(function != nullptr);
    CHECK(LLVMCountParamTypes(LLVMGlobalGetValueType(function)) == 1);
    CHECK(xs_llvm_declare_lil_function(first, "Sink", (XsLilType){.kind = XS_LIL_TYPE_VOID}, import_parameters, 1,
                                       &function, &error) == XS_BACKEND_OK);
    const XsLilType str_parameters[] = {{.kind = XS_LIL_TYPE_STR}};
    CHECK(xs_llvm_declare_lil_function(first, "KeepStr", (XsLilType){.kind = XS_LIL_TYPE_STR}, str_parameters, 1,
                                       &function, &error) == XS_BACKEND_OK);
    CHECK(LLVMCountParamTypes(LLVMGlobalGetValueType(function)) == 1);
    XsLilModule *lil_module = nullptr;
    CHECK(xs_lil_module_create("Lowering", &lil_module, nullptr) == XS_LIL_OK);
    XsLilFunction *lil_function = nullptr;
    CHECK(xs_lil_module_add_function_definition(lil_module, "Answer", (XsLilType){.kind = XS_LIL_TYPE_I64}, nullptr, 0,
                                                &lil_function, nullptr) == XS_LIL_OK);
    XsLilBlock *lil_entry = nullptr;
    CHECK(xs_lil_function_append_block(lil_function, "entry", &lil_entry, nullptr) == XS_LIL_OK);
    XsLilValueId answer = 0;
    CHECK(xs_lil_block_add_const_i64(lil_entry, 42, &answer, nullptr) == XS_LIL_OK);
    CHECK(xs_lil_block_set_return_value(lil_entry, answer, nullptr) == XS_LIL_OK);
    CHECK(xs_llvm_declare_lil_function(first, "Answer", (XsLilType){.kind = XS_LIL_TYPE_I64}, nullptr, 0, &function,
                                       &error) == XS_BACKEND_OK);
    CHECK(xs_llvm_lower_lil_function_body(first, xs_lil_module_function_at(lil_module, 0), &error) == XS_BACKEND_OK);
    CHECK(xs_lil_module_add_function(lil_module, "Import", (XsLilType){.kind = XS_LIL_TYPE_I64}, import_parameters, 1,
                                     nullptr) == XS_LIL_OK);
    CHECK(xs_lil_module_add_function(lil_module, "Sink", (XsLilType){.kind = XS_LIL_TYPE_VOID}, import_parameters, 1,
                                     nullptr) == XS_LIL_OK);
    XsLilFunction *identity = nullptr;
    CHECK(xs_lil_module_add_function_definition(lil_module, "Identity", (XsLilType){.kind = XS_LIL_TYPE_I64},
                                                import_parameters, 1, &identity, nullptr) == XS_LIL_OK);
    XsLilBlock *identity_entry = nullptr;
    CHECK(xs_lil_function_append_block(identity, "entry", &identity_entry, nullptr) == XS_LIL_OK);
    CHECK(xs_lil_block_set_return_value(identity_entry, 0, nullptr) == XS_LIL_OK);
    CHECK(xs_llvm_declare_lil_function(first, "Identity", (XsLilType){.kind = XS_LIL_TYPE_I64}, import_parameters, 1,
                                       &function, &error) == XS_BACKEND_OK);
    CHECK(xs_llvm_lower_lil_function_body(first, identity, &error) == XS_BACKEND_OK);
    XsLilFunction *call_import = nullptr;
    CHECK(xs_lil_module_add_function_definition(lil_module, "CallImport", (XsLilType){.kind = XS_LIL_TYPE_I64}, nullptr,
                                                0, &call_import, nullptr) == XS_LIL_OK);
    XsLilBlock *call_import_entry = nullptr;
    CHECK(xs_lil_function_append_block(call_import, "entry", &call_import_entry, nullptr) == XS_LIL_OK);
    XsLilValueId import_argument = 0;
    XsLilValueId import_result = 0;
    CHECK(xs_lil_block_add_const_i64(call_import_entry, 7, &import_argument, nullptr) == XS_LIL_OK);
    CHECK(xs_lil_block_add_call(call_import_entry, "Import", (XsLilType){.kind = XS_LIL_TYPE_I64}, &import_argument, 1,
                                &import_result, nullptr) == XS_LIL_OK);
    CHECK(xs_lil_block_set_return_value(call_import_entry, import_result, nullptr) == XS_LIL_OK);
    CHECK(xs_llvm_declare_lil_function(first, "CallImport", (XsLilType){.kind = XS_LIL_TYPE_I64}, nullptr, 0, &function,
                                       &error) == XS_BACKEND_OK);
    CHECK(xs_llvm_lower_lil_function_body(first, call_import, &error) == XS_BACKEND_OK);
    XsLilFunction *call_sink = nullptr;
    CHECK(xs_lil_module_add_function_definition(lil_module, "CallSink", (XsLilType){.kind = XS_LIL_TYPE_VOID}, nullptr,
                                                0, &call_sink, nullptr) == XS_LIL_OK);
    XsLilBlock *call_sink_entry = nullptr;
    CHECK(xs_lil_function_append_block(call_sink, "entry", &call_sink_entry, nullptr) == XS_LIL_OK);
    XsLilValueId sink_argument = 0;
    CHECK(xs_lil_block_add_const_i64(call_sink_entry, 9, &sink_argument, nullptr) == XS_LIL_OK);
    CHECK(xs_lil_block_add_void_call(call_sink_entry, "Sink", &sink_argument, 1, nullptr) == XS_LIL_OK);
    CHECK(xs_lil_block_set_return(call_sink_entry, nullptr) == XS_LIL_OK);
    CHECK(xs_llvm_declare_lil_function(first, "CallSink", (XsLilType){.kind = XS_LIL_TYPE_VOID}, nullptr, 0, &function,
                                       &error) == XS_BACKEND_OK);
    CHECK(xs_llvm_lower_lil_function_body(first, call_sink, &error) == XS_BACKEND_OK);
    XsLilFunction *arithmetic = nullptr;
    CHECK(xs_lil_module_add_function_definition(lil_module, "Arithmetic", (XsLilType){.kind = XS_LIL_TYPE_I64},
                                                import_parameters, 1, &arithmetic, nullptr) == XS_LIL_OK);
    XsLilBlock *arithmetic_entry = nullptr;
    CHECK(xs_lil_function_append_block(arithmetic, "entry", &arithmetic_entry, nullptr) == XS_LIL_OK);
    XsLilValueId left = 0;
    XsLilValueId right = 0;
    XsLilValueId sum = 0;
    XsLilValueId difference = 0;
    XsLilValueId product = 0;
    XsLilValueId quotient = 0;
    XsLilValueId remainder = 0;
    XsLilValueId bit_and = 0;
    XsLilValueId bit_or = 0;
    XsLilValueId bit_xor = 0;
    XsLilValueId shifted_left = 0;
    XsLilValueId shifted_right = 0;
    CHECK(xs_lil_block_add_i64(arithmetic_entry, left, right, &sum, nullptr) == XS_LIL_OK);
    CHECK(xs_lil_block_sub_i64(arithmetic_entry, sum, right, &difference, nullptr) == XS_LIL_OK);
    CHECK(xs_lil_block_mul_i64(arithmetic_entry, difference, right, &product, nullptr) == XS_LIL_OK);
    CHECK(xs_lil_block_div_i64(arithmetic_entry, product, right, &quotient, nullptr) == XS_LIL_OK);
    CHECK(xs_lil_block_rem_i64(arithmetic_entry, quotient, right, &remainder, nullptr) == XS_LIL_OK);
    CHECK(xs_lil_block_and_i64(arithmetic_entry, remainder, right, &bit_and, nullptr) == XS_LIL_OK);
    CHECK(xs_lil_block_or_i64(arithmetic_entry, bit_and, right, &bit_or, nullptr) == XS_LIL_OK);
    CHECK(xs_lil_block_xor_i64(arithmetic_entry, bit_or, right, &bit_xor, nullptr) == XS_LIL_OK);
    CHECK(xs_lil_block_shl_i64(arithmetic_entry, bit_xor, right, &shifted_left, nullptr) == XS_LIL_OK);
    CHECK(xs_lil_block_shr_i64(arithmetic_entry, shifted_left, right, &shifted_right, nullptr) == XS_LIL_OK);
    CHECK(xs_lil_block_set_return_value(arithmetic_entry, shifted_right, nullptr) == XS_LIL_OK);
    CHECK(xs_llvm_declare_lil_function(first, "Arithmetic", (XsLilType){.kind = XS_LIL_TYPE_I64}, import_parameters, 1,
                                       &function, &error) == XS_BACKEND_OK);
    CHECK(xs_llvm_lower_lil_function_body(first, arithmetic, &error) == XS_BACKEND_OK);
    XsLilFunction *equality = nullptr;
    CHECK(xs_lil_module_add_function_definition(lil_module, "Equality", (XsLilType){.kind = XS_LIL_TYPE_BOOL},
                                                import_parameters, 1, &equality, nullptr) == XS_LIL_OK);
    XsLilBlock *equality_entry = nullptr;
    CHECK(xs_lil_function_append_block(equality, "entry", &equality_entry, nullptr) == XS_LIL_OK);
    XsLilValueId equality_left = 0;
    XsLilValueId equality_right = 0;
    XsLilValueId equal = 0;
    CHECK(xs_lil_block_eq_i64(equality_entry, equality_left, equality_right, &equal, nullptr) == XS_LIL_OK);
    CHECK(xs_lil_block_set_return_value(equality_entry, equal, nullptr) == XS_LIL_OK);
    CHECK(xs_llvm_declare_lil_function(first, "Equality", (XsLilType){.kind = XS_LIL_TYPE_BOOL}, import_parameters, 1,
                                       &function, &error) == XS_BACKEND_OK);
    CHECK(xs_llvm_lower_lil_function_body(first, equality, &error) == XS_BACKEND_OK);
    XsLilFunction *compare64 = nullptr;
    CHECK(xs_lil_module_add_function_definition(lil_module, "Compare64", (XsLilType){.kind = XS_LIL_TYPE_BOOL},
                                                import_parameters, 1, &compare64, nullptr) == XS_LIL_OK);
    XsLilBlock *compare64_entry = nullptr;
    CHECK(xs_lil_function_append_block(compare64, "entry", &compare64_entry, nullptr) == XS_LIL_OK);
    XsLilValueId compare_right = 0;
    XsLilValueId not_equal = 0;
    XsLilValueId less = 0;
    XsLilValueId less_equal = 0;
    XsLilValueId greater = 0;
    XsLilValueId greater_equal = 0;
    CHECK(xs_lil_block_add_const_i64(compare64_entry, 4, &compare_right, nullptr) == XS_LIL_OK);
    CHECK(xs_lil_block_ne_i64(compare64_entry, 0, compare_right, &not_equal, nullptr) == XS_LIL_OK);
    CHECK(xs_lil_block_lt_i64(compare64_entry, 0, compare_right, &less, nullptr) == XS_LIL_OK);
    CHECK(xs_lil_block_le_i64(compare64_entry, 0, compare_right, &less_equal, nullptr) == XS_LIL_OK);
    CHECK(xs_lil_block_gt_i64(compare64_entry, 0, compare_right, &greater, nullptr) == XS_LIL_OK);
    CHECK(xs_lil_block_ge_i64(compare64_entry, 0, compare_right, &greater_equal, nullptr) == XS_LIL_OK);
    CHECK(xs_lil_block_set_return_value(compare64_entry, greater_equal, nullptr) == XS_LIL_OK);
    CHECK(xs_llvm_declare_lil_function(first, "Compare64", (XsLilType){.kind = XS_LIL_TYPE_BOOL}, import_parameters, 1,
                                       &function, &error) == XS_BACKEND_OK);
    CHECK(xs_llvm_lower_lil_function_body(first, compare64, &error) == XS_BACKEND_OK);
    XsLilFunction *arithmetic32 = nullptr;
    const XsLilType i32_parameter[] = {{.kind = XS_LIL_TYPE_I32}};
    CHECK(xs_lil_module_add_function_definition(lil_module, "Arithmetic32", (XsLilType){.kind = XS_LIL_TYPE_I32},
                                                i32_parameter, 1, &arithmetic32, nullptr) == XS_LIL_OK);
    XsLilBlock *arithmetic32_entry = nullptr;
    CHECK(xs_lil_function_append_block(arithmetic32, "entry", &arithmetic32_entry, nullptr) == XS_LIL_OK);
    XsLilValueId i32_right = 0;
    XsLilValueId i32_sum = 0;
    XsLilValueId i32_difference = 0;
    XsLilValueId i32_product = 0;
    XsLilValueId i32_quotient = 0;
    XsLilValueId i32_remainder = 0;
    XsLilValueId i32_bit_xor = 0;
    CHECK(xs_lil_block_add_const_i32(arithmetic32_entry, 3, &i32_right, nullptr) == XS_LIL_OK);
    CHECK(xs_lil_block_add_i32(arithmetic32_entry, 0, i32_right, &i32_sum, nullptr) == XS_LIL_OK);
    CHECK(xs_lil_block_sub_i32(arithmetic32_entry, i32_sum, i32_right, &i32_difference, nullptr) == XS_LIL_OK);
    CHECK(xs_lil_block_mul_i32(arithmetic32_entry, i32_difference, i32_right, &i32_product, nullptr) == XS_LIL_OK);
    CHECK(xs_lil_block_div_i32(arithmetic32_entry, i32_product, i32_right, &i32_quotient, nullptr) == XS_LIL_OK);
    CHECK(xs_lil_block_rem_i32(arithmetic32_entry, i32_quotient, i32_right, &i32_remainder, nullptr) == XS_LIL_OK);
    CHECK(xs_lil_block_xor_i32(arithmetic32_entry, i32_remainder, i32_sum, &i32_bit_xor, nullptr) == XS_LIL_OK);
    CHECK(xs_lil_block_set_return_value(arithmetic32_entry, i32_bit_xor, nullptr) == XS_LIL_OK);
    CHECK(xs_llvm_declare_lil_function(first, "Arithmetic32", (XsLilType){.kind = XS_LIL_TYPE_I32}, i32_parameter, 1,
                                       &function, &error) == XS_BACKEND_OK);
    CHECK(xs_llvm_lower_lil_function_body(first, arithmetic32, &error) == XS_BACKEND_OK);
    XsLilFunction *equality32 = nullptr;
    CHECK(xs_lil_module_add_function_definition(lil_module, "Equality32", (XsLilType){.kind = XS_LIL_TYPE_BOOL},
                                                i32_parameter, 1, &equality32, nullptr) == XS_LIL_OK);
    XsLilBlock *equality32_entry = nullptr;
    CHECK(xs_lil_function_append_block(equality32, "entry", &equality32_entry, nullptr) == XS_LIL_OK);
    XsLilValueId i32_equal = 0;
    CHECK(xs_lil_block_eq_i32(equality32_entry, 0, 0, &i32_equal, nullptr) == XS_LIL_OK);
    CHECK(xs_lil_block_set_return_value(equality32_entry, i32_equal, nullptr) == XS_LIL_OK);
    CHECK(xs_llvm_declare_lil_function(first, "Equality32", (XsLilType){.kind = XS_LIL_TYPE_BOOL}, i32_parameter, 1,
                                       &function, &error) == XS_BACKEND_OK);
    CHECK(xs_llvm_lower_lil_function_body(first, equality32, &error) == XS_BACKEND_OK);
    XsLilFunction *compare32 = nullptr;
    CHECK(xs_lil_module_add_function_definition(lil_module, "Compare32", (XsLilType){.kind = XS_LIL_TYPE_BOOL},
                                                i32_parameter, 1, &compare32, nullptr) == XS_LIL_OK);
    XsLilBlock *compare32_entry = nullptr;
    CHECK(xs_lil_function_append_block(compare32, "entry", &compare32_entry, nullptr) == XS_LIL_OK);
    XsLilValueId i32_compare_right = 0;
    XsLilValueId i32_lt = 0;
    XsLilValueId i32_le = 0;
    XsLilValueId i32_gt = 0;
    XsLilValueId i32_ge = 0;
    CHECK(xs_lil_block_add_const_i32(compare32_entry, 5, &i32_compare_right, nullptr) == XS_LIL_OK);
    CHECK(xs_lil_block_lt_i32(compare32_entry, 0, i32_compare_right, &i32_lt, nullptr) == XS_LIL_OK);
    CHECK(xs_lil_block_le_i32(compare32_entry, 0, i32_compare_right, &i32_le, nullptr) == XS_LIL_OK);
    CHECK(xs_lil_block_gt_i32(compare32_entry, 0, i32_compare_right, &i32_gt, nullptr) == XS_LIL_OK);
    CHECK(xs_lil_block_ge_i32(compare32_entry, 0, i32_compare_right, &i32_ge, nullptr) == XS_LIL_OK);
    CHECK(xs_lil_block_set_return_value(compare32_entry, i32_lt, nullptr) == XS_LIL_OK);
    CHECK(xs_llvm_declare_lil_function(first, "Compare32", (XsLilType){.kind = XS_LIL_TYPE_BOOL}, i32_parameter, 1,
                                       &function, &error) == XS_BACKEND_OK);
    CHECK(xs_llvm_lower_lil_function_body(first, compare32, &error) == XS_BACKEND_OK);
    XsLilFunction *branch_function = nullptr;
    const XsLilType bool_parameter[] = {{.kind = XS_LIL_TYPE_BOOL}};
    CHECK(xs_lil_module_add_function_definition(lil_module, "Choose", (XsLilType){.kind = XS_LIL_TYPE_VOID},
                                                bool_parameter, 1, &branch_function, nullptr) == XS_LIL_OK);
    XsLilBlock *branch_entry = nullptr;
    XsLilBlock *branch_then = nullptr;
    XsLilBlock *branch_else = nullptr;
    CHECK(xs_lil_function_append_block(branch_function, "entry", &branch_entry, nullptr) == XS_LIL_OK);
    CHECK(xs_lil_function_append_block(branch_function, "then", &branch_then, nullptr) == XS_LIL_OK);
    CHECK(xs_lil_function_append_block(branch_function, "else", &branch_else, nullptr) == XS_LIL_OK);
    CHECK(xs_lil_block_set_branch_if(branch_entry, 0, 1, 2, nullptr) == XS_LIL_OK);
    CHECK(xs_lil_block_set_return(branch_then, nullptr) == XS_LIL_OK);
    CHECK(xs_lil_block_set_return(branch_else, nullptr) == XS_LIL_OK);
    CHECK(xs_lil_module_verify(lil_module, nullptr) == XS_LIL_OK);
    CHECK(xs_llvm_declare_lil_function(first, "Choose", (XsLilType){.kind = XS_LIL_TYPE_VOID}, bool_parameter, 1,
                                       &function, &error) == XS_BACKEND_OK);
    CHECK(xs_llvm_lower_lil_function_body(first, branch_function, &error) == XS_BACKEND_OK);
    XsLilFunction *panic_function = nullptr;
    CHECK(xs_lil_module_add_function_definition(lil_module, "Panic", (XsLilType){.kind = XS_LIL_TYPE_VOID}, nullptr, 0,
                                                &panic_function, nullptr) == XS_LIL_OK);
    XsLilBlock *panic_entry = nullptr;
    CHECK(xs_lil_function_append_block(panic_function, "entry", &panic_entry, nullptr) == XS_LIL_OK);
    CHECK(xs_lil_block_set_panic(panic_entry, nullptr) == XS_LIL_OK);
    CHECK(xs_llvm_declare_lil_function(first, "Panic", (XsLilType){.kind = XS_LIL_TYPE_VOID}, nullptr, 0, &function,
                                       &error) == XS_BACKEND_OK);
    CHECK(xs_llvm_lower_lil_function_body(first, panic_function, &error) == XS_BACKEND_OK);
    XsLilFunction *memory_function = nullptr;
    CHECK(xs_lil_module_add_function_definition(lil_module, "Memory", (XsLilType){.kind = XS_LIL_TYPE_I32}, nullptr, 0,
                                                &memory_function, nullptr) == XS_LIL_OK);
    XsLilSlotId memory_slot = 0;
    CHECK(xs_lil_function_add_slot(memory_function, (XsLilType){.kind = XS_LIL_TYPE_I32}, &memory_slot, nullptr) ==
          XS_LIL_OK);
    XsLilBlock *memory_entry = nullptr;
    CHECK(xs_lil_function_append_block(memory_function, "entry", &memory_entry, nullptr) == XS_LIL_OK);
    XsLilValueId stored_value = 0;
    XsLilValueId loaded_value = 0;
    CHECK(xs_lil_block_add_const_i32(memory_entry, 7, &stored_value, nullptr) == XS_LIL_OK);
    CHECK(xs_lil_block_add_store(memory_entry, memory_slot, stored_value, nullptr) == XS_LIL_OK);
    CHECK(xs_lil_block_add_load(memory_entry, memory_slot, &loaded_value, nullptr) == XS_LIL_OK);
    CHECK(xs_lil_block_set_return_value(memory_entry, loaded_value, nullptr) == XS_LIL_OK);
    CHECK(xs_llvm_declare_lil_function(first, "Memory", (XsLilType){.kind = XS_LIL_TYPE_I32}, nullptr, 0, &function,
                                       &error) == XS_BACKEND_OK);
    CHECK(xs_llvm_lower_lil_function_body(first, memory_function, &error) == XS_BACKEND_OK);
    XsLilFunction *string_function = nullptr;
    CHECK(xs_lil_module_add_function_definition(lil_module, "Greeting", (XsLilType){.kind = XS_LIL_TYPE_STR}, nullptr,
                                                0, &string_function, nullptr) == XS_LIL_OK);
    XsLilBlock *string_entry = nullptr;
    CHECK(xs_lil_function_append_block(string_function, "entry", &string_entry, nullptr) == XS_LIL_OK);
    const uint32_t string_units[] = {UINT16_C(0x004c), UINT16_C(0x0065), UINT16_C(0x0069)};
    XsLilValueId string_value = 0;
    CHECK(xs_lil_block_add_const_str(string_entry, XS_LIL_UTF32_BE, string_units, 3, &string_value, nullptr) ==
          XS_LIL_OK);
    CHECK(xs_lil_block_set_return_value(string_entry, string_value, nullptr) == XS_LIL_OK);
    CHECK(xs_llvm_declare_lil_function(first, "Greeting", (XsLilType){.kind = XS_LIL_TYPE_STR}, nullptr, 0, &function,
                                       &error) == XS_BACKEND_OK);
    CHECK(xs_llvm_lower_lil_function_body(first, string_function, &error) == XS_BACKEND_OK);
    CHECK(xs_llvm_optimize_codegen_unit(first, &error) == XS_BACKEND_OK);
    char ir_path[4096] = {0};
    CHECK(snprintf(ir_path, sizeof(ir_path), "%s.ll", argv[1]) > 0);
    CHECK(xs_llvm_write_ir_file(first, ir_path, &error) == XS_BACKEND_OK);
    CHECK(file_contains(ir_path, "target triple"));
    CHECK(file_contains(ir_path, "declare i64 @Add(i64, i64)"));
    CHECK(file_contains(ir_path, "declare i64 @Import(i64)"));
    CHECK(file_contains(ir_path, "declare void @Sink(i64)"));
    CHECK(file_contains(ir_path, "define i64 @Answer()"));
    CHECK(file_contains(ir_path, "ret i64 42"));
    CHECK(file_contains(ir_path, "define i64 @Identity(i64"));
    CHECK(file_contains(ir_path, "define i64 @CallImport()"));
    CHECK(file_contains(ir_path, "call i64 @Import(i64 7)"));
    CHECK(file_contains(ir_path, "define void @CallSink()"));
    CHECK(file_contains(ir_path, "call void @Sink(i64 9)"));
    CHECK(file_contains(ir_path, "define i64 @Arithmetic(i64"));
    CHECK(file_contains(ir_path, "add i64"));
    CHECK(file_contains(ir_path, "sub i64"));
    CHECK(file_contains(ir_path, "mul i64"));
    CHECK(file_contains(ir_path, "sdiv i64"));
    CHECK(file_contains(ir_path, "srem i64"));
    CHECK(file_contains(ir_path, "and i64"));
    CHECK(file_contains(ir_path, "or i64"));
    CHECK(file_contains(ir_path, "xor i64"));
    CHECK(file_contains(ir_path, "shl i64"));
    CHECK(file_contains(ir_path, "ashr i64"));
    CHECK(file_contains(ir_path, "define i8 @Equality(i64"));
    CHECK(file_contains(ir_path, "icmp eq i64"));
    CHECK(file_contains(ir_path, "define i8 @Compare64(i64"));
    CHECK(file_contains(ir_path, "icmp ne i64"));
    CHECK(file_contains(ir_path, "icmp slt i64"));
    CHECK(file_contains(ir_path, "icmp sle i64"));
    CHECK(file_contains(ir_path, "icmp sgt i64"));
    CHECK(file_contains(ir_path, "icmp sge i64"));
    CHECK(file_contains(ir_path, "define i32 @Arithmetic32(i32"));
    CHECK(file_contains(ir_path, "add i32"));
    CHECK(file_contains(ir_path, "sub i32"));
    CHECK(file_contains(ir_path, "mul i32"));
    CHECK(file_contains(ir_path, "sdiv i32"));
    CHECK(file_contains(ir_path, "srem i32"));
    CHECK(file_contains(ir_path, "xor i32"));
    CHECK(file_contains(ir_path, "define i8 @Equality32(i32"));
    CHECK(file_contains(ir_path, "icmp eq i32"));
    CHECK(file_contains(ir_path, "define i8 @Compare32(i32"));
    CHECK(file_contains(ir_path, "icmp slt i32"));
    CHECK(file_contains(ir_path, "icmp sle i32"));
    CHECK(file_contains(ir_path, "icmp sgt i32"));
    CHECK(file_contains(ir_path, "icmp sge i32"));
    CHECK(file_contains(ir_path, "define void @Choose(i8"));
    CHECK(file_contains(ir_path, "br i1"));
    CHECK(file_contains(ir_path, "define void @Panic()"));
    CHECK(file_contains(ir_path, "call void @llvm.trap()"));
    CHECK(file_contains(ir_path, "unreachable"));
    CHECK(file_contains(ir_path, "define i32 @Memory()"));
    CHECK(file_contains(ir_path, "alloca i32"));
    CHECK(file_contains(ir_path, "store i32 7"));
    CHECK(file_contains(ir_path, "load i32"));
    CHECK(file_contains(ir_path, "define { ptr, i64 } @Greeting()"));
    CHECK(file_contains(ir_path, "private unnamed_addr constant [12 x i8]"));
    CHECK(file_contains(ir_path, "c\"\\00\\00\\00L\\00\\00\\00e\\00\\00\\00i\""));
    CHECK(xs_llvm_emit_object_file(first, argv[1], &error) == XS_BACKEND_OK);
    char array_ir_path[4096] = {0};
    CHECK(snprintf(array_ir_path, sizeof(array_ir_path), "%s.array.ll", argv[1]) > 0);
    CHECK(xs_llvm_write_ir_file(second, array_ir_path, &error) == XS_BACKEND_OK);
    CHECK(file_contains(array_ir_path, "[3 x i32]"));
    CHECK(file_contains(array_ir_path, "extractvalue [3 x i32]"));

    const char *linker_arguments[] = {"--version"};
    XsLinkerInvocation invocation = {
        .program = argv[2],
        .arguments = linker_arguments,
        .argument_count = 1,
    };
    int exit_code = -1;
    CHECK(xs_linker_invoke(&invocation, &exit_code, &error) == XS_BACKEND_OK);
    CHECK(exit_code == 0);

    xs_llvm_codegen_unit_destroy(second);
    xs_llvm_codegen_unit_destroy(first);
    xs_llvm_backend_destroy(backend);
    xs_lil_module_destroy(lil_module);
    xs_lil_module_destroy(array_module);
    remove(ir_path);
    remove(array_ir_path);
    remove(argv[1]);
    return failures == 0 ? 0 : 1;
}
