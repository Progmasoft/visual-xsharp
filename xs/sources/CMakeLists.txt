# SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
# SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.0

# Order expresses real target dependencies: the C++ legacy adapter consumes
# the legacy C library, the driver consumes the renewed compiler pipeline, and
# the old C backend consumes the legacy IR library. No component reaches into
# a later directory through global variables.
add_subdirectory(xlil)
add_subdirectory(lil)
add_subdirectory(package)
add_subdirectory(compiler)
add_subdirectory(driver)
add_subdirectory(backend)
