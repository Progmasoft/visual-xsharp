# SPDX-FileCopyrightText: 2026 Leitwolf <xs-lang.chess031@slmails.com>
# SPDX-License-Identifier: MPL-2.0

set(XS_DIRECT_XHIR_FIXTURE_DIR "${CMAKE_CURRENT_BINARY_DIR}/tests/fixtures/direct_xhir")
file(MAKE_DIRECTORY "${XS_DIRECT_XHIR_FIXTURE_DIR}")
foreach(fixture Supported InvalidReturn)
  configure_file(tests/fixtures/intermediate/${fixture}.xhir
                 "${XS_DIRECT_XHIR_FIXTURE_DIR}/${fixture}.xhir" COPYONLY)
endforeach()
configure_file(tests/fixtures/source/MainCall.xs "${XS_DIRECT_XHIR_FIXTURE_DIR}/MainCall.xs" COPYONLY)
configure_file(tests/fixtures/source/MainFunctionOverloads.xs
               "${XS_DIRECT_XHIR_FIXTURE_DIR}/MainFunctionOverloads.xs" COPYONLY)
configure_file(tests/fixtures/source/MainTupleCalls.xs "${XS_DIRECT_XHIR_FIXTURE_DIR}/MainTupleCalls.xs" COPYONLY)
configure_file(tests/fixtures/source/MainTupleDestructure.xs
               "${XS_DIRECT_XHIR_FIXTURE_DIR}/MainTupleDestructure.xs" COPYONLY)
configure_file(tests/fixtures/source/MainFixedArray.xs "${XS_DIRECT_XHIR_FIXTURE_DIR}/MainFixedArray.xs" COPYONLY)
configure_file(tests/fixtures/source/MainEnumFlow.xs "${XS_DIRECT_XHIR_FIXTURE_DIR}/MainEnumFlow.xs" COPYONLY)
configure_file(tests/fixtures/source/MainDataFields.xs "${XS_DIRECT_XHIR_FIXTURE_DIR}/MainDataFields.xs" COPYONLY)
configure_file(tests/fixtures/source/MainNestedDataFields.xs
               "${XS_DIRECT_XHIR_FIXTURE_DIR}/MainNestedDataFields.xs" COPYONLY)
configure_file(tests/fixtures/source/MainDataInheritance.xs
               "${XS_DIRECT_XHIR_FIXTURE_DIR}/MainDataInheritance.xs" COPYONLY)
configure_file(tests/fixtures/source/MainDataConstructors.xs
               "${XS_DIRECT_XHIR_FIXTURE_DIR}/MainDataConstructors.xs" COPYONLY)
configure_file(tests/fixtures/source/MainDataConstructorFlow.xs
               "${XS_DIRECT_XHIR_FIXTURE_DIR}/MainDataConstructorFlow.xs" COPYONLY)
configure_file(tests/fixtures/source/MainDataMethods.xs
               "${XS_DIRECT_XHIR_FIXTURE_DIR}/MainDataMethods.xs" COPYONLY)
configure_file(tests/fixtures/source/MainDataValueProjection.xs
               "${XS_DIRECT_XHIR_FIXTURE_DIR}/MainDataValueProjection.xs" COPYONLY)
foreach(fixture MainGenericFunctions MainGenericRecursive MainGenericConstraint)
  configure_file(tests/fixtures/source/${fixture}.xs "${XS_DIRECT_XHIR_FIXTURE_DIR}/${fixture}.xs" COPYONLY)
endforeach()

add_test(NAME direct_xhir_MainGenericConstraint_output COMMAND xs build --hir -file
  ${XS_DIRECT_XHIR_FIXTURE_DIR}/MainGenericConstraint.xs)
set_tests_properties(direct_xhir_MainGenericConstraint_output PROPERTIES TIMEOUT 5
  PASS_REGULAR_EXPRESSION "wrote XHIR")

add_test(NAME direct_xhir_native_build COMMAND xs build --hir -file
  ${XS_DIRECT_XHIR_FIXTURE_DIR}/Supported.xhir)
set_tests_properties(direct_xhir_native_build PROPERTIES TIMEOUT 5
  PASS_REGULAR_EXPRESSION "wrote optimized LLVM IR.*executable")
add_test(NAME direct_xhir_native_artifacts COMMAND xs_xse_artifact_tests
  ${XS_DIRECT_XHIR_FIXTURE_DIR}/Supported.ll
  ${XS_DIRECT_XHIR_FIXTURE_DIR}/Supported.o
  ${XS_DIRECT_XHIR_FIXTURE_DIR}/Supported.xse 7 "ret i32 7")
set_tests_properties(direct_xhir_native_artifacts PROPERTIES TIMEOUT 5 DEPENDS direct_xhir_native_build)

add_test(NAME direct_xhir_rejects_wrong_return COMMAND xs build --hir -file
  ${XS_DIRECT_XHIR_FIXTURE_DIR}/InvalidReturn.xhir)
set_tests_properties(direct_xhir_rejects_wrong_return PROPERTIES TIMEOUT 5 WILL_FAIL TRUE)

add_test(NAME direct_xhir_source_output COMMAND xs build --hir -file
  ${XS_DIRECT_XHIR_FIXTURE_DIR}/MainCall.xs)
set_tests_properties(direct_xhir_source_output PROPERTIES TIMEOUT 5 PASS_REGULAR_EXPRESSION "wrote XHIR")
add_test(NAME direct_xhir_source_roundtrip COMMAND xs build --hir -file
  ${XS_DIRECT_XHIR_FIXTURE_DIR}/MainCall.xhir)
set_tests_properties(direct_xhir_source_roundtrip PROPERTIES TIMEOUT 5 DEPENDS direct_xhir_source_output
  PASS_REGULAR_EXPRESSION "wrote optimized LLVM IR.*executable")
add_test(NAME direct_xhir_source_artifacts COMMAND xs_xse_artifact_tests
  ${XS_DIRECT_XHIR_FIXTURE_DIR}/MainCall.ll
  ${XS_DIRECT_XHIR_FIXTURE_DIR}/MainCall.o
  ${XS_DIRECT_XHIR_FIXTURE_DIR}/MainCall.xse 7 "call i32 @Add")
set_tests_properties(direct_xhir_source_artifacts PROPERTIES TIMEOUT 5 DEPENDS direct_xhir_source_roundtrip)

foreach(fixture MainTupleCalls MainTupleDestructure MainFixedArray MainEnumFlow)
  add_test(NAME direct_xhir_${fixture}_output COMMAND xs build --hir -file
    ${XS_DIRECT_XHIR_FIXTURE_DIR}/${fixture}.xs)
  set_tests_properties(direct_xhir_${fixture}_output PROPERTIES TIMEOUT 5 PASS_REGULAR_EXPRESSION "wrote XHIR")
  add_test(NAME direct_xhir_${fixture}_roundtrip COMMAND xs build --hir -file
    ${XS_DIRECT_XHIR_FIXTURE_DIR}/${fixture}.xhir)
  set_tests_properties(direct_xhir_${fixture}_roundtrip PROPERTIES TIMEOUT 5 DEPENDS direct_xhir_${fixture}_output
    PASS_REGULAR_EXPRESSION "wrote optimized LLVM IR.*executable")
  add_test(NAME direct_xhir_${fixture}_artifacts COMMAND xs_xse_artifact_tests
    ${XS_DIRECT_XHIR_FIXTURE_DIR}/${fixture}.ll
    ${XS_DIRECT_XHIR_FIXTURE_DIR}/${fixture}.o
    ${XS_DIRECT_XHIR_FIXTURE_DIR}/${fixture}.xse 7)
  set_tests_properties(direct_xhir_${fixture}_artifacts PROPERTIES TIMEOUT 5 DEPENDS direct_xhir_${fixture}_roundtrip)
endforeach()

foreach(fixture MainGenericFunctions MainGenericRecursive)
  add_test(NAME direct_xhir_${fixture}_output COMMAND xs build --hir -file
    ${XS_DIRECT_XHIR_FIXTURE_DIR}/${fixture}.xs)
  set_tests_properties(direct_xhir_${fixture}_output PROPERTIES TIMEOUT 5 PASS_REGULAR_EXPRESSION "wrote XHIR")
  add_test(NAME direct_xhir_${fixture}_roundtrip COMMAND xs build --hir -file
    ${XS_DIRECT_XHIR_FIXTURE_DIR}/${fixture}.xhir)
  set_tests_properties(direct_xhir_${fixture}_roundtrip PROPERTIES TIMEOUT 5
    DEPENDS direct_xhir_${fixture}_output PASS_REGULAR_EXPRESSION "wrote optimized LLVM IR.*executable")
  add_test(NAME direct_xhir_${fixture}_artifacts COMMAND xs_xse_artifact_tests
    ${XS_DIRECT_XHIR_FIXTURE_DIR}/${fixture}.ll
    ${XS_DIRECT_XHIR_FIXTURE_DIR}/${fixture}.o
    ${XS_DIRECT_XHIR_FIXTURE_DIR}/${fixture}.xse 7)
  set_tests_properties(direct_xhir_${fixture}_artifacts PROPERTIES TIMEOUT 5
    DEPENDS direct_xhir_${fixture}_roundtrip)
endforeach()

add_test(NAME direct_xhir_function_overloads_output COMMAND xs build --hir -file
  ${XS_DIRECT_XHIR_FIXTURE_DIR}/MainFunctionOverloads.xs)
set_tests_properties(direct_xhir_function_overloads_output PROPERTIES TIMEOUT 5 PASS_REGULAR_EXPRESSION "wrote XHIR")
add_test(NAME direct_xhir_function_overloads_roundtrip COMMAND xs build --hir -file
  ${XS_DIRECT_XHIR_FIXTURE_DIR}/MainFunctionOverloads.xhir)
set_tests_properties(direct_xhir_function_overloads_roundtrip PROPERTIES TIMEOUT 5
  DEPENDS direct_xhir_function_overloads_output PASS_REGULAR_EXPRESSION "wrote optimized LLVM IR.*executable")
add_test(NAME direct_xhir_function_overloads_artifacts COMMAND xs_xse_artifact_tests
  ${XS_DIRECT_XHIR_FIXTURE_DIR}/MainFunctionOverloads.ll
  ${XS_DIRECT_XHIR_FIXTURE_DIR}/MainFunctionOverloads.o
  ${XS_DIRECT_XHIR_FIXTURE_DIR}/MainFunctionOverloads.xse 10 "xs$fn$choose$0" "xs$fn$choose$1")
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
  add_test(NAME direct_xhir_${fixture}_output COMMAND xs build --hir -file
    ${XS_DIRECT_XHIR_FIXTURE_DIR}/${fixture}.xs)
  set_tests_properties(direct_xhir_${fixture}_output PROPERTIES TIMEOUT 5 PASS_REGULAR_EXPRESSION "wrote XHIR")
  add_test(NAME direct_xhir_${fixture}_roundtrip COMMAND xs build --hir -file
    ${XS_DIRECT_XHIR_FIXTURE_DIR}/${fixture}.xhir)
  set_tests_properties(direct_xhir_${fixture}_roundtrip PROPERTIES TIMEOUT 5 DEPENDS direct_xhir_${fixture}_output
    PASS_REGULAR_EXPRESSION "wrote optimized LLVM IR.*executable")
  add_test(NAME direct_xhir_${fixture}_artifacts COMMAND xs_xse_artifact_tests
    ${XS_DIRECT_XHIR_FIXTURE_DIR}/${fixture}.ll
    ${XS_DIRECT_XHIR_FIXTURE_DIR}/${fixture}.o
    ${XS_DIRECT_XHIR_FIXTURE_DIR}/${fixture}.xse ${expected_exit})
  set_tests_properties(direct_xhir_${fixture}_artifacts PROPERTIES TIMEOUT 5 DEPENDS direct_xhir_${fixture}_roundtrip)
endforeach()
