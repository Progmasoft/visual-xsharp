# SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
# SPDX-License-Identifier: MPL-2.0

# Project resolution is an independent Kotlin DSL boundary. Keep it covered
# while source compilation moves to the Haskell frontend; the former fixtures
# below this boundary encoded syntax and native-link behavior owned by the
# retired C frontend and must not define the renewed language implementation.
add_test(NAME kotlin_project_resolve COMMAND vxs resolve)
set_tests_properties(kotlin_project_resolve PROPERTIES
  TIMEOUT 180
  WORKING_DIRECTORY "${XS_PROJECT_NATIVE_FIXTURE_DIR}/native_call"
  ENVIRONMENT "XS_PROJECT_EVALUATOR_CLASSPATH=${XS_PROJECT_TEST_CLASSPATH}"
  FIXTURES_REQUIRED kotlin_project_resolver
  FIXTURES_SETUP kotlin_project_lock
  PASS_REGULAR_EXPRESSION "refreshed binary lock file 'Visual.XSharp.Lockfile.sqlite3'"
  LABELS jvm
  RESOURCE_LOCK kotlin_script_runner
  RUN_SERIAL TRUE)

add_test(NAME kotlin_project_resolve_binary_lock COMMAND xs_text_artifact_tests
  "${XS_PROJECT_NATIVE_FIXTURE_DIR}/native_call/Visual.XSharp.Lockfile.sqlite3" "SQLite format 3")
set_tests_properties(kotlin_project_resolve_binary_lock PROPERTIES
  TIMEOUT 5
  FIXTURES_REQUIRED kotlin_project_lock
  LABELS jvm)

set(XS_VXDC_TEST_OUTPUT
    "${XS_PROJECT_NATIVE_FIXTURE_DIR}/native_call/NativeCall.sqlite3.dump")
file(REMOVE "${XS_VXDC_TEST_OUTPUT}")
add_test(NAME vxdc_project_dump COMMAND "${XS_VXDC_TEST_DRIVER}"
  -Projectfile "${XS_PROJECT_NATIVE_FIXTURE_DIR}/native_call/Visual.XSharp.kts"
  -Output "${XS_VXDC_TEST_OUTPUT}")
set_tests_properties(vxdc_project_dump PROPERTIES
  TIMEOUT 180
  FIXTURES_REQUIRED kotlin_project_resolver
  FIXTURES_SETUP vxdc_project_dump
  PASS_REGULAR_EXPRESSION "vxdc: wrote"
  LABELS jvm
  RESOURCE_LOCK kotlin_script_runner
  RUN_SERIAL TRUE)

add_test(NAME vxdc_project_dump_is_sql COMMAND xs_text_artifact_tests
  "${XS_VXDC_TEST_OUTPUT}" "BEGIN TRANSACTION;")
set_tests_properties(vxdc_project_dump_is_sql PROPERTIES
  TIMEOUT 5
  FIXTURES_REQUIRED vxdc_project_dump
  LABELS jvm)
