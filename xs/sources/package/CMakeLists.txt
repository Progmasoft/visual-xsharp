# SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
# SPDX-License-Identifier: MPL-2.0

# Archive transport is independent from dependency resolution. Keeping it in a
# focused target avoids coupling future ViGet work to compiler internals.
add_library(xs_package
  archive_common.c
  archive_reader.c
  archive_writer.c
)

xs_add_public_include_roots(xs_package)
target_link_libraries(xs_package PRIVATE LibArchive::LibArchive bcrypt)
xs_enable_strict_warnings(xs_package)
xs_enable_c_compiler_contract(xs_package PUBLIC)
