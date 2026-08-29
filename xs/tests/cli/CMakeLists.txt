# SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
# SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.0

add_test(NAME cli_version COMMAND vxs --version)
string(REPLACE "." "\\." vxs_project_version_regex "${PROJECT_VERSION}")
set_tests_properties(cli_version PROPERTIES
  TIMEOUT 5
  PASS_REGULAR_EXPRESSION "vxs ${vxs_project_version_regex}"
)

add_test(NAME cli_version_command COMMAND vxs version)
set_tests_properties(cli_version_command PROPERTIES
  TIMEOUT 5
  PASS_REGULAR_EXPRESSION "vxs ${vxs_project_version_regex}"
)

add_test(NAME cli_help COMMAND vxs --help)
set_tests_properties(cli_help PROPERTIES
  TIMEOUT 5
  PASS_REGULAR_EXPRESSION "Commands:"
)

add_test(NAME cli_build_help COMMAND vxs build --help)
set_tests_properties(cli_build_help PROPERTIES
  TIMEOUT 5
  PASS_REGULAR_EXPRESSION "-Emit"
)

add_test(NAME cli_check_help COMMAND vxs check --help)
set_tests_properties(cli_check_help PROPERTIES
  TIMEOUT 5
  PASS_REGULAR_EXPRESSION "-Build"
  FAIL_REGULAR_EXPRESSION "-Emit"
)

add_test(NAME cli_install_help COMMAND vxs install --help)
set_tests_properties(cli_install_help PROPERTIES
  TIMEOUT 5
  PASS_REGULAR_EXPRESSION "Publisher.Name"
)

add_executable(xs_cli_parser_tests
  "${XS_TEST_SOURCE_ROOT}/CliParserTests.cpp"
  "${PROJECT_SOURCE_DIR}/xs/sources/driver/Options.cpp"
)
target_include_directories(xs_cli_parser_tests PRIVATE "${PROJECT_SOURCE_DIR}")
target_compile_definitions(xs_cli_parser_tests PRIVATE
  XS_PROJECT_VERSION="${PROJECT_VERSION}"
)
xs_enable_strict_warnings(xs_cli_parser_tests)
target_link_libraries(xs_cli_parser_tests PRIVATE Catch2::Catch2WithMain)
add_test(NAME cli_parser_model COMMAND xs_cli_parser_tests)
set_tests_properties(cli_parser_model PROPERTIES TIMEOUT 15)
# This test does not link xs_lil, but an ASan-instrumented executable still
# needs the dynamic sanitizer runtime copied beside vxs at the output root.
xs_prepend_test_runtime_path(cli_parser_model vxs)

# These cases prove schema invariants independently from source compilation.
add_test(NAME cli_rejects_missing_value COMMAND vxs build -File)
set_tests_properties(cli_rejects_missing_value PROPERTIES TIMEOUT 5 WILL_FAIL TRUE)

add_test(NAME cli_requires_install_coordinate COMMAND vxs install)
set_tests_properties(cli_requires_install_coordinate PROPERTIES TIMEOUT 5 WILL_FAIL TRUE)
