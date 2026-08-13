# SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
# SPDX-License-Identifier: MPL-2.0

find_package(Java 25 REQUIRED COMPONENTS Runtime)

set(XS_PROJECT_RUNTIME_ROOT "${PROJECT_SOURCE_DIR}/xs_kts")
set(XS_PROJECT_RUNTIME_DIST
    "${XS_PROJECT_RUNTIME_ROOT}/build/install/xs-project-runtime")
set(XS_PROJECT_RUNTIME_LAUNCHER
    "${XS_PROJECT_RUNTIME_DIST}/bin/xs-project-runtime.bat")
set(XS_VXDC_LAUNCHER "${XS_PROJECT_RUNTIME_DIST}/bin/vxdc.bat")

file(GLOB_RECURSE XS_PROJECT_RUNTIME_SOURCES CONFIGURE_DEPENDS
  "${XS_PROJECT_RUNTIME_ROOT}/sources/main/kotlin/*.kt"
)

add_custom_command(
  OUTPUT "${XS_PROJECT_RUNTIME_LAUNCHER}" "${XS_VXDC_LAUNCHER}"
  COMMAND "${XS_PROJECT_RUNTIME_ROOT}/gradlew.bat" --daemon --build-cache
          -p "${XS_PROJECT_RUNTIME_ROOT}" installDist
  DEPENDS
    ${XS_PROJECT_RUNTIME_SOURCES}
    "${XS_PROJECT_RUNTIME_ROOT}/build.gradle.kts"
    "${XS_PROJECT_RUNTIME_ROOT}/gradle.properties"
    "${XS_PROJECT_RUNTIME_ROOT}/settings.gradle.kts"
  WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
  COMMENT "Building the bundled Kotlin project runtime"
  VERBATIM
)

add_custom_target(xs_project_runtime ALL DEPENDS
  "${XS_PROJECT_RUNTIME_LAUNCHER}"
  "${XS_VXDC_LAUNCHER}"
)

target_compile_definitions(xs_compiler PRIVATE
  XS_PROJECT_RUNTIME_BUILD="${XS_PROJECT_RUNTIME_LAUNCHER}"
  XS_PROJECT_RUNTIME_DEFAULT="${CMAKE_INSTALL_PREFIX}/libexec/xs/project-runtime/bin/xs-project-runtime.bat"
)

install(DIRECTORY "${XS_PROJECT_RUNTIME_DIST}/"
  DESTINATION "libexec/xs/project-runtime"
  COMPONENT compiler
  USE_SOURCE_PERMISSIONS
)

# The generated launcher must remain beside its runtime libraries under libexec.
# This small public wrapper keeps `vxdc` on PATH without copying or duplicating
# the Kotlin/JDBC distribution into the installation's native library directory.
configure_file("${CMAKE_CURRENT_LIST_DIR}/vxdc.bat.in"
               "${PROJECT_BINARY_DIR}/vxdc.bat" @ONLY)
install(PROGRAMS "${PROJECT_BINARY_DIR}/vxdc.bat"
  DESTINATION "bin"
  COMPONENT compiler
)
