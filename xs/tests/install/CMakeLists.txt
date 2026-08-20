# SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
# SPDX-License-Identifier: MPL-2.0

add_test(NAME compiler_install_layout COMMAND "${CMAKE_COMMAND}"
  -DXS_BUILD_DIR=${CMAKE_BINARY_DIR}
  -DXS_INSTALL_PREFIX=${XS_TEST_WORK_ROOT}/install-root
  -DXS_VERSION=${PROJECT_VERSION}
  -P "${XS_TEST_SOURCE_ROOT}/cmake/install_layout.cmake"
)
set_tests_properties(compiler_install_layout PROPERTIES TIMEOUT 15)
