# SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
# SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.0

# This target preserves useful legacy IR implementation and tests while the
# renewed Core/Xpp/Xmm pipeline adopts selected algorithms. It is intentionally
# isolated from the new compiler target graph.
add_library(xs_lil SHARED
  ../Runtime/int128.c
  builder.c
  memory.c
  model.c
  model_aggregate.c
  model_array.c
  model_float.c
  model_integer.c
  model_integer_operation.c
  model_string.c
  model_string_compare.c
  parser.c
  parser_aggregate.c
  parser_array.c
  parser_scalar.c
  parser_signature.c
  parser_integer_operation.c
  parser_string.c
  producer.c
  text_emit.c
  TypeName.cpp
  verify.c
  writer.c
)

set_target_properties(xs_lil PROPERTIES
  VERSION "${PROJECT_VERSION}"
  SOVERSION 1
  WINDOWS_EXPORT_ALL_SYMBOLS ON
  RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}"
)

xs_add_public_include_roots(xs_lil)
target_compile_definitions(xs_lil PRIVATE XS_LIL_BUILDING_LIBRARY)
get_target_property(xs_lil_library_type xs_lil TYPE)
if(xs_lil_library_type STREQUAL "SHARED_LIBRARY")
  target_compile_definitions(xs_lil PUBLIC XS_LIL_SHARED)
endif()
xs_enable_strict_warnings(xs_lil)
xs_enable_c_compiler_contract(xs_lil PUBLIC)
