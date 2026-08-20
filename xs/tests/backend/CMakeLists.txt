# SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
# SPDX-License-Identifier: MPL-2.0

add_executable(xs_backend_tests "${XS_TEST_SOURCE_ROOT}/backend_tests.c")
target_link_libraries(xs_backend_tests PRIVATE xs_backend_llvm)
add_test(NAME backend COMMAND xs_backend_tests
  ${XS_BACKEND_TEST_OBJECT}
  ${XS_LLD_EXECUTABLE}
)
set_tests_properties(backend PROPERTIES TIMEOUT 15)
xs_prepend_test_runtime_path(backend xs_lil)
