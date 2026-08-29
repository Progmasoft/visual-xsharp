/*
 * SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
 * SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.0
 */

//! Internal Rust and C++20 compiler-core interoperability.

mod cpp;

/// Returns the ABI version implemented by the linked C++20 bridge.
#[must_use]
pub fn bridge_abi_version() -> u32
{
    cpp::bridge_abi_version()
}

/// Reports whether the linked C++20 bridge accepts an XLIL text version.
#[must_use]
pub fn supports_xlil_version(version: u32) -> bool
{
    cpp::supports_xlil_version(version)
}

#[cfg(test)]
mod tests
{
    use super::{bridge_abi_version, supports_xlil_version};

    #[test]
    fn cpp_bridge_reports_compatible_versions()
    {
        assert_eq!(bridge_abi_version(), 1);
        assert!(supports_xlil_version(0));
        assert!(supports_xlil_version(1));
        assert!(!supports_xlil_version(2));
    }
}
