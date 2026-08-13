# SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
# SPDX-License-Identifier: MPL-2.0

# Project resolution is an independent Kotlin DSL boundary. Keep it covered
# while source compilation moves to the Haskell frontend; the former fixtures
# below this boundary encoded syntax and native-link behavior owned by the
# retired C frontend and must not define the renewed language implementation.
add_test(NAME kotlin_project_resolve COMMAND vxs resolve)
set_tests_properties(kotlin_project_resolve PROPERTIES
  TIMEOUT 60
  WORKING_DIRECTORY "${XS_PROJECT_NATIVE_FIXTURE_DIR}/native_call"
  ENVIRONMENT "XS_PROJECT_RUNTIME=${XS_PROJECT_TEST_DRIVER}"
  FIXTURES_REQUIRED kotlin_project_resolver
  FIXTURES_SETUP kotlin_project_lock
  PASS_REGULAR_EXPRESSION "refreshed binary lock file 'Visual.XSharp.Lockfile.sqlite3'"
  LABELS jvm
  RESOURCE_LOCK kotlin_script_runner)

add_test(NAME kotlin_project_resolve_binary_lock COMMAND xs_text_artifact_tests
  "${XS_PROJECT_NATIVE_FIXTURE_DIR}/native_call/Visual.XSharp.Lockfile.sqlite3" "SQLite format 3")
set_tests_properties(kotlin_project_resolve_binary_lock PROPERTIES
  TIMEOUT 5
  FIXTURES_REQUIRED kotlin_project_lock
  LABELS jvm)
