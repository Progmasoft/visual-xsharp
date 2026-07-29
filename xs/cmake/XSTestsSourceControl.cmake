# SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
# SPDX-License-Identifier: MPL-2.0

add_test(NAME source_native_negative_build COMMAND xs build -file ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainNegative.xs)
set_tests_properties(source_native_negative_build PROPERTIES TIMEOUT 5
                    PASS_REGULAR_EXPRESSION "wrote optimized LLVM IR.*executable")
add_test(NAME source_native_negative_artifacts COMMAND xs_xse_artifact_tests
                                                ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainNegative.ll
                                                ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainNegative.o
                                                ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainNegative.xse 7)
set_tests_properties(source_native_negative_artifacts PROPERTIES DEPENDS source_native_negative_build TIMEOUT 5)
add_test(NAME source_native_positive_build COMMAND xs build -file ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainPositive.xs)
set_tests_properties(source_native_positive_build PROPERTIES TIMEOUT 5
                    PASS_REGULAR_EXPRESSION "wrote optimized LLVM IR.*executable")
add_test(NAME source_native_positive_artifacts COMMAND xs_xse_artifact_tests
                                                ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainPositive.ll
                                                ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainPositive.o
                                                ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainPositive.xse 7)
set_tests_properties(source_native_positive_artifacts PROPERTIES DEPENDS source_native_positive_build TIMEOUT 5)
add_test(NAME source_native_bitwise_build COMMAND xs build -file ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainBitwise.xs)
set_tests_properties(source_native_bitwise_build PROPERTIES TIMEOUT 5
                    PASS_REGULAR_EXPRESSION "wrote optimized LLVM IR.*executable")
add_test(NAME source_native_bitwise_artifacts COMMAND xs_xse_artifact_tests
                                               ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainBitwise.ll
                                               ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainBitwise.o
                                               ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainBitwise.xse 6 "ret i32 6")
set_tests_properties(source_native_bitwise_artifacts PROPERTIES DEPENDS source_native_bitwise_build TIMEOUT 5)
add_test(NAME source_native_xor_build COMMAND xs build -file ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainXor.xs)
set_tests_properties(source_native_xor_build PROPERTIES TIMEOUT 5
                    PASS_REGULAR_EXPRESSION "wrote optimized LLVM IR.*executable")
add_test(NAME source_native_xor_artifacts COMMAND xs_xse_artifact_tests ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainXor.ll
                                           ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainXor.o
                                           ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainXor.xse 5 "ret i32 5")
set_tests_properties(source_native_xor_artifacts PROPERTIES DEPENDS source_native_xor_build TIMEOUT 5)
add_test(NAME source_native_local_build COMMAND xs build -file ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainLocal.xs)
set_tests_properties(source_native_local_build PROPERTIES TIMEOUT 5
                    PASS_REGULAR_EXPRESSION "wrote optimized LLVM IR.*executable")
add_test(NAME source_native_local_artifacts COMMAND xs_xse_artifact_tests
                                             ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainLocal.ll
                                             ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainLocal.o
                                             ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainLocal.xse 7 "load i32")
set_tests_properties(source_native_local_artifacts PROPERTIES DEPENDS source_native_local_build TIMEOUT 5)
add_test(NAME source_native_local_arithmetic_build COMMAND xs build -file
                                                     ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainLocalArithmetic.xs)
set_tests_properties(source_native_local_arithmetic_build PROPERTIES TIMEOUT 5
                    PASS_REGULAR_EXPRESSION "wrote optimized LLVM IR.*executable")
add_test(NAME source_native_local_arithmetic_artifacts COMMAND xs_xse_artifact_tests
                                                        ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainLocalArithmetic.ll
                                                        ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainLocalArithmetic.o
                                                        ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainLocalArithmetic.xse 7
                                                        "add i32")
set_tests_properties(source_native_local_arithmetic_artifacts PROPERTIES DEPENDS source_native_local_arithmetic_build
                                                                         TIMEOUT 5)
add_test(NAME source_native_local_if_build COMMAND xs build -file ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainLocalIf.xs)
set_tests_properties(source_native_local_if_build PROPERTIES TIMEOUT 5
                    PASS_REGULAR_EXPRESSION "wrote optimized LLVM IR.*executable")
add_test(NAME source_native_local_if_artifacts COMMAND xs_xse_artifact_tests
                                                ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainLocalIf.ll
                                                ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainLocalIf.o
                                                ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainLocalIf.xse 7)
set_tests_properties(source_native_local_if_artifacts PROPERTIES DEPENDS source_native_local_if_build TIMEOUT 5)
add_test(NAME source_native_inferred_local_build COMMAND xs build -file
                                                  ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainInferredLocal.xs)
set_tests_properties(source_native_inferred_local_build PROPERTIES TIMEOUT 5
                    PASS_REGULAR_EXPRESSION "wrote optimized LLVM IR.*executable")
add_test(NAME source_native_inferred_local_artifacts COMMAND xs_xse_artifact_tests
                                                     ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainInferredLocal.ll
                                                     ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainInferredLocal.o
                                                     ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainInferredLocal.xse 7
                                                     "load i32")
set_tests_properties(source_native_inferred_local_artifacts PROPERTIES DEPENDS source_native_inferred_local_build
                                                                       TIMEOUT 5)
add_test(NAME source_native_mutable_local_build COMMAND xs build -file
                                                  ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainMutableLocal.xs)
set_tests_properties(source_native_mutable_local_build PROPERTIES TIMEOUT 5
                    PASS_REGULAR_EXPRESSION "wrote optimized LLVM IR.*executable")
add_test(NAME source_native_mutable_local_artifacts COMMAND xs_xse_artifact_tests
                                                     ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainMutableLocal.ll
                                                     ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainMutableLocal.o
                                                     ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainMutableLocal.xse 7)
set_tests_properties(source_native_mutable_local_artifacts PROPERTIES DEPENDS source_native_mutable_local_build
                                                                       TIMEOUT 5)
add_test(NAME source_native_mutable_bool_local_build COMMAND xs build -file
                                                       ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainMutableBoolLocal.xs)
set_tests_properties(source_native_mutable_bool_local_build PROPERTIES TIMEOUT 5
                    PASS_REGULAR_EXPRESSION "wrote optimized LLVM IR.*executable")
add_test(NAME source_native_mutable_bool_local_artifacts COMMAND xs_xse_artifact_tests
                                                          ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainMutableBoolLocal.ll
                                                          ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainMutableBoolLocal.o
                                                          ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainMutableBoolLocal.xse 7)
set_tests_properties(source_native_mutable_bool_local_artifacts PROPERTIES
                     DEPENDS source_native_mutable_bool_local_build TIMEOUT 5)
add_test(NAME source_native_if_assignment_build COMMAND xs build -file
                                                 ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainIfAssignment.xs)
set_tests_properties(source_native_if_assignment_build PROPERTIES TIMEOUT 5
                    PASS_REGULAR_EXPRESSION "wrote optimized LLVM IR.*executable")
add_test(NAME source_native_if_assignment_artifacts COMMAND xs_xse_artifact_tests
                                                    ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainIfAssignment.ll
                                                    ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainIfAssignment.o
                                                    ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainIfAssignment.xse 7 "store i32 7")
set_tests_properties(source_native_if_assignment_artifacts PROPERTIES DEPENDS source_native_if_assignment_build TIMEOUT 5)
add_test(NAME source_native_compound_assignment_build COMMAND xs build -file
                                                         ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainCompoundAssignment.xs)
set_tests_properties(source_native_compound_assignment_build PROPERTIES TIMEOUT 5
                    PASS_REGULAR_EXPRESSION "wrote optimized LLVM IR.*executable")
add_test(NAME source_native_compound_assignment_artifacts COMMAND xs_xse_artifact_tests
                                                            ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainCompoundAssignment.ll
                                                            ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainCompoundAssignment.o
                                                            ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainCompoundAssignment.xse 7 "xor i32")
set_tests_properties(source_native_compound_assignment_artifacts PROPERTIES
                     DEPENDS source_native_compound_assignment_build TIMEOUT 5)
add_test(NAME source_native_if_multiple_assignments_build COMMAND xs build -file
                                                            ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainIfMultipleAssignments.xs)
set_tests_properties(source_native_if_multiple_assignments_build PROPERTIES TIMEOUT 5
                    PASS_REGULAR_EXPRESSION "wrote optimized LLVM IR.*executable")
add_test(NAME source_native_if_multiple_assignments_artifacts COMMAND xs_xse_artifact_tests
                                                               ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainIfMultipleAssignments.ll
                                                               ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainIfMultipleAssignments.o
                                                               ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainIfMultipleAssignments.xse 7
                                                               "br i1")
set_tests_properties(source_native_if_multiple_assignments_artifacts PROPERTIES
                     DEPENDS source_native_if_multiple_assignments_build TIMEOUT 5)
add_test(NAME source_native_nested_if_assignment_build COMMAND xs build -file
                                                        ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainNestedIfAssignment.xs)
set_tests_properties(source_native_nested_if_assignment_build PROPERTIES TIMEOUT 5
                    PASS_REGULAR_EXPRESSION "wrote optimized LLVM IR.*executable")
add_test(NAME source_native_nested_if_assignment_artifacts COMMAND xs_xse_artifact_tests
                                                           ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainNestedIfAssignment.ll
                                                           ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainNestedIfAssignment.o
                                                           ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainNestedIfAssignment.xse 7 "br i1")
set_tests_properties(source_native_nested_if_assignment_artifacts PROPERTIES
                     DEPENDS source_native_nested_if_assignment_build TIMEOUT 5)
add_test(NAME source_native_while_build COMMAND xs build -file ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainWhile.xs)
set_tests_properties(source_native_while_build PROPERTIES TIMEOUT 5
                    PASS_REGULAR_EXPRESSION "wrote optimized LLVM IR.*executable")
add_test(NAME source_native_while_artifacts COMMAND xs_xse_artifact_tests ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainWhile.ll
                                                ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainWhile.o
                                                ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainWhile.xse 7 "br i1")
set_tests_properties(source_native_while_artifacts PROPERTIES DEPENDS source_native_while_build TIMEOUT 5)
add_test(NAME source_native_while_control_build COMMAND xs build -file ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainWhileControl.xs)
set_tests_properties(source_native_while_control_build PROPERTIES TIMEOUT 5
                    PASS_REGULAR_EXPRESSION "wrote optimized LLVM IR.*executable")
add_test(NAME source_native_while_control_artifacts COMMAND xs_xse_artifact_tests
                                                ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainWhileControl.ll
                                                ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainWhileControl.o
                                                ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainWhileControl.xse 7 "br label")
set_tests_properties(source_native_while_control_artifacts PROPERTIES DEPENDS source_native_while_control_build TIMEOUT 5)
add_test(NAME source_native_do_while_build COMMAND xs build -file ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainDoWhile.xs)
set_tests_properties(source_native_do_while_build PROPERTIES TIMEOUT 5
                    PASS_REGULAR_EXPRESSION "wrote optimized LLVM IR.*executable")
add_test(NAME source_native_do_while_artifacts COMMAND xs_xse_artifact_tests
                                                ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainDoWhile.ll
                                                ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainDoWhile.o
                                                ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainDoWhile.xse 7 "store i32 7")
set_tests_properties(source_native_do_while_artifacts PROPERTIES DEPENDS source_native_do_while_build TIMEOUT 5)
add_test(NAME source_native_loop_build COMMAND xs build -file ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainLoop.xs)
set_tests_properties(source_native_loop_build PROPERTIES TIMEOUT 5
                    PASS_REGULAR_EXPRESSION "wrote optimized LLVM IR.*executable")
add_test(NAME source_native_loop_artifacts COMMAND xs_xse_artifact_tests ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainLoop.ll
                                               ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainLoop.o
                                               ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainLoop.xse 7 "br label %bb1")
set_tests_properties(source_native_loop_artifacts PROPERTIES DEPENDS source_native_loop_build TIMEOUT 5)
add_test(NAME source_native_block_locals_build COMMAND xs build -file ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainBlockLocals.xs)
set_tests_properties(source_native_block_locals_build PROPERTIES TIMEOUT 5
                    PASS_REGULAR_EXPRESSION "wrote optimized LLVM IR.*executable")
add_test(NAME source_native_block_locals_artifacts COMMAND xs_xse_artifact_tests
                                                ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainBlockLocals.ll
                                                ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainBlockLocals.o
                                                ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainBlockLocals.xse 10 "alloca i32")
set_tests_properties(source_native_block_locals_artifacts PROPERTIES DEPENDS source_native_block_locals_build TIMEOUT 5)
add_test(NAME source_native_block_local_shadow_build COMMAND xs build -file
                                                     ${XS_SOURCE_NATIVE_FIXTURE_DIR}/BlockLocalShadow.xs)
set_tests_properties(source_native_block_local_shadow_build PROPERTIES TIMEOUT 5
                    PASS_REGULAR_EXPRESSION "wrote optimized LLVM IR.*executable")
add_test(NAME source_native_block_local_shadow_artifacts COMMAND xs_xse_artifact_tests
                                                               ${XS_SOURCE_NATIVE_FIXTURE_DIR}/BlockLocalShadow.ll
                                                               ${XS_SOURCE_NATIVE_FIXTURE_DIR}/BlockLocalShadow.o
                                                               ${XS_SOURCE_NATIVE_FIXTURE_DIR}/BlockLocalShadow.xse 0)
set_tests_properties(source_native_block_local_shadow_artifacts PROPERTIES
                     DEPENDS source_native_block_local_shadow_build TIMEOUT 5)
add_test(NAME source_native_same_scope_duplicate_local COMMAND xs build -file
                                                         ${XS_SOURCE_NATIVE_FIXTURE_DIR}/SameScopeDuplicateLocal.xs)
set_tests_properties(source_native_same_scope_duplicate_local PROPERTIES TIMEOUT 5 WILL_FAIL TRUE)
add_test(NAME source_native_early_return_build COMMAND xs build -file ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainEarlyReturn.xs)
set_tests_properties(source_native_early_return_build PROPERTIES TIMEOUT 5
                    PASS_REGULAR_EXPRESSION "wrote optimized LLVM IR.*executable")
add_test(NAME source_native_early_return_artifacts COMMAND xs_xse_artifact_tests
                                                ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainEarlyReturn.ll
                                                ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainEarlyReturn.o
                                                ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainEarlyReturn.xse 7 "ret i32 7")
set_tests_properties(source_native_early_return_artifacts PROPERTIES DEPENDS source_native_early_return_build TIMEOUT 5)
add_test(NAME source_native_else_if_build COMMAND xs build -file ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainElseIf.xs)
set_tests_properties(source_native_else_if_build PROPERTIES TIMEOUT 5
                    PASS_REGULAR_EXPRESSION "wrote optimized LLVM IR.*executable")
add_test(NAME source_native_else_if_artifacts COMMAND xs_xse_artifact_tests
                                            ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainElseIf.ll
                                            ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainElseIf.o
                                            ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainElseIf.xse 7 "br i1")
set_tests_properties(source_native_else_if_artifacts PROPERTIES DEPENDS source_native_else_if_build TIMEOUT 5)
add_test(NAME source_native_match_build COMMAND xs build -file ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainMatch.xs)
set_tests_properties(source_native_match_build PROPERTIES TIMEOUT 5
                    PASS_REGULAR_EXPRESSION "wrote optimized LLVM IR.*executable")
add_test(NAME source_native_match_artifacts COMMAND xs_xse_artifact_tests ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainMatch.ll
                                             ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainMatch.o
                                             ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainMatch.xse 7 "icmp eq i32")
set_tests_properties(source_native_match_artifacts PROPERTIES DEPENDS source_native_match_build TIMEOUT 5)
add_test(NAME source_native_match_bool_build COMMAND xs build -file ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainMatchBool.xs)
set_tests_properties(source_native_match_bool_build PROPERTIES TIMEOUT 5
                    PASS_REGULAR_EXPRESSION "wrote optimized LLVM IR.*executable")
add_test(NAME source_native_match_bool_artifacts COMMAND xs_xse_artifact_tests
                                                  ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainMatchBool.ll
                                                  ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainMatchBool.o
                                                  ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainMatchBool.xse 7 "br i1")
set_tests_properties(source_native_match_bool_artifacts PROPERTIES DEPENDS source_native_match_bool_build TIMEOUT 5)
add_test(NAME source_native_match_expression_build COMMAND xs build -file
                                                           ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainMatchExpression.xs)
set_tests_properties(source_native_match_expression_build PROPERTIES TIMEOUT 5
                     FIXTURES_SETUP source_native_match_expression)
add_test(NAME source_native_match_expression_artifacts COMMAND xs_xse_artifact_tests
  ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainMatchExpression.ll
  ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainMatchExpression.o
  ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainMatchExpression.xse 7 "icmp eq i32")
set_tests_properties(source_native_match_expression_artifacts PROPERTIES
                     DEPENDS source_native_match_expression_build TIMEOUT 5)
add_test(NAME source_native_for_build COMMAND xs build -file ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainFor.xs)
set_tests_properties(source_native_for_build PROPERTIES TIMEOUT 5
                    PASS_REGULAR_EXPRESSION "wrote optimized LLVM IR.*executable")
add_test(NAME source_native_for_artifacts COMMAND xs_xse_artifact_tests ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainFor.ll
                                           ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainFor.o
                                           ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainFor.xse 8 "icmp slt i32")
set_tests_properties(source_native_for_artifacts PROPERTIES DEPENDS source_native_for_build TIMEOUT 5)
add_test(NAME source_native_postfix_decrement_build COMMAND xs build -file
                                                        ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainPostfixDecrement.xs)
set_tests_properties(source_native_postfix_decrement_build PROPERTIES TIMEOUT 5
                    PASS_REGULAR_EXPRESSION "wrote optimized LLVM IR.*executable")
add_test(NAME source_native_postfix_decrement_artifacts COMMAND xs_xse_artifact_tests
                                                            ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainPostfixDecrement.ll
                                                            ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainPostfixDecrement.o
                                                            ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainPostfixDecrement.xse 7)
set_tests_properties(source_native_postfix_decrement_artifacts PROPERTIES
                     DEPENDS source_native_postfix_decrement_build TIMEOUT 5)
add_test(NAME source_native_update_values_build COMMAND xs build -file
                                                   ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainUpdateValues.xs)
set_tests_properties(source_native_update_values_build PROPERTIES TIMEOUT 5
                    PASS_REGULAR_EXPRESSION "wrote optimized LLVM IR.*executable")
add_test(NAME source_native_update_values_artifacts COMMAND xs_xse_artifact_tests
                                                      ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainUpdateValues.ll
                                                      ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainUpdateValues.o
                                                      ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainUpdateValues.xse 24
                                                      "store i32")
set_tests_properties(source_native_update_values_artifacts PROPERTIES DEPENDS source_native_update_values_build TIMEOUT 5)
add_test(NAME source_native_if_build COMMAND xs build -file ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainIf.xs)
set_tests_properties(source_native_if_build PROPERTIES TIMEOUT 5
                    PASS_REGULAR_EXPRESSION "wrote optimized LLVM IR.*executable")
add_test(NAME source_native_if_artifacts COMMAND xs_xse_artifact_tests ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainIf.ll
                                          ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainIf.o
                                          ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainIf.xse 7 "ret i32 7" "!br label")
set_tests_properties(source_native_if_artifacts PROPERTIES DEPENDS source_native_if_build TIMEOUT 5)
add_test(NAME source_native_if_value_build COMMAND xs build -file ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainIfValue.xs)
set_tests_properties(source_native_if_value_build PROPERTIES TIMEOUT 5 FIXTURES_SETUP source_native_if_value)
add_test(NAME source_native_if_value_artifacts COMMAND xs_xse_artifact_tests
  ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainIfValue.ll
  ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainIfValue.o
  ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainIfValue.xse 7 "call i32 @identity")
set_tests_properties(source_native_if_value_artifacts PROPERTIES DEPENDS source_native_if_value_build TIMEOUT 5)
add_test(NAME source_native_if_not_build COMMAND xs build -file ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainIfNot.xs)
set_tests_properties(source_native_if_not_build PROPERTIES TIMEOUT 5
                    PASS_REGULAR_EXPRESSION "wrote optimized LLVM IR.*executable")
add_test(NAME source_native_if_not_artifacts COMMAND xs_xse_artifact_tests ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainIfNot.ll
                                              ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainIfNot.o
                                              ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainIfNot.xse 7 "ret i32 7" "!br label")
set_tests_properties(source_native_if_not_artifacts PROPERTIES DEPENDS source_native_if_not_build TIMEOUT 5)
add_test(NAME source_native_if_false_build COMMAND xs build -file ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainIfFalse.xs)
set_tests_properties(source_native_if_false_build PROPERTIES TIMEOUT 5
                    PASS_REGULAR_EXPRESSION "wrote optimized LLVM IR.*executable")
add_test(NAME source_native_if_false_artifacts COMMAND xs_xse_artifact_tests
                                                ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainIfFalse.ll
                                                ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainIfFalse.o
                                                ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainIfFalse.xse 7 "ret i32 7")
set_tests_properties(source_native_if_false_artifacts PROPERTIES DEPENDS source_native_if_false_build TIMEOUT 5)
add_test(NAME source_native_if_not_equal_build COMMAND xs build -file
                                                    ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainIfNotEqual.xs)
set_tests_properties(source_native_if_not_equal_build PROPERTIES TIMEOUT 5
                    PASS_REGULAR_EXPRESSION "wrote optimized LLVM IR.*executable")
add_test(NAME source_native_if_not_equal_artifacts COMMAND xs_xse_artifact_tests
                                                    ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainIfNotEqual.ll
                                                    ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainIfNotEqual.o
                                                    ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainIfNotEqual.xse 7 "ret i32 7"
                                                    "!br label")
set_tests_properties(source_native_if_not_equal_artifacts PROPERTIES DEPENDS source_native_if_not_equal_build TIMEOUT 5)
add_test(NAME source_native_bool_local_build COMMAND xs build -file ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainBoolLocal.xs)
set_tests_properties(source_native_bool_local_build PROPERTIES TIMEOUT 5
                    PASS_REGULAR_EXPRESSION "wrote optimized LLVM IR.*executable")
add_test(NAME source_native_bool_local_artifacts COMMAND xs_xse_artifact_tests
                                                 ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainBoolLocal.ll
                                                 ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainBoolLocal.o
                                                 ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainBoolLocal.xse 7)
set_tests_properties(source_native_bool_local_artifacts PROPERTIES DEPENDS source_native_bool_local_build TIMEOUT 5)
add_test(NAME source_native_bool_not_local_build COMMAND xs build -file
                                                   ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainBoolNotLocal.xs)
set_tests_properties(source_native_bool_not_local_build PROPERTIES TIMEOUT 5
                    PASS_REGULAR_EXPRESSION "wrote optimized LLVM IR.*executable")
add_test(NAME source_native_bool_not_local_artifacts COMMAND xs_xse_artifact_tests
                                                     ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainBoolNotLocal.ll
                                                     ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainBoolNotLocal.o
                                                     ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainBoolNotLocal.xse 7)
set_tests_properties(source_native_bool_not_local_artifacts PROPERTIES DEPENDS source_native_bool_not_local_build
                                                                       TIMEOUT 5)
add_test(NAME source_native_inferred_bool_local_build COMMAND xs build -file
                                                       ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainInferredBoolLocal.xs)
set_tests_properties(source_native_inferred_bool_local_build PROPERTIES TIMEOUT 5
                    PASS_REGULAR_EXPRESSION "wrote optimized LLVM IR.*executable")
add_test(NAME source_native_inferred_bool_local_artifacts COMMAND xs_xse_artifact_tests
                                                          ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainInferredBoolLocal.ll
                                                          ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainInferredBoolLocal.o
                                                          ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainInferredBoolLocal.xse 7)
set_tests_properties(source_native_inferred_bool_local_artifacts PROPERTIES
                     DEPENDS source_native_inferred_bool_local_build TIMEOUT 5)
add_test(NAME source_native_inferred_bool_not_local_build COMMAND xs build -file
                                                            ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainInferredBoolNotLocal.xs)
set_tests_properties(source_native_inferred_bool_not_local_build PROPERTIES TIMEOUT 5
                    PASS_REGULAR_EXPRESSION "wrote optimized LLVM IR.*executable")
add_test(NAME source_native_inferred_bool_not_local_artifacts COMMAND xs_xse_artifact_tests
                                                              ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainInferredBoolNotLocal.ll
                                                              ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainInferredBoolNotLocal.o
                                                              ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainInferredBoolNotLocal.xse
                                                              7)
set_tests_properties(source_native_inferred_bool_not_local_artifacts PROPERTIES
                     DEPENDS source_native_inferred_bool_not_local_build TIMEOUT 5)
