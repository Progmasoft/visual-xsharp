# SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
# SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.0

set(vxs_gradle_executable "${PROJECT_SOURCE_DIR}/ProjectSystem/gradlew.bat")
set(vxs_project_test_classpath
  "${PROJECT_SOURCE_DIR}/ProjectSystem/build/install/vxdc/lib/*"
)
set(xs_vxdc_test_driver
  "${PROJECT_SOURCE_DIR}/ProjectSystem/build/install/vxdc/bin/vxdc.bat"
)

add_test(NAME kotlin_project_resolver_build
  COMMAND "${vxs_gradle_executable}" --daemon --build-cache
          -p "${PROJECT_SOURCE_DIR}/ProjectSystem" installDist
)
set_tests_properties(kotlin_project_resolver_build PROPERTIES
  TIMEOUT 180
  FIXTURES_SETUP kotlin_project_resolver
  ENVIRONMENT "GRADLE_OPTS=-Xmx512m"
)

# Copy project fixtures so lockfiles and emitted artifacts remain disposable.
set(vxs_project_fixture_dir "${XS_TEST_WORK_ROOT}/projects")
file(COPY "${XS_TEST_SOURCE_ROOT}/fixtures/projects/"
     DESTINATION "${vxs_project_fixture_dir}")

set(xs_assert_file_contains_script
  "${CMAKE_CURRENT_LIST_DIR}/AssertFileContainsHex.cmake"
)

add_test(NAME kotlin_project_resolve COMMAND vxs resolve)
set_tests_properties(kotlin_project_resolve PROPERTIES
  TIMEOUT 180
  WORKING_DIRECTORY "${vxs_project_fixture_dir}/native_call"
  ENVIRONMENT "XS_PROJECT_EVALUATOR_CLASSPATH=${vxs_project_test_classpath}"
  FIXTURES_REQUIRED kotlin_project_resolver
  FIXTURES_SETUP kotlin_project_lock
  PASS_REGULAR_EXPRESSION "refreshed binary lock file 'Visual.XSharp.Lockfile.sqlite3'"
  LABELS jvm
  RESOURCE_LOCK kotlin_script_runner
  RUN_SERIAL TRUE
)

add_test(NAME kotlin_project_resolve_binary_lock COMMAND "${CMAKE_COMMAND}"
  "-DINPUT=${vxs_project_fixture_dir}/native_call/Visual.XSharp.Lockfile.sqlite3"
  "-DEXPECTED_HEX=53514c69746520666f726d61742033"
  -P "${xs_assert_file_contains_script}"
)
set_tests_properties(kotlin_project_resolve_binary_lock PROPERTIES
  TIMEOUT 15
  FIXTURES_REQUIRED kotlin_project_lock
  LABELS jvm
)

add_test(NAME compiler_check_project COMMAND vxs check)
set_tests_properties(compiler_check_project PROPERTIES
  TIMEOUT 180
  WORKING_DIRECTORY "${vxs_project_fixture_dir}/native_call"
  ENVIRONMENT "XS_PROJECT_EVALUATOR_CLASSPATH=${vxs_project_test_classpath}"
  FIXTURES_REQUIRED kotlin_project_resolver
  PASS_REGULAR_EXPRESSION "is valid through the LLVM boundary"
  LABELS "jvm;compiler"
  RESOURCE_LOCK kotlin_script_runner
  RUN_SERIAL TRUE
)

set(vxs_project_core_artifact
  "${vxs_project_fixture_dir}/native_call/build/debug/Main.core"
)
file(REMOVE "${vxs_project_core_artifact}")
add_test(NAME compiler_emits_project_core COMMAND vxs build -Emit core)
set_tests_properties(compiler_emits_project_core PROPERTIES
  TIMEOUT 180
  WORKING_DIRECTORY "${vxs_project_fixture_dir}/native_call"
  ENVIRONMENT "XS_PROJECT_EVALUATOR_CLASSPATH=${vxs_project_test_classpath}"
  FIXTURES_REQUIRED kotlin_project_resolver
  FIXTURES_SETUP project_core_artifact
  PASS_REGULAR_EXPRESSION "wrote.*Main.core"
  LABELS "jvm;compiler"
  RESOURCE_LOCK kotlin_script_runner
  RUN_SERIAL TRUE
)

# Project mode adds Kotlin evaluation, source discovery, namespace entry
# selection, and configured output placement before the shared native backend.
set(vxs_project_native_executable
  "${vxs_project_fixture_dir}/native_call/build/debug/Main.vxse"
)
file(REMOVE "${vxs_project_native_executable}")
add_test(NAME compiler_builds_project_native COMMAND vxs build)
set_tests_properties(compiler_builds_project_native PROPERTIES
  TIMEOUT 180
  WORKING_DIRECTORY "${vxs_project_fixture_dir}/native_call"
  ENVIRONMENT "XS_PROJECT_EVALUATOR_CLASSPATH=${vxs_project_test_classpath}"
  FIXTURES_REQUIRED kotlin_project_resolver
  FIXTURES_SETUP project_native_executable
  PASS_REGULAR_EXPRESSION "linked native executable.*Main.vxse"
  LABELS "jvm;compiler"
  RESOURCE_LOCK kotlin_script_runner
  RUN_SERIAL TRUE
)
add_test(NAME compiler_executes_project_native COMMAND "${vxs_project_native_executable}")
set_tests_properties(compiler_executes_project_native PROPERTIES
  TIMEOUT 10
  FIXTURES_REQUIRED project_native_executable
  LABELS compiler
)

add_test(NAME compiler_rechecks_project_core COMMAND vxs check
  -Build core -File "${vxs_project_core_artifact}"
)
set_tests_properties(compiler_rechecks_project_core PROPERTIES
  TIMEOUT 30
  FIXTURES_REQUIRED project_core_artifact
  PASS_REGULAR_EXPRESSION "is valid through the LLVM boundary"
  LABELS compiler
)

set(xs_vxdc_output
  "${vxs_project_fixture_dir}/native_call/NativeCall.sqlite3.dump"
)
file(REMOVE "${xs_vxdc_output}")
add_test(NAME vxdc_project_dump COMMAND "${xs_vxdc_test_driver}"
  -Projectfile "${vxs_project_fixture_dir}/native_call/Visual.XSharp.kts"
  -Output "${xs_vxdc_output}"
)
set_tests_properties(vxdc_project_dump PROPERTIES
  TIMEOUT 180
  FIXTURES_REQUIRED kotlin_project_resolver
  FIXTURES_SETUP vxdc_project_dump
  PASS_REGULAR_EXPRESSION "vxdc: wrote"
  LABELS jvm
  RESOURCE_LOCK kotlin_script_runner
  RUN_SERIAL TRUE
)

add_test(NAME vxdc_project_dump_is_sql COMMAND "${CMAKE_COMMAND}"
  "-DINPUT=${xs_vxdc_output}"
  "-DEXPECTED_HEX=424547494e205452414e53414354494f4e3b"
  -P "${xs_assert_file_contains_script}"
)
set_tests_properties(vxdc_project_dump_is_sql PROPERTIES
  TIMEOUT 15
  FIXTURES_REQUIRED vxdc_project_dump
  LABELS jvm
)
