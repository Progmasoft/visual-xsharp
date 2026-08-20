# SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
# SPDX-License-Identifier: MPL-2.0

# Haskell owns source text through verified CorePrep. CMake models that boundary
# as an explicit private executable rather than hiding Cabal inside the native
# driver target.
file(GLOB_RECURSE xs_haskell_frontend_sources CONFIGURE_DEPENDS
  "${CMAKE_CURRENT_SOURCE_DIR}/src/*.hs"
  "${CMAKE_CURRENT_SOURCE_DIR}/app/*.hs"
)

execute_process(
  COMMAND "${XS_CABAL_EXECUTABLE}" list-bin exe:vxs-frontend
  WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}/xs"
  OUTPUT_VARIABLE xs_haskell_frontend_built
  OUTPUT_STRIP_TRAILING_WHITESPACE
  COMMAND_ERROR_IS_FATAL ANY
)

set(XS_HASKELL_FRONTEND
  "${PROJECT_BINARY_DIR}/vxs-frontend${CMAKE_EXECUTABLE_SUFFIX}"
  PARENT_SCOPE
)
set(xs_haskell_frontend_output
  "${PROJECT_BINARY_DIR}/vxs-frontend${CMAKE_EXECUTABLE_SUFFIX}"
)

add_custom_command(
  OUTPUT "${xs_haskell_frontend_output}"
  COMMAND "${XS_CABAL_EXECUTABLE}" build exe:vxs-frontend
  COMMAND "${CMAKE_COMMAND}" -E copy_if_different
          "${xs_haskell_frontend_built}" "${xs_haskell_frontend_output}"
  DEPENDS
    ${xs_haskell_frontend_sources}
    "${CMAKE_CURRENT_SOURCE_DIR}/visual-xsharp-compiler.cabal"
    "${PROJECT_SOURCE_DIR}/xs/cabal.project"
  WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}/xs"
  COMMENT "Building the Haskell Visual X# frontend"
  VERBATIM
)

add_custom_target(vxs_haskell_frontend ALL
  DEPENDS "${xs_haskell_frontend_output}"
)
add_dependencies(vxs vxs_haskell_frontend)
