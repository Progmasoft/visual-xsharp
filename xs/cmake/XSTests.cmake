# SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
# SPDX-License-Identifier: MPL-2.0

function(xs_add_c_test test_name source_file library_name)
  get_filename_component(target_stem "${source_file}" NAME_WE)
  string(REGEX REPLACE "_tests$" "" target_stem "${target_stem}")
  add_executable(xs_${target_stem}_tests "${source_file}")
  target_link_libraries(xs_${target_stem}_tests PRIVATE "${library_name}")
  add_test(NAME "${test_name}" COMMAND xs_${target_stem}_tests ${ARGN})
  set_tests_properties("${test_name}" PROPERTIES TIMEOUT 15)
endfunction()

function(xs_add_cxx_test test_name source_file library_name)
  get_filename_component(target_stem "${source_file}" NAME_WE)
  add_executable(xs_${target_stem} "${source_file}")
  target_link_libraries(xs_${target_stem} PRIVATE "${library_name}" Catch2::Catch2WithMain)
  add_test(NAME "${test_name}" COMMAND xs_${target_stem})
  set_tests_properties("${test_name}" PROPERTIES TIMEOUT 15)
endfunction()

if(NOT XS_BUILD_PROJECT_XS)
  return()
endif()

find_package(Catch2 3 QUIET CONFIG)
if(NOT TARGET Catch2::Catch2WithMain)
  if(NOT EXISTS "${PROJECT_SOURCE_DIR}/third_party/catch2/CMakeLists.txt")
    message(FATAL_ERROR "Catch2 3 is unavailable; initialize dependencies with: git submodule update --init --recursive")
  endif()
  set(CATCH_INSTALL_DOCS OFF CACHE BOOL "" FORCE)
  set(CATCH_INSTALL_EXTRAS OFF CACHE BOOL "" FORCE)
  add_subdirectory("${PROJECT_SOURCE_DIR}/third_party/catch2"
                   "${PROJECT_BINARY_DIR}/third_party/catch2" EXCLUDE_FROM_ALL)
endif()

add_test(NAME cli_version COMMAND vxs --version)
string(REPLACE "." "\\." XS_PROJECT_VERSION_REGEX "${PROJECT_VERSION}")
set_tests_properties(cli_version PROPERTIES TIMEOUT 5 PASS_REGULAR_EXPRESSION "vxs ${XS_PROJECT_VERSION_REGEX}")
add_test(NAME cli_version_command COMMAND vxs version)
set_tests_properties(cli_version_command PROPERTIES TIMEOUT 5 PASS_REGULAR_EXPRESSION "vxs ${XS_PROJECT_VERSION_REGEX}")
add_test(NAME cli_help COMMAND vxs --help)
set_tests_properties(cli_help PROPERTIES TIMEOUT 5 PASS_REGULAR_EXPRESSION "Commands:")
add_test(NAME cli_build_help COMMAND vxs build --help)
set_tests_properties(cli_build_help PROPERTIES TIMEOUT 5 PASS_REGULAR_EXPRESSION "-Emit")
add_test(NAME cli_check_help COMMAND vxs check --help)
set_tests_properties(cli_check_help PROPERTIES TIMEOUT 5 PASS_REGULAR_EXPRESSION "-Build"
  FAIL_REGULAR_EXPRESSION "-Emit")
add_test(NAME cli_install_help COMMAND vxs install --help)
set_tests_properties(cli_install_help PROPERTIES TIMEOUT 5 PASS_REGULAR_EXPRESSION "Publisher.Name")

add_executable(xs_cli_parser_tests tests/CliParserTests.cpp sources/driver/Options.cpp)
target_include_directories(xs_cli_parser_tests PRIVATE "${PROJECT_SOURCE_DIR}")
target_compile_definitions(xs_cli_parser_tests PRIVATE XS_PROJECT_VERSION="${PROJECT_VERSION}")
target_compile_options(xs_cli_parser_tests PRIVATE /W4 /clang:-Wconversion /clang:-Wshadow)
target_link_libraries(xs_cli_parser_tests PRIVATE Catch2::Catch2WithMain)
add_test(NAME cli_parser_model COMMAND xs_cli_parser_tests)
set_tests_properties(cli_parser_model PROPERTIES TIMEOUT 15)

add_test(NAME compiler_install_layout COMMAND "${CMAKE_COMMAND}"
  -DXS_BUILD_DIR=${CMAKE_BINARY_DIR}
  -DXS_INSTALL_PREFIX=${CMAKE_BINARY_DIR}/tests/install-root
  -DXS_VERSION=${PROJECT_VERSION}
  -P ${PROJECT_SOURCE_DIR}/tests/cmake/install_layout.cmake)
set_tests_properties(compiler_install_layout PROPERTIES TIMEOUT 15)

xs_add_c_test(package_archive tests/package_archive_tests.c xs_package)
# Archive coverage performs real compression and filesystem round trips. A cold Windows
# runner can legitimately exceed the five-second unit-test default while another test is
# starting Gradle, so give this integration-shaped test the same budget as install layout.
set_tests_properties(package_archive PROPERTIES TIMEOUT 15)
if(WIN32)
  # LibArchive is linked through a static xs_package target, so TARGET_RUNTIME_DLLS
  # cannot see through to the imported DLL graph. Locate the bin directory matching the
  # import library CMake actually selected; mixing lib with debug/bin loads the wrong ABI.
  set(XS_ARCHIVE_RUNTIME_DLLS)
  foreach(XS_DEPENDENCY_PREFIX IN LISTS CMAKE_PREFIX_PATH)
    if(EXISTS "${XS_DEPENDENCY_PREFIX}/lib/archive.lib")
      file(GLOB XS_PREFIX_RUNTIME_DLLS CONFIGURE_DEPENDS "${XS_DEPENDENCY_PREFIX}/bin/*.dll")
      list(APPEND XS_ARCHIVE_RUNTIME_DLLS ${XS_PREFIX_RUNTIME_DLLS})
    elseif(EXISTS "${XS_DEPENDENCY_PREFIX}/debug/lib/archive.lib")
      file(GLOB XS_PREFIX_RUNTIME_DLLS CONFIGURE_DEPENDS "${XS_DEPENDENCY_PREFIX}/debug/bin/*.dll")
      list(APPEND XS_ARCHIVE_RUNTIME_DLLS ${XS_PREFIX_RUNTIME_DLLS})
    endif()
  endforeach()
  if(XS_ARCHIVE_RUNTIME_DLLS)
    add_custom_command(TARGET xs_package_archive_tests POST_BUILD
      COMMAND "${CMAKE_COMMAND}" -E copy_if_different
              ${XS_ARCHIVE_RUNTIME_DLLS} $<TARGET_FILE_DIR:xs_package_archive_tests>
      COMMAND_EXPAND_LISTS
      COMMENT "Copying archive and compression runtime DLLs"
    )
  endif()
endif()

set(XS_GRADLE_EXECUTABLE "${CMAKE_SOURCE_DIR}/xs_kts/gradlew.bat")
set(XS_PROJECT_TEST_CLASSPATH
    "${CMAKE_SOURCE_DIR}/xs_kts/build/install/vxdc/lib/*")
set(XS_VXDC_TEST_DRIVER
    "${CMAKE_SOURCE_DIR}/xs_kts/build/install/vxdc/bin/vxdc.bat")
add_test(NAME kotlin_project_resolver_build COMMAND "${XS_GRADLE_EXECUTABLE}" --daemon --build-cache
  -p "${CMAKE_SOURCE_DIR}/xs_kts" installDist)
set_tests_properties(kotlin_project_resolver_build PROPERTIES TIMEOUT 180
  FIXTURES_SETUP kotlin_project_resolver ENVIRONMENT "GRADLE_OPTS=-Xmx512m")

# Emission tests must never write generated artifacts into the source checkout.
# Copy the small source fixture to a configuration-owned directory so sibling
# `.core`, `.ll`, or `.bc` outputs remain disposable build products.
set(XS_HASKELL_SOURCE_FIXTURE_DIR "${CMAKE_CURRENT_BINARY_DIR}/tests/fixtures/haskell_frontend")
file(MAKE_DIRECTORY "${XS_HASKELL_SOURCE_FIXTURE_DIR}")
configure_file("${PROJECT_SOURCE_DIR}/tests/fixtures/haskell_frontend/Main.vxs"
               "${XS_HASKELL_SOURCE_FIXTURE_DIR}/Main.vxs" COPYONLY)

add_test(NAME compiler_check_file COMMAND vxs check -File
  ${XS_HASKELL_SOURCE_FIXTURE_DIR}/Main.vxs)
set_tests_properties(compiler_check_file PROPERTIES TIMEOUT 15)
add_test(NAME compiler_rejects_invalid_warning COMMAND vxs check -File
  ${XS_SOURCE_FROM_BINARY}/tests/fixtures/example_project/source/Main.vxs -Warnings invalid)
set_tests_properties(compiler_rejects_invalid_warning PROPERTIES TIMEOUT 5 WILL_FAIL TRUE)
add_test(NAME compiler_rejects_misspelled_werror COMMAND vxs check -File
  ${XS_SOURCE_FROM_BINARY}/tests/fixtures/example_project/source/Main.vxs -Werrror true)
set_tests_properties(compiler_rejects_misspelled_werror PROPERTIES TIMEOUT 5 WILL_FAIL TRUE)
add_test(NAME compiler_accepts_renewed_cli_flags COMMAND vxs check
  -File ${XS_HASKELL_SOURCE_FIXTURE_DIR}/Main.vxs
  -Standard 26 -Compiler-Version latest -Warnings all -Werror true
  -Wexperimental true -Wshadow true -Wundef true -Type-Safe-Format true
  -Backend llvm -Llvm-OptLevel 2 -Llvm-Compiler aot -Llvm-Lto none
  -Xpp-Optimization-Passes true -Xmm-Optimization-Passes true)
set_tests_properties(compiler_accepts_renewed_cli_flags PROPERTIES TIMEOUT 5)
add_test(NAME compiler_rejects_project_flag COMMAND vxs check -Project .)
set_tests_properties(compiler_rejects_project_flag PROPERTIES TIMEOUT 5 WILL_FAIL TRUE)
add_test(NAME compiler_rejects_module_flag COMMAND vxs check --module .)
set_tests_properties(compiler_rejects_module_flag PROPERTIES TIMEOUT 5 WILL_FAIL TRUE)

add_test(NAME compiler_emits_core_from_haskell COMMAND vxs build -File
  ${XS_HASKELL_SOURCE_FIXTURE_DIR}/Main.vxs -Emit core)
set_tests_properties(compiler_emits_core_from_haskell PROPERTIES TIMEOUT 15)

# Schema checks guard command scope, arity, and duplicate policy independently
# from frontend compilation. These are intentional parse failures.
add_test(NAME cli_rejects_duplicate_option COMMAND vxs check
  -File ${XS_HASKELL_SOURCE_FIXTURE_DIR}/Main.vxs
  -File ${XS_HASKELL_SOURCE_FIXTURE_DIR}/Main.vxs)
set_tests_properties(cli_rejects_duplicate_option PROPERTIES TIMEOUT 5 WILL_FAIL TRUE)
add_test(NAME cli_rejects_wrong_command_scope COMMAND vxs check -Emit core
  -File ${XS_HASKELL_SOURCE_FIXTURE_DIR}/Main.vxs)
set_tests_properties(cli_rejects_wrong_command_scope PROPERTIES TIMEOUT 5 WILL_FAIL TRUE)
add_test(NAME cli_rejects_missing_value COMMAND vxs build -File)
set_tests_properties(cli_rejects_missing_value PROPERTIES TIMEOUT 5 WILL_FAIL TRUE)
add_test(NAME cli_requires_install_coordinate COMMAND vxs install)
set_tests_properties(cli_requires_install_coordinate PROPERTIES TIMEOUT 5 WILL_FAIL TRUE)

add_executable(xs_text_artifact_tests tests/text_artifact_tests.c)

# Kotlin project tests still exercise the project-evaluator boundary independently
# from the retired C source frontend. Keep their copied workspaces owned here so
# removing a source-compiler test suite cannot silently erase this fixture root.
set(XS_PROJECT_NATIVE_FIXTURE_DIR "${CMAKE_CURRENT_BINARY_DIR}/tests/fixtures/projects")
file(COPY "${PROJECT_SOURCE_DIR}/tests/fixtures/projects/"
     DESTINATION "${XS_PROJECT_NATIVE_FIXTURE_DIR}")
include(XSTestsKotlin)
include(XSTestsLibraries)
