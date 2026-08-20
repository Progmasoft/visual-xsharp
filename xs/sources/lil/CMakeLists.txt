# SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
# SPDX-License-Identifier: MPL-2.0

# The C++ adapter is a separate component rather than an incidental source
# group inside xs_lil. Consumers may link the adapter through the stable alias
# without learning the underlying legacy C target name.
add_library(xs_lil_cpp STATIC
  Builder.cpp
  Error.cpp
  Module.cpp
)
add_library(xs::lil ALIAS xs_lil_cpp)

xs_add_public_include_roots(xs_lil_cpp)
target_link_libraries(xs_lil_cpp PUBLIC xs_lil)
xs_enable_strict_warnings(xs_lil_cpp)
