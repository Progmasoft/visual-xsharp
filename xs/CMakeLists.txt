# SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
# SPDX-License-Identifier: MPL-2.0

# The xs directory owns orchestration only. Each component declares its own
# sources, usage requirements, and private dependencies in a local
# CMakeLists.txt so changing one subsystem does not grow a monolithic target
# manifest again.
include(cmake/XSDependencies.cmake)
include(cmake/XSBuildPolicy.cmake)

add_subdirectory(sources)
add_subdirectory(haskell/visual-xsharp-compiler)
add_subdirectory(project-system)

include(cmake/XSInstall.cmake)
