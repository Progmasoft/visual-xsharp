/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 */

use std::fmt::Write;

pub fn format_units(units: &[u32]) -> String
{
    format_with_prefix("utf32", units)
}

pub fn format_encoded(encoding: crate::xlil::Utf32Encoding, units: &[u32]) -> String
{
    format_with_prefix(encoding.text_name(), units)
}

fn format_with_prefix(prefix: &str, units: &[u32]) -> String
{
    let mut output = format!("{prefix} [");
    for (index, unit) in units.iter().enumerate()
    {
        if index != 0
        {
            output.push_str(", ");
        }
        let _ = write!(output, "0x{unit:08x}");
    }
    output.push(']');
    output
}

pub fn parse_units(value: &str) -> Option<Vec<u32>>
{
    parse_with_prefix(value, "utf32")
}

pub fn parse_encoded(value: &str) -> Option<(crate::xlil::Utf32Encoding, Vec<u32>)>
{
    for encoding in [
        crate::xlil::Utf32Encoding::LittleEndian,
        crate::xlil::Utf32Encoding::BigEndian,
    ]
    {
        if let Some(units) = parse_with_prefix(value, encoding.text_name())
        {
            return Some((encoding, units));
        }
    }
    None
}

fn parse_with_prefix(value: &str, prefix: &str) -> Option<Vec<u32>>
{
    let values = value.strip_prefix(&format!("{prefix} ["))?.strip_suffix(']')?;
    let units = if values.is_empty()
    {
        Vec::new()
    }
    else
    {
        values
            .split(", ")
            .map(|value| {
                let hexadecimal = value.strip_prefix("0x")?;
                (hexadecimal.len() == 8).then_some(())?;
                u32::from_str_radix(hexadecimal, 16).ok()
            })
            .collect::<Option<Vec<_>>>()?
    };
    units.iter().copied().map(char::from_u32).collect::<Option<Vec<_>>>()?;
    Some(units)
}

#[cfg(test)]
mod tests
{
    use super::*;

    #[test]
    fn roundtrips_utf32_code_units()
    {
        let units = "Leitwolf 🐺".chars().map(u32::from).collect::<Vec<_>>();
        assert_eq!(parse_units(&format_units(&units)), Some(units));
        assert_eq!(parse_units("utf32 [0x0000d800]"), None);
        assert_eq!(parse_units("utf32 [0x41]"), None);
        assert_eq!(
            parse_encoded("utf32le [0x00000041]"),
            Some((crate::xlil::Utf32Encoding::LittleEndian, vec![0x41]))
        );
    }
}
