# SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
# SPDX-License-Identifier: MPL-2.0

if(NOT DEFINED XS_BUILD_DIR OR NOT DEFINED XS_INSTALL_PREFIX OR NOT DEFINED XS_VERSION)
  message(FATAL_ERROR "install layout test requires build dir, prefix, and version")
endif()

file(REMOVE_RECURSE "${XS_INSTALL_PREFIX}")
execute_process(
  COMMAND "${CMAKE_COMMAND}" --install "${XS_BUILD_DIR}" --prefix "${XS_INSTALL_PREFIX}" --component compiler
  RESULT_VARIABLE install_status
  OUTPUT_VARIABLE install_output
  ERROR_VARIABLE install_error
)
if(NOT install_status EQUAL 0)
  message(FATAL_ERROR "compiler install failed:\n${install_output}\n${install_error}")
endif()

set(required_files
  bin/vxs.exe
  bin/vxdc.bat
  bin/vxs-frontend.exe
  libexec/xs/project/bin/vxdc.bat
  libexec/xs/project/lib/visual-xsharp-project-${XS_VERSION}.jar
  include/Visual/C23/compiler_check.h
  include/Visual/C23/int128.hh
  include/Visual/XSharp/lil.hh
  include/Visual/XSharp/lil.hpp
  include/Visual/XSharp/lil/Module.hpp
  include/Visual/XSharp/lil-c/aot.hh
  include/Visual/XSharp/lil-c/model.hh
  lib/xs_lil_cpp.lib
  include/Visual/XSharp/backend/llvm_backend.h
  share/licenses/xs/LICENSE.txt
  share/licenses/xs/NOTICE.txt
)
foreach(relative_path IN LISTS required_files)
  if(NOT EXISTS "${XS_INSTALL_PREFIX}/${relative_path}")
    message(FATAL_ERROR "installed compiler is missing ${relative_path}")
  endif()
endforeach()

if(EXISTS "${XS_INSTALL_PREFIX}/libexec/xs/project/bin/xs-project-runtime.bat")
  message(FATAL_ERROR "installed compiler must not expose a separate xs-project-runtime launcher")
endif()

execute_process(
  COMMAND "${XS_INSTALL_PREFIX}/bin/vxs.exe" --version
  RESULT_VARIABLE version_status
  OUTPUT_VARIABLE version_output
  ERROR_VARIABLE version_error
  OUTPUT_STRIP_TRAILING_WHITESPACE
)
if(NOT version_status EQUAL 0 OR NOT version_output STREQUAL "vxs ${XS_VERSION}")
  message(FATAL_ERROR "installed vxs version check failed: '${version_output}' ${version_error}")
endif()
