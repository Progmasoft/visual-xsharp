# SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
# SPDX-License-Identifier: MPL-2.0

# Retained tests protect reusable algorithms while preventing the retired IR
# catalog from leaking back into the renewed compiler pipeline.
xs_add_c_test(int128 "${XS_TEST_SOURCE_ROOT}/int128_tests.c" xs_lil)
xs_add_c_test(c23_features "${XS_TEST_SOURCE_ROOT}/c23_features_tests.c" xs_lil)
xs_add_c_test(xlil "${XS_TEST_SOURCE_ROOT}/xlil_tests.c" xs_lil)
xs_add_c_test(lil_c_headers "${XS_TEST_SOURCE_ROOT}/lil_c_headers_tests.c" xs_lil)
xs_add_c_test(lil_c_producer "${XS_TEST_SOURCE_ROOT}/lil_c_producer_tests.c" xs_lil)
xs_add_cxx_test(lil_cpp "${XS_TEST_SOURCE_ROOT}/LilCppTests.cpp" xs_lil_cpp)
