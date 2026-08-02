/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 */

#[cxx::bridge(namespace = "xslang::interop")]
mod ffi
{
    unsafe extern "C++" {
        include!("xslang/interop/CompilerBridge.hxx");

        #[cxx_name = "BridgeAbiVersion"]
        fn bridge_abi_version() -> u32;

        #[cxx_name = "SupportsXlilVersion"]
        fn supports_xlil_version(version: u32) -> bool;
    }
}

pub(crate) fn bridge_abi_version() -> u32
{
    ffi::bridge_abi_version()
}

pub(crate) fn supports_xlil_version(version: u32) -> bool
{
    ffi::supports_xlil_version(version)
}
