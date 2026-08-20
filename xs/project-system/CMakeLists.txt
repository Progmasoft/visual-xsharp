# SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
# SPDX-License-Identifier: MPL-2.0

# The Kotlin evaluator is a project-system component, not a compiler source
# group. It owns the bundled distribution and exposes only the classpath needed
# by the native driver plus the separate public VXDC launcher.
find_package(Java 25 REQUIRED COMPONENTS Runtime)

set(xs_project_evaluator_root "${PROJECT_SOURCE_DIR}/xs_kts")
set(xs_project_evaluator_dist
  "${xs_project_evaluator_root}/build/install/vxdc"
)
set(xs_vxdc_launcher "${xs_project_evaluator_dist}/bin/vxdc.bat")

file(GLOB_RECURSE xs_project_evaluator_sources CONFIGURE_DEPENDS
  "${xs_project_evaluator_root}/sources/main/kotlin/*.kt"
)

add_custom_command(
  OUTPUT "${xs_vxdc_launcher}"
  COMMAND "${xs_project_evaluator_root}/gradlew.bat" --daemon --build-cache
          -p "${xs_project_evaluator_root}" installDist
  DEPENDS
    ${xs_project_evaluator_sources}
    "${xs_project_evaluator_root}/build.gradle.kts"
    "${xs_project_evaluator_root}/gradle.properties"
    "${xs_project_evaluator_root}/settings.gradle.kts"
  WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
  COMMENT "Building the bundled Kotlin project evaluator"
  VERBATIM
)

add_custom_target(xs_project_evaluator ALL DEPENDS "${xs_vxdc_launcher}")

target_compile_definitions(xs_compiler PRIVATE
  XS_PROJECT_EVALUATOR_CLASSPATH_BUILD="${xs_project_evaluator_dist}/lib/*"
  XS_PROJECT_EVALUATOR_CLASSPATH_DEFAULT="${CMAKE_INSTALL_PREFIX}/libexec/xs/project/lib/*"
)

install(DIRECTORY "${xs_project_evaluator_dist}/"
  DESTINATION "libexec/xs/project"
  COMPONENT compiler
  USE_SOURCE_PERMISSIONS
)

# VXDC remains a distinct tool. The Kotlin evaluator has no second launcher;
# vxs invokes its main class directly from the installed libexec classpath.
configure_file(
  "${PROJECT_SOURCE_DIR}/xs/cmake/vxdc.bat.in"
  "${PROJECT_BINARY_DIR}/vxdc.bat"
  @ONLY
)
install(PROGRAMS "${PROJECT_BINARY_DIR}/vxdc.bat"
  DESTINATION "bin"
  COMPONENT compiler
)
