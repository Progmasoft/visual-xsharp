# SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
# SPDX-License-Identifier: MPL-2.0

add_executable(xs_backend_tests tests/backend_tests.c)
target_link_libraries(xs_backend_tests PRIVATE xs_backend_llvm)
add_test(NAME backend COMMAND xs_backend_tests ${XS_BACKEND_TEST_OBJECT} ${XS_LLD_EXECUTABLE})
set_tests_properties(backend PROPERTIES TIMEOUT 15)

xs_add_c_test(int128 tests/int128_tests.c xs_lil)
xs_add_c_test(c23_features tests/c23_features_tests.c xs_lil)
xs_add_c_test(xlil tests/xlil_tests.c xs_lil)
xs_add_c_test(lil_c_headers tests/lil_c_headers_tests.c xs_lil)
xs_add_c_test(lil_c_producer tests/lil_c_producer_tests.c xs_lil)
xs_add_cxx_test(lil_cpp tests/LilCppTests.cpp xs_lil_cpp)
xs_add_cxx_test(visual_xsharp_pipeline tests/VisualXSharpPipelineTests.cpp xs_compiler)
target_compile_definitions(xs_VisualXSharpPipelineTests PRIVATE
  XS_COREPREP_GOLDEN_PATH="${PROJECT_SOURCE_DIR}/tests/fixtures/coreprep/wire-v1.hex")
xs_add_cxx_test(core_pipeline tests/CorePipelineTests.cpp xs_compiler)
target_compile_definitions(xs_CorePipelineTests PRIVATE
  XS_CORE_GOLDEN_PATH="${PROJECT_SOURCE_DIR}/tests/fixtures/core/wire-v1.hex")
xs_add_cxx_test(llvm_backend_cpp tests/LLVMBackendTests.cpp xs_compiler)
