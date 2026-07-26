/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <xs-lang.chess031@slmails.com>
 * SPDX-License-Identifier: MPL-2.0
 */

#include "xs/lil-c/api.h"
#include "xs/lil-c/builder.h"
#include "xs/lil-c/function.h"
#include "xs/lil-c/instruction.h"
#include "xs/lil-c/model.h"
#include "xs/lil-c/module.h"
#include "xs/lil-c/text.h"

int main(void)
{
  XsLilError error = {0};
  XsLilModule *module = nullptr;
  if(xs_lil_module_create("PublicCProducer", &module, &error) != XS_LIL_OK)
    return 1;
  XsLilFunction *function = nullptr;
  if(xs_lil_module_add_function_definition(module, "main", (XsLilType){.kind = XS_LIL_TYPE_I32}, nullptr, 0, &function,
                                           &error) != XS_LIL_OK)
  {
    xs_lil_module_destroy(module);
    return 2;
  }
  XsLilBuilder *builder = nullptr;
  XsLilValueId result = XS_LIL_INVALID_VALUE_ID;
  bool valid = xs_lil_c_api_version() == XS_LIL_C_API_VERSION &&
               xs_lil_builder_create(module, &builder, &error) == XS_LIL_OK &&
               xs_lil_builder_append_block(builder, function, "entry", nullptr, &error) == XS_LIL_OK &&
               xs_lil_builder_const_i32(builder, 0, &result, &error) == XS_LIL_OK &&
               xs_lil_builder_return_value(builder, result, &error) == XS_LIL_OK &&
               xs_lil_module_verify(module, &error) == XS_LIL_OK;
  xs_lil_builder_destroy(builder);
  xs_lil_module_destroy(module);
  return valid ? 0 : 3;
}
