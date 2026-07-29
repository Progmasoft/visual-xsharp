# SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
# SPDX-License-Identifier: MPL-2.0

set(XS_DIRECT_XLIL_FIXTURE_DIR "${CMAKE_CURRENT_BINARY_DIR}/tests/fixtures/intermediate")
file(MAKE_DIRECTORY "${XS_DIRECT_XLIL_FIXTURE_DIR}")
foreach(entry_fixture Supported BranchExit CallExit CompareExit CompareNotExit BitwiseExit StackSlotExit FloatConstants FloatOperators StringLiteral StringFlow StringCompare CharFlow IntegerWidths IntegerOperators AggregateNative FixedArrayNative MissingMain ExternMain
                      ParameterizedMain VoidMain I64Main DuplicateMain)
  configure_file(tests/fixtures/intermediate/${entry_fixture}.xlil
                 "${XS_DIRECT_XLIL_FIXTURE_DIR}/${entry_fixture}.xlil" COPYONLY)
endforeach()

add_test(NAME direct_xlil_supported_version COMMAND xs build --xlil -file ${XS_DIRECT_XLIL_FIXTURE_DIR}/Supported.xlil)
set_tests_properties(direct_xlil_supported_version PROPERTIES TIMEOUT 5
                     PASS_REGULAR_EXPRESSION "wrote optimized LLVM IR.*executable")
add_executable(xs_xse_artifact_tests tests/xse_artifact_tests.c)
add_test(NAME direct_xlil_xse_artifacts COMMAND xs_xse_artifact_tests ${XS_DIRECT_XLIL_FIXTURE_DIR}/Supported.ll
                                             ${XS_DIRECT_XLIL_FIXTURE_DIR}/Supported.o
                                             ${XS_DIRECT_XLIL_FIXTURE_DIR}/Supported.xse 0)
set_tests_properties(direct_xlil_xse_artifacts PROPERTIES DEPENDS direct_xlil_supported_version TIMEOUT 5)
add_test(NAME direct_xlil_native_exit_code COMMAND ${XS_DIRECT_XLIL_FIXTURE_DIR}/Supported.xse)
set_tests_properties(direct_xlil_native_exit_code PROPERTIES DEPENDS direct_xlil_supported_version TIMEOUT 5)
add_test(NAME direct_xlil_branch_exit_build COMMAND xs build --xlil -file ${XS_DIRECT_XLIL_FIXTURE_DIR}/BranchExit.xlil)
set_tests_properties(direct_xlil_branch_exit_build PROPERTIES TIMEOUT 5
                     PASS_REGULAR_EXPRESSION "wrote optimized LLVM IR.*executable")
add_test(NAME direct_xlil_branch_exit_artifacts COMMAND xs_xse_artifact_tests
                                                    ${XS_DIRECT_XLIL_FIXTURE_DIR}/BranchExit.ll
                                                    ${XS_DIRECT_XLIL_FIXTURE_DIR}/BranchExit.o
                                                    ${XS_DIRECT_XLIL_FIXTURE_DIR}/BranchExit.xse 7)
set_tests_properties(direct_xlil_branch_exit_artifacts PROPERTIES DEPENDS direct_xlil_branch_exit_build TIMEOUT 5)
add_test(NAME direct_xlil_call_exit_build COMMAND xs build --xlil -file ${XS_DIRECT_XLIL_FIXTURE_DIR}/CallExit.xlil)
set_tests_properties(direct_xlil_call_exit_build PROPERTIES TIMEOUT 5
                     PASS_REGULAR_EXPRESSION "wrote optimized LLVM IR.*executable")
add_test(NAME direct_xlil_call_exit_artifacts COMMAND xs_xse_artifact_tests
                                                  ${XS_DIRECT_XLIL_FIXTURE_DIR}/CallExit.ll
                                                  ${XS_DIRECT_XLIL_FIXTURE_DIR}/CallExit.o
                                                  ${XS_DIRECT_XLIL_FIXTURE_DIR}/CallExit.xse 7)
set_tests_properties(direct_xlil_call_exit_artifacts PROPERTIES DEPENDS direct_xlil_call_exit_build TIMEOUT 5)
add_test(NAME direct_xlil_compare_exit_build COMMAND xs build --xlil -file
                                               ${XS_DIRECT_XLIL_FIXTURE_DIR}/CompareExit.xlil)
set_tests_properties(direct_xlil_compare_exit_build PROPERTIES TIMEOUT 5
                     PASS_REGULAR_EXPRESSION "wrote optimized LLVM IR.*executable")
add_test(NAME direct_xlil_compare_exit_artifacts COMMAND xs_xse_artifact_tests
                                                     ${XS_DIRECT_XLIL_FIXTURE_DIR}/CompareExit.ll
                                                     ${XS_DIRECT_XLIL_FIXTURE_DIR}/CompareExit.o
                                                     ${XS_DIRECT_XLIL_FIXTURE_DIR}/CompareExit.xse 7)
set_tests_properties(direct_xlil_compare_exit_artifacts PROPERTIES DEPENDS direct_xlil_compare_exit_build TIMEOUT 5)
add_test(NAME direct_xlil_compare_not_exit_build COMMAND xs build --xlil -file
                                                   ${XS_DIRECT_XLIL_FIXTURE_DIR}/CompareNotExit.xlil)
set_tests_properties(direct_xlil_compare_not_exit_build PROPERTIES TIMEOUT 5
                     PASS_REGULAR_EXPRESSION "wrote optimized LLVM IR.*executable")
add_test(NAME direct_xlil_compare_not_exit_artifacts COMMAND xs_xse_artifact_tests
                                                         ${XS_DIRECT_XLIL_FIXTURE_DIR}/CompareNotExit.ll
                                                         ${XS_DIRECT_XLIL_FIXTURE_DIR}/CompareNotExit.o
                                                         ${XS_DIRECT_XLIL_FIXTURE_DIR}/CompareNotExit.xse 7)
set_tests_properties(direct_xlil_compare_not_exit_artifacts PROPERTIES DEPENDS direct_xlil_compare_not_exit_build
                                                                       TIMEOUT 5)
add_test(NAME direct_xlil_bitwise_exit_build COMMAND xs build --xlil -file
                                               ${XS_DIRECT_XLIL_FIXTURE_DIR}/BitwiseExit.xlil)
set_tests_properties(direct_xlil_bitwise_exit_build PROPERTIES TIMEOUT 5
                     PASS_REGULAR_EXPRESSION "wrote optimized LLVM IR.*executable")
add_test(NAME direct_xlil_bitwise_exit_artifacts COMMAND xs_xse_artifact_tests
                                                     ${XS_DIRECT_XLIL_FIXTURE_DIR}/BitwiseExit.ll
                                                     ${XS_DIRECT_XLIL_FIXTURE_DIR}/BitwiseExit.o
                                                     ${XS_DIRECT_XLIL_FIXTURE_DIR}/BitwiseExit.xse 6)
set_tests_properties(direct_xlil_bitwise_exit_artifacts PROPERTIES DEPENDS direct_xlil_bitwise_exit_build TIMEOUT 5)
add_test(NAME direct_xlil_stack_slot_exit_build COMMAND xs build --xlil -file
                                                ${XS_DIRECT_XLIL_FIXTURE_DIR}/StackSlotExit.xlil)
set_tests_properties(direct_xlil_stack_slot_exit_build PROPERTIES TIMEOUT 5
                     PASS_REGULAR_EXPRESSION "wrote optimized LLVM IR.*executable")
add_test(NAME direct_xlil_stack_slot_exit_artifacts COMMAND xs_xse_artifact_tests
                                                      ${XS_DIRECT_XLIL_FIXTURE_DIR}/StackSlotExit.ll
                                                      ${XS_DIRECT_XLIL_FIXTURE_DIR}/StackSlotExit.o
                                                      ${XS_DIRECT_XLIL_FIXTURE_DIR}/StackSlotExit.xse 7 "load i32")
set_tests_properties(direct_xlil_stack_slot_exit_artifacts PROPERTIES DEPENDS direct_xlil_stack_slot_exit_build
                                                                        TIMEOUT 5)
add_test(NAME direct_xlil_float_constants_build COMMAND xs build --xlil -file
                                                ${XS_DIRECT_XLIL_FIXTURE_DIR}/FloatConstants.xlil)
set_tests_properties(direct_xlil_float_constants_build PROPERTIES TIMEOUT 5
                     PASS_REGULAR_EXPRESSION "wrote optimized LLVM IR.*executable")
add_test(NAME direct_xlil_float_constants_artifacts COMMAND xs_xse_artifact_tests
                                                      ${XS_DIRECT_XLIL_FIXTURE_DIR}/FloatConstants.ll
                                                      ${XS_DIRECT_XLIL_FIXTURE_DIR}/FloatConstants.o
                                                      ${XS_DIRECT_XLIL_FIXTURE_DIR}/FloatConstants.xse 0
                                                      "define float @F32Value")
set_tests_properties(direct_xlil_float_constants_artifacts PROPERTIES DEPENDS direct_xlil_float_constants_build
                                                                        TIMEOUT 5)
add_test(NAME direct_xlil_float_operators_build COMMAND xs build --xlil -file
                                                ${XS_DIRECT_XLIL_FIXTURE_DIR}/FloatOperators.xlil)
set_tests_properties(direct_xlil_float_operators_build PROPERTIES TIMEOUT 5
                     PASS_REGULAR_EXPRESSION "wrote optimized LLVM IR.*executable")
add_test(NAME direct_xlil_float_operators_artifacts COMMAND xs_xse_artifact_tests
                                                      ${XS_DIRECT_XLIL_FIXTURE_DIR}/FloatOperators.ll
                                                      ${XS_DIRECT_XLIL_FIXTURE_DIR}/FloatOperators.o
                                                      ${XS_DIRECT_XLIL_FIXTURE_DIR}/FloatOperators.xse 0
                                                      "fadd float" "frem float" "fcmp olt double" "fcmp one double")
set_tests_properties(direct_xlil_float_operators_artifacts PROPERTIES DEPENDS direct_xlil_float_operators_build
                                                                        TIMEOUT 5)
add_test(NAME direct_xlil_string_literal_build COMMAND xs build --xlil -file
                                                ${XS_DIRECT_XLIL_FIXTURE_DIR}/StringLiteral.xlil)
set_tests_properties(direct_xlil_string_literal_build PROPERTIES TIMEOUT 5
                     PASS_REGULAR_EXPRESSION "wrote optimized LLVM IR.*executable")
add_test(NAME direct_xlil_string_literal_artifacts COMMAND xs_xse_artifact_tests
                                                      ${XS_DIRECT_XLIL_FIXTURE_DIR}/StringLiteral.ll
                                                      ${XS_DIRECT_XLIL_FIXTURE_DIR}/StringLiteral.o
                                                      ${XS_DIRECT_XLIL_FIXTURE_DIR}/StringLiteral.xse 0
                                                      "define { ptr, i64 } @greeting" "constant [12 x i8]")
set_tests_properties(direct_xlil_string_literal_artifacts PROPERTIES DEPENDS direct_xlil_string_literal_build
                                                                        TIMEOUT 5)
add_test(NAME direct_xlil_string_flow_build COMMAND xs build --xlil -file
                                             ${XS_DIRECT_XLIL_FIXTURE_DIR}/StringFlow.xlil)
set_tests_properties(direct_xlil_string_flow_build PROPERTIES TIMEOUT 5
                     PASS_REGULAR_EXPRESSION "wrote optimized LLVM IR.*executable")
add_test(NAME direct_xlil_string_flow_artifacts COMMAND xs_xse_artifact_tests
                                                   ${XS_DIRECT_XLIL_FIXTURE_DIR}/StringFlow.ll
                                                   ${XS_DIRECT_XLIL_FIXTURE_DIR}/StringFlow.o
                                                   ${XS_DIRECT_XLIL_FIXTURE_DIR}/StringFlow.xse 0
                                                   "define { ptr, i64 } @identity" "call { ptr, i64 } @identity")
set_tests_properties(direct_xlil_string_flow_artifacts PROPERTIES DEPENDS direct_xlil_string_flow_build TIMEOUT 5)
add_test(NAME direct_xlil_string_compare_build COMMAND xs build --xlil -file
                                                ${XS_DIRECT_XLIL_FIXTURE_DIR}/StringCompare.xlil)
set_tests_properties(direct_xlil_string_compare_build PROPERTIES TIMEOUT 5
                     PASS_REGULAR_EXPRESSION "wrote optimized LLVM IR.*executable")
add_test(NAME direct_xlil_string_compare_artifacts COMMAND xs_xse_artifact_tests
                                                    ${XS_DIRECT_XLIL_FIXTURE_DIR}/StringCompare.ll
                                                    ${XS_DIRECT_XLIL_FIXTURE_DIR}/StringCompare.o
                                                    ${XS_DIRECT_XLIL_FIXTURE_DIR}/StringCompare.xse 0
                                                    "call i32 @memcmp")
set_tests_properties(direct_xlil_string_compare_artifacts PROPERTIES DEPENDS direct_xlil_string_compare_build TIMEOUT 5)
add_test(NAME direct_xlil_char_flow_build COMMAND xs build --xlil -file
                                           ${XS_DIRECT_XLIL_FIXTURE_DIR}/CharFlow.xlil)
set_tests_properties(direct_xlil_char_flow_build PROPERTIES TIMEOUT 5
                     PASS_REGULAR_EXPRESSION "wrote optimized LLVM IR.*executable")
add_test(NAME direct_xlil_char_flow_artifacts COMMAND xs_xse_artifact_tests
                                                 ${XS_DIRECT_XLIL_FIXTURE_DIR}/CharFlow.ll
                                                 ${XS_DIRECT_XLIL_FIXTURE_DIR}/CharFlow.o
                                                 ${XS_DIRECT_XLIL_FIXTURE_DIR}/CharFlow.xse 0
                                                 "define i16 @identity" "store i16 937" "call i16 @identity")
set_tests_properties(direct_xlil_char_flow_artifacts PROPERTIES DEPENDS direct_xlil_char_flow_build TIMEOUT 5)
add_test(NAME direct_xlil_integer_widths_build COMMAND xs build --xlil -file
                                                ${XS_DIRECT_XLIL_FIXTURE_DIR}/IntegerWidths.xlil)
set_tests_properties(direct_xlil_integer_widths_build PROPERTIES TIMEOUT 5
                     PASS_REGULAR_EXPRESSION "wrote optimized LLVM IR.*executable")
add_test(NAME direct_xlil_integer_widths_artifacts COMMAND xs_xse_artifact_tests
                                                    ${XS_DIRECT_XLIL_FIXTURE_DIR}/IntegerWidths.ll
                                                    ${XS_DIRECT_XLIL_FIXTURE_DIR}/IntegerWidths.o
                                                    ${XS_DIRECT_XLIL_FIXTURE_DIR}/IntegerWidths.xse 0
                                                    "define i8 @byte_max" "define i16 @short_min"
                                                    "define i128 @integer_min" "ret i128 -1")
set_tests_properties(direct_xlil_integer_widths_artifacts PROPERTIES DEPENDS direct_xlil_integer_widths_build TIMEOUT 5)
add_test(NAME direct_xlil_integer_operators_build COMMAND xs build --xlil -file
                                                 ${XS_DIRECT_XLIL_FIXTURE_DIR}/IntegerOperators.xlil)
set_tests_properties(direct_xlil_integer_operators_build PROPERTIES TIMEOUT 5
                     PASS_REGULAR_EXPRESSION "wrote optimized LLVM IR.*executable")
add_test(NAME direct_xlil_integer_operators_artifacts COMMAND xs_xse_artifact_tests
                                                       ${XS_DIRECT_XLIL_FIXTURE_DIR}/IntegerOperators.ll
                                                       ${XS_DIRECT_XLIL_FIXTURE_DIR}/IntegerOperators.o
                                                       ${XS_DIRECT_XLIL_FIXTURE_DIR}/IntegerOperators.xse 0
                                                       "add i8" "sdiv i8" "udiv i64" "icmp ult i128"
                                                       "lshr i128" "icmp slt i128" "ashr i128")
set_tests_properties(direct_xlil_integer_operators_artifacts PROPERTIES
                     DEPENDS direct_xlil_integer_operators_build TIMEOUT 5)
add_test(NAME direct_xlil_aggregate_native_build COMMAND xs build --xlil -file
                                                ${XS_DIRECT_XLIL_FIXTURE_DIR}/AggregateNative.xlil)
set_tests_properties(direct_xlil_aggregate_native_build PROPERTIES TIMEOUT 5
                     PASS_REGULAR_EXPRESSION "wrote optimized LLVM IR.*executable")
add_test(NAME direct_xlil_aggregate_native_artifacts COMMAND xs_xse_artifact_tests
                                                      ${XS_DIRECT_XLIL_FIXTURE_DIR}/AggregateNative.ll
                                                      ${XS_DIRECT_XLIL_FIXTURE_DIR}/AggregateNative.o
                                                      ${XS_DIRECT_XLIL_FIXTURE_DIR}/AggregateNative.xse 7
                                                      "%Point = type { i32, i32 }" "insertvalue" "extractvalue")
set_tests_properties(direct_xlil_aggregate_native_artifacts PROPERTIES
                     DEPENDS direct_xlil_aggregate_native_build TIMEOUT 5)
add_test(NAME direct_xlil_fixed_array_native_build COMMAND xs build --xlil -file
                                                  ${XS_DIRECT_XLIL_FIXTURE_DIR}/FixedArrayNative.xlil)
set_tests_properties(direct_xlil_fixed_array_native_build PROPERTIES TIMEOUT 5
                     PASS_REGULAR_EXPRESSION "wrote optimized LLVM IR.*executable")
add_test(NAME direct_xlil_fixed_array_native_artifacts COMMAND xs_xse_artifact_tests
                                                        ${XS_DIRECT_XLIL_FIXTURE_DIR}/FixedArrayNative.ll
                                                        ${XS_DIRECT_XLIL_FIXTURE_DIR}/FixedArrayNative.o
                                                        ${XS_DIRECT_XLIL_FIXTURE_DIR}/FixedArrayNative.xse 7
                                                        "[3 x i32]" "insertvalue" "extractvalue")
set_tests_properties(direct_xlil_fixed_array_native_artifacts PROPERTIES
                     DEPENDS direct_xlil_fixed_array_native_build TIMEOUT 5)
foreach(entry_fixture MissingMain ExternMain ParameterizedMain VoidMain I64Main DuplicateMain)
  add_test(NAME direct_xlil_invalid_${entry_fixture} COMMAND xs build --xlil -file ${XS_DIRECT_XLIL_FIXTURE_DIR}/${entry_fixture}.xlil)
  set_tests_properties(direct_xlil_invalid_${entry_fixture} PROPERTIES TIMEOUT 5 WILL_FAIL TRUE)
endforeach()
