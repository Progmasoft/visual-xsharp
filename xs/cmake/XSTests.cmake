# SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
# SPDX-License-Identifier: MPL-2.0

function(xs_add_c_test test_name source_file library_name)
  get_filename_component(target_stem "${source_file}" NAME_WE)
  string(REGEX REPLACE "_tests$" "" target_stem "${target_stem}")
  add_executable(xs_${target_stem}_tests "${source_file}")
  target_link_libraries(xs_${target_stem}_tests PRIVATE "${library_name}")
  add_test(NAME "${test_name}" COMMAND xs_${target_stem}_tests ${ARGN})
  set_tests_properties("${test_name}" PROPERTIES TIMEOUT 5)
endfunction()

function(xs_add_cxx_test test_name source_file library_name)
  get_filename_component(target_stem "${source_file}" NAME_WE)
  add_executable(xs_${target_stem} "${source_file}")
  target_link_libraries(xs_${target_stem} PRIVATE "${library_name}" Catch2::Catch2WithMain)
  add_test(NAME "${test_name}" COMMAND xs_${target_stem})
  set_tests_properties("${test_name}" PROPERTIES TIMEOUT 5)
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
add_test(NAME cli_help COMMAND vxs --help)
set_tests_properties(cli_help PROPERTIES TIMEOUT 5 PASS_REGULAR_EXPRESSION "Commands:")
add_test(NAME cli_build_help COMMAND vxs build --help)
set_tests_properties(cli_build_help PROPERTIES TIMEOUT 5 PASS_REGULAR_EXPRESSION "--output=IR")

add_test(NAME compiler_install_layout COMMAND "${CMAKE_COMMAND}"
  -DXS_BUILD_DIR=${CMAKE_BINARY_DIR}
  -DXS_INSTALL_PREFIX=${CMAKE_BINARY_DIR}/tests/install-root
  -DXS_VERSION=${PROJECT_VERSION}
  -P ${PROJECT_SOURCE_DIR}/tests/cmake/install_layout.cmake)
set_tests_properties(compiler_install_layout PROPERTIES TIMEOUT 15)

xs_add_c_test(lexer tests/lexer_tests.c xs_compiler)
xs_add_c_test(parser tests/parser_tests.c xs_compiler)
xs_add_c_test(diagnostic tests/diagnostic_tests.c xs_compiler)
xs_add_c_test(package_archive tests/package_archive_tests.c xs_package)
if(WIN32)
  list(GET CMAKE_PREFIX_PATH 0 XS_DEPENDENCY_PREFIX)
  file(GLOB XS_TEST_RUNTIME_DLLS CONFIGURE_DEPENDS "${XS_DEPENDENCY_PREFIX}/debug/bin/*.dll")
  add_custom_command(TARGET xs_package_archive_tests POST_BUILD
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            ${XS_TEST_RUNTIME_DLLS} $<TARGET_FILE_DIR:xs_package_archive_tests>
    COMMAND_EXPAND_LISTS
    COMMENT "Copying archive and compression runtime DLLs"
  )
endif()

set(XS_GRADLE_EXECUTABLE "${CMAKE_SOURCE_DIR}/xs_kts/gradlew.bat")
set(XS_PROJECT_TEST_DRIVER
    "${CMAKE_SOURCE_DIR}/xs_kts/build/install/xs-project-runtime/bin/xs-project-runtime.bat")
add_test(NAME kotlin_project_resolver_build COMMAND "${XS_GRADLE_EXECUTABLE}" --daemon --build-cache
  -p "${CMAKE_SOURCE_DIR}/xs_kts" installDist)
set_tests_properties(kotlin_project_resolver_build PROPERTIES TIMEOUT 180
  FIXTURES_SETUP kotlin_project_resolver ENVIRONMENT "GRADLE_OPTS=-Xmx512m")

add_test(NAME example_source COMMAND vxs check -file
  ${XS_SOURCE_FROM_BINARY}/tests/fixtures/example_project/source/Main.vxs)
set_tests_properties(example_source PROPERTIES TIMEOUT 5)
add_test(NAME macro_source COMMAND vxs check -file
  ${XS_SOURCE_FROM_BINARY}/tests/fixtures/macro_project/source/Main.vxs)
set_tests_properties(macro_source PROPERTIES TIMEOUT 5)
add_test(NAME compiler_check_file COMMAND vxs check -file
  ${XS_SOURCE_FROM_BINARY}/tests/fixtures/example_project/source/Main.vxs)
set_tests_properties(compiler_check_file PROPERTIES TIMEOUT 5)
add_test(NAME compiler_test_file COMMAND vxs test -file
  ${XS_SOURCE_FROM_BINARY}/tests/fixtures/projects/test_command/Sources/Test/arithmetic.vxs)
set_tests_properties(compiler_test_file PROPERTIES TIMEOUT 5
  PASS_REGULAR_EXPRESSION "test result: ok. 1 passed; 0 failed; 1 ignored")
add_test(NAME compiler_check_file_verbose COMMAND vxs check -file
  ${XS_SOURCE_FROM_BINARY}/tests/fixtures/example_project/source/Main.vxs
  --warning all --werror true --verbose true)
set_tests_properties(compiler_check_file_verbose PROPERTIES TIMEOUT 5
  PASS_REGULAR_EXPRESSION "verbose: command=check.*warning=all.*werror=true")
add_test(NAME compiler_rejects_invalid_warning COMMAND vxs check -file
  ${XS_SOURCE_FROM_BINARY}/tests/fixtures/example_project/source/Main.vxs --warning invalid)
set_tests_properties(compiler_rejects_invalid_warning PROPERTIES TIMEOUT 5 WILL_FAIL TRUE)
add_test(NAME compiler_rejects_misspelled_werror COMMAND vxs check -file
  ${XS_SOURCE_FROM_BINARY}/tests/fixtures/example_project/source/Main.vxs --werrror true)
set_tests_properties(compiler_rejects_misspelled_werror PROPERTIES TIMEOUT 5 WILL_FAIL TRUE)

set(XS_INTERMEDIATE_OUTPUT_FIXTURE_DIR "${CMAKE_CURRENT_BINARY_DIR}/tests/fixtures/intermediate_output")
file(MAKE_DIRECTORY "${XS_INTERMEDIATE_OUTPUT_FIXTURE_DIR}")
configure_file(tests/fixtures/source/MainTupleCalls.vxs
               "${XS_INTERMEDIATE_OUTPUT_FIXTURE_DIR}/MainTupleCalls.vxs" COPYONLY)
add_executable(xs_text_artifact_tests tests/text_artifact_tests.c)
foreach(output hir mir xlil)
  string(TOUPPER "${output}" output_upper)
  if(NOT output STREQUAL "xlil")
    set(output_upper "X${output_upper}")
  endif()
  set(output_extension ".x${output}")
  set(function_record "function")
  if(output STREQUAL "xlil")
    set(output_extension ".xlil")
    set(function_record ".func")
  endif()
  add_test(NAME build_file_output_${output} COMMAND vxs build --output ${output} -file
    ${XS_INTERMEDIATE_OUTPUT_FIXTURE_DIR}/MainTupleCalls.vxs)
  set_tests_properties(build_file_output_${output} PROPERTIES TIMEOUT 5
    PASS_REGULAR_EXPRESSION "wrote ${output_upper}")
  add_test(NAME build_file_short_${output} COMMAND vxs build --${output} -file
    ${XS_INTERMEDIATE_OUTPUT_FIXTURE_DIR}/MainTupleCalls.vxs)
  set_tests_properties(build_file_short_${output} PROPERTIES TIMEOUT 5
    PASS_REGULAR_EXPRESSION "wrote ${output_upper}")
  add_test(NAME build_file_output_${output}_artifact COMMAND xs_text_artifact_tests
    ${XS_INTERMEDIATE_OUTPUT_FIXTURE_DIR}/MainTupleCalls${output_extension}
    "${output_extension} version 1" "${function_record} make_pair" "${function_record} main")
  set_tests_properties(build_file_output_${output}_artifact PROPERTIES TIMEOUT 5
    DEPENDS build_file_output_${output})
endforeach()

add_test(NAME build_file_output_xlil_native_roundtrip COMMAND vxs build --xlil -file
  ${XS_INTERMEDIATE_OUTPUT_FIXTURE_DIR}/MainTupleCalls.xlil)
set_tests_properties(build_file_output_xlil_native_roundtrip PROPERTIES TIMEOUT 5
  DEPENDS build_file_output_xlil_artifact
  PASS_REGULAR_EXPRESSION "wrote optimized LLVM IR.*executable")

foreach(format xhir xmir xlil)
  set(format_option "${format}")
  if(format STREQUAL "xhir")
    set(format_option "hir")
  elseif(format STREQUAL "xmir")
    set(format_option "mir")
  endif()
  add_test(NAME direct_${format}_unsupported_version COMMAND vxs build --${format_option} -file ${XS_SOURCE_FROM_BINARY}/tests/fixtures/intermediate/Unsupported.${format})
  set_tests_properties(direct_${format}_unsupported_version PROPERTIES TIMEOUT 5 WILL_FAIL TRUE)
endforeach()

include(XSTestsDirectXlil)
include(XSTestsDirectXhir)
include(XSTestsDirectXmir)
include(XSTestsSourceValues)
include(XSTestsSourceControl)
include(XSTestsSourceCalls)
include(XSTestsKotlin)
include(XSTestsLibraries)
