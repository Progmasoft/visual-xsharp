# SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
# SPDX-License-Identifier: MPL-2.0

if(NOT LLVM_DIR AND DEFINED ENV{LLVM_DIR})
  set(LLVM_DIR "$ENV{LLVM_DIR}")
elseif(NOT LLVM_DIR AND DEFINED ENV{LLVM_ROOT})
  list(PREPEND CMAKE_PREFIX_PATH "$ENV{LLVM_ROOT}")
endif()
find_package(LLVM REQUIRED CONFIG)
find_package(LibArchive REQUIRED)
find_library(XS_LLVM_LIBRARY NAMES LLVM-C HINTS ${LLVM_LIBRARY_DIRS} REQUIRED)
find_program(XS_CABAL_EXECUTABLE NAMES cabal REQUIRED)

# Some dependency package files alter this directory-scoped default.  Keep every
# Visual X# target on the DLL CRT so objects can safely cross shared-library
# boundaries and match the test executables.
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreadedDLL")

add_library(xs_compiler
  ../Visual/XSharp/Backend/LLVM/Artifact.cpp
  ../Visual/XSharp/Backend/LLVM/Codegen.cpp
  ../Visual/XSharp/Backend/LLVM/Verifier.cpp
  ../Visual/XSharp/Pipeline.cpp
  ../Visual/XSharp/Pipeline/Driver.cpp
  ../Visual/XSharp/Core/IR.cpp
  ../Visual/XSharp/Core/Verifier.cpp
  ../Visual/XSharp/Core/Wire.cpp
  ../Visual/XSharp/Core/CorePrep/Prepare.cpp
  ../Visual/XSharp/Core/CorePrep/Verifier.cpp
  ../Visual/XSharp/Core/CorePrep/Verifier/Semantics.cpp
  ../Visual/XSharp/Core/CorePrep/Wire/Decode.cpp
  ../Visual/XSharp/Core/CorePrep/Wire/Encode.cpp
  sources/driver/Cli.cpp
  sources/driver/CorePipeline.cpp
  sources/driver/Options.cpp
  sources/driver/project_driver.c
)

add_library(xs_lil SHARED
  sources/int128.c
  sources/xlil/builder.c
  sources/xlil/memory.c
  sources/xlil/model.c
  sources/xlil/model_aggregate.c
  sources/xlil/model_array.c
  sources/xlil/model_float.c
  sources/xlil/model_integer.c
  sources/xlil/model_integer_operation.c
  sources/xlil/model_string.c
  sources/xlil/model_string_compare.c
  sources/xlil/parser.c
  sources/xlil/parser_aggregate.c
  sources/xlil/parser_array.c
  sources/xlil/parser_scalar.c
  sources/xlil/parser_signature.c
  sources/xlil/parser_integer_operation.c
  sources/xlil/parser_string.c
  sources/xlil/producer.c
  sources/xlil/text_emit.c
  sources/xlil/TypeName.cpp
  sources/xlil/verify.c
  sources/xlil/writer.c
)
set_target_properties(xs_lil PROPERTIES VERSION "${PROJECT_VERSION}" SOVERSION 1
                                        WINDOWS_EXPORT_ALL_SYMBOLS ON
                                        RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}")

add_library(xs_lil_cpp STATIC
  sources/lil/Builder.cpp
  sources/lil/Error.cpp
  sources/lil/Module.cpp
)
add_library(xs::lil ALIAS xs_lil_cpp)
target_include_directories(xs_lil_cpp PUBLIC "${PROJECT_SOURCE_DIR}/include" include)
target_link_libraries(xs_lil_cpp PUBLIC xs_lil)
target_compile_options(xs_lil_cpp PRIVATE /W4 /clang:-Wconversion /clang:-Wshadow)

add_library(xs_package
  sources/package/archive_common.c
  sources/package/archive_reader.c
  sources/package/archive_writer.c
)

target_include_directories(xs_compiler PUBLIC "${PROJECT_SOURCE_DIR}" "${PROJECT_SOURCE_DIR}/include" include
                                      PRIVATE ${LLVM_INCLUDE_DIRS})
target_include_directories(xs_lil PUBLIC "${PROJECT_SOURCE_DIR}/include" include)
target_include_directories(xs_package PUBLIC "${PROJECT_SOURCE_DIR}/include" include)
target_compile_definitions(xs_lil PRIVATE XS_LIL_BUILDING_LIBRARY)
get_target_property(XS_LIL_LIBRARY_TYPE xs_lil TYPE)
if(XS_LIL_LIBRARY_TYPE STREQUAL "SHARED_LIBRARY")
  target_compile_definitions(xs_lil PUBLIC XS_LIL_SHARED)
endif()
target_link_libraries(xs_compiler PRIVATE ${XS_LLVM_LIBRARY})
target_link_libraries(xs_package PRIVATE LibArchive::LibArchive bcrypt)
target_compile_definitions(xs_compiler PRIVATE XS_PROJECT_VERSION="${PROJECT_VERSION}"
                                            XS_CLANG_EXECUTABLE="${CMAKE_C_COMPILER}")
if(CMAKE_C_COMPILER_TARGET)
  target_compile_definitions(xs_compiler PRIVATE XS_CONFIGURED_TARGET_TRIPLE="${CMAKE_C_COMPILER_TARGET}")
endif()
target_compile_options(xs_compiler PRIVATE /W4 /clang:-Wconversion /clang:-Wshadow)
target_compile_options(xs_lil PRIVATE /W4 /clang:-Wconversion /clang:-Wshadow)
target_compile_options(xs_package PRIVATE /W4 /clang:-Wconversion /clang:-Wshadow)
target_compile_options(xs_compiler PUBLIC "$<$<COMPILE_LANGUAGE:C>:/FI${XS_COMPILER_CHECK_HEADER}>")
target_compile_options(xs_lil PUBLIC "$<$<COMPILE_LANGUAGE:C>:/FI${XS_COMPILER_CHECK_HEADER}>")
target_compile_options(xs_package PUBLIC "$<$<COMPILE_LANGUAGE:C>:/FI${XS_COMPILER_CHECK_HEADER}>")

add_executable(vxs sources/Main.cpp)
set_target_properties(vxs PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}")
if(APPLE)
  set_target_properties(vxs PROPERTIES INSTALL_RPATH "@loader_path/../lib")
elseif(UNIX)
  set_target_properties(vxs PROPERTIES INSTALL_RPATH "\$ORIGIN/../lib")
endif()
target_link_libraries(vxs PRIVATE xs_compiler)

# The public driver is native C++, while the language-owned frontend is a small
# Haskell process. Keeping this as an explicit build artifact makes the boundary
# visible and avoids embedding a second runtime into the C++ executable.
file(GLOB_RECURSE XS_HASKELL_FRONTEND_SOURCES CONFIGURE_DEPENDS
  "${PROJECT_SOURCE_DIR}/xs/haskell/visual-xsharp-compiler/src/*.hs"
  "${PROJECT_SOURCE_DIR}/xs/haskell/visual-xsharp-compiler/app/*.hs")
execute_process(
  COMMAND "${XS_CABAL_EXECUTABLE}" list-bin exe:vxs-frontend
  WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}/xs"
  OUTPUT_VARIABLE XS_HASKELL_FRONTEND_BUILT
  OUTPUT_STRIP_TRAILING_WHITESPACE
  COMMAND_ERROR_IS_FATAL ANY
)
set(XS_HASKELL_FRONTEND "${PROJECT_BINARY_DIR}/vxs-frontend${CMAKE_EXECUTABLE_SUFFIX}")
add_custom_command(
  OUTPUT "${XS_HASKELL_FRONTEND}"
  COMMAND "${XS_CABAL_EXECUTABLE}" build exe:vxs-frontend
  COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${XS_HASKELL_FRONTEND_BUILT}" "${XS_HASKELL_FRONTEND}"
  DEPENDS ${XS_HASKELL_FRONTEND_SOURCES}
          "${PROJECT_SOURCE_DIR}/xs/haskell/visual-xsharp-compiler/visual-xsharp-compiler.cabal"
          "${PROJECT_SOURCE_DIR}/xs/cabal.project"
  WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}/xs"
  COMMENT "Building the Haskell Visual X# frontend"
  VERBATIM
)
add_custom_target(vxs_haskell_frontend ALL DEPENDS "${XS_HASKELL_FRONTEND}")
add_dependencies(vxs vxs_haskell_frontend)

add_library(xs_backend_llvm
  sources/backend/linker.c
  sources/backend/llvm_backend.c
  sources/backend/llvm_emission.c
  sources/backend/llvm_aggregate.c
  sources/backend/llvm_integer.c
  sources/backend/llvm_string.c
)
target_include_directories(xs_backend_llvm PUBLIC "${PROJECT_SOURCE_DIR}/include" include ${LLVM_INCLUDE_DIRS})
target_link_libraries(xs_backend_llvm PUBLIC xs_lil PRIVATE ${XS_LLVM_LIBRARY})
target_compile_options(xs_backend_llvm PRIVATE /W4 /clang:-Wconversion /clang:-Wshadow)
target_compile_options(xs_backend_llvm PUBLIC "$<$<COMPILE_LANGUAGE:C>:/FI${XS_COMPILER_CHECK_HEADER}>")
