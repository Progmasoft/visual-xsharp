# SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
# SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.0

set(XS_INSTALL_BINDIR "bin" CACHE STRING "Visual X# compiler executable installation directory")
set(XS_INSTALL_INCLUDEDIR "include" CACHE STRING "Visual X# public header installation directory")
set(XS_INSTALL_LICENSEDIR "share/licenses/xs" CACHE STRING "Visual X# compiler license installation directory")

file(GLOB_RECURSE XS_COMMON_PUBLIC_HEADERS RELATIVE "${PROJECT_SOURCE_DIR}/include/Visual"
  "${PROJECT_SOURCE_DIR}/include/Visual/*.h" "${PROJECT_SOURCE_DIR}/include/Visual/*.hh"
  "${PROJECT_SOURCE_DIR}/include/Visual/*.hpp")
foreach(relative_header IN LISTS XS_COMMON_PUBLIC_HEADERS)
  if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/include/Visual/${relative_header}")
    message(FATAL_ERROR "Public header '${relative_header}' exists in both install source trees.")
  endif()
endforeach()

install(TARGETS vxs
  RUNTIME DESTINATION "${XS_INSTALL_BINDIR}"
  COMPONENT compiler
)

# ClangCL's dynamic AddressSanitizer runtime must travel beside an instrumented
# executable. Build-tree tests already find the configure-time copy at the
# project output root; the installed compiler needs the same invariant so it
# can be launched without inheriting a compiler-specific runtime directory.
if(XS_ENABLE_SANITIZERS)
  install(FILES "${XS_ASAN_DYNAMIC_DLL}"
    DESTINATION "${XS_INSTALL_BINDIR}"
    COMPONENT compiler
  )
endif()

# The Haskell frontend is a private sibling executable discovered relative to vxs.
# Installing both into the same directory preserves that location invariant.
install(PROGRAMS "${XS_HASKELL_FRONTEND}"
  TYPE BIN
  COMPONENT compiler
)

install(TARGETS xs_lil xs_lil_cpp xs_package
  ARCHIVE DESTINATION "lib" COMPONENT compiler
  LIBRARY DESTINATION "lib" COMPONENT compiler NAMELINK_COMPONENT compiler
  RUNTIME DESTINATION "${XS_INSTALL_BINDIR}" COMPONENT compiler
)

install(DIRECTORY "${PROJECT_SOURCE_DIR}/include/Visual/"
  DESTINATION "${XS_INSTALL_INCLUDEDIR}/Visual"
  COMPONENT compiler
  FILES_MATCHING PATTERN "*.h" PATTERN "*.hh" PATTERN "*.hpp"
)

install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/include/Visual/"
  DESTINATION "${XS_INSTALL_INCLUDEDIR}/Visual"
  COMPONENT compiler
  FILES_MATCHING PATTERN "*.h" PATTERN "*.hh" PATTERN "*.hpp"
)

install(FILES "${PROJECT_SOURCE_DIR}/LICENSE.txt" "${PROJECT_SOURCE_DIR}/NOTICE.txt"
  DESTINATION "${XS_INSTALL_LICENSEDIR}"
  COMPONENT compiler
)
