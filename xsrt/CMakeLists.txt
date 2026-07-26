# SPDX-FileCopyrightText: 2026 Leitwolf <xs-lang.chess031@slmails.com>
# SPDX-License-Identifier: MPL-2.0

add_library(xsrt sources/optional_string.c)
add_library(XS::Runtime ALIAS xsrt)

target_include_directories(
  xsrt
  PUBLIC "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
         "$<INSTALL_INTERFACE:include>")
target_compile_options(xsrt PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wshadow)
target_compile_options(xsrt PUBLIC -include "${XS_COMPILER_CHECK_HEADER}")

if(BUILD_TESTING)
  add_executable(xsrt_optional_string_tests tests/optional_string_tests.c)
  target_link_libraries(xsrt_optional_string_tests PRIVATE XS::Runtime)
  add_test(NAME runtime_optional_string COMMAND xsrt_optional_string_tests)
  set_tests_properties(runtime_optional_string PROPERTIES TIMEOUT 5)
endif()

install(
  TARGETS xsrt
  ARCHIVE DESTINATION lib
  LIBRARY DESTINATION lib
  RUNTIME DESTINATION bin
  COMPONENT runtime)
install(
  DIRECTORY include/xs/
  DESTINATION include/xs
  COMPONENT runtime
  FILES_MATCHING
  PATTERN "*.h")
install(
  FILES "${PROJECT_SOURCE_DIR}/LICENSE.txt" "${PROJECT_SOURCE_DIR}/NOTICE.txt"
  DESTINATION share/licenses/xsrt
  COMPONENT runtime)
