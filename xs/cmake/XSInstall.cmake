# SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
# SPDX-License-Identifier: MPL-2.0

set(XS_INSTALL_BINDIR "bin" CACHE STRING "X# compiler executable installation directory")
set(XS_INSTALL_INCLUDEDIR "include" CACHE STRING "X# public header installation directory")
set(XS_INSTALL_LICENSEDIR "share/licenses/xs" CACHE STRING "X# compiler license installation directory")

file(GLOB_RECURSE XS_COMMON_PUBLIC_HEADERS RELATIVE "${PROJECT_SOURCE_DIR}/include/xs"
  "${PROJECT_SOURCE_DIR}/include/xs/*.h")
foreach(relative_header IN LISTS XS_COMMON_PUBLIC_HEADERS)
  if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/include/xs/${relative_header}")
    message(FATAL_ERROR "Public header '${relative_header}' exists in both install source trees.")
  endif()
endforeach()

install(TARGETS xs
  RUNTIME DESTINATION "${XS_INSTALL_BINDIR}"
  COMPONENT compiler
)

install(TARGETS xs_lil xs_package
  ARCHIVE DESTINATION "lib" COMPONENT compiler
  LIBRARY DESTINATION "lib" COMPONENT compiler NAMELINK_COMPONENT compiler
  RUNTIME DESTINATION "${XS_INSTALL_BINDIR}" COMPONENT compiler
)

install(DIRECTORY "${PROJECT_SOURCE_DIR}/include/xs/"
  DESTINATION "${XS_INSTALL_INCLUDEDIR}/xs"
  COMPONENT compiler
  FILES_MATCHING PATTERN "*.h"
)

install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/include/xs/"
  DESTINATION "${XS_INSTALL_INCLUDEDIR}/xs"
  COMPONENT compiler
  FILES_MATCHING PATTERN "*.h"
)

install(FILES "${PROJECT_SOURCE_DIR}/LICENSE.txt" "${PROJECT_SOURCE_DIR}/NOTICE.txt"
  DESTINATION "${XS_INSTALL_LICENSEDIR}"
  COMPONENT compiler
)
