# SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
# SPDX-License-Identifier: MPL-2.0

set(XS_DIRECT_XHIR_FIXTURE_DIR "${CMAKE_CURRENT_BINARY_DIR}/tests/fixtures/direct_xhir")
file(MAKE_DIRECTORY "${XS_DIRECT_XHIR_FIXTURE_DIR}")
foreach(fixture Supported InvalidReturn)
  configure_file(tests/fixtures/intermediate/${fixture}.xhir
                 "${XS_DIRECT_XHIR_FIXTURE_DIR}/${fixture}.xhir" COPYONLY)
endforeach()
configure_file(tests/fixtures/source/MainCall.vxs "${XS_DIRECT_XHIR_FIXTURE_DIR}/MainCall.vxs" COPYONLY)
configure_file(tests/fixtures/source/MainFunctionOverloads.vxs
               "${XS_DIRECT_XHIR_FIXTURE_DIR}/MainFunctionOverloads.vxs" COPYONLY)
configure_file(tests/fixtures/source/MainTupleCalls.vxs "${XS_DIRECT_XHIR_FIXTURE_DIR}/MainTupleCalls.vxs" COPYONLY)
configure_file(tests/fixtures/source/MainTupleDestructure.vxs
               "${XS_DIRECT_XHIR_FIXTURE_DIR}/MainTupleDestructure.vxs" COPYONLY)
configure_file(tests/fixtures/source/MainFixedArray.vxs "${XS_DIRECT_XHIR_FIXTURE_DIR}/MainFixedArray.vxs" COPYONLY)
configure_file(tests/fixtures/source/MainEnumFlow.vxs "${XS_DIRECT_XHIR_FIXTURE_DIR}/MainEnumFlow.vxs" COPYONLY)
configure_file(tests/fixtures/source/MainDataFields.vxs "${XS_DIRECT_XHIR_FIXTURE_DIR}/MainDataFields.vxs" COPYONLY)
configure_file(tests/fixtures/source/MainNestedDataFields.vxs
               "${XS_DIRECT_XHIR_FIXTURE_DIR}/MainNestedDataFields.vxs" COPYONLY)
configure_file(tests/fixtures/source/MainDataInheritance.vxs
               "${XS_DIRECT_XHIR_FIXTURE_DIR}/MainDataInheritance.vxs" COPYONLY)
configure_file(tests/fixtures/source/MainDataConstructors.vxs
               "${XS_DIRECT_XHIR_FIXTURE_DIR}/MainDataConstructors.vxs" COPYONLY)
configure_file(tests/fixtures/source/MainDataConstructorFlow.vxs
               "${XS_DIRECT_XHIR_FIXTURE_DIR}/MainDataConstructorFlow.vxs" COPYONLY)
configure_file(tests/fixtures/source/MainDataMethods.vxs
               "${XS_DIRECT_XHIR_FIXTURE_DIR}/MainDataMethods.vxs" COPYONLY)
configure_file(tests/fixtures/source/MainDataValueProjection.vxs
               "${XS_DIRECT_XHIR_FIXTURE_DIR}/MainDataValueProjection.vxs" COPYONLY)
foreach(fixture MainGenericFunctions MainGenericRecursive MainGenericConstraint)
  configure_file(tests/fixtures/source/${fixture}.vxs "${XS_DIRECT_XHIR_FIXTURE_DIR}/${fixture}.vxs" COPYONLY)
endforeach()

add_test(NAME direct_xhir_MainGenericConstraint_output COMMAND vxs build --hir -file
  ${XS_DIRECT_XHIR_FIXTURE_DIR}/MainGenericConstraint.vxs)
set_tests_properties(direct_xhir_MainGenericConstraint_output PROPERTIES TIMEOUT 5
  PASS_REGULAR_EXPRESSION "wrote XHIR")

add_test(NAME direct_xhir_native_build COMMAND vxs build --hir -file
  ${XS_DIRECT_XHIR_FIXTURE_DIR}/Supported.xhir)
set_tests_properties(direct_xhir_native_build PROPERTIES TIMEOUT 5
  PASS_REGULAR_EXPRESSION "wrote optimized LLVM IR.*executable")
add_test(NAME direct_xhir_native_artifacts COMMAND xs_xse_artifact_tests
  ${XS_DIRECT_XHIR_FIXTURE_DIR}/Supported.ll
  ${XS_DIRECT_XHIR_FIXTURE_DIR}/Supported.obj
  ${XS_DIRECT_XHIR_FIXTURE_DIR}/Supported.vxse 7 "ret i32 7")
set_tests_properties(direct_xhir_native_artifacts PROPERTIES TIMEOUT 5 DEPENDS direct_xhir_native_build)

add_test(NAME direct_xhir_rejects_wrong_return COMMAND vxs build --hir -file
  ${XS_DIRECT_XHIR_FIXTURE_DIR}/InvalidReturn.xhir)
set_tests_properties(direct_xhir_rejects_wrong_return PROPERTIES TIMEOUT 5 WILL_FAIL TRUE)

add_test(NAME direct_xhir_source_output COMMAND vxs build --hir -file
  ${XS_DIRECT_XHIR_FIXTURE_DIR}/MainCall.vxs)
set_tests_properties(direct_xhir_source_output PROPERTIES TIMEOUT 5 PASS_REGULAR_EXPRESSION "wrote XHIR")
add_test(NAME direct_xhir_source_roundtrip COMMAND vxs build --hir -file
  ${XS_DIRECT_XHIR_FIXTURE_DIR}/MainCall.xhir)
set_tests_properties(direct_xhir_source_roundtrip PROPERTIES TIMEOUT 5 DEPENDS direct_xhir_source_output
  PASS_REGULAR_EXPRESSION "wrote optimized LLVM IR.*executable")
add_test(NAME direct_xhir_source_artifacts COMMAND xs_xse_artifact_tests
  ${XS_DIRECT_XHIR_FIXTURE_DIR}/MainCall.ll
  ${XS_DIRECT_XHIR_FIXTURE_DIR}/MainCall.obj
  ${XS_DIRECT_XHIR_FIXTURE_DIR}/MainCall.vxse 7 "call i32 @Add")
set_tests_properties(direct_xhir_source_artifacts PROPERTIES TIMEOUT 5 DEPENDS direct_xhir_source_roundtrip)

foreach(fixture MainTupleCalls MainTupleDestructure MainFixedArray MainEnumFlow)
  add_test(NAME direct_xhir_${fixture}_output COMMAND vxs build --hir -file
    ${XS_DIRECT_XHIR_FIXTURE_DIR}/${fixture}.vxs)
  set_tests_properties(direct_xhir_${fixture}_output PROPERTIES TIMEOUT 5 PASS_REGULAR_EXPRESSION "wrote XHIR")
  add_test(NAME direct_xhir_${fixture}_roundtrip COMMAND vxs build --hir -file
    ${XS_DIRECT_XHIR_FIXTURE_DIR}/${fixture}.xhir)
  set_tests_properties(direct_xhir_${fixture}_roundtrip PROPERTIES TIMEOUT 5 DEPENDS direct_xhir_${fixture}_output
    PASS_REGULAR_EXPRESSION "wrote optimized LLVM IR.*executable")
  add_test(NAME direct_xhir_${fixture}_artifacts COMMAND xs_xse_artifact_tests
    ${XS_DIRECT_XHIR_FIXTURE_DIR}/${fixture}.ll
    ${XS_DIRECT_XHIR_FIXTURE_DIR}/${fixture}.obj
    ${XS_DIRECT_XHIR_FIXTURE_DIR}/${fixture}.vxse 7)
  set_tests_properties(direct_xhir_${fixture}_artifacts PROPERTIES TIMEOUT 5 DEPENDS direct_xhir_${fixture}_roundtrip)
endforeach()

foreach(fixture MainGenericFunctions MainGenericRecursive)
  add_test(NAME direct_xhir_${fixture}_output COMMAND vxs build --hir -file
    ${XS_DIRECT_XHIR_FIXTURE_DIR}/${fixture}.vxs)
  set_tests_properties(direct_xhir_${fixture}_output PROPERTIES TIMEOUT 5 PASS_REGULAR_EXPRESSION "wrote XHIR")
  add_test(NAME direct_xhir_${fixture}_roundtrip COMMAND vxs build --hir -file
    ${XS_DIRECT_XHIR_FIXTURE_DIR}/${fixture}.xhir)
  set_tests_properties(direct_xhir_${fixture}_roundtrip PROPERTIES TIMEOUT 5
    DEPENDS direct_xhir_${fixture}_output PASS_REGULAR_EXPRESSION "wrote optimized LLVM IR.*executable")
  add_test(NAME direct_xhir_${fixture}_artifacts COMMAND xs_xse_artifact_tests
    ${XS_DIRECT_XHIR_FIXTURE_DIR}/${fixture}.ll
    ${XS_DIRECT_XHIR_FIXTURE_DIR}/${fixture}.obj
    ${XS_DIRECT_XHIR_FIXTURE_DIR}/${fixture}.vxse 7)
  set_tests_properties(direct_xhir_${fixture}_artifacts PROPERTIES TIMEOUT 5
    DEPENDS direct_xhir_${fixture}_roundtrip)
endforeach()

add_test(NAME direct_xhir_function_overloads_output COMMAND vxs build --hir -file
  ${XS_DIRECT_XHIR_FIXTURE_DIR}/MainFunctionOverloads.vxs)
set_tests_properties(direct_xhir_function_overloads_output PROPERTIES TIMEOUT 5 PASS_REGULAR_EXPRESSION "wrote XHIR")
add_test(NAME direct_xhir_function_overloads_roundtrip COMMAND vxs build --hir -file
  ${XS_DIRECT_XHIR_FIXTURE_DIR}/MainFunctionOverloads.xhir)
set_tests_properties(direct_xhir_function_overloads_roundtrip PROPERTIES TIMEOUT 5
  DEPENDS direct_xhir_function_overloads_output PASS_REGULAR_EXPRESSION "wrote optimized LLVM IR.*executable")
add_test(NAME direct_xhir_function_overloads_artifacts COMMAND xs_xse_artifact_tests
  ${XS_DIRECT_XHIR_FIXTURE_DIR}/MainFunctionOverloads.ll
  ${XS_DIRECT_XHIR_FIXTURE_DIR}/MainFunctionOverloads.obj
  ${XS_DIRECT_XHIR_FIXTURE_DIR}/MainFunctionOverloads.vxse 10 "xs$fn$choose$0" "xs$fn$choose$1")
set_tests_properties(direct_xhir_function_overloads_artifacts PROPERTIES TIMEOUT 5
  DEPENDS direct_xhir_function_overloads_roundtrip)

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
  add_test(NAME direct_xhir_${fixture}_output COMMAND vxs build --hir -file
    ${XS_DIRECT_XHIR_FIXTURE_DIR}/${fixture}.vxs)
  set_tests_properties(direct_xhir_${fixture}_output PROPERTIES TIMEOUT 5 PASS_REGULAR_EXPRESSION "wrote XHIR")
  add_test(NAME direct_xhir_${fixture}_roundtrip COMMAND vxs build --hir -file
    ${XS_DIRECT_XHIR_FIXTURE_DIR}/${fixture}.xhir)
  set_tests_properties(direct_xhir_${fixture}_roundtrip PROPERTIES TIMEOUT 5 DEPENDS direct_xhir_${fixture}_output
    PASS_REGULAR_EXPRESSION "wrote optimized LLVM IR.*executable")
  add_test(NAME direct_xhir_${fixture}_artifacts COMMAND xs_xse_artifact_tests
    ${XS_DIRECT_XHIR_FIXTURE_DIR}/${fixture}.ll
    ${XS_DIRECT_XHIR_FIXTURE_DIR}/${fixture}.obj
    ${XS_DIRECT_XHIR_FIXTURE_DIR}/${fixture}.vxse ${expected_exit})
  set_tests_properties(direct_xhir_${fixture}_artifacts PROPERTIES TIMEOUT 5 DEPENDS direct_xhir_${fixture}_roundtrip)
endforeach()
