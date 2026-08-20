# SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
# SPDX-License-Identifier: MPL-2.0

set(XS_TEST_SOURCE_ROOT "${PROJECT_SOURCE_DIR}/tests")
set(XS_TEST_WORK_ROOT "${CMAKE_CURRENT_BINARY_DIR}/work")

# Modular test executables no longer share an output directory with every
# production DLL. Declare runtime lookup paths on the tests themselves so a
# missing DLL is reported by CTest instead of opening an interactive Windows
# loader dialog that can stall unattended builds.
function(xs_prepend_test_runtime_path test_name runtime_target)
  if(WIN32)
    set_property(TEST "${test_name}" APPEND PROPERTY ENVIRONMENT_MODIFICATION
      "PATH=path_list_prepend:$<TARGET_FILE_DIR:${runtime_target}>"
    )
  endif()
endfunction()

find_package(Catch2 3 QUIET CONFIG)
if(NOT TARGET Catch2::Catch2WithMain)
  if(NOT EXISTS "${PROJECT_SOURCE_DIR}/third_party/catch2/CMakeLists.txt")
    message(FATAL_ERROR
      "Catch2 3 is unavailable; initialize dependencies with: git submodule update --init --recursive"
    )
  endif()
  set(CATCH_INSTALL_DOCS OFF CACHE BOOL "" FORCE)
  set(CATCH_INSTALL_EXTRAS OFF CACHE BOOL "" FORCE)
  add_subdirectory("${PROJECT_SOURCE_DIR}/third_party/catch2"
                   "${PROJECT_BINARY_DIR}/third_party/catch2" EXCLUDE_FROM_ALL)
endif()

function(xs_add_c_test test_name source_file library_name)
  get_filename_component(target_stem "${source_file}" NAME_WE)
  string(REGEX REPLACE "_tests$" "" target_stem "${target_stem}")
  add_executable(xs_${target_stem}_tests "${source_file}")
  target_link_libraries(xs_${target_stem}_tests PRIVATE "${library_name}")
  add_test(NAME "${test_name}" COMMAND xs_${target_stem}_tests ${ARGN})
  set_tests_properties("${test_name}" PROPERTIES TIMEOUT 15)
  xs_prepend_test_runtime_path("${test_name}" xs_lil)
endfunction()

function(xs_add_cxx_test test_name source_file library_name)
  get_filename_component(target_stem "${source_file}" NAME_WE)
  add_executable(xs_${target_stem} "${source_file}")
  target_link_libraries(xs_${target_stem} PRIVATE
    "${library_name}"
    Catch2::Catch2WithMain
  )
  add_test(NAME "${test_name}" COMMAND xs_${target_stem})
  set_tests_properties("${test_name}" PROPERTIES TIMEOUT 15)
  xs_prepend_test_runtime_path("${test_name}" xs_lil)
endfunction()

add_subdirectory(cli)
add_subdirectory(compiler)
add_subdirectory(package)
add_subdirectory(legacy-ir)
add_subdirectory(backend)
add_subdirectory(project-system)
add_subdirectory(install)
