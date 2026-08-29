/*
 * SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
 * SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.0
 */

#[cxx::bridge(namespace = "xslang::interop")]
mod ffi
{
    unsafe extern "C++" {
        include!("xslang/interop/CompilerBridge.hpp");

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
