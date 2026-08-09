# SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
# SPDX-License-Identifier: MPL-2.0

set(XS_DIRECT_XMIR_FIXTURE_DIR "${CMAKE_CURRENT_BINARY_DIR}/tests/fixtures/direct_xmir")
file(MAKE_DIRECTORY "${XS_DIRECT_XMIR_FIXTURE_DIR}")
foreach(fixture Supported InvalidLocal)
  configure_file(tests/fixtures/intermediate/${fixture}.xmir
                 "${XS_DIRECT_XMIR_FIXTURE_DIR}/${fixture}.xmir" COPYONLY)
endforeach()
configure_file(tests/fixtures/source/MainCall.xs "${XS_DIRECT_XMIR_FIXTURE_DIR}/MainCall.xs" COPYONLY)
configure_file(tests/fixtures/source/MainFunctionOverloads.xs
               "${XS_DIRECT_XMIR_FIXTURE_DIR}/MainFunctionOverloads.xs" COPYONLY)
configure_file(tests/fixtures/source/MainTupleCalls.xs "${XS_DIRECT_XMIR_FIXTURE_DIR}/MainTupleCalls.xs" COPYONLY)
configure_file(tests/fixtures/source/MainTupleDestructure.xs
               "${XS_DIRECT_XMIR_FIXTURE_DIR}/MainTupleDestructure.xs" COPYONLY)
configure_file(tests/fixtures/source/MainFixedArray.xs "${XS_DIRECT_XMIR_FIXTURE_DIR}/MainFixedArray.xs" COPYONLY)
configure_file(tests/fixtures/source/MainEnumFlow.xs "${XS_DIRECT_XMIR_FIXTURE_DIR}/MainEnumFlow.xs" COPYONLY)
configure_file(tests/fixtures/source/MainDataFields.xs "${XS_DIRECT_XMIR_FIXTURE_DIR}/MainDataFields.xs" COPYONLY)
configure_file(tests/fixtures/source/MainNestedDataFields.xs
               "${XS_DIRECT_XMIR_FIXTURE_DIR}/MainNestedDataFields.xs" COPYONLY)
configure_file(tests/fixtures/source/MainDataInheritance.xs
               "${XS_DIRECT_XMIR_FIXTURE_DIR}/MainDataInheritance.xs" COPYONLY)
configure_file(tests/fixtures/source/MainDataConstructors.xs
               "${XS_DIRECT_XMIR_FIXTURE_DIR}/MainDataConstructors.xs" COPYONLY)
configure_file(tests/fixtures/source/MainDataConstructorFlow.xs
               "${XS_DIRECT_XMIR_FIXTURE_DIR}/MainDataConstructorFlow.xs" COPYONLY)
configure_file(tests/fixtures/source/MainDataMethods.xs
               "${XS_DIRECT_XMIR_FIXTURE_DIR}/MainDataMethods.xs" COPYONLY)
configure_file(tests/fixtures/source/MainDataValueProjection.xs
               "${XS_DIRECT_XMIR_FIXTURE_DIR}/MainDataValueProjection.xs" COPYONLY)
foreach(fixture MainGenericFunctions MainGenericRecursive)
  configure_file(tests/fixtures/source/${fixture}.xs "${XS_DIRECT_XMIR_FIXTURE_DIR}/${fixture}.xs" COPYONLY)
endforeach()

add_test(NAME direct_xmir_native_build COMMAND vxs build --mir -file
  ${XS_DIRECT_XMIR_FIXTURE_DIR}/Supported.xmir)
set_tests_properties(direct_xmir_native_build PROPERTIES TIMEOUT 5
  PASS_REGULAR_EXPRESSION "wrote optimized LLVM IR.*executable")
add_test(NAME direct_xmir_native_artifacts COMMAND xs_xse_artifact_tests
  ${XS_DIRECT_XMIR_FIXTURE_DIR}/Supported.ll
  ${XS_DIRECT_XMIR_FIXTURE_DIR}/Supported.obj
  ${XS_DIRECT_XMIR_FIXTURE_DIR}/Supported.vxse 7 "ret i32 7")
set_tests_properties(direct_xmir_native_artifacts PROPERTIES TIMEOUT 5 DEPENDS direct_xmir_native_build)

add_test(NAME direct_xmir_rejects_invalid_local COMMAND vxs build --mir -file
  ${XS_DIRECT_XMIR_FIXTURE_DIR}/InvalidLocal.xmir)
set_tests_properties(direct_xmir_rejects_invalid_local PROPERTIES TIMEOUT 5 WILL_FAIL TRUE)

add_test(NAME direct_xmir_source_output COMMAND vxs build --mir -file
  ${XS_DIRECT_XMIR_FIXTURE_DIR}/MainCall.xs)
set_tests_properties(direct_xmir_source_output PROPERTIES TIMEOUT 5 PASS_REGULAR_EXPRESSION "wrote XMIR")
add_test(NAME direct_xmir_source_roundtrip COMMAND vxs build --mir -file
  ${XS_DIRECT_XMIR_FIXTURE_DIR}/MainCall.xmir)
set_tests_properties(direct_xmir_source_roundtrip PROPERTIES TIMEOUT 5 DEPENDS direct_xmir_source_output
  PASS_REGULAR_EXPRESSION "wrote optimized LLVM IR.*executable")
add_test(NAME direct_xmir_source_artifacts COMMAND xs_xse_artifact_tests
  ${XS_DIRECT_XMIR_FIXTURE_DIR}/MainCall.ll
  ${XS_DIRECT_XMIR_FIXTURE_DIR}/MainCall.obj
  ${XS_DIRECT_XMIR_FIXTURE_DIR}/MainCall.vxse 7 "call i32 @Add")
set_tests_properties(direct_xmir_source_artifacts PROPERTIES TIMEOUT 5 DEPENDS direct_xmir_source_roundtrip)

foreach(fixture MainTupleCalls MainTupleDestructure MainFixedArray MainEnumFlow)
  add_test(NAME direct_xmir_${fixture}_output COMMAND vxs build --mir -file
    ${XS_DIRECT_XMIR_FIXTURE_DIR}/${fixture}.xs)
  set_tests_properties(direct_xmir_${fixture}_output PROPERTIES TIMEOUT 5 PASS_REGULAR_EXPRESSION "wrote XMIR")
  add_test(NAME direct_xmir_${fixture}_roundtrip COMMAND vxs build --mir -file
    ${XS_DIRECT_XMIR_FIXTURE_DIR}/${fixture}.xmir)
  set_tests_properties(direct_xmir_${fixture}_roundtrip PROPERTIES TIMEOUT 5 DEPENDS direct_xmir_${fixture}_output
    PASS_REGULAR_EXPRESSION "wrote optimized LLVM IR.*executable")
  add_test(NAME direct_xmir_${fixture}_artifacts COMMAND xs_xse_artifact_tests
    ${XS_DIRECT_XMIR_FIXTURE_DIR}/${fixture}.ll
    ${XS_DIRECT_XMIR_FIXTURE_DIR}/${fixture}.obj
    ${XS_DIRECT_XMIR_FIXTURE_DIR}/${fixture}.vxse 7)
  set_tests_properties(direct_xmir_${fixture}_artifacts PROPERTIES TIMEOUT 5 DEPENDS direct_xmir_${fixture}_roundtrip)
endforeach()

foreach(fixture MainGenericFunctions MainGenericRecursive)
  add_test(NAME direct_xmir_${fixture}_output COMMAND vxs build --mir -file
    ${XS_DIRECT_XMIR_FIXTURE_DIR}/${fixture}.xs)
  set_tests_properties(direct_xmir_${fixture}_output PROPERTIES TIMEOUT 5 PASS_REGULAR_EXPRESSION "wrote XMIR")
  add_test(NAME direct_xmir_${fixture}_roundtrip COMMAND vxs build --mir -file
    ${XS_DIRECT_XMIR_FIXTURE_DIR}/${fixture}.xmir)
  set_tests_properties(direct_xmir_${fixture}_roundtrip PROPERTIES TIMEOUT 5
    DEPENDS direct_xmir_${fixture}_output PASS_REGULAR_EXPRESSION "wrote optimized LLVM IR.*executable")
  add_test(NAME direct_xmir_${fixture}_artifacts COMMAND xs_xse_artifact_tests
    ${XS_DIRECT_XMIR_FIXTURE_DIR}/${fixture}.ll
    ${XS_DIRECT_XMIR_FIXTURE_DIR}/${fixture}.obj
    ${XS_DIRECT_XMIR_FIXTURE_DIR}/${fixture}.vxse 7)
  set_tests_properties(direct_xmir_${fixture}_artifacts PROPERTIES TIMEOUT 5
    DEPENDS direct_xmir_${fixture}_roundtrip)
endforeach()

add_test(NAME direct_xmir_function_overloads_output COMMAND vxs build --mir -file
  ${XS_DIRECT_XMIR_FIXTURE_DIR}/MainFunctionOverloads.xs)
set_tests_properties(direct_xmir_function_overloads_output PROPERTIES TIMEOUT 5 PASS_REGULAR_EXPRESSION "wrote XMIR")
add_test(NAME direct_xmir_function_overloads_roundtrip COMMAND vxs build --mir -file
  ${XS_DIRECT_XMIR_FIXTURE_DIR}/MainFunctionOverloads.xmir)
set_tests_properties(direct_xmir_function_overloads_roundtrip PROPERTIES TIMEOUT 5
  DEPENDS direct_xmir_function_overloads_output PASS_REGULAR_EXPRESSION "wrote optimized LLVM IR.*executable")
add_test(NAME direct_xmir_function_overloads_artifacts COMMAND xs_xse_artifact_tests
  ${XS_DIRECT_XMIR_FIXTURE_DIR}/MainFunctionOverloads.ll
  ${XS_DIRECT_XMIR_FIXTURE_DIR}/MainFunctionOverloads.obj
  ${XS_DIRECT_XMIR_FIXTURE_DIR}/MainFunctionOverloads.vxse 10 "xs$fn$choose$0" "xs$fn$choose$1")
set_tests_properties(direct_xmir_function_overloads_artifacts PROPERTIES TIMEOUT 5
  DEPENDS direct_xmir_function_overloads_roundtrip)

foreach(fixture MainDataFields MainNestedDataFields MainDataInheritance MainDataConstructors MainDataConstructorFlow MainDataMethods
                MainDataValueProjection)
  if(fixture STREQUAL "MainDataFields")
    set(expected_exit 9)
  elseif(fixture STREQUAL "MainDataInheritance")
    set(expected_exit 25)
  elseif(fixture STREQUAL "MainDataConstructors")
    set(expected_exit 16)
  elseif(fixture STREQUAL "MainDataConstructorFlow")
    set(expected_exit 9)
  elseif(fixture STREQUAL "MainDataMethods")
    set(expected_exit 15)
  elseif(fixture STREQUAL "MainDataValueProjection")
    set(expected_exit 10)
  else()
    set(expected_exit 22)
  endif()
  add_test(NAME direct_xmir_${fixture}_output COMMAND vxs build --mir -file
    ${XS_DIRECT_XMIR_FIXTURE_DIR}/${fixture}.xs)
  set_tests_properties(direct_xmir_${fixture}_output PROPERTIES TIMEOUT 5 PASS_REGULAR_EXPRESSION "wrote XMIR")
  add_test(NAME direct_xmir_${fixture}_roundtrip COMMAND vxs build --mir -file
    ${XS_DIRECT_XMIR_FIXTURE_DIR}/${fixture}.xmir)
  set_tests_properties(direct_xmir_${fixture}_roundtrip PROPERTIES TIMEOUT 5 DEPENDS direct_xmir_${fixture}_output
    PASS_REGULAR_EXPRESSION "wrote optimized LLVM IR.*executable")
  add_test(NAME direct_xmir_${fixture}_artifacts COMMAND xs_xse_artifact_tests
    ${XS_DIRECT_XMIR_FIXTURE_DIR}/${fixture}.ll
    ${XS_DIRECT_XMIR_FIXTURE_DIR}/${fixture}.obj
    ${XS_DIRECT_XMIR_FIXTURE_DIR}/${fixture}.vxse ${expected_exit})
  set_tests_properties(direct_xmir_${fixture}_artifacts PROPERTIES TIMEOUT 5 DEPENDS direct_xmir_${fixture}_roundtrip)
endforeach()
