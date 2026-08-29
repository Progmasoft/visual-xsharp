# SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
# SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.0

xs_add_c_test(
  package_archive
  "${XS_TEST_SOURCE_ROOT}/package_archive_tests.c"
  xs_package
)

# Archive coverage performs compression and filesystem round trips. A cold
# Windows runner can exceed the unit-test default while Gradle starts.
set_tests_properties(package_archive PROPERTIES TIMEOUT 15)

if(WIN32)
  # xs_package is static, so TARGET_RUNTIME_DLLS cannot traverse its imported
  # DLL graph. Select the runtime directory matching the chosen import library;
  # mixing release lib with debug/bin loads the wrong ABI.
  set(xs_archive_runtime_dlls)
  foreach(dependency_prefix IN LISTS CMAKE_PREFIX_PATH)
    if(EXISTS "${dependency_prefix}/lib/archive.lib")
      file(GLOB prefix_runtime_dlls CONFIGURE_DEPENDS
        "${dependency_prefix}/bin/*.dll"
      )
      list(APPEND xs_archive_runtime_dlls ${prefix_runtime_dlls})
    elseif(EXISTS "${dependency_prefix}/debug/lib/archive.lib")
      file(GLOB prefix_runtime_dlls CONFIGURE_DEPENDS
        "${dependency_prefix}/debug/bin/*.dll"
      )
      list(APPEND xs_archive_runtime_dlls ${prefix_runtime_dlls})
    endif()
  endforeach()

  if(xs_archive_runtime_dlls)
    add_custom_command(TARGET xs_package_archive_tests POST_BUILD
      COMMAND "${CMAKE_COMMAND}" -E copy_if_different
              ${xs_archive_runtime_dlls}
              $<TARGET_FILE_DIR:xs_package_archive_tests>
      COMMAND_EXPAND_LISTS
      COMMENT "Copying archive and compression runtime DLLs"
    )
  endif()
endif()
