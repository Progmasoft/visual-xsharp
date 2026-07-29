/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 */

#include "xs/lil.h"
#include "xs/mir.h"
#include "xs/mir/borrow_checker.h"

#include <stdio.h>

static int failures;

#define CHECK(condition)                                                                                               \
  do                                                                                                                   \
  {                                                                                                                    \
    if(!(condition))                                                                                                   \
    {                                                                                                                  \
      fprintf(stderr, "%s:%d: check failed: %s\n", __FILE__, __LINE__, #condition);                                    \
      ++failures;                                                                                                      \
    }                                                                                                                  \
  } while(0)

static void test_immutable_initialization_on_exclusive_branches(void)
{
  XsMirError error = {0};
  XsMirModule *module = nullptr;
  CHECK(xs_mir_module_create("flow", &module, &error) == XS_MIR_OK);
  XsMirFunction *function = nullptr;
  CHECK(xs_mir_module_add_function_definition(module, "App.Flow", (XsMirType){.kind = XS_LIL_TYPE_I64}, nullptr, 0,
                                              &function, &error) == XS_MIR_OK);
  XsMirLocalId local = 0;
  CHECK(xs_mir_function_add_local(function, XS_MIR_LOCAL_VARIABLE, "value", (XsMirType){.kind = XS_LIL_TYPE_I64}, false,
                                  &local, &error) == XS_MIR_OK);
  XsMirPlace *place = nullptr;
  CHECK(xs_mir_function_add_local_place(function, local, &place, &error) == XS_MIR_OK);
  XsMirBlock *entry = nullptr;
  XsMirBlock *then_block = nullptr;
  XsMirBlock *else_block = nullptr;
  XsMirBlock *join = nullptr;
  CHECK(xs_mir_function_append_block(function, "entry", &entry, &error) == XS_MIR_OK);
  CHECK(xs_mir_function_append_block(function, "then", &then_block, &error) == XS_MIR_OK);
  CHECK(xs_mir_function_append_block(function, "else", &else_block, &error) == XS_MIR_OK);
  CHECK(xs_mir_function_append_block(function, "join", &join, &error) == XS_MIR_OK);
  XsMirValueId condition = 0;
  XsMirValueId value = 0;
  XsMirValueId loaded = 0;
  CHECK(xs_mir_block_add_const_bool(entry, true, &condition, &error) == XS_MIR_OK);
  CHECK(xs_mir_block_set_branch(entry, condition, then_block, else_block, &error) == XS_MIR_OK);
  CHECK(xs_mir_block_add_const_i64(then_block, 1, &value, &error) == XS_MIR_OK);
  CHECK(xs_mir_block_add_store(then_block, place, value, &error) == XS_MIR_OK);
  CHECK(xs_mir_block_set_goto(then_block, join, &error) == XS_MIR_OK);
  CHECK(xs_mir_block_add_store(else_block, place, value, &error) == XS_MIR_OK);
  CHECK(xs_mir_block_set_goto(else_block, join, &error) == XS_MIR_OK);
  CHECK(xs_mir_block_add_load(join, place, (XsMirType){.kind = XS_LIL_TYPE_I64}, &loaded, &error) == XS_MIR_OK);
  CHECK(xs_mir_block_set_return_value(join, loaded, &error) == XS_MIR_OK);
  CHECK(xs_mir_borrow_check_module(module, &error) == XS_MIR_OK);
  xs_mir_module_destroy(module);
}

static void test_borrow_checker_rejects_load_not_initialized_on_every_path(void)
{
  XsMirError error = {0};
  XsMirModule *module = nullptr;
  CHECK(xs_mir_module_create("flow", &module, &error) == XS_MIR_OK);
  XsMirFunction *function = nullptr;
  CHECK(xs_mir_module_add_function_definition(module, "App.Uninitialized", (XsMirType){.kind = XS_LIL_TYPE_I64},
                                              nullptr, 0, &function, &error) == XS_MIR_OK);
  XsMirLocalId local = 0;
  CHECK(xs_mir_function_add_local(function, XS_MIR_LOCAL_VARIABLE, "value", (XsMirType){.kind = XS_LIL_TYPE_I64}, true,
                                  &local, &error) == XS_MIR_OK);
  XsMirPlace *place = nullptr;
  CHECK(xs_mir_function_add_local_place(function, local, &place, &error) == XS_MIR_OK);
  XsMirBlock *entry = nullptr;
  XsMirBlock *then_block = nullptr;
  XsMirBlock *else_block = nullptr;
  XsMirBlock *join = nullptr;
  CHECK(xs_mir_function_append_block(function, "entry", &entry, &error) == XS_MIR_OK);
  CHECK(xs_mir_function_append_block(function, "then", &then_block, &error) == XS_MIR_OK);
  CHECK(xs_mir_function_append_block(function, "else", &else_block, &error) == XS_MIR_OK);
  CHECK(xs_mir_function_append_block(function, "join", &join, &error) == XS_MIR_OK);
  XsMirValueId condition = 0;
  XsMirValueId value = 0;
  XsMirValueId loaded = 0;
  CHECK(xs_mir_block_add_const_bool(entry, true, &condition, &error) == XS_MIR_OK);
  CHECK(xs_mir_block_set_branch(entry, condition, then_block, else_block, &error) == XS_MIR_OK);
  CHECK(xs_mir_block_add_const_i64(then_block, 1, &value, &error) == XS_MIR_OK);
  CHECK(xs_mir_block_add_store(then_block, place, value, &error) == XS_MIR_OK);
  CHECK(xs_mir_block_set_goto(then_block, join, &error) == XS_MIR_OK);
  CHECK(xs_mir_block_set_goto(else_block, join, &error) == XS_MIR_OK);
  CHECK(xs_mir_block_add_load(join, place, (XsMirType){.kind = XS_LIL_TYPE_I64}, &loaded, &error) == XS_MIR_OK);
  CHECK(xs_mir_block_set_return_value(join, loaded, &error) == XS_MIR_OK);
  CHECK(xs_mir_borrow_check_module(module, &error) == XS_MIR_INVALID_ARGUMENT);
  xs_mir_module_destroy(module);
}

int main(void)
{
  test_immutable_initialization_on_exclusive_branches();
  test_borrow_checker_rejects_load_not_initialized_on_every_path();
  return failures == 0 ? 0 : 1;
}
