# SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
# SPDX-License-Identifier: MPL-2.0

add_test(NAME source_native_call_build COMMAND vxs build -File ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainCall.vxs)
set_tests_properties(source_native_call_build PROPERTIES TIMEOUT 5
                    PASS_REGULAR_EXPRESSION "wrote optimized LLVM IR.*executable")
add_test(NAME source_native_call_artifacts COMMAND xs_xse_artifact_tests ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainCall.ll
                                            ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainCall.obj
                                            ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainCall.vxse 7 "call i32 @Add")
set_tests_properties(source_native_call_artifacts PROPERTIES DEPENDS source_native_call_build TIMEOUT 5)
add_test(NAME source_native_nested_call_build COMMAND vxs build -File
                                                 ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainNestedCall.vxs)
set_tests_properties(source_native_nested_call_build PROPERTIES TIMEOUT 5
                    PASS_REGULAR_EXPRESSION "wrote optimized LLVM IR.*executable")
add_test(NAME source_native_nested_call_artifacts COMMAND xs_xse_artifact_tests
                                                   ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainNestedCall.ll
                                                   ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainNestedCall.obj
                                                   ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainNestedCall.vxse 7
                                                   "call i32 @Add")
set_tests_properties(source_native_nested_call_artifacts PROPERTIES DEPENDS source_native_nested_call_build TIMEOUT 5)
add_test(NAME source_native_local_call_build COMMAND vxs build -File ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainLocalCall.vxs)
set_tests_properties(source_native_local_call_build PROPERTIES TIMEOUT 5
                    PASS_REGULAR_EXPRESSION "wrote optimized LLVM IR.*executable")
add_test(NAME source_native_local_call_artifacts COMMAND xs_xse_artifact_tests
                                                  ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainLocalCall.ll
                                                  ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainLocalCall.obj
                                                  ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainLocalCall.vxse 7
                                                  "call i32 @Add")
set_tests_properties(source_native_local_call_artifacts PROPERTIES DEPENDS source_native_local_call_build TIMEOUT 5)
add_test(NAME source_native_recursive_call_build COMMAND vxs build -File
  ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainRecursiveCall.vxs)
set_tests_properties(source_native_recursive_call_build PROPERTIES TIMEOUT 5
  PASS_REGULAR_EXPRESSION "wrote optimized LLVM IR.*executable")
add_test(NAME source_native_recursive_call_artifacts COMMAND xs_xse_artifact_tests
  ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainRecursiveCall.ll
  ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainRecursiveCall.obj
  ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainRecursiveCall.vxse 120 "call i32 @factorial")
set_tests_properties(source_native_recursive_call_artifacts PROPERTIES
  DEPENDS source_native_recursive_call_build TIMEOUT 5)
add_test(NAME source_native_unit_calls_build COMMAND vxs build -File
  ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainUnitCalls.vxs)
set_tests_properties(source_native_unit_calls_build PROPERTIES TIMEOUT 5
  PASS_REGULAR_EXPRESSION "wrote optimized LLVM IR.*executable")
add_test(NAME source_native_unit_calls_artifacts COMMAND xs_xse_artifact_tests
  ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainUnitCalls.ll
  ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainUnitCalls.obj
  ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainUnitCalls.vxse 7
  "call void @touch" "call i32 @identity" "call void @countdown")
set_tests_properties(source_native_unit_calls_artifacts PROPERTIES
  DEPENDS source_native_unit_calls_build TIMEOUT 5)
add_test(NAME source_native_short_circuit_build COMMAND vxs build -File
  ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainShortCircuit.vxs)
set_tests_properties(source_native_short_circuit_build PROPERTIES TIMEOUT 5
  PASS_REGULAR_EXPRESSION "wrote optimized LLVM IR.*executable")
add_test(NAME source_native_short_circuit_artifacts COMMAND xs_xse_artifact_tests
  ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainShortCircuit.ll
  ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainShortCircuit.obj
  ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainShortCircuit.vxse 7 "define i8 @fail_if_called")
set_tests_properties(source_native_short_circuit_artifacts PROPERTIES
  DEPENDS source_native_short_circuit_build TIMEOUT 5)
add_test(NAME source_native_bool_call_build COMMAND vxs build -File ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainBoolCall.vxs)
set_tests_properties(source_native_bool_call_build PROPERTIES TIMEOUT 5
                    PASS_REGULAR_EXPRESSION "wrote optimized LLVM IR.*executable")
add_test(NAME source_native_bool_call_artifacts COMMAND xs_xse_artifact_tests
                                                 ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainBoolCall.ll
                                                 ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainBoolCall.obj
                                                 ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainBoolCall.vxse 7
                                                 "call i8 @IsPositive")
set_tests_properties(source_native_bool_call_artifacts PROPERTIES DEPENDS source_native_bool_call_build TIMEOUT 5)
add_test(NAME source_native_bool_call_local_build COMMAND vxs build -File
                                                    ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainBoolCallLocal.vxs)
set_tests_properties(source_native_bool_call_local_build PROPERTIES TIMEOUT 5
                    PASS_REGULAR_EXPRESSION "wrote optimized LLVM IR.*executable")
add_test(NAME source_native_bool_call_local_artifacts COMMAND xs_xse_artifact_tests
                                                       ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainBoolCallLocal.ll
                                                       ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainBoolCallLocal.obj
                                                       ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainBoolCallLocal.vxse 7
                                                       "call i8 @IsZero")
set_tests_properties(source_native_bool_call_local_artifacts PROPERTIES DEPENDS source_native_bool_call_local_build
                                                                        TIMEOUT 5)
add_test(NAME source_native_bool_parameter_call_build COMMAND vxs build -File
                                                        ${XS_SOURCE_NATIVE_FIXTURE_DIR}/BoolParameterCallMain.vxs)
set_tests_properties(source_native_bool_parameter_call_build PROPERTIES TIMEOUT 5
                    PASS_REGULAR_EXPRESSION "wrote optimized LLVM IR.*executable")
add_test(NAME source_native_bool_parameter_call_artifacts COMMAND xs_xse_artifact_tests
                                                            ${XS_SOURCE_NATIVE_FIXTURE_DIR}/BoolParameterCallMain.ll
                                                            ${XS_SOURCE_NATIVE_FIXTURE_DIR}/BoolParameterCallMain.obj
                                                            ${XS_SOURCE_NATIVE_FIXTURE_DIR}/BoolParameterCallMain.vxse 1
                                                            "call i32 @Choose")
set_tests_properties(source_native_bool_parameter_call_artifacts PROPERTIES
                     DEPENDS source_native_bool_parameter_call_build TIMEOUT 5)

add_test(NAME source_native_generic_functions_build COMMAND vxs build -File
  ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainGenericFunctions.vxs)
set_tests_properties(source_native_generic_functions_build PROPERTIES TIMEOUT 5
  PASS_REGULAR_EXPRESSION "wrote optimized LLVM IR.*executable")
add_test(NAME source_native_generic_functions_artifacts COMMAND xs_xse_artifact_tests
  ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainGenericFunctions.ll
  ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainGenericFunctions.obj
  ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainGenericFunctions.vxse 7
  "hidden$G1$Long" "forward$G2$Long" "identity$G0$Int" "first$G3$Long")
set_tests_properties(source_native_generic_functions_artifacts PROPERTIES
  DEPENDS source_native_generic_functions_build TIMEOUT 5)

add_test(NAME source_native_generic_recursive_build COMMAND vxs build -File
  ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainGenericRecursive.vxs)
set_tests_properties(source_native_generic_recursive_build PROPERTIES TIMEOUT 5
  PASS_REGULAR_EXPRESSION "wrote optimized LLVM IR.*executable")
add_test(NAME source_native_generic_recursive_artifacts COMMAND xs_xse_artifact_tests
  ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainGenericRecursive.ll
  ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainGenericRecursive.obj
  ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainGenericRecursive.vxse 7
  "countdown$G0$Long")
set_tests_properties(source_native_generic_recursive_artifacts PROPERTIES
  DEPENDS source_native_generic_recursive_build TIMEOUT 5)

add_test(NAME source_native_generic_constraint_build COMMAND vxs build -File
  ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainGenericConstraint.vxs)
set_tests_properties(source_native_generic_constraint_build PROPERTIES TIMEOUT 5
  PASS_REGULAR_EXPRESSION "wrote optimized LLVM IR.*executable")
add_test(NAME source_native_generic_constraint_artifacts COMMAND xs_xse_artifact_tests
  ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainGenericConstraint.ll
  ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainGenericConstraint.obj
  ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainGenericConstraint.vxse 7
  "constrained$G0$Worker")
set_tests_properties(source_native_generic_constraint_artifacts PROPERTIES
  DEPENDS source_native_generic_constraint_build TIMEOUT 5)
