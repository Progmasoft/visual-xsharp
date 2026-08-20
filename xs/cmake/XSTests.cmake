# SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
# SPDX-License-Identifier: MPL-2.0

if(NOT XS_BUILD_PROJECT_XS)
  return()
endif()

# Production targets are declared by xs before the repository-level test gate
# includes this file. Delegate test ownership back to the same component tree
# instead of rebuilding one global list here.
add_subdirectory("${PROJECT_SOURCE_DIR}/xs/tests"
                 "${PROJECT_BINARY_DIR}/projects/xs/tests")
