# SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
# SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.0

# Emission tests write beside their input. Copy the fixture into the build tree
# so generated Core/LLVM artifacts never pollute the source checkout.
set(xs_haskell_fixture_dir "${XS_TEST_WORK_ROOT}/haskell-frontend")
file(MAKE_DIRECTORY "${xs_haskell_fixture_dir}")
configure_file(
  "${XS_TEST_SOURCE_ROOT}/fixtures/haskell_frontend/Main.vxs"
  "${xs_haskell_fixture_dir}/Main.vxs"
  COPYONLY
)

add_test(NAME compiler_check_file COMMAND vxs check
  -File "${xs_haskell_fixture_dir}/Main.vxs"
)
set_tests_properties(compiler_check_file PROPERTIES TIMEOUT 15)

add_test(NAME compiler_rejects_invalid_warning COMMAND vxs check
  -File "${XS_TEST_SOURCE_ROOT}/fixtures/example_project/source/Main.vxs"
  -Warnings invalid
)
set_tests_properties(compiler_rejects_invalid_warning PROPERTIES TIMEOUT 5 WILL_FAIL TRUE)

add_test(NAME compiler_rejects_misspelled_werror COMMAND vxs check
  -File "${XS_TEST_SOURCE_ROOT}/fixtures/example_project/source/Main.vxs"
  -Werrror true
)
set_tests_properties(compiler_rejects_misspelled_werror PROPERTIES TIMEOUT 5 WILL_FAIL TRUE)

add_test(NAME compiler_accepts_renewed_cli_flags COMMAND vxs check
  -File "${xs_haskell_fixture_dir}/Main.vxs"
  -Standard 26 -Compiler-Version latest -Warnings all -Werror true
  -Wexperimental true -Wshadow true -Wundef true -Type-Safe-Format true
  -Backend llvm -Llvm-OptLevel 2 -Llvm-Compiler aot -Llvm-Lto none
  -Xpp-Optimization-Passes true -Xmm-Optimization-Passes true
)
set_tests_properties(compiler_accepts_renewed_cli_flags PROPERTIES TIMEOUT 5)

add_test(NAME compiler_rejects_project_flag COMMAND vxs check -Project .)
set_tests_properties(compiler_rejects_project_flag PROPERTIES TIMEOUT 5 WILL_FAIL TRUE)

add_test(NAME compiler_rejects_module_flag COMMAND vxs check --module .)
set_tests_properties(compiler_rejects_module_flag PROPERTIES TIMEOUT 5 WILL_FAIL TRUE)

add_test(NAME compiler_emits_core_from_haskell COMMAND vxs build
  -File "${xs_haskell_fixture_dir}/Main.vxs" -Emit core
)
set_tests_properties(compiler_emits_core_from_haskell PROPERTIES TIMEOUT 15)

add_test(NAME cli_rejects_duplicate_option COMMAND vxs check
  -File "${xs_haskell_fixture_dir}/Main.vxs"
  -File "${xs_haskell_fixture_dir}/Main.vxs"
)
set_tests_properties(cli_rejects_duplicate_option PROPERTIES TIMEOUT 5 WILL_FAIL TRUE)

add_test(NAME cli_rejects_wrong_command_scope COMMAND vxs check -Emit core
  -File "${xs_haskell_fixture_dir}/Main.vxs"
)
set_tests_properties(cli_rejects_wrong_command_scope PROPERTIES TIMEOUT 5 WILL_FAIL TRUE)

xs_add_cxx_test(
  visual_xsharp_pipeline
  "${XS_TEST_SOURCE_ROOT}/VisualXSharpPipelineTests.cpp"
  xs_compiler_pipeline
)
target_compile_definitions(xs_VisualXSharpPipelineTests PRIVATE
  XS_COREPREP_GOLDEN_PATH="${XS_TEST_SOURCE_ROOT}/fixtures/coreprep/wire-v1.hex"
)

xs_add_cxx_test(
  core_pipeline
  "${XS_TEST_SOURCE_ROOT}/CorePipelineTests.cpp"
  xs_compiler
)
target_compile_definitions(xs_CorePipelineTests PRIVATE
  XS_CORE_GOLDEN_PATH="${XS_TEST_SOURCE_ROOT}/fixtures/core/wire-v1.hex"
)

xs_add_cxx_test(
  llvm_backend_cpp
  "${XS_TEST_SOURCE_ROOT}/LLVMBackendTests.cpp"
  xs_compiler_pipeline
)
