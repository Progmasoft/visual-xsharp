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
find_library(XS_LLVM_LIBRARY NAMES LLVM-C HINTS ${LLVM_LIBRARY_DIRS} REQUIRED)
find_program(XS_CABAL_EXECUTABLE NAMES cabal REQUIRED)

# Some dependency package files alter this directory-scoped default. Keep all
# Visual X# components on the DLL CRT so objects remain safe across library
# boundaries and agree with test executables.
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreadedDLL")
