# SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
# SPDX-License-Identifier: MPL-2.0

foreach(source_fixture MissingMain NonLiteralMain OutOfRangeMain OutOfRangeByteMain OutOfRangeUIntegerMain
                       ParameterizedMain WrongReturnMain UnknownCallMain
                       WrongCallArityMain NonLongReturnCallMain
                       BoolCallAsLongMain UnitCallAsLongMain InvalidLogicalOperands ImmutableLocalReassignment
                       MatchMissingElse MatchPatternTypeMismatch RecursiveDataParameter
                       AmbiguousInheritedDataField IncompleteDataConstructor LegacyDataConstructorCall DuplicateDataMethod
                       ForEachNonArray ForEachBindingMismatch TupleUnknownMember TupleAssignmentMismatch
                       TuplePatternArityMismatch TuplePatternTypeMismatch TuplePatternDuplicateBinding
                       TupleDeclarationArityMismatch TupleDeclarationTypeMismatch TupleDeclarationDuplicateBinding
                       UnknownGenericCall WrongGenericArity GenericTypeMismatch ExpandingGenericRecursion
                       UnsatisfiedGenericConstraint NonInterfaceGenericConstraint)
  add_test(NAME source_native_invalid_${source_fixture} COMMAND xs build -file
                                                           ${XS_SOURCE_NATIVE_FIXTURE_DIR}/${source_fixture}.xs)
  set_tests_properties(source_native_invalid_${source_fixture} PROPERTIES TIMEOUT 5 WILL_FAIL TRUE)
endforeach()

add_executable(xs_backend_tests tests/backend_tests.c)
target_link_libraries(xs_backend_tests PRIVATE xs_backend_llvm)
add_test(NAME backend COMMAND xs_backend_tests ${XS_BACKEND_TEST_OBJECT} ${XS_LLD_EXECUTABLE})
set_tests_properties(backend PROPERTIES TIMEOUT 10)

xs_add_c_test(modules tests/module_tests.c xs_compiler ${XS_SOURCE_FROM_BINARY}/tests/fixtures/module_project
              ${XS_SOURCE_FROM_BINARY}/tests/fixtures/duplicate_modules)
xs_add_c_test(hir tests/hir_tests.c xs_compiler)
xs_add_c_test(hir_members tests/hir_member_tests.c xs_compiler)
xs_add_c_test(hir_macro tests/hir_macro_tests.c xs_compiler)
xs_add_c_test(hir_types tests/hir_type_tests.c xs_compiler)
xs_add_c_test(hir_expressions tests/hir_expression_tests.c xs_compiler)
xs_add_c_test(inheritance tests/inheritance_tests.c xs_compiler)
xs_add_c_test(int128 tests/int128_tests.c xs_lil)
xs_add_c_test(c23_features tests/c23_features_tests.c xs_lil)
xs_add_c_test(xlil tests/xlil_tests.c xs_compiler)
xs_add_c_test(lil_c_headers tests/lil_c_headers_tests.c xs_lil)
xs_add_c_test(lil_c_producer tests/lil_c_producer_tests.c xs_lil)
xs_add_c_test(mir tests/mir_tests.c xs_compiler)
xs_add_c_test(mir_flow tests/mir_flow_tests.c xs_compiler)
xs_add_c_test(syntax_ast tests/syntax_ast_tests.c xs_compiler)
xs_add_c_test(syntax_decl tests/syntax_decl_tests.c xs_compiler)
xs_add_c_test(syntax_macro tests/syntax_macro_tests.c xs_compiler)
xs_add_c_test(source_include tests/source_include_tests.c xs_compiler)
xs_add_c_test(syntax_macro_view tests/syntax_macro_view_tests.c xs_compiler)
set(XS_SPEC_PROGRAM_FIXTURE_DIR "${CMAKE_CURRENT_BINARY_DIR}/tests/fixtures/spec_programs")
file(MAKE_DIRECTORY "${XS_SPEC_PROGRAM_FIXTURE_DIR}")
file(GLOB XS_SPEC_PROGRAM_SOURCES CONFIGURE_DEPENDS RELATIVE "${CMAKE_SOURCE_DIR}" "Spec/Programs/*.xs")
set(XS_SPEC_PROGRAM_FIXTURES)
foreach(spec_program IN LISTS XS_SPEC_PROGRAM_SOURCES)
  get_filename_component(spec_program_name "${spec_program}" NAME)
  get_filename_component(spec_program_stem "${spec_program}" NAME_WE)
  configure_file("${spec_program}" "${XS_SPEC_PROGRAM_FIXTURE_DIR}/${spec_program_name}" COPYONLY)
  list(APPEND XS_SPEC_PROGRAM_FIXTURES "tests/fixtures/spec_programs/${spec_program_name}")
  add_test(NAME spec_program_check_${spec_program_stem}
           COMMAND xs check -file "${XS_SPEC_PROGRAM_FIXTURE_DIR}/${spec_program_name}")
  set_tests_properties(spec_program_check_${spec_program_stem} PROPERTIES TIMEOUT 5)
endforeach()
xs_add_c_test(spec_program_syntax tests/spec_program_syntax_tests.c xs_compiler ${XS_SPEC_PROGRAM_FIXTURES})
add_executable(xs_compiler_core_bridge_tests tests/compiler_core_bridge_tests.c)
target_link_libraries(xs_compiler_core_bridge_tests PRIVATE xs_compiler xslang_compiler_core)
add_test(NAME compiler_core_bridge COMMAND xs_compiler_core_bridge_tests)
set_tests_properties(compiler_core_bridge PROPERTIES TIMEOUT 5)
