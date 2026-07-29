/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 */

//! Build-time version metadata for the target-independent compiler core.

fn main()
{
  let package_version = std::env::var("CARGO_PKG_VERSION").expect("Cargo provides CARGO_PKG_VERSION");
  println!("cargo::rerun-if-changed=Cargo.toml");
  println!("cargo::rustc-env=XSLANG_BUILD_VERSION={package_version}");
  for format in ["XHIR", "XMIR", "XLIL"]
  {
    println!("cargo::rustc-env=XSLANG_{format}_VERSION=0");
  }
}
