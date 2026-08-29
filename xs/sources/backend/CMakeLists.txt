# SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
# SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.0

# This is the retained C/legacy-IR backend, not the renewed C++20 Xmm backend.
# Keeping the target isolated makes that ownership boundary visible and lets it
# retire without disturbing the main compiler pipeline.
add_library(xs_backend_llvm
  linker.c
  llvm_backend.c
  llvm_emission.c
  llvm_aggregate.c
  llvm_integer.c
  llvm_string.c
)

xs_add_public_include_roots(xs_backend_llvm)
# Public backend headers expose LLVM-C handle types, so consumers need the
# LLVM include root as part of the target's public compile interface.
target_include_directories(xs_backend_llvm SYSTEM PUBLIC ${LLVM_INCLUDE_DIRS})
target_link_libraries(xs_backend_llvm PUBLIC xs_lil PRIVATE ${XS_LLVM_LIBRARY})
xs_enable_strict_warnings(xs_backend_llvm)
xs_enable_c_compiler_contract(xs_backend_llvm PUBLIC)
