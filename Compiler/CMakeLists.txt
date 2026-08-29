# SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
# SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.0

# Bazel owns every production C++20 compiler target. CMake is intentionally
# restricted to retained legacy libraries while their behavior is either
# adopted by the renewed pipeline or retired.
include(Build/CMake/XSDependencies.cmake)
include(Build/CMake/XSBuildPolicy.cmake)

add_subdirectory(Legacy)
add_subdirectory(Package)
