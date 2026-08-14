# SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
# SPDX-License-Identifier: MPL-2.0

find_package(Java 25 REQUIRED COMPONENTS Runtime)

set(XS_PROJECT_EVALUATOR_ROOT "${PROJECT_SOURCE_DIR}/xs_kts")
set(XS_PROJECT_EVALUATOR_DIST
    "${XS_PROJECT_EVALUATOR_ROOT}/build/install/vxdc")
set(XS_VXDC_LAUNCHER "${XS_PROJECT_EVALUATOR_DIST}/bin/vxdc.bat")

file(GLOB_RECURSE XS_PROJECT_EVALUATOR_SOURCES CONFIGURE_DEPENDS
  "${XS_PROJECT_EVALUATOR_ROOT}/sources/main/kotlin/*.kt"
)

add_custom_command(
  OUTPUT "${XS_VXDC_LAUNCHER}"
  COMMAND "${XS_PROJECT_EVALUATOR_ROOT}/gradlew.bat" --daemon --build-cache
          -p "${XS_PROJECT_EVALUATOR_ROOT}" installDist
  DEPENDS
    ${XS_PROJECT_EVALUATOR_SOURCES}
    "${XS_PROJECT_EVALUATOR_ROOT}/build.gradle.kts"
    "${XS_PROJECT_EVALUATOR_ROOT}/gradle.properties"
    "${XS_PROJECT_EVALUATOR_ROOT}/settings.gradle.kts"
  WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
  COMMENT "Building the bundled Kotlin project evaluator"
  VERBATIM
)

add_custom_target(xs_project_evaluator ALL DEPENDS
  "${XS_VXDC_LAUNCHER}"
)

target_compile_definitions(xs_compiler PRIVATE
  XS_PROJECT_EVALUATOR_CLASSPATH_BUILD="${XS_PROJECT_EVALUATOR_DIST}/lib/*"
  XS_PROJECT_EVALUATOR_CLASSPATH_DEFAULT="${CMAKE_INSTALL_PREFIX}/libexec/xs/project/lib/*"
)

install(DIRECTORY "${XS_PROJECT_EVALUATOR_DIST}/"
  DESTINATION "libexec/xs/project"
  COMPONENT compiler
  USE_SOURCE_PERMISSIONS
)

# VXDC remains a distinct tool. The Kotlin project evaluator has no launcher;
# vxs starts its main class directly from the installed libexec classpath.
configure_file("${CMAKE_CURRENT_LIST_DIR}/vxdc.bat.in"
               "${PROJECT_BINARY_DIR}/vxdc.bat" @ONLY)
install(PROGRAMS "${PROJECT_BINARY_DIR}/vxdc.bat"
  DESTINATION "bin"
  COMPONENT compiler
)
