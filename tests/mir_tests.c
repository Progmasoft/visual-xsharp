/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 */

#include "xs/hir/symbol_table.h"
#include "xs/lil.h"
#include "xs/mir.h"
#include "xs/mir/borrow_checker.h"
#include "xs/mir/hir_lowering.h"
#include "xs/mir/jit.h"
#include "xs/mir/optimizer.h"
#include "xs/mir/xlil_lowering.h"
#include "xs/syntax_parser.h"

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

static void test_module_and_text_writer(void)
{
    XsMirError error = {0};
    XsMirModule *module = nullptr;
    CHECK(xs_mir_module_create("project", &module, &error) == XS_MIR_OK);
    CHECK(module != nullptr);
    CHECK(strcmp(xs_mir_module_name(module), "project") == 0);
    const XsMirType parameters[] = {{.kind = XS_LIL_TYPE_I64}, {.kind = XS_LIL_TYPE_I64}};
    CHECK(xs_mir_module_add_function_declaration(module, "App.Add", (XsMirType){.kind = XS_LIL_TYPE_I64}, parameters, 2,
                                                 &error) == XS_MIR_OK);
    CHECK(xs_mir_module_function_count(module) == 1);

    FILE *stream = tmpfile();
    if(stream == nullptr)
    {
        ++failures;
        xs_mir_module_destroy(module);
        return;
    }
    CHECK(xs_mir_module_write_text(module, stream, &error) == XS_MIR_OK);
    CHECK(fseek(stream, 0, SEEK_SET) == 0);
    char buffer[128] = {0};
    CHECK(fgets(buffer, sizeof(buffer), stream) != nullptr);
    CHECK(strstr(buffer, "mir module project\n") != nullptr);
    CHECK(fgets(buffer, sizeof(buffer), stream) != nullptr);
    CHECK(strstr(buffer, "declare fn App.Add(i64, i64) -> i64\n") != nullptr);
    fclose(stream);
    xs_mir_module_destroy(module);
}

static void test_function_definition_blocks_and_terminators(void)
{
    XsMirError error = {0};
    XsMirModule *module = nullptr;
    CHECK(xs_mir_module_create("project", &module, &error) == XS_MIR_OK);
    XsMirFunction *function = nullptr;
    CHECK(xs_mir_module_add_function_definition(module, "App.Main", (XsMirType){.kind = XS_LIL_TYPE_I64}, nullptr, 0,
                                                &function, &error) == XS_MIR_OK);
    CHECK(function != nullptr);
    CHECK(xs_mir_function_is_definition(function));
    XsMirLocalId argument = 0;
    XsMirLocalId state = 0;
    CHECK(xs_mir_function_add_local(function, XS_MIR_LOCAL_PARAMETER, "argc", (XsMirType){.kind = XS_LIL_TYPE_I32},
                                    false, &argument, &error) == XS_MIR_OK);
    CHECK(xs_mir_function_add_local(function, XS_MIR_LOCAL_VARIABLE, "state", (XsMirType){.kind = XS_LIL_TYPE_I64},
                                    true, &state, &error) == XS_MIR_OK);
    CHECK(xs_mir_function_local_count(function) == 2);
    CHECK(xs_mir_function_local_kind(function, argument) == XS_MIR_LOCAL_PARAMETER);
    CHECK(strcmp(xs_mir_function_local_name(function, state), "state") == 0);
    CHECK(xs_mir_function_local_type(function, state).kind == XS_LIL_TYPE_I64);
    CHECK(xs_mir_function_local_is_mutable(function, state));
    XsMirPlace *place = nullptr;
    CHECK(xs_mir_function_add_local_place(function, state, &place, &error) == XS_MIR_OK);
    CHECK(xs_mir_place_id(place) == 0);
    CHECK(xs_mir_place_root_local(place) == state);
    CHECK(xs_mir_place_add_field_projection(place, 0, &error) == XS_MIR_OK);
    CHECK(xs_mir_place_add_deref_projection(place, &error) == XS_MIR_OK);
    CHECK(xs_mir_place_add_index_projection(place, 17, &error) == XS_MIR_OK);
    CHECK(xs_mir_place_projection_count(place) == 3);
    CHECK(xs_mir_place_projection_kind(place, 0) == XS_MIR_PLACE_PROJECTION_FIELD);
    CHECK(xs_mir_place_projection_kind(place, 1) == XS_MIR_PLACE_PROJECTION_DEREF);
    CHECK(xs_mir_place_projection_kind(place, 2) == XS_MIR_PLACE_PROJECTION_INDEX);
    XsMirBlock *entry = nullptr;
    XsMirBlock *done = nullptr;
    CHECK(xs_mir_function_append_block(function, "entry", &entry, &error) == XS_MIR_OK);
    CHECK(xs_mir_function_append_block(function, "done", &done, &error) == XS_MIR_OK);
    XsMirValueId constant = 0;
    CHECK(xs_mir_block_add_const_i64(entry, 42, &constant, &error) == XS_MIR_OK);
    CHECK(xs_mir_function_value_type(function, constant).kind == XS_LIL_TYPE_I64);
    CHECK(xs_mir_block_add_store(entry, place, constant, &error) == XS_MIR_OK);
    XsMirValueId loaded = 0;
    CHECK(xs_mir_block_add_load(entry, place, (XsMirType){.kind = XS_LIL_TYPE_I64}, &loaded, &error) == XS_MIR_OK);
    CHECK(xs_mir_function_value_type(function, loaded).kind == XS_LIL_TYPE_I64);
    CHECK(xs_mir_block_instruction_count(entry) == 3);
    CHECK(xs_mir_block_instruction_kind(entry, 0) == XS_MIR_INSTRUCTION_CONST_I64);
    CHECK(xs_mir_block_instruction_kind(entry, 1) == XS_MIR_INSTRUCTION_STORE);
    CHECK(xs_mir_block_instruction_kind(entry, 2) == XS_MIR_INSTRUCTION_LOAD);
    CHECK(xs_mir_function_block_count(function) == 2);
    CHECK(xs_mir_block_id(entry) == 0);
    CHECK(strcmp(xs_mir_block_label(done), "done") == 0);
    CHECK(xs_mir_block_set_goto(entry, done, &error) == XS_MIR_OK);
    CHECK(xs_mir_block_set_return_value(done, loaded, &error) == XS_MIR_OK);
    CHECK(xs_mir_block_terminator_kind(entry) == XS_MIR_TERMINATOR_GOTO);
    CHECK(xs_mir_block_terminator_kind(done) == XS_MIR_TERMINATOR_RETURN);

    FILE *stream = tmpfile();
    if(stream == nullptr)
    {
        ++failures;
        xs_mir_module_destroy(module);
        return;
    }
    CHECK(xs_mir_module_write_text(module, stream, &error) == XS_MIR_OK);
    CHECK(fseek(stream, 0, SEEK_SET) == 0);
    char buffer[512] = {0};
    size_t read = fread(buffer, 1, sizeof(buffer) - 1, stream);
    buffer[read] = '\0';
    CHECK(strstr(buffer, "define fn App.Main() -> i64 {\n") != nullptr);
    CHECK(strstr(buffer, "local 0 param argc: i32\n") != nullptr);
    CHECK(strstr(buffer, "local 1 var mut state: i64\n") != nullptr);
    CHECK(strstr(buffer, "bb0 entry:\n  v0 = const.i64 42\n  store place0, v0\n  v1 = load place0\n  goto bb1\n") !=
          nullptr);
    CHECK(strstr(buffer, "bb1 done:\n  return 1\n") != nullptr);
    fclose(stream);
    CHECK(xs_mir_borrow_check_module(module, &error) == XS_MIR_OK);
    xs_mir_module_destroy(module);
}

static void test_borrow_checker_allows_immutable_initialization_and_rejects_reassignment(void)
{
    XsMirError error = {0};
    XsMirModule *module = nullptr;
    CHECK(xs_mir_module_create("project", &module, &error) == XS_MIR_OK);
    XsMirFunction *function = nullptr;
    CHECK(xs_mir_module_add_function_definition(module, "App.BadStore", (XsMirType){.kind = XS_LIL_TYPE_VOID}, nullptr,
                                                0, &function, &error) == XS_MIR_OK);
    XsMirLocalId local = 0;
    CHECK(xs_mir_function_add_local(function, XS_MIR_LOCAL_VARIABLE, "frozen", (XsMirType){.kind = XS_LIL_TYPE_I64},
                                    false, &local, &error) == XS_MIR_OK);
    XsMirPlace *place = nullptr;
    CHECK(xs_mir_function_add_local_place(function, local, &place, &error) == XS_MIR_OK);
    XsMirBlock *entry = nullptr;
    CHECK(xs_mir_function_append_block(function, "entry", &entry, &error) == XS_MIR_OK);
    XsMirValueId value = 0;
    CHECK(xs_mir_block_add_const_i64(entry, 7, &value, &error) == XS_MIR_OK);
    CHECK(xs_mir_block_add_store(entry, place, value, &error) == XS_MIR_OK);
    CHECK(xs_mir_block_set_return(entry, &error) == XS_MIR_OK);
    CHECK(xs_mir_borrow_check_module(module, &error) == XS_MIR_OK);

    XsMirFunction *bad = nullptr;
    CHECK(xs_mir_module_add_function_definition(module, "App.BadReassign", (XsMirType){.kind = XS_LIL_TYPE_VOID},
                                                nullptr, 0, &bad, &error) == XS_MIR_OK);
    XsMirLocalId bad_local = 0;
    CHECK(xs_mir_function_add_local(bad, XS_MIR_LOCAL_VARIABLE, "frozen", (XsMirType){.kind = XS_LIL_TYPE_I64}, false,
                                    &bad_local, &error) == XS_MIR_OK);
    XsMirPlace *bad_place = nullptr;
    CHECK(xs_mir_function_add_local_place(bad, bad_local, &bad_place, &error) == XS_MIR_OK);
    XsMirBlock *bad_entry = nullptr;
    CHECK(xs_mir_function_append_block(bad, "entry", &bad_entry, &error) == XS_MIR_OK);
    XsMirValueId bad_value = 0;
    CHECK(xs_mir_block_add_const_i64(bad_entry, 9, &bad_value, &error) == XS_MIR_OK);
    CHECK(xs_mir_block_add_store(bad_entry, bad_place, bad_value, &error) == XS_MIR_OK);
    CHECK(xs_mir_block_add_store(bad_entry, bad_place, bad_value, &error) == XS_MIR_OK);
    CHECK(xs_mir_block_set_return(bad_entry, &error) == XS_MIR_OK);
    CHECK(xs_mir_borrow_check_module(module, &error) == XS_MIR_UNSUPPORTED);
    xs_mir_module_destroy(module);
}

static void test_borrow_checker_rejects_missing_terminator(void)
{
    XsMirError error = {0};
    XsMirModule *module = nullptr;
    CHECK(xs_mir_module_create("project", &module, &error) == XS_MIR_OK);
    XsMirFunction *function = nullptr;
    CHECK(xs_mir_module_add_function_definition(module, "App.MissingTerminator", (XsMirType){.kind = XS_LIL_TYPE_VOID},
                                                nullptr, 0, &function, &error) == XS_MIR_OK);
    XsMirBlock *entry = nullptr;
    CHECK(xs_mir_function_append_block(function, "entry", &entry, &error) == XS_MIR_OK);
    CHECK(xs_mir_borrow_check_module(module, &error) == XS_MIR_INVALID_ARGUMENT);
    xs_mir_module_destroy(module);
}

static void test_borrow_checker_rejects_unknown_value_operand(void)
{
    XsMirError error = {0};
    XsMirModule *module = nullptr;
    CHECK(xs_mir_module_create("project", &module, &error) == XS_MIR_OK);
    XsMirFunction *function = nullptr;
    CHECK(xs_mir_module_add_function_definition(module, "App.BadOperand", (XsMirType){.kind = XS_LIL_TYPE_I64}, nullptr,
                                                0, &function, &error) == XS_MIR_OK);
    XsMirBlock *entry = nullptr;
    CHECK(xs_mir_function_append_block(function, "entry", &entry, &error) == XS_MIR_OK);
    XsMirValueId known = 0;
    XsMirValueId sum = 0;
    CHECK(xs_mir_block_add_const_i64(entry, 1, &known, &error) == XS_MIR_OK);
    CHECK(xs_mir_block_add_i64(entry, known, 99, &sum, &error) == XS_MIR_OK);
    CHECK(xs_mir_block_set_return_value(entry, sum, &error) == XS_MIR_OK);
    CHECK(xs_mir_borrow_check_module(module, &error) == XS_MIR_INVALID_ARGUMENT);
    xs_mir_module_destroy(module);
}

static void test_borrow_checker_rejects_wrong_i64_operand_type(void)
{
    XsMirError error = {0};
    XsMirModule *module = nullptr;
    CHECK(xs_mir_module_create("project", &module, &error) == XS_MIR_OK);
    XsMirFunction *function = nullptr;
    CHECK(xs_mir_module_add_function_definition(module, "App.BadType", (XsMirType){.kind = XS_LIL_TYPE_I64}, nullptr, 0,
                                                &function, &error) == XS_MIR_OK);
    XsMirLocalId local = 0;
    CHECK(xs_mir_function_add_local(function, XS_MIR_LOCAL_VARIABLE, "flag", (XsMirType){.kind = XS_LIL_TYPE_BOOL},
                                    true, &local, &error) == XS_MIR_OK);
    XsMirPlace *place = nullptr;
    CHECK(xs_mir_function_add_local_place(function, local, &place, &error) == XS_MIR_OK);
    XsMirBlock *entry = nullptr;
    CHECK(xs_mir_function_append_block(function, "entry", &entry, &error) == XS_MIR_OK);
    XsMirValueId flag = 0;
    XsMirValueId one = 0;
    XsMirValueId sum = 0;
    CHECK(xs_mir_block_add_load(entry, place, (XsMirType){.kind = XS_LIL_TYPE_BOOL}, &flag, &error) == XS_MIR_OK);
    CHECK(xs_mir_block_add_const_i64(entry, 1, &one, &error) == XS_MIR_OK);
    CHECK(xs_mir_block_add_i64(entry, flag, one, &sum, &error) == XS_MIR_OK);
    CHECK(xs_mir_block_set_return_value(entry, sum, &error) == XS_MIR_OK);
    CHECK(xs_mir_borrow_check_module(module, &error) == XS_MIR_INVALID_ARGUMENT);
    xs_mir_module_destroy(module);
}

static void test_branch_terminator_checks_condition_and_reachability(void)
{
    XsMirError error = {0};
    XsMirModule *module = nullptr;
    CHECK(xs_mir_module_create("project", &module, &error) == XS_MIR_OK);
    XsMirFunction *function = nullptr;
    CHECK(xs_mir_module_add_function_definition(module, "App.Branch", (XsMirType){.kind = XS_LIL_TYPE_VOID}, nullptr, 0,
                                                &function, &error) == XS_MIR_OK);
    XsMirLocalId condition_local = 0;
    CHECK(xs_mir_function_add_local(function, XS_MIR_LOCAL_VARIABLE, "condition", (XsMirType){.kind = XS_LIL_TYPE_BOOL},
                                    true, &condition_local, &error) == XS_MIR_OK);
    XsMirPlace *condition_place = nullptr;
    CHECK(xs_mir_function_add_local_place(function, condition_local, &condition_place, &error) == XS_MIR_OK);
    XsMirBlock *entry = nullptr;
    XsMirBlock *then_block = nullptr;
    XsMirBlock *else_block = nullptr;
    XsMirBlock *dead = nullptr;
    CHECK(xs_mir_function_append_block(function, "entry", &entry, &error) == XS_MIR_OK);
    CHECK(xs_mir_function_append_block(function, "then", &then_block, &error) == XS_MIR_OK);
    CHECK(xs_mir_function_append_block(function, "else", &else_block, &error) == XS_MIR_OK);
    CHECK(xs_mir_function_append_block(function, "dead", &dead, &error) == XS_MIR_OK);
    XsMirValueId initial = 0;
    XsMirValueId condition = 0;
    CHECK(xs_mir_block_add_const_bool(entry, true, &initial, &error) == XS_MIR_OK);
    CHECK(xs_mir_block_add_store(entry, condition_place, initial, &error) == XS_MIR_OK);
    CHECK(xs_mir_block_add_load(entry, condition_place, (XsMirType){.kind = XS_LIL_TYPE_BOOL}, &condition, &error) ==
          XS_MIR_OK);
    CHECK(xs_mir_block_set_branch(entry, condition, then_block, else_block, &error) == XS_MIR_OK);
    CHECK(xs_mir_block_set_return(then_block, &error) == XS_MIR_OK);
    CHECK(xs_mir_block_set_return(else_block, &error) == XS_MIR_OK);
    CHECK(xs_mir_block_set_unreachable(dead, &error) == XS_MIR_OK);
    CHECK(xs_mir_borrow_check_module(module, &error) == XS_MIR_OK);

    FILE *stream = tmpfile();
    if(stream == nullptr)
    {
        ++failures;
        xs_mir_module_destroy(module);
        return;
    }
    CHECK(xs_mir_module_write_text(module, stream, &error) == XS_MIR_OK);
    CHECK(fseek(stream, 0, SEEK_SET) == 0);
    char buffer[1024] = {0};
    size_t read = fread(buffer, 1, sizeof(buffer) - 1, stream);
    buffer[read] = '\0';
    CHECK(strstr(buffer, "branch v1, bb1, bb2\n") != nullptr);
    fclose(stream);

    CHECK(xs_mir_optimize_module_cfg(module, &error) == XS_MIR_OK);
    CHECK(xs_mir_function_block_count(function) == 3);
    CHECK(xs_mir_borrow_check_module(module, &error) == XS_MIR_OK);
    xs_mir_module_destroy(module);
}

static void test_borrow_checker_rejects_non_bool_branch_condition(void)
{
    XsMirError error = {0};
    XsMirModule *module = nullptr;
    CHECK(xs_mir_module_create("project", &module, &error) == XS_MIR_OK);
    XsMirFunction *function = nullptr;
    CHECK(xs_mir_module_add_function_definition(module, "App.BadBranch", (XsMirType){.kind = XS_LIL_TYPE_VOID}, nullptr,
                                                0, &function, &error) == XS_MIR_OK);
    XsMirBlock *entry = nullptr;
    XsMirBlock *then_block = nullptr;
    XsMirBlock *else_block = nullptr;
    CHECK(xs_mir_function_append_block(function, "entry", &entry, &error) == XS_MIR_OK);
    CHECK(xs_mir_function_append_block(function, "then", &then_block, &error) == XS_MIR_OK);
    CHECK(xs_mir_function_append_block(function, "else", &else_block, &error) == XS_MIR_OK);
    XsMirValueId condition = 0;
    CHECK(xs_mir_block_add_const_i64(entry, 1, &condition, &error) == XS_MIR_OK);
    CHECK(xs_mir_block_set_branch(entry, condition, then_block, else_block, &error) == XS_MIR_OK);
    CHECK(xs_mir_block_set_return(then_block, &error) == XS_MIR_OK);
    CHECK(xs_mir_block_set_return(else_block, &error) == XS_MIR_OK);
    CHECK(xs_mir_borrow_check_module(module, &error) == XS_MIR_INVALID_ARGUMENT);
    xs_mir_module_destroy(module);
}

static void test_cfg_optimizer_removes_unreachable_blocks(void)
{
    XsMirError error = {0};
    XsMirModule *module = nullptr;
    CHECK(xs_mir_module_create("project", &module, &error) == XS_MIR_OK);
    XsMirFunction *function = nullptr;
    CHECK(xs_mir_module_add_function_definition(module, "App.Cfg", (XsMirType){.kind = XS_LIL_TYPE_VOID}, nullptr, 0,
                                                &function, &error) == XS_MIR_OK);
    XsMirBlock *entry = nullptr;
    XsMirBlock *done = nullptr;
    XsMirBlock *dead = nullptr;
    CHECK(xs_mir_function_append_block(function, "entry", &entry, &error) == XS_MIR_OK);
    CHECK(xs_mir_function_append_block(function, "done", &done, &error) == XS_MIR_OK);
    CHECK(xs_mir_function_append_block(function, "dead", &dead, &error) == XS_MIR_OK);
    CHECK(xs_mir_block_set_goto(entry, done, &error) == XS_MIR_OK);
    CHECK(xs_mir_block_set_return(done, &error) == XS_MIR_OK);
    CHECK(xs_mir_block_set_unreachable(dead, &error) == XS_MIR_OK);
    CHECK(xs_mir_function_block_count(function) == 3);
    CHECK(xs_mir_optimize_module_cfg(module, &error) == XS_MIR_OK);
    CHECK(xs_mir_function_block_count(function) == 2);
    const XsMirBlock *second = xs_mir_function_block_at(function, 1);
    CHECK(second != nullptr && strcmp(xs_mir_block_label(second), "done") == 0);
    CHECK(xs_mir_borrow_check_module(module, &error) == XS_MIR_OK);
    xs_mir_module_destroy(module);
}

static void test_constant_optimizer_folds_i64_add(void)
{
    XsMirError error = {0};
    XsMirModule *module = nullptr;
    CHECK(xs_mir_module_create("project", &module, &error) == XS_MIR_OK);
    XsMirFunction *function = nullptr;
    CHECK(xs_mir_module_add_function_definition(module, "App.Fold", (XsMirType){.kind = XS_LIL_TYPE_I64}, nullptr, 0,
                                                &function, &error) == XS_MIR_OK);
    XsMirBlock *entry = nullptr;
    CHECK(xs_mir_function_append_block(function, "entry", &entry, &error) == XS_MIR_OK);
    XsMirValueId left = 0;
    XsMirValueId right = 0;
    XsMirValueId sum = 0;
    XsMirValueId difference = 0;
    XsMirValueId product = 0;
    XsMirValueId quotient = 0;
    XsMirValueId remainder = 0;
    XsMirValueId bit_and = 0;
    XsMirValueId bit_or = 0;
    XsMirValueId bit_xor = 0;
    XsMirValueId shifted_left = 0;
    XsMirValueId shifted_right = 0;
    XsMirValueId equal = 0;
    XsMirValueId not_equal = 0;
    XsMirValueId less = 0;
    XsMirValueId less_equal = 0;
    XsMirValueId greater = 0;
    XsMirValueId greater_equal = 0;
    CHECK(xs_mir_block_add_const_i64(entry, 2, &left, &error) == XS_MIR_OK);
    CHECK(xs_mir_block_add_const_i64(entry, 3, &right, &error) == XS_MIR_OK);
    CHECK(xs_mir_block_add_i64(entry, left, right, &sum, &error) == XS_MIR_OK);
    CHECK(xs_mir_block_sub_i64(entry, sum, left, &difference, &error) == XS_MIR_OK);
    CHECK(xs_mir_block_mul_i64(entry, difference, right, &product, &error) == XS_MIR_OK);
    CHECK(xs_mir_block_div_i64(entry, product, right, &quotient, &error) == XS_MIR_OK);
    CHECK(xs_mir_block_rem_i64(entry, quotient, right, &remainder, &error) == XS_MIR_OK);
    CHECK(xs_mir_block_and_i64(entry, left, right, &bit_and, &error) == XS_MIR_OK);
    CHECK(xs_mir_block_or_i64(entry, left, right, &bit_or, &error) == XS_MIR_OK);
    CHECK(xs_mir_block_shl_i64(entry, left, right, &shifted_left, &error) == XS_MIR_OK);
    CHECK(xs_mir_block_shr_i64(entry, shifted_left, right, &shifted_right, &error) == XS_MIR_OK);
    CHECK(xs_mir_block_eq_i64(entry, remainder, left, &equal, &error) == XS_MIR_OK);
    CHECK(xs_mir_block_ne_i64(entry, remainder, left, &not_equal, &error) == XS_MIR_OK);
    CHECK(xs_mir_block_lt_i64(entry, left, right, &less, &error) == XS_MIR_OK);
    CHECK(xs_mir_block_le_i64(entry, left, right, &less_equal, &error) == XS_MIR_OK);
    CHECK(xs_mir_block_gt_i64(entry, left, right, &greater, &error) == XS_MIR_OK);
    CHECK(xs_mir_block_ge_i64(entry, left, right, &greater_equal, &error) == XS_MIR_OK);
    CHECK(xs_mir_block_instruction_kind(entry, 2) == XS_MIR_INSTRUCTION_ADD_I64);
    CHECK(xs_mir_block_set_return_value(entry, quotient, &error) == XS_MIR_OK);
    CHECK(xs_mir_optimize_module_constants(module, &error) == XS_MIR_OK);
    CHECK(xs_mir_block_instruction_kind(entry, 2) == XS_MIR_INSTRUCTION_CONST_I64);
    CHECK(xs_mir_block_instruction_kind(entry, 3) == XS_MIR_INSTRUCTION_CONST_I64);
    CHECK(xs_mir_block_instruction_kind(entry, 4) == XS_MIR_INSTRUCTION_CONST_I64);
    CHECK(xs_mir_block_instruction_kind(entry, 5) == XS_MIR_INSTRUCTION_CONST_I64);
    CHECK(xs_mir_block_instruction_kind(entry, 6) == XS_MIR_INSTRUCTION_CONST_I64);
    CHECK(xs_mir_block_instruction_kind(entry, 7) == XS_MIR_INSTRUCTION_CONST_I64);
    CHECK(xs_mir_block_instruction_kind(entry, 8) == XS_MIR_INSTRUCTION_CONST_I64);
    CHECK(xs_mir_block_instruction_kind(entry, 9) == XS_MIR_INSTRUCTION_CONST_I64);
    CHECK(xs_mir_block_instruction_kind(entry, 10) == XS_MIR_INSTRUCTION_CONST_I64);
    CHECK(xs_mir_block_instruction_kind(entry, 11) == XS_MIR_INSTRUCTION_CONST_BOOL);
    CHECK(xs_mir_block_instruction_kind(entry, 12) == XS_MIR_INSTRUCTION_CONST_BOOL);
    CHECK(xs_mir_block_instruction_kind(entry, 13) == XS_MIR_INSTRUCTION_CONST_BOOL);
    CHECK(xs_mir_block_instruction_kind(entry, 14) == XS_MIR_INSTRUCTION_CONST_BOOL);
    CHECK(xs_mir_block_instruction_kind(entry, 15) == XS_MIR_INSTRUCTION_CONST_BOOL);
    CHECK(xs_mir_block_instruction_kind(entry, 16) == XS_MIR_INSTRUCTION_CONST_BOOL);

    FILE *stream = tmpfile();
    if(stream == nullptr)
    {
        ++failures;
        xs_mir_module_destroy(module);
        return;
    }
    CHECK(xs_mir_module_write_text(module, stream, &error) == XS_MIR_OK);
    CHECK(fseek(stream, 0, SEEK_SET) == 0);
    char buffer[1024] = {0};
    size_t read = fread(buffer, 1, sizeof(buffer) - 1, stream);
    buffer[read] = '\0';
    CHECK(strstr(buffer, "v2 = const.i64 5\n") != nullptr);
    CHECK(strstr(buffer, "v9 = const.i64 16\n") != nullptr);
    CHECK(strstr(buffer, "v11 = const.bool false\n") != nullptr);
    CHECK(strstr(buffer, "v12 = const.bool true\n") != nullptr);
    fclose(stream);
    xs_mir_module_destroy(module);
}

static void test_constant_optimizer_folds_i32_family(void)
{
    XsMirError error = {0};
    XsMirModule *module = nullptr;
    CHECK(xs_mir_module_create("project", &module, &error) == XS_MIR_OK);
    XsMirFunction *function = nullptr;
    CHECK(xs_mir_module_add_function_definition(module, "App.Fold32", (XsMirType){.kind = XS_LIL_TYPE_I32}, nullptr, 0,
                                                &function, &error) == XS_MIR_OK);
    XsMirBlock *entry = nullptr;
    CHECK(xs_mir_function_append_block(function, "entry", &entry, &error) == XS_MIR_OK);
    XsMirValueId one = 0;
    XsMirValueId two = 0;
    XsMirValueId sum = 0;
    XsMirValueId difference = 0;
    XsMirValueId product = 0;
    XsMirValueId quotient = 0;
    XsMirValueId remainder = 0;
    XsMirValueId bit_and = 0;
    XsMirValueId bit_or = 0;
    XsMirValueId bit_xor = 0;
    XsMirValueId shifted_left = 0;
    XsMirValueId shifted_right = 0;
    XsMirValueId equal = 0;
    XsMirValueId greater = 0;
    CHECK(xs_mir_block_add_const_i32(entry, 1, &one, &error) == XS_MIR_OK);
    CHECK(xs_mir_block_add_const_i32(entry, 2, &two, &error) == XS_MIR_OK);
    CHECK(xs_mir_block_add_i32(entry, one, two, &sum, &error) == XS_MIR_OK);
    CHECK(xs_mir_block_sub_i32(entry, sum, one, &difference, &error) == XS_MIR_OK);
    CHECK(xs_mir_block_mul_i32(entry, difference, two, &product, &error) == XS_MIR_OK);
    CHECK(xs_mir_block_div_i32(entry, product, two, &quotient, &error) == XS_MIR_OK);
    CHECK(xs_mir_block_rem_i32(entry, quotient, two, &remainder, &error) == XS_MIR_OK);
    CHECK(xs_mir_block_and_i32(entry, sum, product, &bit_and, &error) == XS_MIR_OK);
    CHECK(xs_mir_block_or_i32(entry, bit_and, sum, &bit_or, &error) == XS_MIR_OK);
    CHECK(xs_mir_block_xor_i32(entry, bit_or, sum, &bit_xor, &error) == XS_MIR_OK);
    CHECK(xs_mir_block_shl_i32(entry, bit_xor, one, &shifted_left, &error) == XS_MIR_OK);
    CHECK(xs_mir_block_shr_i32(entry, shifted_left, one, &shifted_right, &error) == XS_MIR_OK);
    CHECK(xs_mir_block_eq_i32(entry, shifted_right, sum, &equal, &error) == XS_MIR_OK);
    CHECK(xs_mir_block_gt_i32(entry, shifted_right, two, &greater, &error) == XS_MIR_OK);
    CHECK(xs_mir_block_set_return_value(entry, shifted_right, &error) == XS_MIR_OK);
    CHECK(xs_mir_optimize_module_constants(module, &error) == XS_MIR_OK);
    for(size_t index = 2; index <= 11; ++index)
        CHECK(xs_mir_block_instruction_kind(entry, index) == XS_MIR_INSTRUCTION_CONST_I32);
    CHECK(xs_mir_block_instruction_kind(entry, 12) == XS_MIR_INSTRUCTION_CONST_BOOL);
    CHECK(xs_mir_block_instruction_kind(entry, 13) == XS_MIR_INSTRUCTION_CONST_BOOL);

    FILE *stream = tmpfile();
    if(stream == nullptr)
    {
        ++failures;
        xs_mir_module_destroy(module);
        return;
    }
    CHECK(xs_mir_module_write_text(module, stream, &error) == XS_MIR_OK);
    CHECK(fseek(stream, 0, SEEK_SET) == 0);
    char buffer[1024] = {0};
    size_t read = fread(buffer, 1, sizeof(buffer) - 1, stream);
    buffer[read] = '\0';
    CHECK(strstr(buffer, "v2 = const.i32 3\n") != nullptr);
    CHECK(strstr(buffer, "v11 = const.i32 0\n") != nullptr);
    CHECK(strstr(buffer, "v12 = const.bool false\n") != nullptr);
    fclose(stream);
    xs_mir_module_destroy(module);
}

static void test_constant_optimizer_folds_bool_branch(void)
{
    XsMirError error = {0};
    XsMirModule *module = nullptr;
    CHECK(xs_mir_module_create("project", &module, &error) == XS_MIR_OK);
    XsMirFunction *function = nullptr;
    CHECK(xs_mir_module_add_function_definition(module, "App.FoldBranch", (XsMirType){.kind = XS_LIL_TYPE_I32}, nullptr,
                                                0, &function, &error) == XS_MIR_OK);
    XsMirBlock *entry = nullptr;
    XsMirBlock *then_block = nullptr;
    XsMirBlock *else_block = nullptr;
    CHECK(xs_mir_function_append_block(function, "entry", &entry, &error) == XS_MIR_OK);
    CHECK(xs_mir_function_append_block(function, "then", &then_block, &error) == XS_MIR_OK);
    CHECK(xs_mir_function_append_block(function, "else", &else_block, &error) == XS_MIR_OK);
    XsMirValueId condition = 0;
    XsMirValueId then_value = 0;
    XsMirValueId else_value = 0;
    CHECK(xs_mir_block_add_const_bool(entry, false, &condition, &error) == XS_MIR_OK);
    CHECK(xs_mir_block_set_branch(entry, condition, then_block, else_block, &error) == XS_MIR_OK);
    CHECK(xs_mir_block_add_const_i32(then_block, 7, &then_value, &error) == XS_MIR_OK);
    CHECK(xs_mir_block_set_return_value(then_block, then_value, &error) == XS_MIR_OK);
    CHECK(xs_mir_block_add_const_i32(else_block, 3, &else_value, &error) == XS_MIR_OK);
    CHECK(xs_mir_block_set_return_value(else_block, else_value, &error) == XS_MIR_OK);
    CHECK(xs_mir_block_terminator_kind(entry) == XS_MIR_TERMINATOR_BRANCH);
    CHECK(xs_mir_optimize_module_constants(module, &error) == XS_MIR_OK);
    CHECK(xs_mir_block_terminator_kind(entry) == XS_MIR_TERMINATOR_GOTO);
    CHECK(xs_mir_optimize_module_cfg(module, &error) == XS_MIR_OK);
    CHECK(xs_mir_function_block_count(function) == 2);
    const XsMirBlock *live = xs_mir_function_block_at(function, 1);
    CHECK(live != nullptr && strcmp(xs_mir_block_label(live), "else") == 0);
    CHECK(xs_mir_borrow_check_module(module, &error) == XS_MIR_OK);
    xs_mir_module_destroy(module);
}

static void test_constant_optimizer_folds_bool_not(void)
{
    XsMirError error = {0};
    XsMirModule *module = nullptr;
    CHECK(xs_mir_module_create("project", &module, &error) == XS_MIR_OK);
    XsMirFunction *function = nullptr;
    CHECK(xs_mir_module_add_function_definition(module, "App.FoldNot", (XsMirType){.kind = XS_LIL_TYPE_BOOL}, nullptr,
                                                0, &function, &error) == XS_MIR_OK);
    XsMirBlock *entry = nullptr;
    CHECK(xs_mir_function_append_block(function, "entry", &entry, &error) == XS_MIR_OK);
    XsMirValueId flag = 0;
    XsMirValueId inverted = 0;
    CHECK(xs_mir_block_add_const_bool(entry, false, &flag, &error) == XS_MIR_OK);
    CHECK(xs_mir_block_not_bool(entry, flag, &inverted, &error) == XS_MIR_OK);
    CHECK(xs_mir_block_set_return_value(entry, inverted, &error) == XS_MIR_OK);
    CHECK(xs_mir_optimize_module_constants(module, &error) == XS_MIR_OK);
    CHECK(xs_mir_block_instruction_kind(entry, 1) == XS_MIR_INSTRUCTION_CONST_BOOL);

    FILE *stream = tmpfile();
    if(stream == nullptr)
    {
        ++failures;
        xs_mir_module_destroy(module);
        return;
    }
    CHECK(xs_mir_module_write_text(module, stream, &error) == XS_MIR_OK);
    CHECK(fseek(stream, 0, SEEK_SET) == 0);
    char buffer[512] = {0};
    size_t read = fread(buffer, 1, sizeof(buffer) - 1U, stream);
    buffer[read] = '\0';
    CHECK(strstr(buffer, "v1 = const.bool true\n") != nullptr);
    fclose(stream);
    CHECK(xs_mir_borrow_check_module(module, &error) == XS_MIR_OK);
    xs_mir_module_destroy(module);
}

static void test_hir_function_declaration_lowering(void)
{
    const char *text = ""
                       "fn Add(a: Int, b: Int) -> Int { return a + b; }\n"
                       "data User { value: Int; }\n";
    XsSource source = {.path = "MirLowering.vxs", .module_name = "App", .text = text, .length = strlen(text)};
    XsDiagnostics diagnostics;
    XsSyntaxTree tree;
    XsHirSymbolTable symbols;
    xs_diagnostics_init(&diagnostics);
    xs_hir_symbol_table_init(&symbols);
    CHECK(xs_syntax_parse(&source, 101, &diagnostics, &tree));
    CHECK(xs_hir_collect_symbols(&tree, &symbols, &diagnostics));

    XsMirError error = {0};
    XsMirModule *module = nullptr;
    CHECK(xs_mir_module_create("project", &module, &error) == XS_MIR_OK);
    CHECK(xs_mir_module_add_hir_function_declarations(module, &symbols, &error) == XS_MIR_OK);
    CHECK(xs_mir_module_function_count(module) == 1);
    const XsMirFunction *function = xs_mir_module_function_at(module, 0);
    CHECK(function != nullptr && strcmp(xs_mir_function_name(function), "App.Add") == 0);
    CHECK(xs_mir_function_return_type(function).kind == XS_LIL_TYPE_I64);
    CHECK(xs_mir_function_parameter_count(function) == 2);
    xs_mir_module_destroy(module);
    xs_hir_symbol_table_free(&symbols);
    xs_syntax_tree_free(&tree);
    xs_diagnostics_free(&diagnostics);
}

static void test_xlil_declaration_lowering(void)
{
    XsMirError error = {0};
    XsMirModule *mir = nullptr;
    CHECK(xs_mir_module_create("project", &mir, &error) == XS_MIR_OK);
    const XsMirType parameters[] = {{.kind = XS_LIL_TYPE_BOOL}};
    CHECK(xs_mir_module_add_function_declaration(mir, "App.Check", (XsMirType){.kind = XS_LIL_TYPE_BOOL}, parameters, 1,
                                                 &error) == XS_MIR_OK);
    XsLilError lil_error = {0};
    XsLilModule *xlil = nullptr;
    CHECK(xs_lil_module_create("project", &xlil, &lil_error) == XS_LIL_OK);
    CHECK(xs_lil_module_add_mir_function_declarations(xlil, mir, &error) == XS_MIR_OK);
    CHECK(xs_lil_module_function_count(xlil) == 1);
    xs_lil_module_destroy(xlil);
    xs_mir_module_destroy(mir);
}

static void test_xlil_body_lowering_for_const_return(void)
{
    XsMirError error = {0};
    XsMirModule *mir = nullptr;
    CHECK(xs_mir_module_create("project", &mir, &error) == XS_MIR_OK);
    XsMirFunction *function = nullptr;
    CHECK(xs_mir_module_add_function_definition(mir, "App.Answer", (XsMirType){.kind = XS_LIL_TYPE_I64}, nullptr, 0,
                                                &function, &error) == XS_MIR_OK);
    XsMirBlock *entry = nullptr;
    CHECK(xs_mir_function_append_block(function, "entry", &entry, &error) == XS_MIR_OK);
    XsMirValueId value = 0;
    CHECK(xs_mir_block_add_const_i64(entry, 42, &value, &error) == XS_MIR_OK);
    CHECK(xs_mir_block_set_return_value(entry, value, &error) == XS_MIR_OK);
    CHECK(xs_mir_borrow_check_module(mir, &error) == XS_MIR_OK);

    XsLilError lil_error = {0};
    XsLilModule *xlil = nullptr;
    CHECK(xs_lil_module_create("project", &xlil, &lil_error) == XS_LIL_OK);
    CHECK(xs_lil_module_add_mir_function_bodies(xlil, mir, &error) == XS_MIR_OK);
    FILE *stream = tmpfile();
    if(stream == nullptr)
    {
        ++failures;
        xs_lil_module_destroy(xlil);
        xs_mir_module_destroy(mir);
        return;
    }
    CHECK(xs_lil_module_write_text(xlil, stream, &lil_error) == XS_LIL_OK);
    CHECK(fseek(stream, 0, SEEK_SET) == 0);
    char buffer[256] = {0};
    size_t read = fread(buffer, 1, sizeof(buffer) - 1, stream);
    buffer[read] = '\0';
    CHECK(strstr(buffer, ".func App.Answer : () -> i64\n") != nullptr);
    CHECK(strstr(buffer, "bb0.entry:\n  %r0:i64 = const.i64 42\n  ret %r0\n.end\n") != nullptr);
    fclose(stream);
    xs_lil_module_destroy(xlil);
    xs_mir_module_destroy(mir);
}

static void test_xlil_body_lowering_for_const_i32_return(void)
{
    XsMirError error = {0};
    XsMirModule *mir = nullptr;
    CHECK(xs_mir_module_create("project", &mir, &error) == XS_MIR_OK);
    XsMirFunction *function = nullptr;
    CHECK(xs_mir_module_add_function_definition(mir, "main", (XsMirType){.kind = XS_LIL_TYPE_I32}, nullptr, 0,
                                                &function, &error) == XS_MIR_OK);
    XsMirBlock *entry = nullptr;
    CHECK(xs_mir_function_append_block(function, "entry", &entry, &error) == XS_MIR_OK);
    XsMirValueId value = 0;
    CHECK(xs_mir_block_add_const_i32(entry, 7, &value, &error) == XS_MIR_OK);
    CHECK(xs_mir_block_instruction_kind(entry, 0) == XS_MIR_INSTRUCTION_CONST_I32);
    CHECK(xs_mir_block_set_return_value(entry, value, &error) == XS_MIR_OK);
    CHECK(xs_mir_borrow_check_module(mir, &error) == XS_MIR_OK);

    XsLilError lil_error = {0};
    XsLilModule *xlil = nullptr;
    CHECK(xs_lil_module_create("project", &xlil, &lil_error) == XS_LIL_OK);
    CHECK(xs_lil_module_add_mir_function_bodies(xlil, mir, &error) == XS_MIR_OK);
    FILE *stream = tmpfile();
    if(stream == nullptr)
    {
        ++failures;
        xs_lil_module_destroy(xlil);
        xs_mir_module_destroy(mir);
        return;
    }
    CHECK(xs_lil_module_write_text(xlil, stream, &lil_error) == XS_LIL_OK);
    CHECK(fseek(stream, 0, SEEK_SET) == 0);
    char buffer[256] = {0};
    size_t read = fread(buffer, 1, sizeof(buffer) - 1, stream);
    buffer[read] = '\0';
    CHECK(strstr(buffer, ".func main : () -> i32\n") != nullptr);
    CHECK(strstr(buffer, "bb0.entry:\n  %r0:i32 = const.i32 7\n  ret %r0\n.end\n") != nullptr);
    fclose(stream);
    xs_lil_module_destroy(xlil);
    xs_mir_module_destroy(mir);
}

static void test_xlil_body_lowering_for_i32_arithmetic_return(void)
{
    XsMirError error = {0};
    XsMirModule *mir = nullptr;
    CHECK(xs_mir_module_create("project", &mir, &error) == XS_MIR_OK);
    XsMirFunction *function = nullptr;
    CHECK(xs_mir_module_add_function_definition(mir, "main", (XsMirType){.kind = XS_LIL_TYPE_I32}, nullptr, 0,
                                                &function, &error) == XS_MIR_OK);
    XsMirBlock *entry = nullptr;
    CHECK(xs_mir_function_append_block(function, "entry", &entry, &error) == XS_MIR_OK);
    XsMirValueId one = 0;
    XsMirValueId two = 0;
    XsMirValueId sum = 0;
    XsMirValueId product = 0;
    XsMirValueId quotient = 0;
    XsMirValueId remainder = 0;
    XsMirValueId bit_and = 0;
    XsMirValueId bit_or = 0;
    XsMirValueId bit_xor = 0;
    XsMirValueId shifted_left = 0;
    XsMirValueId shifted_right = 0;
    CHECK(xs_mir_block_add_const_i32(entry, 1, &one, &error) == XS_MIR_OK);
    CHECK(xs_mir_block_add_const_i32(entry, 2, &two, &error) == XS_MIR_OK);
    CHECK(xs_mir_block_add_i32(entry, one, two, &sum, &error) == XS_MIR_OK);
    CHECK(xs_mir_block_mul_i32(entry, sum, two, &product, &error) == XS_MIR_OK);
    CHECK(xs_mir_block_div_i32(entry, product, two, &quotient, &error) == XS_MIR_OK);
    CHECK(xs_mir_block_rem_i32(entry, quotient, two, &remainder, &error) == XS_MIR_OK);
    CHECK(xs_mir_block_and_i32(entry, sum, quotient, &bit_and, &error) == XS_MIR_OK);
    CHECK(xs_mir_block_or_i32(entry, bit_and, remainder, &bit_or, &error) == XS_MIR_OK);
    CHECK(xs_mir_block_xor_i32(entry, bit_or, sum, &bit_xor, &error) == XS_MIR_OK);
    CHECK(xs_mir_block_shl_i32(entry, bit_xor, one, &shifted_left, &error) == XS_MIR_OK);
    CHECK(xs_mir_block_shr_i32(entry, shifted_left, one, &shifted_right, &error) == XS_MIR_OK);
    CHECK(xs_mir_block_set_return_value(entry, shifted_right, &error) == XS_MIR_OK);
    CHECK(xs_mir_borrow_check_module(mir, &error) == XS_MIR_OK);

    XsLilError lil_error = {0};
    XsLilModule *xlil = nullptr;
    CHECK(xs_lil_module_create("project", &xlil, &lil_error) == XS_LIL_OK);
    CHECK(xs_lil_module_add_mir_function_bodies(xlil, mir, &error) == XS_MIR_OK);
    FILE *stream = tmpfile();
    if(stream == nullptr)
    {
        ++failures;
        xs_lil_module_destroy(xlil);
        xs_mir_module_destroy(mir);
        return;
    }
    CHECK(xs_lil_module_write_text(xlil, stream, &lil_error) == XS_LIL_OK);
    CHECK(fseek(stream, 0, SEEK_SET) == 0);
    char buffer[512] = {0};
    size_t read = fread(buffer, 1, sizeof(buffer) - 1, stream);
    buffer[read] = '\0';
    CHECK(strstr(buffer, "%r2:i32 = add.i32 %r0, %r1\n") != nullptr);
    CHECK(strstr(buffer, "%r3:i32 = mul.i32 %r2, %r1\n") != nullptr);
    CHECK(strstr(buffer, "%r4:i32 = div.i32 %r3, %r1\n") != nullptr);
    CHECK(strstr(buffer, "%r5:i32 = rem.i32 %r4, %r1\n") != nullptr);
    CHECK(strstr(buffer, "%r6:i32 = and.i32 %r2, %r4\n") != nullptr);
    CHECK(strstr(buffer, "%r7:i32 = or.i32 %r6, %r5\n") != nullptr);
    CHECK(strstr(buffer, "%r8:i32 = xor.i32 %r7, %r2\n") != nullptr);
    CHECK(strstr(buffer, "%r9:i32 = shl.i32 %r8, %r0\n") != nullptr);
    CHECK(strstr(buffer, "%r10:i32 = shr.i32 %r9, %r0\n") != nullptr);
    fclose(stream);
    xs_lil_module_destroy(xlil);
    xs_mir_module_destroy(mir);
}

static void test_xlil_body_lowering_for_i32_branch_return(void)
{
    XsMirError error = {0};
    XsMirModule *mir = nullptr;
    CHECK(xs_mir_module_create("project", &mir, &error) == XS_MIR_OK);
    XsMirFunction *function = nullptr;
    CHECK(xs_mir_module_add_function_definition(mir, "main", (XsMirType){.kind = XS_LIL_TYPE_I32}, nullptr, 0,
                                                &function, &error) == XS_MIR_OK);
    XsMirBlock *entry = nullptr;
    XsMirBlock *then_block = nullptr;
    XsMirBlock *else_block = nullptr;
    CHECK(xs_mir_function_append_block(function, "entry", &entry, &error) == XS_MIR_OK);
    CHECK(xs_mir_function_append_block(function, "then", &then_block, &error) == XS_MIR_OK);
    CHECK(xs_mir_function_append_block(function, "else", &else_block, &error) == XS_MIR_OK);
    XsMirValueId condition = 0;
    XsMirValueId then_value = 0;
    XsMirValueId else_value = 0;
    CHECK(xs_mir_block_add_const_bool(entry, true, &condition, &error) == XS_MIR_OK);
    CHECK(xs_mir_block_set_branch(entry, condition, then_block, else_block, &error) == XS_MIR_OK);
    CHECK(xs_mir_block_add_const_i32(then_block, 7, &then_value, &error) == XS_MIR_OK);
    CHECK(xs_mir_block_set_return_value(then_block, then_value, &error) == XS_MIR_OK);
    CHECK(xs_mir_block_add_const_i32(else_block, 3, &else_value, &error) == XS_MIR_OK);
    CHECK(xs_mir_block_set_return_value(else_block, else_value, &error) == XS_MIR_OK);
    CHECK(xs_mir_borrow_check_module(mir, &error) == XS_MIR_OK);

    XsLilError lil_error = {0};
    XsLilModule *xlil = nullptr;
    CHECK(xs_lil_module_create("project", &xlil, &lil_error) == XS_LIL_OK);
    CHECK(xs_lil_module_add_mir_function_bodies(xlil, mir, &error) == XS_MIR_OK);
    FILE *stream = tmpfile();
    if(stream == nullptr)
    {
        ++failures;
        xs_lil_module_destroy(xlil);
        xs_mir_module_destroy(mir);
        return;
    }
    CHECK(xs_lil_module_write_text(xlil, stream, &lil_error) == XS_LIL_OK);
    CHECK(fseek(stream, 0, SEEK_SET) == 0);
    char buffer[768] = {0};
    size_t read = fread(buffer, 1, sizeof(buffer) - 1, stream);
    buffer[read] = '\0';
    CHECK(strstr(buffer, "%r0:bool = const.bool true\n") != nullptr);
    CHECK(strstr(buffer, "br_if %r0, bb1, bb2\n") != nullptr);
    CHECK(strstr(buffer, "bb1.then:\n  %r1:i32 = const.i32 7\n  ret %r1\n") != nullptr);
    CHECK(strstr(buffer, "bb2.else:\n  %r2:i32 = const.i32 3\n  ret %r2\n") != nullptr);
    fclose(stream);
    xs_lil_module_destroy(xlil);
    xs_mir_module_destroy(mir);
}

static void test_xlil_body_lowering_for_call_return(void)
{
    XsMirError error = {0};
    XsMirModule *mir = nullptr;
    CHECK(xs_mir_module_create("project", &mir, &error) == XS_MIR_OK);
    XsMirType parameter = {.kind = XS_LIL_TYPE_I32};
    CHECK(xs_mir_module_add_function_declaration(mir, "Add", (XsMirType){.kind = XS_LIL_TYPE_I32}, &parameter, 1,
                                                 &error) == XS_MIR_OK);
    XsMirFunction *function = nullptr;
    CHECK(xs_mir_module_add_function_definition(mir, "main", (XsMirType){.kind = XS_LIL_TYPE_I32}, nullptr, 0,
                                                &function, &error) == XS_MIR_OK);
    XsMirBlock *entry = nullptr;
    CHECK(xs_mir_function_append_block(function, "entry", &entry, &error) == XS_MIR_OK);
    XsMirValueId argument = 0;
    XsMirValueId result = 0;
    CHECK(xs_mir_block_add_const_i32(entry, 7, &argument, &error) == XS_MIR_OK);
    CHECK(xs_mir_block_add_call(entry, "Add", (XsMirType){.kind = XS_LIL_TYPE_I32}, &argument, 1, &result, &error) ==
          XS_MIR_OK);
    CHECK(xs_mir_block_instruction_kind(entry, 1) == XS_MIR_INSTRUCTION_CALL);
    CHECK(strcmp(xs_mir_block_instruction_callee(entry, 1), "Add") == 0);
    CHECK(xs_mir_block_instruction_argument_count(entry, 1) == 1);
    CHECK(xs_mir_block_instruction_argument(entry, 1, 0) == argument);
    CHECK(xs_mir_block_set_return_value(entry, result, &error) == XS_MIR_OK);
    CHECK(xs_mir_borrow_check_module(mir, &error) == XS_MIR_OK);

    XsLilError lil_error = {0};
    XsLilModule *xlil = nullptr;
    CHECK(xs_lil_module_create("project", &xlil, &lil_error) == XS_LIL_OK);
    CHECK(xs_lil_module_add_mir_function_bodies(xlil, mir, &error) == XS_MIR_OK);
    FILE *stream = tmpfile();
    if(stream == nullptr)
    {
        ++failures;
        xs_lil_module_destroy(xlil);
        xs_mir_module_destroy(mir);
        return;
    }
    CHECK(xs_lil_module_write_text(xlil, stream, &lil_error) == XS_LIL_OK);
    CHECK(fseek(stream, 0, SEEK_SET) == 0);
    char buffer[768] = {0};
    size_t read = fread(buffer, 1, sizeof(buffer) - 1U, stream);
    buffer[read] = '\0';
    CHECK(strstr(buffer, "%r1:i32 = call Add(%r0)\n") != nullptr);
    CHECK(strstr(buffer, "ret %r1\n") != nullptr);
    fclose(stream);
    xs_lil_module_destroy(xlil);
    xs_mir_module_destroy(mir);
}

static void test_xlil_body_lowering_for_bool_not_branch(void)
{
    XsMirError error = {0};
    XsMirModule *mir = nullptr;
    CHECK(xs_mir_module_create("project", &mir, &error) == XS_MIR_OK);
    XsMirFunction *function = nullptr;
    CHECK(xs_mir_module_add_function_definition(mir, "main", (XsMirType){.kind = XS_LIL_TYPE_I32}, nullptr, 0,
                                                &function, &error) == XS_MIR_OK);
    XsMirBlock *entry = nullptr;
    XsMirBlock *then_block = nullptr;
    XsMirBlock *else_block = nullptr;
    CHECK(xs_mir_function_append_block(function, "entry", &entry, &error) == XS_MIR_OK);
    CHECK(xs_mir_function_append_block(function, "then", &then_block, &error) == XS_MIR_OK);
    CHECK(xs_mir_function_append_block(function, "else", &else_block, &error) == XS_MIR_OK);
    XsMirValueId condition = 0;
    XsMirValueId inverted = 0;
    XsMirValueId then_value = 0;
    XsMirValueId else_value = 0;
    CHECK(xs_mir_block_add_const_bool(entry, false, &condition, &error) == XS_MIR_OK);
    CHECK(xs_mir_block_not_bool(entry, condition, &inverted, &error) == XS_MIR_OK);
    CHECK(xs_mir_block_set_branch(entry, inverted, then_block, else_block, &error) == XS_MIR_OK);
    CHECK(xs_mir_block_add_const_i32(then_block, 7, &then_value, &error) == XS_MIR_OK);
    CHECK(xs_mir_block_set_return_value(then_block, then_value, &error) == XS_MIR_OK);
    CHECK(xs_mir_block_add_const_i32(else_block, 2, &else_value, &error) == XS_MIR_OK);
    CHECK(xs_mir_block_set_return_value(else_block, else_value, &error) == XS_MIR_OK);
    CHECK(xs_mir_borrow_check_module(mir, &error) == XS_MIR_OK);

    XsLilError lil_error = {0};
    XsLilModule *xlil = nullptr;
    CHECK(xs_lil_module_create("project", &xlil, &lil_error) == XS_LIL_OK);
    CHECK(xs_lil_module_add_mir_function_bodies(xlil, mir, &error) == XS_MIR_OK);
    FILE *stream = tmpfile();
    if(stream == nullptr)
    {
        ++failures;
        xs_lil_module_destroy(xlil);
        xs_mir_module_destroy(mir);
        return;
    }
    CHECK(xs_lil_module_write_text(xlil, stream, &lil_error) == XS_LIL_OK);
    CHECK(fseek(stream, 0, SEEK_SET) == 0);
    char buffer[768] = {0};
    size_t read = fread(buffer, 1, sizeof(buffer) - 1U, stream);
    buffer[read] = '\0';
    CHECK(strstr(buffer, "%r1:bool = not.bool %r0\n") != nullptr);
    CHECK(strstr(buffer, "br_if %r1, bb1, bb2\n") != nullptr);
    fclose(stream);
    xs_lil_module_destroy(xlil);
    xs_mir_module_destroy(mir);
}

static void test_xlil_body_lowering_for_panic(void)
{
    XsMirError error = {0};
    XsMirModule *mir = nullptr;
    CHECK(xs_mir_module_create("project", &mir, &error) == XS_MIR_OK);
    XsMirFunction *function = nullptr;
    CHECK(xs_mir_module_add_function_definition(mir, "fail", (XsMirType){.kind = XS_LIL_TYPE_VOID}, nullptr, 0,
                                                &function, &error) == XS_MIR_OK);
    XsMirBlock *entry = nullptr;
    CHECK(xs_mir_function_append_block(function, "entry", &entry, &error) == XS_MIR_OK);
    CHECK(xs_mir_block_set_panic(entry, &error) == XS_MIR_OK);
    CHECK(xs_mir_borrow_check_module(mir, &error) == XS_MIR_OK);

    XsLilError lil_error = {0};
    XsLilModule *xlil = nullptr;
    CHECK(xs_lil_module_create("project", &xlil, &lil_error) == XS_LIL_OK);
    CHECK(xs_lil_module_add_mir_function_bodies(xlil, mir, &error) == XS_MIR_OK);
    const XsLilFunction *lowered = xs_lil_module_function_at(xlil, 0);
    const XsLilBlock *block = xs_lil_function_block_at(lowered, 0);
    CHECK(xs_lil_block_terminator_kind(block) == XS_LIL_TERMINATOR_PANIC);
    xs_lil_module_destroy(xlil);
    xs_mir_module_destroy(mir);
}

static void test_xlil_body_lowering_for_local_memory(void)
{
    XsMirError error = {0};
    XsMirModule *mir = nullptr;
    CHECK(xs_mir_module_create("project", &mir, &error) == XS_MIR_OK);
    XsMirFunction *function = nullptr;
    CHECK(xs_mir_module_add_function_definition(mir, "main", (XsMirType){.kind = XS_LIL_TYPE_I32}, nullptr, 0,
                                                &function, &error) == XS_MIR_OK);
    XsMirLocalId local = 0;
    CHECK(xs_mir_function_add_local(function, XS_MIR_LOCAL_VARIABLE, "value", (XsMirType){.kind = XS_LIL_TYPE_I32},
                                    true, &local, &error) == XS_MIR_OK);
    XsMirPlace *place = nullptr;
    CHECK(xs_mir_function_add_local_place(function, local, &place, &error) == XS_MIR_OK);
    XsMirBlock *entry = nullptr;
    CHECK(xs_mir_function_append_block(function, "entry", &entry, &error) == XS_MIR_OK);
    XsMirValueId constant = 0;
    XsMirValueId loaded = 0;
    CHECK(xs_mir_block_add_const_i32(entry, 7, &constant, &error) == XS_MIR_OK);
    CHECK(xs_mir_block_add_store(entry, place, constant, &error) == XS_MIR_OK);
    CHECK(xs_mir_block_add_load(entry, place, (XsMirType){.kind = XS_LIL_TYPE_I32}, &loaded, &error) == XS_MIR_OK);
    CHECK(xs_mir_block_set_return_value(entry, loaded, &error) == XS_MIR_OK);
    CHECK(xs_mir_borrow_check_module(mir, &error) == XS_MIR_OK);

    XsLilError lil_error = {0};
    XsLilModule *xlil = nullptr;
    CHECK(xs_lil_module_create("project", &xlil, &lil_error) == XS_LIL_OK);
    CHECK(xs_lil_module_add_mir_function_bodies(xlil, mir, &error) == XS_MIR_OK);
    const XsLilFunction *lowered = xs_lil_module_function_at(xlil, 0);
    CHECK(xs_lil_function_slot_count(lowered) == 1);
    const XsLilBlock *block = xs_lil_function_block_at(lowered, 0);
    CHECK(xs_lil_block_instruction_kind(block, 1) == XS_LIL_INSTRUCTION_STORE);
    CHECK(xs_lil_block_instruction_kind(block, 2) == XS_LIL_INSTRUCTION_LOAD);
    xs_lil_module_destroy(xlil);
    xs_mir_module_destroy(mir);
}

int main(void)
{
    test_module_and_text_writer();
    test_function_definition_blocks_and_terminators();
    test_borrow_checker_allows_immutable_initialization_and_rejects_reassignment();
    test_borrow_checker_rejects_missing_terminator();
    test_borrow_checker_rejects_unknown_value_operand();
    test_borrow_checker_rejects_wrong_i64_operand_type();
    test_branch_terminator_checks_condition_and_reachability();
    test_borrow_checker_rejects_non_bool_branch_condition();
    test_cfg_optimizer_removes_unreachable_blocks();
    test_constant_optimizer_folds_i64_add();
    test_constant_optimizer_folds_i32_family();
    test_constant_optimizer_folds_bool_branch();
    test_constant_optimizer_folds_bool_not();
    test_hir_function_declaration_lowering();
    test_xlil_declaration_lowering();
    test_xlil_body_lowering_for_const_return();
    test_xlil_body_lowering_for_const_i32_return();
    test_xlil_body_lowering_for_i32_arithmetic_return();
    test_xlil_body_lowering_for_i32_branch_return();
    test_xlil_body_lowering_for_call_return();
    test_xlil_body_lowering_for_bool_not_branch();
    test_xlil_body_lowering_for_panic();
    test_xlil_body_lowering_for_local_memory();
    return failures == 0 ? 0 : 1;
}
