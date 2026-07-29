# SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
# SPDX-License-Identifier: MPL-2.0

add_test(NAME kotlin_project_resolve COMMAND xs resolve)
set_tests_properties(kotlin_project_resolve PROPERTIES TIMEOUT 60
  WORKING_DIRECTORY "${XS_PROJECT_NATIVE_FIXTURE_DIR}/native_call"
  ENVIRONMENT "XS_PROJECT_RUNTIME=${XS_PROJECT_TEST_DRIVER}"
  FIXTURES_REQUIRED kotlin_project_resolver FIXTURES_SETUP kotlin_project_lock
  PASS_REGULAR_EXPRESSION "refreshed binary lock file 'xs.lock.sqlite3'")
add_test(NAME kotlin_project_resolve_binary_lock COMMAND xs_text_artifact_tests
  ${XS_PROJECT_NATIVE_FIXTURE_DIR}/native_call/xs.lock.sqlite3 "SQLite format 3")
set_tests_properties(kotlin_project_resolve_binary_lock PROPERTIES TIMEOUT 5
  FIXTURES_REQUIRED kotlin_project_lock)

add_test(NAME kotlin_project_call_build COMMAND xs build)
set_tests_properties(kotlin_project_call_build PROPERTIES TIMEOUT 60
  WORKING_DIRECTORY "${XS_PROJECT_NATIVE_FIXTURE_DIR}/native_call"
  ENVIRONMENT "XS_PROJECT_RUNTIME=${XS_PROJECT_TEST_DRIVER}"
  FIXTURES_REQUIRED kotlin_project_resolver FIXTURES_SETUP kotlin_project_call_native
  PASS_REGULAR_EXPRESSION "wrote optimized LLVM IR.*executable")
add_test(NAME kotlin_project_call_artifacts COMMAND xs_xse_artifact_tests
  ${XS_PROJECT_NATIVE_FIXTURE_DIR}/native_call/sources/main.ll
  ${XS_PROJECT_NATIVE_FIXTURE_DIR}/native_call/sources/main.o
  ${XS_PROJECT_NATIVE_FIXTURE_DIR}/native_call/sources/main.xse 7)
set_tests_properties(kotlin_project_call_artifacts PROPERTIES TIMEOUT 5
  FIXTURES_REQUIRED kotlin_project_call_native)
add_test(NAME kotlin_project_lock_artifact COMMAND xs_text_artifact_tests
  ${XS_PROJECT_NATIVE_FIXTURE_DIR}/native_call/xs.lock.sqlite3 "SQLite format 3")
set_tests_properties(kotlin_project_lock_artifact PROPERTIES TIMEOUT 5
  FIXTURES_REQUIRED kotlin_project_call_native)
add_test(NAME kotlin_project_recursive_build COMMAND xs build)
set_tests_properties(kotlin_project_recursive_build PROPERTIES TIMEOUT 60
  WORKING_DIRECTORY "${XS_PROJECT_NATIVE_FIXTURE_DIR}/recursive"
  ENVIRONMENT "XS_PROJECT_RUNTIME=${XS_PROJECT_TEST_DRIVER}"
  FIXTURES_REQUIRED kotlin_project_resolver FIXTURES_SETUP kotlin_project_recursive
  PASS_REGULAR_EXPRESSION "wrote optimized LLVM IR.*executable")
add_test(NAME kotlin_project_recursive_artifacts COMMAND xs_xse_artifact_tests
  ${XS_PROJECT_NATIVE_FIXTURE_DIR}/recursive/sources/main.ll
  ${XS_PROJECT_NATIVE_FIXTURE_DIR}/recursive/sources/main.o
  ${XS_PROJECT_NATIVE_FIXTURE_DIR}/recursive/sources/main.xse 7
  "call i8 @is_even" "call i8 @is_odd")
set_tests_properties(kotlin_project_recursive_artifacts PROPERTIES
  TIMEOUT 5 FIXTURES_REQUIRED kotlin_project_recursive)
add_test(NAME kotlin_project_generic_functions_build COMMAND xs build)
set_tests_properties(kotlin_project_generic_functions_build PROPERTIES TIMEOUT 60
  WORKING_DIRECTORY "${XS_PROJECT_NATIVE_FIXTURE_DIR}/generic_functions"
  ENVIRONMENT "XS_PROJECT_RUNTIME=${XS_PROJECT_TEST_DRIVER}"
  FIXTURES_REQUIRED kotlin_project_resolver FIXTURES_SETUP kotlin_project_generic_functions
  PASS_REGULAR_EXPRESSION "wrote optimized LLVM IR.*executable")
add_test(NAME kotlin_project_generic_functions_artifacts COMMAND xs_xse_artifact_tests
  ${XS_PROJECT_NATIVE_FIXTURE_DIR}/generic_functions/sources/main.ll
  ${XS_PROJECT_NATIVE_FIXTURE_DIR}/generic_functions/sources/main.o
  ${XS_PROJECT_NATIVE_FIXTURE_DIR}/generic_functions/sources/main.xse 7
  "identity$G0$Long")
set_tests_properties(kotlin_project_generic_functions_artifacts PROPERTIES
  TIMEOUT 5 FIXTURES_REQUIRED kotlin_project_generic_functions)
add_test(NAME kotlin_project_multi_file_native_build COMMAND xs build)
set_tests_properties(kotlin_project_multi_file_native_build PROPERTIES TIMEOUT 60
  WORKING_DIRECTORY "${XS_PROJECT_NATIVE_FIXTURE_DIR}/multi_file"
  ENVIRONMENT "XS_PROJECT_RUNTIME=${XS_PROJECT_TEST_DRIVER}"
  FIXTURES_REQUIRED kotlin_project_resolver FIXTURES_SETUP kotlin_project_multi_file_native
  PASS_REGULAR_EXPRESSION "wrote optimized LLVM IR.*executable")
add_test(NAME kotlin_project_multi_file_native_artifacts COMMAND xs_xse_artifact_tests
  ${XS_PROJECT_NATIVE_FIXTURE_DIR}/multi_file/sources/main.ll
  ${XS_PROJECT_NATIVE_FIXTURE_DIR}/multi_file/sources/main.o
  ${XS_PROJECT_NATIVE_FIXTURE_DIR}/multi_file/sources/main.xse 7
  "call i32 @add")
set_tests_properties(kotlin_project_multi_file_native_artifacts PROPERTIES
  TIMEOUT 5 FIXTURES_REQUIRED kotlin_project_multi_file_native)
foreach(output hir mir xlil)
  string(TOUPPER "${output}" output_upper)
  set(output_extension ".x${output}")
  set(function_record "function")
  if(output STREQUAL "xlil")
    set(output_extension ".xlil")
    set(function_record ".func")
  else()
    set(output_upper "X${output_upper}")
  endif()
  add_test(NAME kotlin_project_output_${output} COMMAND xs build --output ${output})
  set_tests_properties(kotlin_project_output_${output} PROPERTIES TIMEOUT 60
    WORKING_DIRECTORY "${XS_PROJECT_NATIVE_FIXTURE_DIR}/multi_file"
    ENVIRONMENT "XS_PROJECT_RUNTIME=${XS_PROJECT_TEST_DRIVER}"
    FIXTURES_REQUIRED kotlin_project_resolver
    PASS_REGULAR_EXPRESSION "wrote ${output_upper}")
  add_test(NAME kotlin_project_output_${output}_artifact COMMAND xs_text_artifact_tests
    ${XS_PROJECT_NATIVE_FIXTURE_DIR}/multi_file/sources/main${output_extension}
    "${output_extension} version 0" "${function_record} add" "${function_record} main")
  set_tests_properties(kotlin_project_output_${output}_artifact PROPERTIES TIMEOUT 5
    DEPENDS kotlin_project_output_${output})
endforeach()
add_test(NAME kotlin_project_integer_widths_build COMMAND xs build)
set_tests_properties(kotlin_project_integer_widths_build PROPERTIES TIMEOUT 60
  WORKING_DIRECTORY "${XS_PROJECT_NATIVE_FIXTURE_DIR}/integer_widths"
  ENVIRONMENT "XS_PROJECT_RUNTIME=${XS_PROJECT_TEST_DRIVER}"
  FIXTURES_REQUIRED kotlin_project_resolver FIXTURES_SETUP kotlin_project_integer_widths
  PASS_REGULAR_EXPRESSION "wrote optimized LLVM IR.*executable")
add_test(NAME kotlin_project_integer_widths_artifacts COMMAND xs_xse_artifact_tests
  ${XS_PROJECT_NATIVE_FIXTURE_DIR}/integer_widths/sources/main.ll
  ${XS_PROJECT_NATIVE_FIXTURE_DIR}/integer_widths/sources/main.o
  ${XS_PROJECT_NATIVE_FIXTURE_DIR}/integer_widths/sources/main.xse 0
  "define i128 @integer_min" "ret i128 -1")
set_tests_properties(kotlin_project_integer_widths_artifacts PROPERTIES
  TIMEOUT 5 FIXTURES_REQUIRED kotlin_project_integer_widths)
add_test(NAME kotlin_project_integer_widths_run COMMAND xs run)
set_tests_properties(kotlin_project_integer_widths_run PROPERTIES TIMEOUT 60
  WORKING_DIRECTORY "${XS_PROJECT_NATIVE_FIXTURE_DIR}/integer_widths"
  ENVIRONMENT "XS_PROJECT_RUNTIME=${XS_PROJECT_TEST_DRIVER}"
  FIXTURES_REQUIRED kotlin_project_resolver
  PASS_REGULAR_EXPRESSION "wrote optimized LLVM IR.*executable")
add_test(NAME kotlin_project_integer_operators_build COMMAND xs build)
set_tests_properties(kotlin_project_integer_operators_build PROPERTIES TIMEOUT 60
  WORKING_DIRECTORY "${XS_PROJECT_NATIVE_FIXTURE_DIR}/integer_operators"
  ENVIRONMENT "XS_PROJECT_RUNTIME=${XS_PROJECT_TEST_DRIVER}"
  FIXTURES_REQUIRED kotlin_project_resolver FIXTURES_SETUP kotlin_project_integer_operators
  PASS_REGULAR_EXPRESSION "wrote optimized LLVM IR.*executable")
add_test(NAME kotlin_project_integer_operators_artifacts COMMAND xs_xse_artifact_tests
  ${XS_PROJECT_NATIVE_FIXTURE_DIR}/integer_operators/sources/main.ll
  ${XS_PROJECT_NATIVE_FIXTURE_DIR}/integer_operators/sources/main.o
  ${XS_PROJECT_NATIVE_FIXTURE_DIR}/integer_operators/sources/main.xse 0
  "sdiv i8" "udiv i64" "icmp ult i128" "icmp slt i128")
set_tests_properties(kotlin_project_integer_operators_artifacts PROPERTIES
  TIMEOUT 5 FIXTURES_REQUIRED kotlin_project_integer_operators)
add_test(NAME kotlin_project_module_check COMMAND xs check --module ./Modules)
set_tests_properties(kotlin_project_module_check PROPERTIES TIMEOUT 60
  WORKING_DIRECTORY "${XS_PROJECT_NATIVE_FIXTURE_DIR}/modules"
  ENVIRONMENT "XS_PROJECT_RUNTIME=${XS_PROJECT_TEST_DRIVER}"
  FIXTURES_REQUIRED kotlin_project_resolver
  PASS_REGULAR_EXPRESSION "source\\[1\\].*Modules/Math/add.xs")
add_test(NAME kotlin_project_module_requires_root COMMAND xs check)
set_tests_properties(kotlin_project_module_requires_root PROPERTIES TIMEOUT 60 WILL_FAIL TRUE
  WORKING_DIRECTORY "${XS_PROJECT_NATIVE_FIXTURE_DIR}/modules"
  ENVIRONMENT "XS_PROJECT_RUNTIME=${XS_PROJECT_TEST_DRIVER}"
  FIXTURES_REQUIRED kotlin_project_resolver)
add_test(NAME kotlin_project_test_validate COMMAND xs test)
set_tests_properties(kotlin_project_test_validate PROPERTIES TIMEOUT 60
  WORKING_DIRECTORY "${XS_PROJECT_NATIVE_FIXTURE_DIR}/test_command"
  ENVIRONMENT "XS_PROJECT_RUNTIME=${XS_PROJECT_TEST_DRIVER}"
  FIXTURES_REQUIRED kotlin_project_resolver
  PASS_REGULAR_EXPRESSION "test result: ok. 1 passed; 0 failed; 1 ignored")
add_test(NAME kotlin_project_check_excludes_test_registry COMMAND xs check)
set_tests_properties(kotlin_project_check_excludes_test_registry PROPERTIES TIMEOUT 60
  WORKING_DIRECTORY "${XS_PROJECT_NATIVE_FIXTURE_DIR}/test_command_invalid"
  ENVIRONMENT "XS_PROJECT_RUNTIME=${XS_PROJECT_TEST_DRIVER}"
  FIXTURES_REQUIRED kotlin_project_resolver)
add_test(NAME kotlin_project_test_rejects_invalid_source COMMAND xs test)
set_tests_properties(kotlin_project_test_rejects_invalid_source PROPERTIES TIMEOUT 60 WILL_FAIL TRUE
  WORKING_DIRECTORY "${XS_PROJECT_NATIVE_FIXTURE_DIR}/test_command_invalid"
  ENVIRONMENT "XS_PROJECT_RUNTIME=${XS_PROJECT_TEST_DRIVER}"
  FIXTURES_REQUIRED kotlin_project_resolver)
add_test(NAME kotlin_project_test_honors_should_panic COMMAND xs test)
set_tests_properties(kotlin_project_test_honors_should_panic PROPERTIES TIMEOUT 60
  WORKING_DIRECTORY "${XS_PROJECT_NATIVE_FIXTURE_DIR}/test_command_should_panic"
  ENVIRONMENT "XS_PROJECT_RUNTIME=${XS_PROJECT_TEST_DRIVER}"
  FIXTURES_REQUIRED kotlin_project_resolver
  PASS_REGULAR_EXPRESSION "test result: ok. 1 passed; 0 failed; 0 ignored")
add_test(NAME kotlin_project_test_reports_runtime_failure COMMAND xs test)
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
  kotlin_project_output_hir kotlin_project_output_hir_artifact
  kotlin_project_output_mir kotlin_project_output_mir_artifact
  kotlin_project_output_xlil kotlin_project_output_xlil_artifact
  kotlin_project_integer_widths_build kotlin_project_integer_widths_artifacts
  kotlin_project_integer_widths_run
  kotlin_project_integer_operators_build kotlin_project_integer_operators_artifacts
  kotlin_project_module_check
  kotlin_project_module_requires_root
  kotlin_project_test_validate
  kotlin_project_check_excludes_test_registry
  kotlin_project_test_rejects_invalid_source
  kotlin_project_test_honors_should_panic
  kotlin_project_test_reports_runtime_failure
  PROPERTIES LABELS jvm)
