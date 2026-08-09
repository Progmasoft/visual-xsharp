# SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
# SPDX-License-Identifier: MPL-2.0

add_test(NAME source_native_negative_build COMMAND vxs build -file ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainNegative.xs)
set_tests_properties(source_native_negative_build PROPERTIES TIMEOUT 5
                    PASS_REGULAR_EXPRESSION "wrote optimized LLVM IR.*executable")
add_test(NAME source_native_negative_artifacts COMMAND xs_xse_artifact_tests
                                                ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainNegative.ll
                                                ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainNegative.obj
                                                ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainNegative.vxse 7)
set_tests_properties(source_native_negative_artifacts PROPERTIES DEPENDS source_native_negative_build TIMEOUT 5)
add_test(NAME source_native_positive_build COMMAND vxs build -file ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainPositive.xs)
set_tests_properties(source_native_positive_build PROPERTIES TIMEOUT 5
                    PASS_REGULAR_EXPRESSION "wrote optimized LLVM IR.*executable")
add_test(NAME source_native_positive_artifacts COMMAND xs_xse_artifact_tests
                                                ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainPositive.ll
                                                ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainPositive.obj
                                                ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainPositive.vxse 7)
set_tests_properties(source_native_positive_artifacts PROPERTIES DEPENDS source_native_positive_build TIMEOUT 5)
add_test(NAME source_native_bitwise_build COMMAND vxs build -file ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainBitwise.xs)
set_tests_properties(source_native_bitwise_build PROPERTIES TIMEOUT 5
                    PASS_REGULAR_EXPRESSION "wrote optimized LLVM IR.*executable")
add_test(NAME source_native_bitwise_artifacts COMMAND xs_xse_artifact_tests
                                               ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainBitwise.ll
                                               ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainBitwise.obj
                                               ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainBitwise.vxse 6 "ret i32 6")
set_tests_properties(source_native_bitwise_artifacts PROPERTIES DEPENDS source_native_bitwise_build TIMEOUT 5)
add_test(NAME source_native_xor_build COMMAND vxs build -file ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainXor.xs)
set_tests_properties(source_native_xor_build PROPERTIES TIMEOUT 5
                    PASS_REGULAR_EXPRESSION "wrote optimized LLVM IR.*executable")
add_test(NAME source_native_xor_artifacts COMMAND xs_xse_artifact_tests ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainXor.ll
                                           ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainXor.obj
                                           ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainXor.vxse 5 "ret i32 5")
set_tests_properties(source_native_xor_artifacts PROPERTIES DEPENDS source_native_xor_build TIMEOUT 5)
add_test(NAME source_native_local_build COMMAND vxs build -file ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainLocal.xs)
set_tests_properties(source_native_local_build PROPERTIES TIMEOUT 5
                    PASS_REGULAR_EXPRESSION "wrote optimized LLVM IR.*executable")
add_test(NAME source_native_local_artifacts COMMAND xs_xse_artifact_tests
                                             ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainLocal.ll
                                             ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainLocal.obj
                                             ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainLocal.vxse 7 "load i32")
set_tests_properties(source_native_local_artifacts PROPERTIES DEPENDS source_native_local_build TIMEOUT 5)
add_test(NAME source_native_local_arithmetic_build COMMAND vxs build -file
                                                     ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainLocalArithmetic.xs)
set_tests_properties(source_native_local_arithmetic_build PROPERTIES TIMEOUT 5
                    PASS_REGULAR_EXPRESSION "wrote optimized LLVM IR.*executable")
add_test(NAME source_native_local_arithmetic_artifacts COMMAND xs_xse_artifact_tests
                                                        ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainLocalArithmetic.ll
                                                        ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainLocalArithmetic.obj
                                                        ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainLocalArithmetic.vxse 7
                                                        "add i32")
set_tests_properties(source_native_local_arithmetic_artifacts PROPERTIES DEPENDS source_native_local_arithmetic_build
                                                                         TIMEOUT 5)
add_test(NAME source_native_local_if_build COMMAND vxs build -file ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainLocalIf.xs)
set_tests_properties(source_native_local_if_build PROPERTIES TIMEOUT 5
                    PASS_REGULAR_EXPRESSION "wrote optimized LLVM IR.*executable")
add_test(NAME source_native_local_if_artifacts COMMAND xs_xse_artifact_tests
                                                ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainLocalIf.ll
                                                ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainLocalIf.obj
                                                ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainLocalIf.vxse 7)
set_tests_properties(source_native_local_if_artifacts PROPERTIES DEPENDS source_native_local_if_build TIMEOUT 5)
add_test(NAME source_native_inferred_local_build COMMAND vxs build -file
                                                  ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainInferredLocal.xs)
set_tests_properties(source_native_inferred_local_build PROPERTIES TIMEOUT 5
                    PASS_REGULAR_EXPRESSION "wrote optimized LLVM IR.*executable")
add_test(NAME source_native_inferred_local_artifacts COMMAND xs_xse_artifact_tests
                                                     ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainInferredLocal.ll
                                                     ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainInferredLocal.obj
                                                     ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainInferredLocal.vxse 7
                                                     "load i32")
set_tests_properties(source_native_inferred_local_artifacts PROPERTIES DEPENDS source_native_inferred_local_build
                                                                       TIMEOUT 5)
add_test(NAME source_native_mutable_local_build COMMAND vxs build -file
                                                  ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainMutableLocal.xs)
set_tests_properties(source_native_mutable_local_build PROPERTIES TIMEOUT 5
                    PASS_REGULAR_EXPRESSION "wrote optimized LLVM IR.*executable")
add_test(NAME source_native_mutable_local_artifacts COMMAND xs_xse_artifact_tests
                                                     ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainMutableLocal.ll
                                                     ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainMutableLocal.obj
                                                     ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainMutableLocal.vxse 7)
set_tests_properties(source_native_mutable_local_artifacts PROPERTIES DEPENDS source_native_mutable_local_build
                                                                       TIMEOUT 5)
add_test(NAME source_native_mutable_bool_local_build COMMAND vxs build -file
                                                       ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainMutableBoolLocal.xs)
set_tests_properties(source_native_mutable_bool_local_build PROPERTIES TIMEOUT 5
                    PASS_REGULAR_EXPRESSION "wrote optimized LLVM IR.*executable")
add_test(NAME source_native_mutable_bool_local_artifacts COMMAND xs_xse_artifact_tests
                                                          ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainMutableBoolLocal.ll
                                                          ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainMutableBoolLocal.obj
                                                          ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainMutableBoolLocal.vxse 7)
set_tests_properties(source_native_mutable_bool_local_artifacts PROPERTIES
                     DEPENDS source_native_mutable_bool_local_build TIMEOUT 5)
add_test(NAME source_native_if_assignment_build COMMAND vxs build -file
                                                 ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainIfAssignment.xs)
set_tests_properties(source_native_if_assignment_build PROPERTIES TIMEOUT 5
                    PASS_REGULAR_EXPRESSION "wrote optimized LLVM IR.*executable")
add_test(NAME source_native_if_assignment_artifacts COMMAND xs_xse_artifact_tests
                                                    ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainIfAssignment.ll
                                                    ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainIfAssignment.obj
                                                    ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainIfAssignment.vxse 7 "store i32 7")
set_tests_properties(source_native_if_assignment_artifacts PROPERTIES DEPENDS source_native_if_assignment_build TIMEOUT 5)
add_test(NAME source_native_compound_assignment_build COMMAND vxs build -file
                                                         ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainCompoundAssignment.xs)
set_tests_properties(source_native_compound_assignment_build PROPERTIES TIMEOUT 5
                    PASS_REGULAR_EXPRESSION "wrote optimized LLVM IR.*executable")
add_test(NAME source_native_compound_assignment_artifacts COMMAND xs_xse_artifact_tests
                                                            ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainCompoundAssignment.ll
                                                            ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainCompoundAssignment.obj
                                                            ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainCompoundAssignment.vxse 7 "xor i32")
set_tests_properties(source_native_compound_assignment_artifacts PROPERTIES
                     DEPENDS source_native_compound_assignment_build TIMEOUT 5)
add_test(NAME source_native_if_multiple_assignments_build COMMAND vxs build -file
                                                            ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainIfMultipleAssignments.xs)
set_tests_properties(source_native_if_multiple_assignments_build PROPERTIES TIMEOUT 5
                    PASS_REGULAR_EXPRESSION "wrote optimized LLVM IR.*executable")
add_test(NAME source_native_if_multiple_assignments_artifacts COMMAND xs_xse_artifact_tests
                                                               ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainIfMultipleAssignments.ll
                                                               ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainIfMultipleAssignments.obj
                                                               ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainIfMultipleAssignments.vxse 7
                                                               "br i1")
set_tests_properties(source_native_if_multiple_assignments_artifacts PROPERTIES
                     DEPENDS source_native_if_multiple_assignments_build TIMEOUT 5)
add_test(NAME source_native_nested_if_assignment_build COMMAND vxs build -file
                                                        ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainNestedIfAssignment.xs)
set_tests_properties(source_native_nested_if_assignment_build PROPERTIES TIMEOUT 5
                    PASS_REGULAR_EXPRESSION "wrote optimized LLVM IR.*executable")
add_test(NAME source_native_nested_if_assignment_artifacts COMMAND xs_xse_artifact_tests
                                                           ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainNestedIfAssignment.ll
                                                           ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainNestedIfAssignment.obj
                                                           ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainNestedIfAssignment.vxse 7 "br i1")
set_tests_properties(source_native_nested_if_assignment_artifacts PROPERTIES
                     DEPENDS source_native_nested_if_assignment_build TIMEOUT 5)
add_test(NAME source_native_while_build COMMAND vxs build -file ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainWhile.xs)
set_tests_properties(source_native_while_build PROPERTIES TIMEOUT 5
                    PASS_REGULAR_EXPRESSION "wrote optimized LLVM IR.*executable")
add_test(NAME source_native_while_artifacts COMMAND xs_xse_artifact_tests ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainWhile.ll
                                                ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainWhile.obj
                                                ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainWhile.vxse 7 "br i1")
set_tests_properties(source_native_while_artifacts PROPERTIES DEPENDS source_native_while_build TIMEOUT 5)
add_test(NAME source_native_while_control_build COMMAND vxs build -file ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainWhileControl.xs)
set_tests_properties(source_native_while_control_build PROPERTIES TIMEOUT 5
                    PASS_REGULAR_EXPRESSION "wrote optimized LLVM IR.*executable")
add_test(NAME source_native_while_control_artifacts COMMAND xs_xse_artifact_tests
                                                ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainWhileControl.ll
                                                ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainWhileControl.obj
                                                ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainWhileControl.vxse 7 "br label")
set_tests_properties(source_native_while_control_artifacts PROPERTIES DEPENDS source_native_while_control_build TIMEOUT 5)
add_test(NAME source_native_do_while_build COMMAND vxs build -file ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainDoWhile.xs)
set_tests_properties(source_native_do_while_build PROPERTIES TIMEOUT 5
                    PASS_REGULAR_EXPRESSION "wrote optimized LLVM IR.*executable")
add_test(NAME source_native_do_while_artifacts COMMAND xs_xse_artifact_tests
                                                ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainDoWhile.ll
                                                ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainDoWhile.obj
                                                ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainDoWhile.vxse 7 "store i32 7")
set_tests_properties(source_native_do_while_artifacts PROPERTIES DEPENDS source_native_do_while_build TIMEOUT 5)
add_test(NAME source_native_loop_build COMMAND vxs build -file ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainLoop.xs)
set_tests_properties(source_native_loop_build PROPERTIES TIMEOUT 5
                    PASS_REGULAR_EXPRESSION "wrote optimized LLVM IR.*executable")
add_test(NAME source_native_loop_artifacts COMMAND xs_xse_artifact_tests ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainLoop.ll
                                               ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainLoop.obj
                                               ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainLoop.vxse 7 "br label %bb1")
set_tests_properties(source_native_loop_artifacts PROPERTIES DEPENDS source_native_loop_build TIMEOUT 5)
add_test(NAME source_native_block_locals_build COMMAND vxs build -file ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainBlockLocals.xs)
set_tests_properties(source_native_block_locals_build PROPERTIES TIMEOUT 5
                    PASS_REGULAR_EXPRESSION "wrote optimized LLVM IR.*executable")
add_test(NAME source_native_block_locals_artifacts COMMAND xs_xse_artifact_tests
                                                ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainBlockLocals.ll
                                                ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainBlockLocals.obj
                                                ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainBlockLocals.vxse 10 "alloca i32")
set_tests_properties(source_native_block_locals_artifacts PROPERTIES DEPENDS source_native_block_locals_build TIMEOUT 5)
add_test(NAME source_native_block_local_shadow_build COMMAND vxs build -file
                                                     ${XS_SOURCE_NATIVE_FIXTURE_DIR}/BlockLocalShadow.xs)
set_tests_properties(source_native_block_local_shadow_build PROPERTIES TIMEOUT 5
                    PASS_REGULAR_EXPRESSION "wrote optimized LLVM IR.*executable")
add_test(NAME source_native_block_local_shadow_artifacts COMMAND xs_xse_artifact_tests
                                                               ${XS_SOURCE_NATIVE_FIXTURE_DIR}/BlockLocalShadow.ll
                                                               ${XS_SOURCE_NATIVE_FIXTURE_DIR}/BlockLocalShadow.obj
                                                               ${XS_SOURCE_NATIVE_FIXTURE_DIR}/BlockLocalShadow.vxse 0)
set_tests_properties(source_native_block_local_shadow_artifacts PROPERTIES
                     DEPENDS source_native_block_local_shadow_build TIMEOUT 5)
add_test(NAME source_native_same_scope_duplicate_local COMMAND vxs build -file
                                                         ${XS_SOURCE_NATIVE_FIXTURE_DIR}/SameScopeDuplicateLocal.xs)
set_tests_properties(source_native_same_scope_duplicate_local PROPERTIES TIMEOUT 5 WILL_FAIL TRUE)
add_test(NAME source_native_early_return_build COMMAND vxs build -file ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainEarlyReturn.xs)
set_tests_properties(source_native_early_return_build PROPERTIES TIMEOUT 5
                    PASS_REGULAR_EXPRESSION "wrote optimized LLVM IR.*executable")
add_test(NAME source_native_early_return_artifacts COMMAND xs_xse_artifact_tests
                                                ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainEarlyReturn.ll
                                                ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainEarlyReturn.obj
                                                ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainEarlyReturn.vxse 7 "ret i32 7")
set_tests_properties(source_native_early_return_artifacts PROPERTIES DEPENDS source_native_early_return_build TIMEOUT 5)
add_test(NAME source_native_else_if_build COMMAND vxs build -file ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainElseIf.xs)
set_tests_properties(source_native_else_if_build PROPERTIES TIMEOUT 5
                    PASS_REGULAR_EXPRESSION "wrote optimized LLVM IR.*executable")
add_test(NAME source_native_else_if_artifacts COMMAND xs_xse_artifact_tests
                                            ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainElseIf.ll
                                            ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainElseIf.obj
                                            ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainElseIf.vxse 7 "br i1")
set_tests_properties(source_native_else_if_artifacts PROPERTIES DEPENDS source_native_else_if_build TIMEOUT 5)
add_test(NAME source_native_match_build COMMAND vxs build -file ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainMatch.xs)
set_tests_properties(source_native_match_build PROPERTIES TIMEOUT 5
                    PASS_REGULAR_EXPRESSION "wrote optimized LLVM IR.*executable")
add_test(NAME source_native_match_artifacts COMMAND xs_xse_artifact_tests ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainMatch.ll
                                             ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainMatch.obj
                                             ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainMatch.vxse 7 "icmp eq i32")
set_tests_properties(source_native_match_artifacts PROPERTIES DEPENDS source_native_match_build TIMEOUT 5)
add_test(NAME source_native_match_bool_build COMMAND vxs build -file ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainMatchBool.xs)
set_tests_properties(source_native_match_bool_build PROPERTIES TIMEOUT 5
                    PASS_REGULAR_EXPRESSION "wrote optimized LLVM IR.*executable")
add_test(NAME source_native_match_bool_artifacts COMMAND xs_xse_artifact_tests
                                                  ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainMatchBool.ll
                                                  ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainMatchBool.obj
                                                  ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainMatchBool.vxse 7 "br i1")
set_tests_properties(source_native_match_bool_artifacts PROPERTIES DEPENDS source_native_match_bool_build TIMEOUT 5)
add_test(NAME source_native_match_expression_build COMMAND vxs build -file
                                                           ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainMatchExpression.xs)
set_tests_properties(source_native_match_expression_build PROPERTIES TIMEOUT 5
                     FIXTURES_SETUP source_native_match_expression)
add_test(NAME source_native_match_expression_artifacts COMMAND xs_xse_artifact_tests
  ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainMatchExpression.ll
  ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainMatchExpression.obj
  ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainMatchExpression.vxse 7 "icmp eq i32")
set_tests_properties(source_native_match_expression_artifacts PROPERTIES
                     DEPENDS source_native_match_expression_build TIMEOUT 5)
add_test(NAME source_native_for_build COMMAND vxs build -file ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainFor.xs)
set_tests_properties(source_native_for_build PROPERTIES TIMEOUT 5
                    PASS_REGULAR_EXPRESSION "wrote optimized LLVM IR.*executable")
add_test(NAME source_native_for_artifacts COMMAND xs_xse_artifact_tests ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainFor.ll
                                           ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainFor.obj
                                           ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainFor.vxse 8 "icmp slt i32")
set_tests_properties(source_native_for_artifacts PROPERTIES DEPENDS source_native_for_build TIMEOUT 5)
add_test(NAME source_native_postfix_decrement_build COMMAND vxs build -file
                                                        ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainPostfixDecrement.xs)
set_tests_properties(source_native_postfix_decrement_build PROPERTIES TIMEOUT 5
                    PASS_REGULAR_EXPRESSION "wrote optimized LLVM IR.*executable")
add_test(NAME source_native_postfix_decrement_artifacts COMMAND xs_xse_artifact_tests
                                                            ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainPostfixDecrement.ll
                                                            ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainPostfixDecrement.obj
                                                            ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainPostfixDecrement.vxse 7)
set_tests_properties(source_native_postfix_decrement_artifacts PROPERTIES
                     DEPENDS source_native_postfix_decrement_build TIMEOUT 5)
add_test(NAME source_native_update_values_build COMMAND vxs build -file
                                                   ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainUpdateValues.xs)
set_tests_properties(source_native_update_values_build PROPERTIES TIMEOUT 5
                    PASS_REGULAR_EXPRESSION "wrote optimized LLVM IR.*executable")
add_test(NAME source_native_update_values_artifacts COMMAND xs_xse_artifact_tests
                                                      ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainUpdateValues.ll
                                                      ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainUpdateValues.obj
                                                      ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainUpdateValues.vxse 24
                                                      "store i32")
set_tests_properties(source_native_update_values_artifacts PROPERTIES DEPENDS source_native_update_values_build TIMEOUT 5)
add_test(NAME source_native_if_build COMMAND vxs build -file ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainIf.xs)
set_tests_properties(source_native_if_build PROPERTIES TIMEOUT 5
                    PASS_REGULAR_EXPRESSION "wrote optimized LLVM IR.*executable")
add_test(NAME source_native_if_artifacts COMMAND xs_xse_artifact_tests ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainIf.ll
                                          ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainIf.obj
                                          ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainIf.vxse 7 "ret i32 7" "!br label")
set_tests_properties(source_native_if_artifacts PROPERTIES DEPENDS source_native_if_build TIMEOUT 5)
add_test(NAME source_native_if_value_build COMMAND vxs build -file ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainIfValue.xs)
set_tests_properties(source_native_if_value_build PROPERTIES TIMEOUT 5 FIXTURES_SETUP source_native_if_value)
add_test(NAME source_native_if_value_artifacts COMMAND xs_xse_artifact_tests
  ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainIfValue.ll
  ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainIfValue.obj
  ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainIfValue.vxse 7 "call i32 @identity")
set_tests_properties(source_native_if_value_artifacts PROPERTIES DEPENDS source_native_if_value_build TIMEOUT 5)
add_test(NAME source_native_if_not_build COMMAND vxs build -file ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainIfNot.xs)
set_tests_properties(source_native_if_not_build PROPERTIES TIMEOUT 5
                    PASS_REGULAR_EXPRESSION "wrote optimized LLVM IR.*executable")
add_test(NAME source_native_if_not_artifacts COMMAND xs_xse_artifact_tests ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainIfNot.ll
                                              ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainIfNot.obj
                                              ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainIfNot.vxse 7 "ret i32 7" "!br label")
set_tests_properties(source_native_if_not_artifacts PROPERTIES DEPENDS source_native_if_not_build TIMEOUT 5)
add_test(NAME source_native_if_false_build COMMAND vxs build -file ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainIfFalse.xs)
set_tests_properties(source_native_if_false_build PROPERTIES TIMEOUT 5
                    PASS_REGULAR_EXPRESSION "wrote optimized LLVM IR.*executable")
add_test(NAME source_native_if_false_artifacts COMMAND xs_xse_artifact_tests
                                                ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainIfFalse.ll
                                                ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainIfFalse.obj
                                                ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainIfFalse.vxse 7 "ret i32 7")
set_tests_properties(source_native_if_false_artifacts PROPERTIES DEPENDS source_native_if_false_build TIMEOUT 5)
add_test(NAME source_native_if_not_equal_build COMMAND vxs build -file
                                                    ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainIfNotEqual.xs)
set_tests_properties(source_native_if_not_equal_build PROPERTIES TIMEOUT 5
                    PASS_REGULAR_EXPRESSION "wrote optimized LLVM IR.*executable")
add_test(NAME source_native_if_not_equal_artifacts COMMAND xs_xse_artifact_tests
                                                    ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainIfNotEqual.ll
                                                    ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainIfNotEqual.obj
                                                    ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainIfNotEqual.vxse 7 "ret i32 7"
                                                    "!br label")
set_tests_properties(source_native_if_not_equal_artifacts PROPERTIES DEPENDS source_native_if_not_equal_build TIMEOUT 5)
add_test(NAME source_native_bool_local_build COMMAND vxs build -file ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainBoolLocal.xs)
set_tests_properties(source_native_bool_local_build PROPERTIES TIMEOUT 5
                    PASS_REGULAR_EXPRESSION "wrote optimized LLVM IR.*executable")
add_test(NAME source_native_bool_local_artifacts COMMAND xs_xse_artifact_tests
                                                 ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainBoolLocal.ll
                                                 ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainBoolLocal.obj
                                                 ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainBoolLocal.vxse 7)
set_tests_properties(source_native_bool_local_artifacts PROPERTIES DEPENDS source_native_bool_local_build TIMEOUT 5)
add_test(NAME source_native_bool_not_local_build COMMAND vxs build -file
                                                   ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainBoolNotLocal.xs)
set_tests_properties(source_native_bool_not_local_build PROPERTIES TIMEOUT 5
                    PASS_REGULAR_EXPRESSION "wrote optimized LLVM IR.*executable")
add_test(NAME source_native_bool_not_local_artifacts COMMAND xs_xse_artifact_tests
                                                     ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainBoolNotLocal.ll
                                                     ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainBoolNotLocal.obj
                                                     ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainBoolNotLocal.vxse 7)
set_tests_properties(source_native_bool_not_local_artifacts PROPERTIES DEPENDS source_native_bool_not_local_build
                                                                       TIMEOUT 5)
add_test(NAME source_native_inferred_bool_local_build COMMAND vxs build -file
                                                       ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainInferredBoolLocal.xs)
set_tests_properties(source_native_inferred_bool_local_build PROPERTIES TIMEOUT 5
                    PASS_REGULAR_EXPRESSION "wrote optimized LLVM IR.*executable")
add_test(NAME source_native_inferred_bool_local_artifacts COMMAND xs_xse_artifact_tests
                                                          ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainInferredBoolLocal.ll
                                                          ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainInferredBoolLocal.obj
                                                          ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainInferredBoolLocal.vxse 7)
set_tests_properties(source_native_inferred_bool_local_artifacts PROPERTIES
                     DEPENDS source_native_inferred_bool_local_build TIMEOUT 5)
add_test(NAME source_native_inferred_bool_not_local_build COMMAND vxs build -file
                                                            ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainInferredBoolNotLocal.xs)
set_tests_properties(source_native_inferred_bool_not_local_build PROPERTIES TIMEOUT 5
                    PASS_REGULAR_EXPRESSION "wrote optimized LLVM IR.*executable")
add_test(NAME source_native_inferred_bool_not_local_artifacts COMMAND xs_xse_artifact_tests
                                                              ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainInferredBoolNotLocal.ll
                                                              ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainInferredBoolNotLocal.obj
                                                              ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainInferredBoolNotLocal.vxse
                                                              7)
set_tests_properties(source_native_inferred_bool_not_local_artifacts PROPERTIES
                     DEPENDS source_native_inferred_bool_not_local_build TIMEOUT 5)
