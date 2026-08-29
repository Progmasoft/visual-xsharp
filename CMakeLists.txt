# SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
# SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.0

cmake_minimum_required(VERSION 3.31)

list(APPEND CMAKE_MODULE_PATH
  "${CMAKE_CURRENT_SOURCE_DIR}/xs/cmake"
)

if(DEFINED CMAKE_C_COMPILER)
  get_filename_component(XS_REQUESTED_C_COMPILER "${CMAKE_C_COMPILER}" NAME)
  if(XS_REQUESTED_C_COMPILER MATCHES "^(gcc|g\\+\\+|cc)(-[0-9]+)?$")
    message(FATAL_ERROR "GNU compiler drivers are unsupported; use ClangCL.")
  endif()
endif()
if(DEFINED CMAKE_CXX_COMPILER)
  get_filename_component(XS_REQUESTED_CXX_COMPILER "${CMAKE_CXX_COMPILER}" NAME)
  if(XS_REQUESTED_CXX_COMPILER MATCHES "^(gcc|g[+][+]|c[+][+])(-[0-9]+)?$")
    message(FATAL_ERROR "GNU compiler drivers are unsupported; use ClangCL.")
  endif()
endif()

# ClangCL is a single MSVC-compatible driver binary used for both C and C++.
find_program(XS_CLANGCL_EXECUTABLE NAMES clang-cl REQUIRED)
find_program(XS_LLD_EXECUTABLE NAMES lld-link REQUIRED)

set(CMAKE_C_COMPILER "${XS_CLANGCL_EXECUTABLE}" CACHE FILEPATH "ClangCL C compiler" FORCE)
set(CMAKE_CXX_COMPILER "${XS_CLANGCL_EXECUTABLE}" CACHE FILEPATH "ClangCL C++ compiler" FORCE)

set(XS_MONOREPO_STABLE_PROJECTS xs)
set(XS_MONOREPO_FUTURE_PROJECTS)
set(XS_MONOREPO_STABLE_RUNTIMES xsrt)
set(XS_MONOREPO_FUTURE_RUNTIMES)
set(XS_ENABLE_PROJECTS "xs" CACHE STRING "Semicolon-separated monorepo projects, or 'all'.")
set(XS_ENABLE_RUNTIMES "xsrt" CACHE STRING "Semicolon-separated monorepo runtimes, or 'all'.")

function(xs_monorepo_normalize_list out_variable input_items all_items)
  if("${input_items}" STREQUAL "all")
    set(${out_variable} ${all_items} PARENT_SCOPE)
    return()
  endif()
  string(REPLACE "," ";" normalized "${input_items}")
  set(result)
  foreach(item_name IN LISTS normalized)
    string(STRIP "${item_name}" item_name)
    if(NOT item_name STREQUAL "")
      list(APPEND result "${item_name}")
    endif()
  endforeach()
  set(${out_variable} ${result} PARENT_SCOPE)
endfunction()

function(xs_monorepo_validate_projects)
  xs_monorepo_normalize_list(enabled "${XS_ENABLE_PROJECTS}" "${XS_MONOREPO_STABLE_PROJECTS}")
  if(NOT enabled)
    message(FATAL_ERROR "XS_ENABLE_PROJECTS must name at least one project.")
  endif()
  foreach(name IN LISTS enabled)
    if(name IN_LIST XS_MONOREPO_STABLE_PROJECTS)
      continue()
    elseif(name IN_LIST XS_MONOREPO_FUTURE_PROJECTS)
      message(FATAL_ERROR "Monorepo project '${name}' is registered but not buildable yet.")
    endif()
    message(FATAL_ERROR "Unknown monorepo project '${name}'.")
  endforeach()
  set(XS_ENABLED_PROJECTS ${enabled} CACHE INTERNAL "Normalized monorepo projects")
endfunction()

function(xs_monorepo_validate_runtimes)
  xs_monorepo_normalize_list(enabled "${XS_ENABLE_RUNTIMES}" "${XS_MONOREPO_STABLE_RUNTIMES}")
  foreach(name IN LISTS enabled)
    if(name IN_LIST XS_MONOREPO_STABLE_RUNTIMES)
      continue()
    elseif(name IN_LIST XS_MONOREPO_FUTURE_RUNTIMES)
      message(FATAL_ERROR "Monorepo runtime '${name}' is registered but not buildable yet.")
    endif()
    message(FATAL_ERROR "Unknown monorepo runtime '${name}'.")
  endforeach()
  set(XS_ENABLED_RUNTIMES ${enabled} CACHE INTERNAL "Normalized monorepo runtimes")
endfunction()

function(xs_monorepo_project_enabled name out_variable)
  if(name IN_LIST XS_ENABLED_PROJECTS)
    set(${out_variable} TRUE PARENT_SCOPE)
  else()
    set(${out_variable} FALSE PARENT_SCOPE)
  endif()
endfunction()

function(xs_monorepo_runtime_enabled name out_variable)
  if(name IN_LIST XS_ENABLED_RUNTIMES)
    set(${out_variable} TRUE PARENT_SCOPE)
  else()
    set(${out_variable} FALSE PARENT_SCOPE)
  endif()
endfunction()

function(xs_validate_llvm_toolchain)
  get_filename_component(compiler_name "${CMAKE_C_COMPILER}" NAME)
  if(NOT CMAKE_C_COMPILER_ID MATCHES "Clang" OR NOT CMAKE_C_SIMULATE_ID MATCHES "MSVC" OR
     NOT compiler_name MATCHES "^clang-cl(-[0-9]+)?(\\.exe)?$")
    message(FATAL_ERROR "The Visual X# compiler requires ClangCL.")
  endif()
  get_filename_component(cxx_compiler_name "${CMAKE_CXX_COMPILER}" NAME)
  if(NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang" OR NOT CMAKE_CXX_SIMULATE_ID MATCHES "MSVC" OR
     NOT cxx_compiler_name MATCHES "^clang-cl(-[0-9]+)?(\\.exe)?$")
    message(FATAL_ERROR "The Visual X# compiler requires ClangCL for C++20 sources.")
  endif()
  if(NOT CMAKE_GENERATOR MATCHES "^Ninja")
    message(FATAL_ERROR "The Visual X# compiler requires Ninja.")
  endif()
  if(CMAKE_C_FLAGS MATCHES "(^| )-std=gnu" OR CMAKE_C_FLAGS_DEBUG MATCHES "(^| )-std=gnu" OR
     CMAKE_C_FLAGS_RELEASE MATCHES "(^| )-std=gnu")
    message(FATAL_ERROR "GNU C dialect flags are unsupported; use strict C23.")
  endif()
  if(CMAKE_CXX_FLAGS MATCHES "(^| )-std=gnu[+][+]" OR CMAKE_CXX_FLAGS_DEBUG MATCHES "(^| )-std=gnu[+][+]" OR
     CMAKE_CXX_FLAGS_RELEASE MATCHES "(^| )-std=gnu[+][+]")
    message(FATAL_ERROR "GNU C++ dialect flags are unsupported; use strict C++20.")
  endif()
endfunction()

# LLVM's official Windows development archive is built against the static
# multithreaded CRT. Keep every native target on the same ABI contract: LLVM
# C++ objects carry /FAILIFMISMATCH records and must never be force-linked
# against an incompatible runtime.
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded")

xs_monorepo_validate_projects()
xs_monorepo_validate_runtimes()
xs_monorepo_project_enabled(xs XS_BUILD_PROJECT_XS)
xs_monorepo_runtime_enabled(xsrt XS_BUILD_RUNTIME_XSRT)
if(NOT XS_BUILD_PROJECT_XS)
  message(FATAL_ERROR "This checkout currently has one buildable compiler project: 'xs'.")
endif()

project(vxs_project VERSION 0.3.2 LANGUAGES C CXX)
xs_validate_llvm_toolchain()

file(RELATIVE_PATH XS_SOURCE_FROM_BINARY "${CMAKE_CURRENT_BINARY_DIR}" "${CMAKE_CURRENT_SOURCE_DIR}")
cmake_path(NORMAL_PATH XS_SOURCE_FROM_BINARY)
if(XS_SOURCE_FROM_BINARY STREQUAL "")
  set(XS_SOURCE_FROM_BINARY ".")
endif()
set(XS_COMPILER_CHECK_HEADER "${XS_SOURCE_FROM_BINARY}/include/Visual/C23/compiler_check.h")
set(XS_BACKEND_TEST_OBJECT "xs_backend_test.obj")

set(CMAKE_C_STANDARD 23)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
option(XS_ENABLE_SANITIZERS "Enable Clang address and undefined behavior sanitizers" OFF)
if(XS_ENABLE_SANITIZERS)
  add_compile_options(/fsanitize=address /Oy-)
  # Rust/CXX and binary dependencies are not ASan-instrumented. Keep the MSVC
  # STL fail-if-mismatch contract uniform while instrumenting our own code.
  add_compile_definitions(_DISABLE_STL_ANNOTATION)
  execute_process(
    COMMAND "${CMAKE_C_COMPILER}" /clang:-print-resource-dir
    OUTPUT_VARIABLE XS_CLANG_RESOURCE_DIR
    OUTPUT_STRIP_TRAILING_WHITESPACE
    COMMAND_ERROR_IS_FATAL ANY
  )
  file(TO_CMAKE_PATH "${XS_CLANG_RESOURCE_DIR}/lib/windows" XS_ASAN_RUNTIME_DIR)
  find_library(XS_ASAN_DYNAMIC_RUNTIME NAMES clang_rt.asan_dynamic-x86_64
               HINTS "${XS_ASAN_RUNTIME_DIR}" NO_DEFAULT_PATH REQUIRED)
  find_library(XS_ASAN_DYNAMIC_RUNTIME_THUNK NAMES clang_rt.asan_dynamic_runtime_thunk-x86_64
               HINTS "${XS_ASAN_RUNTIME_DIR}" NO_DEFAULT_PATH REQUIRED)
  find_file(XS_ASAN_DYNAMIC_DLL NAMES clang_rt.asan_dynamic-x86_64.dll
            HINTS "${XS_ASAN_RUNTIME_DIR}" NO_DEFAULT_PATH REQUIRED)
  add_link_options("${XS_ASAN_DYNAMIC_RUNTIME}"
                   "/include:__asan_seh_interceptor"
                   "/wholearchive:${XS_ASAN_DYNAMIC_RUNTIME_THUNK}"
                   "/INCREMENTAL:NO")
  file(COPY_FILE "${XS_ASAN_DYNAMIC_DLL}"
       "${CMAKE_BINARY_DIR}/clang_rt.asan_dynamic-x86_64.dll" ONLY_IF_DIFFERENT)
endif()

include(CTest)

if(XS_BUILD_PROJECT_XS)
  add_subdirectory(xs "${CMAKE_CURRENT_BINARY_DIR}/projects/xs")
endif()
if(XS_BUILD_RUNTIME_XSRT)
  add_subdirectory(xsrt "${CMAKE_CURRENT_BINARY_DIR}/runtimes/xsrt")
endif()

if(BUILD_TESTING)
  include(XSTests)
endif()
