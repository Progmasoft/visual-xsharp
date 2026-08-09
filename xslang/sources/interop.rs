/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 */

//! Internal Rust and C++23 Preview compiler-core interoperability.

mod cpp;

/// Returns the ABI version implemented by the linked C++23 Preview bridge.
#[must_use]
pub fn bridge_abi_version() -> u32
{
    cpp::bridge_abi_version()
}

/// Reports whether the linked C++23 Preview bridge accepts an XLIL text version.
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
