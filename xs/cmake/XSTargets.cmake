# SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
# SPDX-License-Identifier: MPL-2.0

if(NOT LLVM_DIR AND DEFINED ENV{LLVM_DIR})
  set(LLVM_DIR "$ENV{LLVM_DIR}")
elseif(NOT LLVM_DIR AND DEFINED ENV{LLVM_ROOT})
  list(PREPEND CMAKE_PREFIX_PATH "$ENV{LLVM_ROOT}")
endif()
find_package(LLVM REQUIRED CONFIG)
find_package(LibArchive REQUIRED)
find_package(OpenSSL REQUIRED COMPONENTS Crypto)
find_library(XS_LLVM_LIBRARY NAMES LLVM-C HINTS ${LLVM_LIBRARY_DIRS} REQUIRED)
find_package(Threads REQUIRED)
find_package(fmt REQUIRED CONFIG)
find_program(XS_CARGO_EXECUTABLE NAMES cargo REQUIRED)

# Some dependency package files alter this directory-scoped default.  Keep every
# Visual X# target on the DLL CRT so objects can safely cross shared-library
# boundaries and match the test executables.
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreadedDLL")

if(NOT EXISTS "${PROJECT_SOURCE_DIR}/third_party/dimcli/libs/dimcli/cli.cpp")
  message(FATAL_ERROR "DIMCLI is missing; initialize dependencies with: git submodule update --init --recursive")
endif()
add_library(xs_dimcli STATIC
  "${PROJECT_SOURCE_DIR}/third_party/dimcli/libs/dimcli/cli.cpp"
)
target_include_directories(xs_dimcli SYSTEM PUBLIC "${PROJECT_SOURCE_DIR}/third_party/dimcli/libs")
target_compile_options(xs_dimcli PRIVATE /clang:-Wno-deprecated-declarations)

set(XS_XSLANG_TARGET_DIR "${PROJECT_BINARY_DIR}/xslang-target")
set(XS_XSLANG_STATIC_LIBRARY "${XS_XSLANG_TARGET_DIR}/debug/xslang.lib")
file(GLOB_RECURSE XS_XSLANG_RUST_SOURCES CONFIGURE_DEPENDS "${PROJECT_SOURCE_DIR}/xslang/sources/*.rs")
add_custom_command(
  OUTPUT "${XS_XSLANG_STATIC_LIBRARY}"
  COMMAND "${CMAKE_COMMAND}" -E env "RUSTFLAGS=-C target-feature=-crt-static"
          "${XS_CARGO_EXECUTABLE}" build --lib --target-dir "${XS_XSLANG_TARGET_DIR}"
  DEPENDS ${XS_XSLANG_RUST_SOURCES} "${PROJECT_SOURCE_DIR}/xslang/Cargo.toml"
          "${PROJECT_SOURCE_DIR}/xslang/build.rs"
          "${PROJECT_SOURCE_DIR}/xslang/rust-toolchain.toml"
  WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}/xslang"
  COMMENT "Building the Rust compiler core"
  VERBATIM
)
add_custom_target(xslang_compiler_core_build DEPENDS "${XS_XSLANG_STATIC_LIBRARY}")
add_library(xslang_compiler_core STATIC IMPORTED GLOBAL)
set_target_properties(xslang_compiler_core PROPERTIES IMPORTED_LOCATION "${XS_XSLANG_STATIC_LIBRARY}")
add_dependencies(xslang_compiler_core xslang_compiler_core_build)
target_link_libraries(xslang_compiler_core INTERFACE Threads::Threads ${CMAKE_CXX_IMPLICIT_LINK_LIBRARIES})
if(WIN32)
  target_link_libraries(xslang_compiler_core INTERFACE bcrypt ntdll userenv ws2_32)
endif()

add_library(xs_compiler
  ../Visual/XSharp/Pipeline.cpp
  ../Visual/XSharp/Core/CorePrep/Verifier.cpp
  sources/Ast.cpp
  sources/codegen/Plan.cpp
  sources/compiler_core/syntax_packet.c
  sources/Diagnostic.cpp
  sources/driver/cli.c
  sources/driver/compiler_core_native.c
  sources/driver/DirectXhir.cpp
  sources/driver/DirectXmir.cpp
  sources/driver/direct_xlil.c
  sources/driver/native_artifact.c
  sources/driver/Options.cpp
  sources/driver/project_driver.c
  sources/driver/test_runner.c
  sources/lexer.c
  sources/macro/expanded_view.c
  sources/macro/expansion.c
  sources/macro/fragment.c
  sources/macro/reparse.c
  sources/macro/rewrite.c
  sources/macro/validation.c
  sources/mir/borrow_checker.c
  sources/mir/hir_lowering.c
  sources/mir/model_blocks.c
  sources/mir/model.c
  sources/mir/model_writer.c
  sources/mir/optimizer.c
  sources/mir/xlil_lowering.c
  sources/mono/Plan.cpp
  sources/hir/cffi.c
  sources/hir/dump.c
  sources/hir/expression_check.c
  sources/hir/expression_check_api.c
  sources/hir/expression_check_string.c
  sources/hir/result_constructor.c
  sources/hir/standard_library.c
  sources/hir/module_graph.c
  sources/hir/module_model.c
  sources/hir/module_registry.c
  sources/hir/import_resolver.c
  sources/hir/inheritance.c
  sources/hir/name_resolution.c
  sources/hir/symbol_table.c
  sources/hir/syntax_helpers.c
  sources/hir/type_info.c
  sources/hir/type_resolution.c
  sources/hir/type_resolution_macro_view.c
  sources/parser.c
  sources/source_include.c
  sources/syntax_ast.c
  sources/syntax_parser.c
  sources/syntax/parser_macro.c
  sources/syntax/parser_declaration.c
  sources/syntax/parser_expression.c
  sources/syntax/parser_statement.c
  sources/syntax/parser_type.c
  sources/token.c
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
                                        WINDOWS_EXPORT_ALL_SYMBOLS ON)

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
target_link_libraries(xs_compiler PUBLIC xs_lil PRIVATE xslang_compiler_core xs_dimcli fmt::fmt)
target_link_libraries(xs_package PRIVATE LibArchive::LibArchive OpenSSL::Crypto)
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
target_link_libraries(vxs PRIVATE xs_backend_llvm)
if(WIN32)
  add_custom_command(TARGET vxs POST_BUILD
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            $<TARGET_RUNTIME_DLLS:vxs> $<TARGET_FILE_DIR:vxs>
    COMMAND_EXPAND_LISTS
    COMMENT "Copying the Visual X# runtime DLLs"
  )
endif()
