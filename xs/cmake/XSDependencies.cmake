# SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
# SPDX-License-Identifier: MPL-2.0

# Dependency discovery belongs above individual components so every target
# resolves one canonical imported target or executable. Component manifests
# consume these results but do not repeat environment probing.
if(NOT LLVM_DIR AND DEFINED ENV{LLVM_DIR})
  set(LLVM_DIR "$ENV{LLVM_DIR}")
elseif(NOT LLVM_DIR AND DEFINED ENV{LLVM_ROOT})
  list(PREPEND CMAKE_PREFIX_PATH "$ENV{LLVM_ROOT}")
endif()

find_package(LLVM REQUIRED CONFIG)
find_package(LibArchive REQUIRED)
find_package(fmt CONFIG REQUIRED)
# The renewed C++20 backend uses LLVM's native C++ IR and pass-manager APIs.
# Component targets keep that dependency independent from the LLVM-C import
# library retained only by the isolated legacy C backend below.
include("${CMAKE_CURRENT_LIST_DIR}/XSLLVM.cmake")
xs_resolve_llvm_cpp_libraries(XS_LLVM_CPP_LIBRARIES)
find_library(XS_LLVM_LIBRARY NAMES LLVM-C HINTS ${LLVM_LIBRARY_DIRS} REQUIRED)
find_program(XS_CABAL_EXECUTABLE NAMES cabal REQUIRED)

# Some dependency package files alter this directory-scoped default. Restore
# the static multithreaded CRT selected at the root so LLVM component objects,
# Visual X# libraries, executables, and in-tree tests share one ABI contract.
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded")
