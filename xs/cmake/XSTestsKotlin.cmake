# SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
# SPDX-License-Identifier: MPL-2.0

add_test(NAME kotlin_project_resolve COMMAND vxs resolve)
set_tests_properties(kotlin_project_resolve PROPERTIES TIMEOUT 60
  WORKING_DIRECTORY "${XS_PROJECT_NATIVE_FIXTURE_DIR}/native_call"
  ENVIRONMENT "XS_PROJECT_RUNTIME=${XS_PROJECT_TEST_DRIVER}"
  FIXTURES_REQUIRED kotlin_project_resolver FIXTURES_SETUP kotlin_project_lock
  PASS_REGULAR_EXPRESSION "refreshed binary lock file 'Visual.XSharp.Lockfile.sqlite3'")
add_test(NAME kotlin_project_resolve_binary_lock COMMAND xs_text_artifact_tests
  ${XS_PROJECT_NATIVE_FIXTURE_DIR}/native_call/Visual.XSharp.Lockfile.sqlite3 "SQLite format 3")
set_tests_properties(kotlin_project_resolve_binary_lock PROPERTIES TIMEOUT 5
  FIXTURES_REQUIRED kotlin_project_lock)

add_test(NAME kotlin_project_call_build COMMAND vxs build)
set_tests_properties(kotlin_project_call_build PROPERTIES TIMEOUT 60
  WORKING_DIRECTORY "${XS_PROJECT_NATIVE_FIXTURE_DIR}/native_call"
  ENVIRONMENT "XS_PROJECT_RUNTIME=${XS_PROJECT_TEST_DRIVER}"
  FIXTURES_REQUIRED kotlin_project_resolver FIXTURES_SETUP kotlin_project_call_native
  PASS_REGULAR_EXPRESSION "wrote optimized LLVM IR.*executable")
add_test(NAME kotlin_project_call_artifacts COMMAND xs_xse_artifact_tests
  ${XS_PROJECT_NATIVE_FIXTURE_DIR}/native_call/sources/main.ll
  ${XS_PROJECT_NATIVE_FIXTURE_DIR}/native_call/sources/main.obj
  ${XS_PROJECT_NATIVE_FIXTURE_DIR}/native_call/sources/main.vxse 7)
set_tests_properties(kotlin_project_call_artifacts PROPERTIES TIMEOUT 5
  FIXTURES_REQUIRED kotlin_project_call_native)
add_test(NAME kotlin_project_lock_artifact COMMAND xs_text_artifact_tests
  ${XS_PROJECT_NATIVE_FIXTURE_DIR}/native_call/Visual.XSharp.Lockfile.sqlite3 "SQLite format 3")
set_tests_properties(kotlin_project_lock_artifact PROPERTIES TIMEOUT 5
  FIXTURES_REQUIRED kotlin_project_call_native)
add_test(NAME kotlin_project_recursive_build COMMAND vxs build)
set_tests_properties(kotlin_project_recursive_build PROPERTIES TIMEOUT 60
  WORKING_DIRECTORY "${XS_PROJECT_NATIVE_FIXTURE_DIR}/recursive"
  ENVIRONMENT "XS_PROJECT_RUNTIME=${XS_PROJECT_TEST_DRIVER}"
  FIXTURES_REQUIRED kotlin_project_resolver FIXTURES_SETUP kotlin_project_recursive
  PASS_REGULAR_EXPRESSION "wrote optimized LLVM IR.*executable")
add_test(NAME kotlin_project_recursive_artifacts COMMAND xs_xse_artifact_tests
  ${XS_PROJECT_NATIVE_FIXTURE_DIR}/recursive/sources/main.ll
  ${XS_PROJECT_NATIVE_FIXTURE_DIR}/recursive/sources/main.obj
  ${XS_PROJECT_NATIVE_FIXTURE_DIR}/recursive/sources/main.vxse 7
  "call i8 @is_even" "call i8 @is_odd")
set_tests_properties(kotlin_project_recursive_artifacts PROPERTIES
  TIMEOUT 5 FIXTURES_REQUIRED kotlin_project_recursive)
add_test(NAME kotlin_project_generic_functions_build COMMAND vxs build)
set_tests_properties(kotlin_project_generic_functions_build PROPERTIES TIMEOUT 60
  WORKING_DIRECTORY "${XS_PROJECT_NATIVE_FIXTURE_DIR}/generic_functions"
  ENVIRONMENT "XS_PROJECT_RUNTIME=${XS_PROJECT_TEST_DRIVER}"
  FIXTURES_REQUIRED kotlin_project_resolver FIXTURES_SETUP kotlin_project_generic_functions
  PASS_REGULAR_EXPRESSION "wrote optimized LLVM IR.*executable")
add_test(NAME kotlin_project_generic_functions_artifacts COMMAND xs_xse_artifact_tests
  ${XS_PROJECT_NATIVE_FIXTURE_DIR}/generic_functions/sources/main.ll
  ${XS_PROJECT_NATIVE_FIXTURE_DIR}/generic_functions/sources/main.obj
  ${XS_PROJECT_NATIVE_FIXTURE_DIR}/generic_functions/sources/main.vxse 7
  "identity$G0$Long")
set_tests_properties(kotlin_project_generic_functions_artifacts PROPERTIES
  TIMEOUT 5 FIXTURES_REQUIRED kotlin_project_generic_functions)
add_test(NAME kotlin_project_multi_file_native_build COMMAND vxs build)
set_tests_properties(kotlin_project_multi_file_native_build PROPERTIES TIMEOUT 60
  WORKING_DIRECTORY "${XS_PROJECT_NATIVE_FIXTURE_DIR}/multi_file"
  ENVIRONMENT "XS_PROJECT_RUNTIME=${XS_PROJECT_TEST_DRIVER}"
  FIXTURES_REQUIRED kotlin_project_resolver FIXTURES_SETUP kotlin_project_multi_file_native
  PASS_REGULAR_EXPRESSION "wrote optimized LLVM IR.*executable")
add_test(NAME kotlin_project_multi_file_native_artifacts COMMAND xs_xse_artifact_tests
  ${XS_PROJECT_NATIVE_FIXTURE_DIR}/multi_file/sources/main.ll
  ${XS_PROJECT_NATIVE_FIXTURE_DIR}/multi_file/sources/main.obj
  ${XS_PROJECT_NATIVE_FIXTURE_DIR}/multi_file/sources/main.vxse 7
  "call i32 @add")
set_tests_properties(kotlin_project_multi_file_native_artifacts PROPERTIES
  TIMEOUT 5 FIXTURES_REQUIRED kotlin_project_multi_file_native)
add_test(NAME kotlin_project_optional_coalesce_build COMMAND vxs build)
set_tests_properties(kotlin_project_optional_coalesce_build PROPERTIES TIMEOUT 60
  WORKING_DIRECTORY "${XS_PROJECT_NATIVE_FIXTURE_DIR}/optional_coalesce"
  ENVIRONMENT "XS_PROJECT_RUNTIME=${XS_PROJECT_TEST_DRIVER}"
  FIXTURES_REQUIRED kotlin_project_resolver FIXTURES_SETUP kotlin_project_optional_coalesce
  PASS_REGULAR_EXPRESSION "wrote optimized LLVM IR.*executable")
add_test(NAME kotlin_project_optional_coalesce_artifacts COMMAND xs_xse_artifact_tests
  ${XS_PROJECT_NATIVE_FIXTURE_DIR}/optional_coalesce/sources/main.ll
  ${XS_PROJECT_NATIVE_FIXTURE_DIR}/optional_coalesce/sources/main.obj
  ${XS_PROJECT_NATIVE_FIXTURE_DIR}/optional_coalesce/sources/main.vxse 9
  "%optional.0 = type { i8, i32 }" "br i1")
set_tests_properties(kotlin_project_optional_coalesce_artifacts PROPERTIES
  TIMEOUT 5 FIXTURES_REQUIRED kotlin_project_optional_coalesce)

add_test(NAME kotlin_project_optional_update_build COMMAND vxs build)
set_tests_properties(kotlin_project_optional_update_build PROPERTIES TIMEOUT 60
  WORKING_DIRECTORY "${XS_PROJECT_NATIVE_FIXTURE_DIR}/optional_update"
  ENVIRONMENT "XS_PROJECT_RUNTIME=${XS_PROJECT_TEST_DRIVER}"
  FIXTURES_REQUIRED kotlin_project_resolver FIXTURES_SETUP kotlin_project_optional_update
  PASS_REGULAR_EXPRESSION "wrote optimized LLVM IR.*executable")

add_test(NAME kotlin_project_optional_update_artifacts COMMAND xs_xse_artifact_tests
  ${XS_PROJECT_NATIVE_FIXTURE_DIR}/optional_update/sources/main.ll
  ${XS_PROJECT_NATIVE_FIXTURE_DIR}/optional_update/sources/main.obj
  ${XS_PROJECT_NATIVE_FIXTURE_DIR}/optional_update/sources/main.vxse 13
  "%optional.0 = type { i8, i32 }" "br i1" "llvm.trap")
set_tests_properties(kotlin_project_optional_update_artifacts PROPERTIES
  TIMEOUT 5 FIXTURES_REQUIRED kotlin_project_optional_update)

add_test(NAME kotlin_project_result_propagation_build COMMAND vxs build)
set_tests_properties(kotlin_project_result_propagation_build PROPERTIES TIMEOUT 60
  WORKING_DIRECTORY "${XS_PROJECT_NATIVE_FIXTURE_DIR}/result_propagation"
  ENVIRONMENT "XS_PROJECT_RUNTIME=${XS_PROJECT_TEST_DRIVER}"
  FIXTURES_REQUIRED kotlin_project_resolver FIXTURES_SETUP kotlin_project_result_propagation
  PASS_REGULAR_EXPRESSION "wrote optimized LLVM IR.*executable")

add_test(NAME kotlin_project_result_propagation_artifacts COMMAND xs_xse_artifact_tests
  ${XS_PROJECT_NATIVE_FIXTURE_DIR}/result_propagation/sources/main.ll
  ${XS_PROJECT_NATIVE_FIXTURE_DIR}/result_propagation/sources/main.obj
  ${XS_PROJECT_NATIVE_FIXTURE_DIR}/result_propagation/sources/main.vxse 13
  "%result.0 = type { i8, i32, i32 }" "extractvalue %result.0" "br i1")
set_tests_properties(kotlin_project_result_propagation_artifacts PROPERTIES
  TIMEOUT 5 FIXTURES_REQUIRED kotlin_project_result_propagation)
add_test(NAME kotlin_project_integer_widths_build COMMAND vxs build)
set_tests_properties(kotlin_project_integer_widths_build PROPERTIES TIMEOUT 60
  WORKING_DIRECTORY "${XS_PROJECT_NATIVE_FIXTURE_DIR}/integer_widths"
  ENVIRONMENT "XS_PROJECT_RUNTIME=${XS_PROJECT_TEST_DRIVER}"
  FIXTURES_REQUIRED kotlin_project_resolver FIXTURES_SETUP kotlin_project_integer_widths
  PASS_REGULAR_EXPRESSION "wrote optimized LLVM IR.*executable")
add_test(NAME kotlin_project_integer_widths_artifacts COMMAND xs_xse_artifact_tests
  ${XS_PROJECT_NATIVE_FIXTURE_DIR}/integer_widths/sources/main.ll
  ${XS_PROJECT_NATIVE_FIXTURE_DIR}/integer_widths/sources/main.obj
  ${XS_PROJECT_NATIVE_FIXTURE_DIR}/integer_widths/sources/main.vxse 0
  "define i128 @integer_min" "ret i128 -1")
set_tests_properties(kotlin_project_integer_widths_artifacts PROPERTIES
  TIMEOUT 5 FIXTURES_REQUIRED kotlin_project_integer_widths)
add_test(NAME kotlin_project_integer_widths_run COMMAND vxs run)
set_tests_properties(kotlin_project_integer_widths_run PROPERTIES TIMEOUT 60
  WORKING_DIRECTORY "${XS_PROJECT_NATIVE_FIXTURE_DIR}/integer_widths"
  ENVIRONMENT "XS_PROJECT_RUNTIME=${XS_PROJECT_TEST_DRIVER}"
  FIXTURES_REQUIRED kotlin_project_resolver
  PASS_REGULAR_EXPRESSION "wrote optimized LLVM IR.*executable")
add_test(NAME kotlin_project_integer_operators_build COMMAND vxs build)
set_tests_properties(kotlin_project_integer_operators_build PROPERTIES TIMEOUT 60
  WORKING_DIRECTORY "${XS_PROJECT_NATIVE_FIXTURE_DIR}/integer_operators"
  ENVIRONMENT "XS_PROJECT_RUNTIME=${XS_PROJECT_TEST_DRIVER}"
  FIXTURES_REQUIRED kotlin_project_resolver FIXTURES_SETUP kotlin_project_integer_operators
  PASS_REGULAR_EXPRESSION "wrote optimized LLVM IR.*executable")
add_test(NAME kotlin_project_integer_operators_artifacts COMMAND xs_xse_artifact_tests
  ${XS_PROJECT_NATIVE_FIXTURE_DIR}/integer_operators/sources/main.ll
  ${XS_PROJECT_NATIVE_FIXTURE_DIR}/integer_operators/sources/main.obj
  ${XS_PROJECT_NATIVE_FIXTURE_DIR}/integer_operators/sources/main.vxse 0
  "sdiv i8" "udiv i64" "icmp ult i128" "icmp slt i128")
set_tests_properties(kotlin_project_integer_operators_artifacts PROPERTIES
  TIMEOUT 5 FIXTURES_REQUIRED kotlin_project_integer_operators)
add_test(NAME kotlin_project_test_validate COMMAND vxs test)
set_tests_properties(kotlin_project_test_validate PROPERTIES TIMEOUT 60
  WORKING_DIRECTORY "${XS_PROJECT_NATIVE_FIXTURE_DIR}/test_command"
  ENVIRONMENT "XS_PROJECT_RUNTIME=${XS_PROJECT_TEST_DRIVER}"
  FIXTURES_REQUIRED kotlin_project_resolver
  PASS_REGULAR_EXPRESSION "test result: ok. 1 passed; 0 failed; 1 ignored")
add_test(NAME kotlin_project_check_excludes_test_registry COMMAND vxs check)
set_tests_properties(kotlin_project_check_excludes_test_registry PROPERTIES TIMEOUT 60
  WORKING_DIRECTORY "${XS_PROJECT_NATIVE_FIXTURE_DIR}/test_command_invalid"
  ENVIRONMENT "XS_PROJECT_RUNTIME=${XS_PROJECT_TEST_DRIVER}"
  FIXTURES_REQUIRED kotlin_project_resolver)
add_test(NAME kotlin_project_test_rejects_invalid_source COMMAND vxs test)
set_tests_properties(kotlin_project_test_rejects_invalid_source PROPERTIES TIMEOUT 60 WILL_FAIL TRUE
  WORKING_DIRECTORY "${XS_PROJECT_NATIVE_FIXTURE_DIR}/test_command_invalid"
  ENVIRONMENT "XS_PROJECT_RUNTIME=${XS_PROJECT_TEST_DRIVER}"
  FIXTURES_REQUIRED kotlin_project_resolver)
add_test(NAME kotlin_project_test_honors_should_panic COMMAND vxs test)
set_tests_properties(kotlin_project_test_honors_should_panic PROPERTIES TIMEOUT 60
  WORKING_DIRECTORY "${XS_PROJECT_NATIVE_FIXTURE_DIR}/test_command_should_panic"
  ENVIRONMENT "XS_PROJECT_RUNTIME=${XS_PROJECT_TEST_DRIVER}"
  FIXTURES_REQUIRED kotlin_project_resolver
  PASS_REGULAR_EXPRESSION "test result: ok. 1 passed; 0 failed; 0 ignored")
add_test(NAME kotlin_project_test_reports_runtime_failure COMMAND vxs test)
set_tests_properties(kotlin_project_test_reports_runtime_failure PROPERTIES TIMEOUT 60 WILL_FAIL TRUE
  WORKING_DIRECTORY "${XS_PROJECT_NATIVE_FIXTURE_DIR}/test_command_fail"
  ENVIRONMENT "XS_PROJECT_RUNTIME=${XS_PROJECT_TEST_DRIVER}"
  FIXTURES_REQUIRED kotlin_project_resolver)
set_tests_properties(
  kotlin_project_resolver_build
  kotlin_project_resolve kotlin_project_resolve_binary_lock
  kotlin_project_call_build kotlin_project_call_artifacts kotlin_project_lock_artifact
  kotlin_project_recursive_build kotlin_project_recursive_artifacts
  kotlin_project_generic_functions_build kotlin_project_generic_functions_artifacts
  kotlin_project_multi_file_native_build kotlin_project_multi_file_native_artifacts
  kotlin_project_optional_coalesce_build kotlin_project_optional_coalesce_artifacts
  kotlin_project_optional_update_build kotlin_project_optional_update_artifacts
  kotlin_project_result_propagation_build kotlin_project_result_propagation_artifacts
  kotlin_project_integer_widths_build kotlin_project_integer_widths_artifacts
  kotlin_project_integer_widths_run
  kotlin_project_integer_operators_build kotlin_project_integer_operators_artifacts
  kotlin_project_test_validate
  kotlin_project_check_excludes_test_registry
  kotlin_project_test_rejects_invalid_source
  kotlin_project_test_honors_should_panic
  kotlin_project_test_reports_runtime_failure
  PROPERTIES LABELS jvm)

set_tests_properties(
  kotlin_project_resolve
  kotlin_project_call_build
  kotlin_project_recursive_build
  kotlin_project_generic_functions_build
  kotlin_project_multi_file_native_build
  kotlin_project_optional_coalesce_build
  kotlin_project_optional_update_build
  kotlin_project_result_propagation_build
  kotlin_project_integer_widths_build
  kotlin_project_integer_widths_run
  kotlin_project_integer_operators_build
  kotlin_project_test_validate
  kotlin_project_check_excludes_test_registry
  kotlin_project_test_rejects_invalid_source
  kotlin_project_test_honors_should_panic
  kotlin_project_test_reports_runtime_failure
  PROPERTIES RESOURCE_LOCK kotlin_script_runner TIMEOUT 120)
