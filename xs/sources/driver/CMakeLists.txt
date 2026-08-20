# SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
# SPDX-License-Identifier: MPL-2.0

# Preserve xs_compiler as the established integration target while making its
# responsibility precise: typed CLI options, project evaluation, and dispatch
# around the separately linkable compiler pipeline.
add_library(xs_compiler STATIC
  Cli.cpp
  CorePipeline.cpp
  Options.cpp
  ProjectDriver.cpp
)

xs_add_public_include_roots(xs_compiler)
target_link_libraries(xs_compiler
  PUBLIC xs_compiler_pipeline
  PRIVATE fmt::fmt-header-only
)
target_compile_definitions(xs_compiler PRIVATE
  XS_PROJECT_VERSION="${PROJECT_VERSION}"
)
xs_enable_strict_warnings(xs_compiler)

add_executable(vxs ../Main.cpp)
set_target_properties(vxs PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}"
)
if(APPLE)
  set_target_properties(vxs PROPERTIES INSTALL_RPATH "@loader_path/../lib")
elseif(UNIX)
  set_target_properties(vxs PROPERTIES INSTALL_RPATH "\$ORIGIN/../lib")
endif()
target_link_libraries(vxs PRIVATE xs_compiler)
