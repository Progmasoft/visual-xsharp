/*
 * SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
 * SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.0
 */

//! Build-time version metadata for the target-independent compiler core.

fn main()
{
    cxx_build::bridge("sources/interop/cpp.rs")
        .cpp(true)
        .file("native/sources/interop/CompilerBridge.cpp")
        .include("native/include")
        .compiler("clang-cl")
        .flag("/std:c++20")
        .flag_if_supported("/W4")
        .compile("xslang_cpp_bridge");

    let package_version = std::env::var("CARGO_PKG_VERSION").expect("Cargo provides CARGO_PKG_VERSION");
    println!("cargo::rerun-if-changed=Cargo.toml");
    println!("cargo::rerun-if-changed=native/include/xslang/interop/CompilerBridge.hpp");
    println!("cargo::rerun-if-changed=native/sources/interop/CompilerBridge.cpp");
    println!("cargo::rerun-if-changed=sources/interop/cpp.rs");
    println!("cargo::rustc-env=XSLANG_BUILD_VERSION={package_version}");
    for format in ["XHIR", "XMIR", "XLIL"]
    {
        println!("cargo::rustc-env=XSLANG_{format}_VERSION=1");
    }
}
