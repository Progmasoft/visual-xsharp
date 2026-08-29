# SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
# SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.0

# The renewed in-memory compiler pipeline is deliberately independent from argv
# parsing, project evaluation, and process dispatch. Tests can therefore verify
# Core/CorePrep/Xpp/Xmm/LLVM behavior without linking the CLI implementation.
add_library(xs_compiler_pipeline STATIC
  "${PROJECT_SOURCE_DIR}/Visual/XSharp/Backend/LLVM/Artifact.cpp"
  "${PROJECT_SOURCE_DIR}/Visual/XSharp/Backend/LLVM/Codegen.cpp"
  "${PROJECT_SOURCE_DIR}/Visual/XSharp/Backend/LLVM/Verifier.cpp"
  "${PROJECT_SOURCE_DIR}/Visual/XSharp/Pipeline.cpp"
  "${PROJECT_SOURCE_DIR}/Visual/XSharp/Pipeline/Driver.cpp"
  "${PROJECT_SOURCE_DIR}/Visual/XSharp/Core/IR.cpp"
  "${PROJECT_SOURCE_DIR}/Visual/XSharp/Core/Verifier.cpp"
  "${PROJECT_SOURCE_DIR}/Visual/XSharp/Core/Wire.cpp"
  "${PROJECT_SOURCE_DIR}/Visual/XSharp/Core/CorePrep/Prepare.cpp"
  "${PROJECT_SOURCE_DIR}/Visual/XSharp/Core/CorePrep/Verifier.cpp"
  "${PROJECT_SOURCE_DIR}/Visual/XSharp/Core/CorePrep/Verifier/Semantics.cpp"
  "${PROJECT_SOURCE_DIR}/Visual/XSharp/Core/CorePrep/Wire/Decode.cpp"
  "${PROJECT_SOURCE_DIR}/Visual/XSharp/Core/CorePrep/Wire/Encode.cpp"
)

xs_add_public_include_roots(xs_compiler_pipeline)
target_include_directories(xs_compiler_pipeline SYSTEM PRIVATE ${LLVM_INCLUDE_DIRS})
target_link_libraries(xs_compiler_pipeline PRIVATE ${XS_LLVM_CPP_LIBRARIES})
xs_enable_strict_warnings(xs_compiler_pipeline)
