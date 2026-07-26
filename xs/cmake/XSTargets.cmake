# SPDX-FileCopyrightText: 2026 Leitwolf <xs-lang.chess031@slmails.com>
# SPDX-License-Identifier: MPL-2.0

find_package(LLVM REQUIRED CONFIG)
find_package(LibArchive REQUIRED)
find_package(OpenSSL REQUIRED COMPONENTS Crypto)
find_library(XS_LLVM_LIBRARY NAMES LLVM-${LLVM_VERSION_MAJOR} LLVM HINTS ${LLVM_LIBRARY_DIRS} REQUIRED)
find_package(Threads REQUIRED)
find_program(XS_CARGO_EXECUTABLE NAMES cargo REQUIRED)

set(XS_XSLANG_TARGET_DIR "${PROJECT_BINARY_DIR}/xslang-target")
set(XS_XSLANG_STATIC_LIBRARY "${XS_XSLANG_TARGET_DIR}/debug/libxslang.a")
file(GLOB_RECURSE XS_XSLANG_RUST_SOURCES CONFIGURE_DEPENDS "${PROJECT_SOURCE_DIR}/xslang/sources/*.rs")
add_custom_command(
  OUTPUT "${XS_XSLANG_STATIC_LIBRARY}"
  COMMAND "${XS_CARGO_EXECUTABLE}" build --lib --target-dir "${XS_XSLANG_TARGET_DIR}"
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
target_link_libraries(xslang_compiler_core INTERFACE Threads::Threads ${CMAKE_DL_LIBS} m)

add_library(xs_compiler
  sources/ast.c
  sources/codegen/units.c
  sources/compiler_core/syntax_packet.c
  sources/diagnostic.c
  sources/driver/cli.c
  sources/driver/compiler_core_native.c
  sources/driver/direct_xhir.c
  sources/driver/direct_xmir.c
  sources/driver/direct_xlil.c
  sources/driver/native_artifact.c
  sources/driver/options.c
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
  sources/mono/plan.c
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

add_library(xs_lil
  sources/int128.c
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
  sources/xlil/verify.c
  sources/xlil/writer.c
)

add_library(xs_package
  sources/package/archive_common.c
  sources/package/archive_reader.c
  sources/package/archive_writer.c
)

target_include_directories(xs_compiler PUBLIC "${PROJECT_SOURCE_DIR}/include" include)
target_include_directories(xs_lil PUBLIC "${PROJECT_SOURCE_DIR}/include" include)
target_include_directories(xs_package PUBLIC "${PROJECT_SOURCE_DIR}/include" include)
target_link_libraries(xs_compiler PUBLIC xs_lil PRIVATE xslang_compiler_core)
target_link_libraries(xs_package PRIVATE LibArchive::LibArchive OpenSSL::Crypto)
target_compile_definitions(xs_package PUBLIC _POSIX_C_SOURCE=200809L)
target_compile_definitions(xs_compiler PRIVATE _POSIX_C_SOURCE=200809L XS_PROJECT_VERSION="${PROJECT_VERSION}"
                                            XS_CLANG_EXECUTABLE="${CMAKE_C_COMPILER}")
if(CMAKE_C_COMPILER_TARGET)
  target_compile_definitions(xs_compiler PRIVATE XS_CONFIGURED_TARGET_TRIPLE="${CMAKE_C_COMPILER_TARGET}")
endif()
target_compile_options(xs_compiler PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wshadow)
target_compile_options(xs_lil PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wshadow)
target_compile_options(xs_package PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wshadow)
target_compile_options(xs_compiler PUBLIC -include "${XS_COMPILER_CHECK_HEADER}")
target_compile_options(xs_lil PUBLIC -include "${XS_COMPILER_CHECK_HEADER}")
target_compile_options(xs_package PUBLIC -include "${XS_COMPILER_CHECK_HEADER}")

add_executable(xs sources/main.c)
set_target_properties(xs PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}")
target_link_libraries(xs PRIVATE xs_compiler)

add_library(xs_backend_llvm
  sources/backend/linker.c
  sources/backend/llvm_backend.c
  sources/backend/llvm_aggregate.c
  sources/backend/llvm_integer.c
  sources/backend/llvm_string.c
)
target_include_directories(xs_backend_llvm PUBLIC "${PROJECT_SOURCE_DIR}/include" include PRIVATE ${LLVM_INCLUDE_DIRS})
target_link_libraries(xs_backend_llvm PUBLIC xs_lil PRIVATE ${XS_LLVM_LIBRARY})
target_compile_options(xs_backend_llvm PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wshadow)
target_compile_options(xs_backend_llvm PUBLIC -include "${XS_COMPILER_CHECK_HEADER}")
target_link_libraries(xs PRIVATE xs_backend_llvm)
