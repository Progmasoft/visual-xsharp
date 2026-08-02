/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 */

pub fn decode_character(token: &str) -> Option<u32>
{
    let inner = token.strip_prefix('\'')?.strip_suffix('\'')?;
    if let Some(escape) = inner.strip_prefix('\\')
    {
        return decode_escape(escape);
    }
    let mut characters = inner.chars();
    let value = characters.next()?;
    if characters.next().is_some()
    {
        return None;
    }
    Some(u32::from(value))
}

pub fn format_character(value: u32) -> String
{
    match value
    {
        0 => "'\\0'".to_string(),
        7 => "'\\a'".to_string(),
        8 => "'\\b'".to_string(),
        9 => "'\\t'".to_string(),
        10 => "'\\n'".to_string(),
        11 => "'\\v'".to_string(),
        12 => "'\\f'".to_string(),
        13 => "'\\r'".to_string(),
        value if value == u32::from(b'\'') => "'\\\''".to_string(),
        value if value == u32::from(b'\\') => "'\\\\'".to_string(),
        0x20..=0x7e => format!("'{}'", char::from_u32(value).unwrap_or('?')),
        value if value <= 0xffff => format!("'\\u{value:04x}'"),
        _ => format!("'\\U{value:08x}'"),
    }
}

fn decode_escape(value: &str) -> Option<u32>
{
    let simple = match value
    {
        "'" => Some(u32::from(b'\'')),
        "\"" => Some(u32::from(b'\"')),
        "\\" => Some(u32::from(b'\\')),
        "0" => Some(0),
        "a" => Some(7),
        "b" => Some(8),
        "f" => Some(12),
        "n" => Some(10),
        "r" => Some(13),
        "t" => Some(9),
        "v" => Some(11),
        _ => None,
    };
    simple.or_else(|| decode_hex_escape(value))
}

fn decode_hex_escape(value: &str) -> Option<u32>
{
    let (digits, width) = if let Some(digits) = value.strip_prefix('x')
    {
        (digits, 2)
    }
    else if let Some(digits) = value.strip_prefix('u')
    {
        (digits, 4)
    }
    else
    {
        (value.strip_prefix('U')?, 8)
    };
    (digits.len() == width).then_some(())?;
    let value = u32::from_str_radix(digits, 16).ok()?;
    char::from_u32(value).map(u32::from)
}

#[cfg(test)]
mod tests
{
    use super::*;

    #[test]
    fn decodes_utf32_scalar_character_tokens()
    {
        assert_eq!(decode_character("'A'"), Some(0x0041));
        assert_eq!(decode_character("'Ω'"), Some(0x03a9));
        assert_eq!(decode_character("'😀'"), Some(0x1f600));
        assert_eq!(decode_character("'\\n'"), Some(0x000a));
        assert_eq!(decode_character("'\\u03a9'"), Some(0x03a9));
        assert_eq!(decode_character("'\\U0000ffff'"), Some(0xffff));
        assert_eq!(decode_character("'\\U00010000'"), Some(0x10000));
        assert_eq!(format_character(0x0041), "'A'");
        assert_eq!(format_character(0x03a9), "'\\u03a9'");
    }
}
