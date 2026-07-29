# SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
# SPDX-License-Identifier: MPL-2.0

set(XS_SOURCE_NATIVE_FIXTURE_DIR "${CMAKE_CURRENT_BINARY_DIR}/tests/fixtures/source")
file(MAKE_DIRECTORY "${XS_SOURCE_NATIVE_FIXTURE_DIR}")
foreach(source_fixture MainReturn0 MainReturn7 MainArithmetic MainDivision MainRemainder MainOperatorCall MainFunctionOverloads MainIntOperators MainFloatConstants MainFloatOperators MainStringLiteral MainStringFlow MainStringCompare MainCharFlow MainIntegerWidths MainIntegerOperators MainDataFields MainNestedDataFields MainDataInheritance MainDataConstructors MainDataConstructorFlow MainDataMethods MainDataValueProjection MainDataReturn MainNestedDataReturn MainNegative MainPositive
                       MainBitwise MainXor MainLocal MainLocalArithmetic MainLocalIf MainInferredLocal MainIf MainIfValue
                       MainIfNot
                       MainIfFalse MainIfNotEqual MainBoolLocal MainBoolNotLocal MainInferredBoolLocal
                       MainInferredBoolNotLocal MainCall MainNestedCall MainLocalCall MainBoolCall MainBoolCallLocal
                       MainRecursiveCall MainUnitCalls MainShortCircuit MainGenericFunctions MainGenericRecursive
                       MainGenericConstraint
                       MainMutableLocal MainMutableBoolLocal MainIfAssignment MainCompoundAssignment
                       MainIfMultipleAssignments MainNestedIfAssignment MainWhile MainWhileControl MainDoWhile MainLoop
                       MainBlockLocals MainFixedArray MainArrayMissingDefaults
                       MainArrayExcessDiscard MainInferredSizeArray MainArrayMutation MainDefaultFixedArray
                       MainDynamicArray MainDynamicArrayIndex MainDynamicArrayMutation MainArrayProperties
                       MainForEach MainTuple MainNamedTuple MainTupleCalls MainNestedTuple MainTupleArray MainTupleMutation
                       MainTupleDestructure
                       MainTupleForEach TupleUnknownMember TupleAssignmentMismatch TuplePatternArityMismatch
                       TuplePatternTypeMismatch TuplePatternDuplicateBinding TupleDeclarationArityMismatch
                       TupleDeclarationTypeMismatch TupleDeclarationDuplicateBinding
                       MainOptionalSomeCoalesce MainOptionalNoneCoalesce
                       MainOptionalAssignUnwrap MainOptionalAssignKeepsSome MainOptionalForcedSome
                       MainOptionalMemberSome MainOptionalMemberNone
                       MainResultPropagation MainResultError MainResultNestedOptional
                       MainResultMatch MainResultMatchError
                       InvalidCoalesceLeft InvalidOptionalForcedLong InvalidOptionalAssignImmutable
                       InvalidOptionalAssignNonOptional InvalidOptionalAssignType
                       InvalidResultPropagationOutside InvalidResultErrorMismatch InvalidResultOkPayload
                       InvalidResultConstructorOutside InvalidResultMatchDuplicate InvalidResultMatchVariant
                       CollectionSetCheck
                       MainEarlyReturn MainElseIf MainMatch MainMatchBool MainMatchExpression MainEnumFlow MainFor
                       MainPostfixDecrement MainUpdateValues
                       ImmutableLocalReassignment BlockLocalShadow SameScopeDuplicateLocal
                       MissingMain NonLiteralMain OutOfRangeMain OutOfRangeByteMain OutOfRangeUIntegerMain
                       ParameterizedMain WrongReturnMain UnknownCallMain
                       WrongCallArityMain BoolParameterCallMain NonLongReturnCallMain
                       BoolCallAsLongMain UnitCallAsLongMain InvalidLogicalOperands MatchMissingElse
                       MatchPatternTypeMismatch RecursiveDataParameter
                       AmbiguousInheritedDataField IncompleteDataConstructor LegacyDataConstructorCall DuplicateDataMethod ForEachNonArray ForEachBindingMismatch
                       UnknownGenericCall WrongGenericArity GenericTypeMismatch ExpandingGenericRecursion
                       UnsatisfiedGenericConstraint NonInterfaceGenericConstraint)
  configure_file(tests/fixtures/source/${source_fixture}.xs "${XS_SOURCE_NATIVE_FIXTURE_DIR}/${source_fixture}.xs"
                 COPYONLY)
endforeach()

function(xs_add_source_native_array_test fixture expected_exit expected_layout)
  set(instruction_pattern "extractvalue")
  if(ARGC GREATER 3)
    set(instruction_pattern "${ARGV3}")
  endif()
  string(TOLOWER "${fixture}" test_suffix)
  add_test(NAME source_native_${test_suffix}_build COMMAND xs build -file
                                                        ${XS_SOURCE_NATIVE_FIXTURE_DIR}/${fixture}.xs)
  set_tests_properties(source_native_${test_suffix}_build PROPERTIES TIMEOUT 5
                       PASS_REGULAR_EXPRESSION "wrote optimized LLVM IR.*executable")
  add_test(NAME source_native_${test_suffix}_artifacts COMMAND xs_xse_artifact_tests
                                                            ${XS_SOURCE_NATIVE_FIXTURE_DIR}/${fixture}.ll
                                                            ${XS_SOURCE_NATIVE_FIXTURE_DIR}/${fixture}.o
                                                            ${XS_SOURCE_NATIVE_FIXTURE_DIR}/${fixture}.xse
                                                            ${expected_exit} "${expected_layout}" "${instruction_pattern}")
  set_tests_properties(source_native_${test_suffix}_artifacts PROPERTIES
                       DEPENDS source_native_${test_suffix}_build TIMEOUT 5)
endfunction()

xs_add_source_native_array_test(MainArrayMissingDefaults 0 "[4 x i32]")
xs_add_source_native_array_test(MainArrayExcessDiscard 7 "[2 x i32]")
xs_add_source_native_array_test(MainInferredSizeArray 7 "{ ptr, i64 }" "malloc")
xs_add_source_native_array_test(MainArrayMutation 7 "[3 x i32]")
xs_add_source_native_array_test(MainDefaultFixedArray 0 "[3 x i32]")
xs_add_source_native_array_test(MainDynamicArray 11 "{ ptr, i64 }" "array.length")
xs_add_source_native_array_test(MainDynamicArrayIndex 7 "[3 x i32]" "getelementptr")
xs_add_source_native_array_test(MainDynamicArrayMutation 7 "[3 x i32]" "llvm.trap")
xs_add_source_native_array_test(MainArrayProperties 11 "[3 x i32]" "extractvalue")
xs_add_source_native_array_test(MainForEach 8 "[5 x i32]" "getelementptr")

function(xs_add_source_native_tuple_test fixture expected_exit)
  string(TOLOWER "${fixture}" test_suffix)
  add_test(NAME source_native_${test_suffix}_build COMMAND xs build -file
                                                        ${XS_SOURCE_NATIVE_FIXTURE_DIR}/${fixture}.xs)
  set_tests_properties(source_native_${test_suffix}_build PROPERTIES TIMEOUT 5
                       PASS_REGULAR_EXPRESSION "wrote optimized LLVM IR.*executable")
  add_test(NAME source_native_${test_suffix}_artifacts COMMAND xs_xse_artifact_tests
                                                            ${XS_SOURCE_NATIVE_FIXTURE_DIR}/${fixture}.ll
                                                            ${XS_SOURCE_NATIVE_FIXTURE_DIR}/${fixture}.o
                                                            ${XS_SOURCE_NATIVE_FIXTURE_DIR}/${fixture}.xse
                                                            ${expected_exit} ${ARGN})
  set_tests_properties(source_native_${test_suffix}_artifacts PROPERTIES
                       DEPENDS source_native_${test_suffix}_build TIMEOUT 5)
endfunction()

xs_add_source_native_tuple_test(MainTuple 7 "%tuple.0 = type { i32, i32 }" "extractvalue")
xs_add_source_native_tuple_test(MainNamedTuple 7 "%tuple.0 = type { i32, i32 }" "extractvalue")
xs_add_source_native_tuple_test(MainTupleCalls 7 "call %tuple.0 @make_pair" "call i32 @select_right")
xs_add_source_native_tuple_test(MainNestedTuple 7 "%tuple.1 = type { %tuple.0, i32 }" "extractvalue")
xs_add_source_native_tuple_test(MainTupleArray 7 "[2 x %tuple.0]" "extractvalue")
xs_add_source_native_tuple_test(MainTupleMutation 7 "store %tuple.0" "insertvalue")
xs_add_source_native_tuple_test(MainTupleForEach 10 "[2 x %tuple.1]" "extractvalue")
xs_add_source_native_tuple_test(MainTupleDestructure 7 "%tuple.1 = type { %tuple.0, i32 }" "extractvalue")
xs_add_source_native_tuple_test(MainEnumFlow 7 "%Color = type { i32 }" "define %Color @next" "extractvalue %Color"
                                "call %Color @next")

xs_add_source_native_tuple_test(MainOptionalSomeCoalesce 7 "%optional.0 = type { i8, i32 }"
                                "extractvalue %optional.0" "br i1")
xs_add_source_native_tuple_test(MainOptionalNoneCoalesce 3 "%optional.0 = type { i8, i32 }"
                                "zeroinitializer" "br i1")
xs_add_source_native_tuple_test(MainOptionalAssignUnwrap 7 "%optional.0 = type { i8, i32 }"
                                "br i1" "llvm.trap" "store %optional.0")
xs_add_source_native_tuple_test(MainOptionalAssignKeepsSome 5 "%optional.0 = type { i8, i32 }"
                                "br i1" "llvm.trap" "store %optional.0")
xs_add_source_native_tuple_test(MainOptionalForcedSome 11 "%optional.0 = type { i8, i32 }"
                                "extractvalue %optional.0" "llvm.trap")
xs_add_source_native_tuple_test(MainOptionalMemberSome 7 "%Point = type { i32, i32 }"
                                "%optional.0 = type { i8, %Point }" "%optional.1 = type { i8, i32 }" "br i1")
xs_add_source_native_tuple_test(MainOptionalMemberNone 13 "%Point = type { i32, i32 }"
                                "%optional.0 = type { i8, %Point }" "%optional.1 = type { i8, i32 }" "br i1")
xs_add_source_native_tuple_test(MainResultPropagation 7 "%result.0 = type { i8, i32, i32 }"
                                "define %result.0 @Produce" "define %result.0 @Forward"
                                "extractvalue %result.0" "br i1")
xs_add_source_native_tuple_test(MainResultError 13 "%result.0 = type { i8, i32, i32 }"
                                "define %result.0 @Fail" "define %result.0 @ForwardFailure"
                                "extractvalue %result.0" "br i1")
xs_add_source_native_tuple_test(MainResultNestedOptional 7 "%optional.0 = type { i8, i32 }"
                                "%result.0 = type { i8, %optional.0, i32 }"
                                "%result.1 = type { i8, i32, i32 }"
                                "define %result.0 @Maybe" "define %result.1 @Read" "br i1")
xs_add_source_native_tuple_test(MainResultMatch 7 "%result.0 = type { i8, i32, i32 }"
                                "define i32 @Read" "extractvalue %result.0" "br i1")
xs_add_source_native_tuple_test(MainResultMatchError 9 "%result.0 = type { i8, i32, i32 }"
                                "define i32 @Read" "extractvalue %result.0" "br i1")

add_test(NAME compiler_check_builtin_set COMMAND xs check -file
                                                 ${XS_SOURCE_NATIVE_FIXTURE_DIR}/CollectionSetCheck.xs)
set_tests_properties(compiler_check_builtin_set PROPERTIES TIMEOUT 5)

set(XS_PROJECT_NATIVE_FIXTURE_DIR "${CMAKE_CURRENT_BINARY_DIR}/tests/fixtures/projects")
file(COPY tests/fixtures/projects/ DESTINATION "${XS_PROJECT_NATIVE_FIXTURE_DIR}")

add_test(NAME source_native_return0_build COMMAND xs build -file ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainReturn0.xs)
set_tests_properties(source_native_return0_build PROPERTIES TIMEOUT 5
                    PASS_REGULAR_EXPRESSION "wrote optimized LLVM IR.*executable")
add_test(NAME source_native_return0_artifacts COMMAND xs_xse_artifact_tests ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainReturn0.ll
                                                ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainReturn0.o
                                                ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainReturn0.xse 0)
set_tests_properties(source_native_return0_artifacts PROPERTIES DEPENDS source_native_return0_build TIMEOUT 5)

add_test(NAME source_native_return0_run COMMAND xs run -file ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainReturn0.xs)
set_tests_properties(source_native_return0_run PROPERTIES TIMEOUT 5
  PASS_REGULAR_EXPRESSION "wrote optimized LLVM IR.*executable")

add_executable(xs_cli_run_tests tests/cli_run_tests.c)
add_test(NAME source_native_return7_run_exit COMMAND xs_cli_run_tests $<TARGET_FILE:xs>
  ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainReturn7.xs 7)
set_tests_properties(source_native_return7_run_exit PROPERTIES TIMEOUT 5)
add_test(NAME source_native_return7_build COMMAND xs build -file ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainReturn7.xs)
set_tests_properties(source_native_return7_build PROPERTIES TIMEOUT 5
                    PASS_REGULAR_EXPRESSION "wrote optimized LLVM IR.*executable")
add_test(NAME source_native_return7_artifacts COMMAND xs_xse_artifact_tests ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainReturn7.ll
                                                ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainReturn7.o
                                                ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainReturn7.xse 7)
set_tests_properties(source_native_return7_artifacts PROPERTIES DEPENDS source_native_return7_build TIMEOUT 5)
add_test(NAME source_native_arithmetic_build COMMAND xs build -file ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainArithmetic.xs)
set_tests_properties(source_native_arithmetic_build PROPERTIES TIMEOUT 5
                    PASS_REGULAR_EXPRESSION "wrote optimized LLVM IR.*executable")
add_test(NAME source_native_arithmetic_artifacts COMMAND xs_xse_artifact_tests
                                                  ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainArithmetic.ll
                                                  ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainArithmetic.o
                                                  ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainArithmetic.xse 7 "ret i32 7")
set_tests_properties(source_native_arithmetic_artifacts PROPERTIES DEPENDS source_native_arithmetic_build TIMEOUT 5)
add_test(NAME source_native_division_build COMMAND xs build -file ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainDivision.xs)
set_tests_properties(source_native_division_build PROPERTIES TIMEOUT 5
                    PASS_REGULAR_EXPRESSION "wrote optimized LLVM IR.*executable")
add_test(NAME source_native_division_artifacts COMMAND xs_xse_artifact_tests
                                                ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainDivision.ll
                                                ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainDivision.o
                                                ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainDivision.xse 7)
set_tests_properties(source_native_division_artifacts PROPERTIES DEPENDS source_native_division_build TIMEOUT 5)
add_test(NAME source_native_remainder_build COMMAND xs build -file ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainRemainder.xs)
set_tests_properties(source_native_remainder_build PROPERTIES TIMEOUT 5
                    PASS_REGULAR_EXPRESSION "wrote optimized LLVM IR.*executable")
add_test(NAME source_native_remainder_artifacts COMMAND xs_xse_artifact_tests
                                                 ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainRemainder.ll
                                                 ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainRemainder.o
                                                 ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainRemainder.xse 7)
set_tests_properties(source_native_remainder_artifacts PROPERTIES DEPENDS source_native_remainder_build TIMEOUT 5)
add_test(NAME source_native_operator_call_build COMMAND xs build -file
                                                       ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainOperatorCall.xs)
set_tests_properties(source_native_operator_call_build PROPERTIES TIMEOUT 5
                    PASS_REGULAR_EXPRESSION "wrote optimized LLVM IR.*executable")
add_test(NAME source_native_operator_call_artifacts COMMAND xs_xse_artifact_tests
                                                    ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainOperatorCall.ll
                                                    ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainOperatorCall.o
                                                    ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainOperatorCall.xse 7)
set_tests_properties(source_native_operator_call_artifacts PROPERTIES DEPENDS source_native_operator_call_build TIMEOUT 5)
add_test(NAME source_native_int_operators_build COMMAND xs build -file
                                                   ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainIntOperators.xs)
set_tests_properties(source_native_int_operators_build PROPERTIES TIMEOUT 5
                    PASS_REGULAR_EXPRESSION "wrote optimized LLVM IR.*executable")
add_test(NAME source_native_int_operators_artifacts COMMAND xs_xse_artifact_tests
                                                       ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainIntOperators.ll
                                                       ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainIntOperators.o
                                                       ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainIntOperators.xse 7
                                                       "define i64 @wide_ops" "sdiv i64" "srem i64" "ashr i64"
                                                       "icmp sge i64")
set_tests_properties(source_native_int_operators_artifacts PROPERTIES DEPENDS source_native_int_operators_build TIMEOUT 5)
add_test(NAME source_native_float_constants_build COMMAND xs build -file
                                                   ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainFloatConstants.xs)
set_tests_properties(source_native_float_constants_build PROPERTIES TIMEOUT 5
                    PASS_REGULAR_EXPRESSION "wrote optimized LLVM IR.*executable")
add_test(NAME source_native_float_constants_artifacts COMMAND xs_xse_artifact_tests
                                                         ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainFloatConstants.ll
                                                         ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainFloatConstants.o
                                                         ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainFloatConstants.xse 0
                                                         "define float @single_value" "ret float 1.500000e+00"
                                                         "define double @double_value" "ret double 1.500000e+00")
set_tests_properties(source_native_float_constants_artifacts PROPERTIES DEPENDS source_native_float_constants_build
                                                                           TIMEOUT 5)
add_test(NAME source_native_float_operators_build COMMAND xs build -file
                                                   ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainFloatOperators.xs)
set_tests_properties(source_native_float_operators_build PROPERTIES TIMEOUT 5
                    PASS_REGULAR_EXPRESSION "wrote optimized LLVM IR.*executable")
add_test(NAME source_native_float_operators_artifacts COMMAND xs_xse_artifact_tests
                                                         ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainFloatOperators.ll
                                                         ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainFloatOperators.o
  ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainFloatOperators.xse 0
  "fadd float" "frem float" "fdiv double" "fcmp olt double" "fcmp one double")
set_tests_properties(source_native_float_operators_artifacts PROPERTIES DEPENDS source_native_float_operators_build
                                                                           TIMEOUT 5)
add_test(NAME source_native_string_literal_build COMMAND xs build -file
                                                   ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainStringLiteral.xs)
set_tests_properties(source_native_string_literal_build PROPERTIES TIMEOUT 5
                    PASS_REGULAR_EXPRESSION "wrote optimized LLVM IR.*executable")
add_test(NAME source_native_string_literal_artifacts COMMAND xs_xse_artifact_tests
                                                         ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainStringLiteral.ll
                                                         ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainStringLiteral.o
                                                         ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainStringLiteral.xse 0
                                                         "define { ptr, i64 } @greeting" "constant [12 x i8]")
set_tests_properties(source_native_string_literal_artifacts PROPERTIES DEPENDS source_native_string_literal_build
                                                                          TIMEOUT 5)
add_test(NAME source_native_string_flow_build COMMAND xs build -file
                                                ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainStringFlow.xs)
set_tests_properties(source_native_string_flow_build PROPERTIES TIMEOUT 5
                    PASS_REGULAR_EXPRESSION "wrote optimized LLVM IR.*executable")
add_test(NAME source_native_string_flow_artifacts COMMAND xs_xse_artifact_tests
                                                      ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainStringFlow.ll
                                                      ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainStringFlow.o
                                                      ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainStringFlow.xse 0
                                                      "define { ptr, i64 } @identity" "call { ptr, i64 } @identity")
set_tests_properties(source_native_string_flow_artifacts PROPERTIES DEPENDS source_native_string_flow_build TIMEOUT 5)
add_test(NAME source_native_string_compare_build COMMAND xs build -file
                                               ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainStringCompare.xs)
set_tests_properties(source_native_string_compare_build PROPERTIES TIMEOUT 5
                     PASS_REGULAR_EXPRESSION "wrote optimized LLVM IR.*executable")
add_test(NAME source_native_string_compare_artifacts COMMAND xs_xse_artifact_tests
                                                    ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainStringCompare.ll
                                                    ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainStringCompare.o
                                                    ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainStringCompare.xse 0
                                                    "call i32 @memcmp")
set_tests_properties(source_native_string_compare_artifacts PROPERTIES DEPENDS source_native_string_compare_build TIMEOUT 5)
add_test(NAME source_native_char_flow_build COMMAND xs build -file
                                              ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainCharFlow.xs)
set_tests_properties(source_native_char_flow_build PROPERTIES TIMEOUT 5
                    PASS_REGULAR_EXPRESSION "wrote optimized LLVM IR.*executable")
add_test(NAME source_native_char_flow_artifacts COMMAND xs_xse_artifact_tests
                                                    ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainCharFlow.ll
                                                    ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainCharFlow.o
                                                    ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainCharFlow.xse 0
                                                    "define i32 @identity" "store i32 937" "call i32 @identity")
set_tests_properties(source_native_char_flow_artifacts PROPERTIES DEPENDS source_native_char_flow_build TIMEOUT 5)
add_test(NAME source_native_integer_widths_build COMMAND xs build -file
                                                  ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainIntegerWidths.xs)
set_tests_properties(source_native_integer_widths_build PROPERTIES TIMEOUT 5
                    PASS_REGULAR_EXPRESSION "wrote optimized LLVM IR.*executable")
add_test(NAME source_native_integer_widths_artifacts COMMAND xs_xse_artifact_tests
                                                       ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainIntegerWidths.ll
                                                       ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainIntegerWidths.o
                                                       ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainIntegerWidths.xse 0
                                                       "define i8 @byte_max" "define i16 @short_min"
                                                       "define i128 @integer_min" "ret i128 -1")
set_tests_properties(source_native_integer_widths_artifacts PROPERTIES DEPENDS source_native_integer_widths_build
                                                                       TIMEOUT 5)
add_test(NAME source_native_integer_operators_build COMMAND xs build -file
                                                   ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainIntegerOperators.xs)
set_tests_properties(source_native_integer_operators_build PROPERTIES TIMEOUT 5
                     PASS_REGULAR_EXPRESSION "wrote optimized LLVM IR.*executable")
add_test(NAME source_native_integer_operators_artifacts COMMAND xs_xse_artifact_tests
                                                        ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainIntegerOperators.ll
                                                        ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainIntegerOperators.o
                                                        ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainIntegerOperators.xse 0
                                                        "add i8" "sdiv i8" "udiv i64" "icmp ult i128"
                                                        "lshr i128" "icmp slt i128" "ashr i128")
set_tests_properties(source_native_integer_operators_artifacts PROPERTIES
                     DEPENDS source_native_integer_operators_build TIMEOUT 5)

add_test(NAME source_native_fixed_array_build COMMAND xs build -file
                                               ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainFixedArray.xs)
set_tests_properties(source_native_fixed_array_build PROPERTIES TIMEOUT 5
                     PASS_REGULAR_EXPRESSION "wrote optimized LLVM IR.*executable")
add_test(NAME source_native_fixed_array_artifacts COMMAND xs_xse_artifact_tests
                                                   ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainFixedArray.ll
                                                   ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainFixedArray.o
                                                   ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainFixedArray.xse 7
                                                   "[3 x i32]" "extractvalue [3 x i32]")
set_tests_properties(source_native_fixed_array_artifacts PROPERTIES DEPENDS source_native_fixed_array_build TIMEOUT 5)

add_test(NAME source_native_data_fields_build COMMAND xs build -file
                                                  ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainDataFields.xs)
set_tests_properties(source_native_data_fields_build PROPERTIES TIMEOUT 5
                     PASS_REGULAR_EXPRESSION "wrote optimized LLVM IR.*executable")
add_test(NAME source_native_data_fields_artifacts COMMAND xs_xse_artifact_tests
                                                       ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainDataFields.ll
                                                       ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainDataFields.o
                                                       ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainDataFields.xse 9
                                                       "alloca i32" "store i32 2" "store i32 3" "add i32")
set_tests_properties(source_native_data_fields_artifacts PROPERTIES DEPENDS source_native_data_fields_build TIMEOUT 5)

add_test(NAME source_native_nested_data_fields_build COMMAND xs build -file
                                                         ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainNestedDataFields.xs)
set_tests_properties(source_native_nested_data_fields_build PROPERTIES TIMEOUT 5
                            PASS_REGULAR_EXPRESSION "wrote optimized LLVM IR.*executable")
add_test(NAME source_native_nested_data_fields_artifacts COMMAND xs_xse_artifact_tests
                                                              ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainNestedDataFields.ll
                                                              ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainNestedDataFields.o
                                                              ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainNestedDataFields.xse 22
                                                              "define i32 @total(i32" "store i32 2" "store i32 3"
                                                              "store i32 7" "call i32 @total")
set_tests_properties(source_native_nested_data_fields_artifacts PROPERTIES
                     DEPENDS source_native_nested_data_fields_build TIMEOUT 5)

add_test(NAME source_native_data_inheritance_build COMMAND xs build -file
                                                    ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainDataInheritance.xs)
set_tests_properties(source_native_data_inheritance_build PROPERTIES TIMEOUT 5
                     PASS_REGULAR_EXPRESSION "wrote optimized LLVM IR.*executable")
add_test(NAME source_native_data_inheritance_artifacts COMMAND xs_xse_artifact_tests
                                                         ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainDataInheritance.ll
                                                         ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainDataInheritance.o
                                                         ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainDataInheritance.xse 25
                                                         "define i32 @total(i32 %0, i32 %1, i32 %2, i32 %3, i32 %4)"
                                                         "store i32 1" "store i32 2" "call i32 @total")
set_tests_properties(source_native_data_inheritance_artifacts PROPERTIES
                     DEPENDS source_native_data_inheritance_build TIMEOUT 5)

add_test(NAME source_native_data_constructors_build COMMAND xs build -file
                                                    ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainDataConstructors.xs)
set_tests_properties(source_native_data_constructors_build PROPERTIES TIMEOUT 5
                     PASS_REGULAR_EXPRESSION "wrote optimized LLVM IR.*executable")
add_test(NAME source_native_data_constructors_artifacts COMMAND xs_xse_artifact_tests
                                                         ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainDataConstructors.ll
                                                         ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainDataConstructors.o
                                                         ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainDataConstructors.xse 16
                                                         "define %Point @\"xs\$ctor\$Point\$0\""
                                                         "define %Point @\"xs\$ctor\$Point\$1\""
                                                         "define %Point @\"xs\$ctor\$Point\$2\""
                                                         "call %Point @\"xs\$ctor\$Point\$0\""
                                                         "call %Point @\"xs\$ctor\$Point\$1\""
                                                         "call %Point @\"xs\$ctor\$Point\$2\"")
set_tests_properties(source_native_data_constructors_artifacts PROPERTIES
                     DEPENDS source_native_data_constructors_build TIMEOUT 5)

add_test(NAME source_native_data_constructor_flow_build COMMAND xs build -file
                                                       ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainDataConstructorFlow.xs)
set_tests_properties(source_native_data_constructor_flow_build PROPERTIES TIMEOUT 5
                     PASS_REGULAR_EXPRESSION "wrote optimized LLVM IR.*executable")
add_test(NAME source_native_data_constructor_flow_artifacts COMMAND xs_xse_artifact_tests
                                                            ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainDataConstructorFlow.ll
                                                            ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainDataConstructorFlow.o
                                                            ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainDataConstructorFlow.xse 9
                                                            "call %Point @\"xs\$ctor\$Point\$0\""
                                                            "call %Point @\"xs\$ctor\$Point\$1\""
                                                            "call i32 @total"
                                                            "call %Point @make_point"
                                                            "extractvalue %Point")
set_tests_properties(source_native_data_constructor_flow_artifacts PROPERTIES
                     DEPENDS source_native_data_constructor_flow_build TIMEOUT 5)

add_test(NAME source_native_data_methods_build COMMAND xs build -file
                                               ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainDataMethods.xs)
set_tests_properties(source_native_data_methods_build PROPERTIES TIMEOUT 5
                     PASS_REGULAR_EXPRESSION "wrote optimized LLVM IR.*executable")
add_test(NAME source_native_data_methods_artifacts COMMAND xs_xse_artifact_tests
  ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainDataMethods.ll
  ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainDataMethods.o
  ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainDataMethods.xse 15
  "xs$method$Point$total$0" "xs$method$Point$add$1"
  "xs$method$Point$add$2" "xs$method$Point$identity$3")
set_tests_properties(source_native_data_methods_artifacts PROPERTIES
                     DEPENDS source_native_data_methods_build TIMEOUT 5)

add_test(NAME source_native_data_value_projection_build COMMAND xs build -file
  ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainDataValueProjection.xs)
set_tests_properties(source_native_data_value_projection_build PROPERTIES TIMEOUT 5
  PASS_REGULAR_EXPRESSION "wrote optimized LLVM IR.*executable")
add_test(NAME source_native_data_value_projection_artifacts COMMAND xs_xse_artifact_tests
  ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainDataValueProjection.ll
  ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainDataValueProjection.o
  ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainDataValueProjection.xse 10
  "call %Point @make_point" "extractvalue %Point" "extractvalue %Coordinates")
set_tests_properties(source_native_data_value_projection_artifacts PROPERTIES
  DEPENDS source_native_data_value_projection_build TIMEOUT 5)

add_test(NAME source_native_function_overloads_build COMMAND xs build -file
                                                    ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainFunctionOverloads.xs)
set_tests_properties(source_native_function_overloads_build PROPERTIES TIMEOUT 5
                     PASS_REGULAR_EXPRESSION "wrote optimized LLVM IR.*executable")
add_test(NAME source_native_function_overloads_artifacts COMMAND xs_xse_artifact_tests
  ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainFunctionOverloads.ll
  ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainFunctionOverloads.o
  ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainFunctionOverloads.xse 10
  "xs$fn$choose$0" "xs$fn$choose$1")
set_tests_properties(source_native_function_overloads_artifacts PROPERTIES
                     DEPENDS source_native_function_overloads_build TIMEOUT 5)

add_test(NAME source_native_data_return_build COMMAND xs build -file
                                                ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainDataReturn.xs)
set_tests_properties(source_native_data_return_build PROPERTIES TIMEOUT 5
                    PASS_REGULAR_EXPRESSION "wrote optimized LLVM IR.*executable")
add_test(NAME source_native_data_return_artifacts COMMAND xs_xse_artifact_tests
                                                    ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainDataReturn.ll
                                                    ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainDataReturn.o
                                                    ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainDataReturn.xse 5
                                                    "%Point = type { i32, i32 }"
                                                    "define %Point @make_point" "call %Point @make_point")
set_tests_properties(source_native_data_return_artifacts PROPERTIES DEPENDS source_native_data_return_build TIMEOUT 5)

add_test(NAME source_native_nested_data_return_build COMMAND xs build -file
                                                       ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainNestedDataReturn.xs)
set_tests_properties(source_native_nested_data_return_build PROPERTIES TIMEOUT 5
                    PASS_REGULAR_EXPRESSION "wrote optimized LLVM IR.*executable")
add_test(NAME source_native_nested_data_return_artifacts COMMAND xs_xse_artifact_tests
                                                           ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainNestedDataReturn.ll
                                                           ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainNestedDataReturn.o
                                                           ${XS_SOURCE_NATIVE_FIXTURE_DIR}/MainNestedDataReturn.xse 9
                                                           "%Coordinates = type { i32, i32 }"
                                                           "%Point = type { %Coordinates, i32 }"
                                                           "call %Point @make_point")
set_tests_properties(source_native_nested_data_return_artifacts PROPERTIES
                    DEPENDS source_native_nested_data_return_build TIMEOUT 5)
