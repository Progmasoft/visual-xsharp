# SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
# SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.0

# Keep the warning contract centralized while leaving language standards and
# dependencies explicit on each component target.
function(xs_enable_strict_warnings target_name)
  target_compile_options(${target_name} PRIVATE
    /W4
    /clang:-Wconversion
    /clang:-Wshadow
  )
endfunction()

# C translation units still require the shared compiler contract during the
# gradual C23 retirement. The generator expression deliberately leaves C++20
# translation units untouched.
function(xs_enable_c_compiler_contract target_name visibility)
  target_compile_options(${target_name} ${visibility}
    "$<$<COMPILE_LANGUAGE:C>:/FI${XS_COMPILER_CHECK_HEADER}>"
  )
endfunction()

# Compiler-owned headers and common ABI headers live in distinct trees. This
# helper prevents individual components from accidentally exposing LLVM's
# implementation headers as part of the public include surface.
function(xs_add_public_include_roots target_name)
  target_include_directories(${target_name} PUBLIC
    "${PROJECT_SOURCE_DIR}"
    "${PROJECT_SOURCE_DIR}/include"
    "${PROJECT_SOURCE_DIR}/Compiler/include"
  )
endfunction()
