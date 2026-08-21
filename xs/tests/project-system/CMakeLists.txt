# SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
# SPDX-License-Identifier: MPL-2.0

set(xs_gradle_executable "${PROJECT_SOURCE_DIR}/xs_kts/gradlew.bat")
set(xs_project_test_classpath
  "${PROJECT_SOURCE_DIR}/xs_kts/build/install/vxdc/lib/*"
)
set(xs_vxdc_test_driver
  "${PROJECT_SOURCE_DIR}/xs_kts/build/install/vxdc/bin/vxdc.bat"
)

add_test(NAME kotlin_project_resolver_build
  COMMAND "${xs_gradle_executable}" --daemon --build-cache
          -p "${PROJECT_SOURCE_DIR}/xs_kts" installDist
)
set_tests_properties(kotlin_project_resolver_build PROPERTIES
  TIMEOUT 180
  FIXTURES_SETUP kotlin_project_resolver
  ENVIRONMENT "GRADLE_OPTS=-Xmx512m"
)

# Copy project fixtures so lockfiles and emitted artifacts remain disposable.
set(xs_project_fixture_dir "${XS_TEST_WORK_ROOT}/projects")
file(COPY "${XS_TEST_SOURCE_ROOT}/fixtures/projects/"
     DESTINATION "${xs_project_fixture_dir}")

add_executable(xs_text_artifact_tests
  "${XS_TEST_SOURCE_ROOT}/text_artifact_tests.c"
)

add_test(NAME kotlin_project_resolve COMMAND vxs resolve)
set_tests_properties(kotlin_project_resolve PROPERTIES
  TIMEOUT 180
  WORKING_DIRECTORY "${xs_project_fixture_dir}/native_call"
  ENVIRONMENT "XS_PROJECT_EVALUATOR_CLASSPATH=${xs_project_test_classpath}"
  FIXTURES_REQUIRED kotlin_project_resolver
  FIXTURES_SETUP kotlin_project_lock
  PASS_REGULAR_EXPRESSION "refreshed binary lock file 'Visual.XSharp.Lockfile.sqlite3'"
  LABELS jvm
  RESOURCE_LOCK kotlin_script_runner
  RUN_SERIAL TRUE
)

add_test(NAME kotlin_project_resolve_binary_lock COMMAND xs_text_artifact_tests
  "${xs_project_fixture_dir}/native_call/Visual.XSharp.Lockfile.sqlite3"
  "SQLite format 3"
)
set_tests_properties(kotlin_project_resolve_binary_lock PROPERTIES
  TIMEOUT 5
  FIXTURES_REQUIRED kotlin_project_lock
  LABELS jvm
)

add_test(NAME compiler_check_project COMMAND vxs check)
set_tests_properties(compiler_check_project PROPERTIES
  TIMEOUT 180
  WORKING_DIRECTORY "${xs_project_fixture_dir}/native_call"
  ENVIRONMENT "XS_PROJECT_EVALUATOR_CLASSPATH=${xs_project_test_classpath}"
  FIXTURES_REQUIRED kotlin_project_resolver
  PASS_REGULAR_EXPRESSION "is valid through the LLVM boundary"
  LABELS "jvm;compiler"
  RESOURCE_LOCK kotlin_script_runner
  RUN_SERIAL TRUE
)

set(xs_project_core_artifact
  "${xs_project_fixture_dir}/native_call/build/debug/Main.core"
)
file(REMOVE "${xs_project_core_artifact}")
add_test(NAME compiler_emits_project_core COMMAND vxs build -Emit core)
set_tests_properties(compiler_emits_project_core PROPERTIES
  TIMEOUT 180
  WORKING_DIRECTORY "${xs_project_fixture_dir}/native_call"
  ENVIRONMENT "XS_PROJECT_EVALUATOR_CLASSPATH=${xs_project_test_classpath}"
  FIXTURES_REQUIRED kotlin_project_resolver
  FIXTURES_SETUP project_core_artifact
  PASS_REGULAR_EXPRESSION "wrote.*Main.core"
  LABELS "jvm;compiler"
  RESOURCE_LOCK kotlin_script_runner
  RUN_SERIAL TRUE
)

add_test(NAME compiler_rechecks_project_core COMMAND vxs check
  -Build core -File "${xs_project_core_artifact}"
)
set_tests_properties(compiler_rechecks_project_core PROPERTIES
  TIMEOUT 30
  FIXTURES_REQUIRED project_core_artifact
  PASS_REGULAR_EXPRESSION "is valid through the LLVM boundary"
  LABELS compiler
)

set(xs_vxdc_output
  "${xs_project_fixture_dir}/native_call/NativeCall.sqlite3.dump"
)
file(REMOVE "${xs_vxdc_output}")
add_test(NAME vxdc_project_dump COMMAND "${xs_vxdc_test_driver}"
  -Projectfile "${xs_project_fixture_dir}/native_call/Visual.XSharp.kts"
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

add_test(NAME vxdc_project_dump_is_sql COMMAND xs_text_artifact_tests
  "${xs_vxdc_output}" "BEGIN TRANSACTION;"
)
set_tests_properties(vxdc_project_dump_is_sql PROPERTIES
  TIMEOUT 5
  FIXTURES_REQUIRED vxdc_project_dump
  LABELS jvm
)
