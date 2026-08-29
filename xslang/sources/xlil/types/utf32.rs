/*
 * SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
 * SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.0
 */

use crate::xlil::{BuildError, Builder, Utf32Encoding, ValueId};

/// Converts Rust text into target-endian UTF-32 code points for an XLIL
/// string constant.
///
/// The Rust source text is not retained in the XLIL model. Canonical XLIL
/// output contains only the selected encoding and numeric code points.
#[derive(Clone, Debug, Eq, PartialEq)]
pub struct Utf32Builder
{
    encoding: Utf32Encoding,
    code_points: Vec<u32>,
}

impl Utf32Builder
{
    /// Creates an empty UTF-32 value using native target byte order.
    #[must_use]
    pub const fn empty() -> Self
    {
        Self {
            encoding: Utf32Encoding::native(),
            code_points: Vec::new(),
        }
    }

    /// Converts text using the compilation host's native byte order.
    #[must_use]
    pub fn new(text: impl AsRef<str>) -> Self
    {
        Self::with_encoding(text, Utf32Encoding::native())
    }

    /// Converts text using an explicitly selected target byte order.
    #[must_use]
    pub fn with_encoding(text: impl AsRef<str>, encoding: Utf32Encoding) -> Self
    {
        Self {
            encoding,
            code_points: text.as_ref().chars().map(u32::from).collect(),
        }
    }

    /// Builds a value from already validated Unicode scalar values.
    pub fn from_code_points(code_points: impl IntoIterator<Item = char>, encoding: Utf32Encoding) -> Self
    {
        Self {
            encoding,
            code_points: code_points.into_iter().map(u32::from).collect(),
        }
    }

    /// Returns the target storage byte order.
    #[must_use]
    pub const fn encoding(&self) -> Utf32Encoding
    {
        self.encoding
    }

    /// Changes the target storage byte order without changing code points.
    pub fn set_encoding(&mut self, encoding: Utf32Encoding)
    {
        self.encoding = encoding;
    }

    /// Returns the converted Unicode scalar values.
    #[must_use]
    pub fn code_points(&self) -> &[u32]
    {
        &self.code_points
    }

    /// Consumes the builder and returns its code points.
    #[must_use]
    pub fn into_code_points(self) -> Vec<u32>
    {
        self.code_points
    }

    /// Returns the number of Unicode scalar values.
    #[must_use]
    pub const fn len(&self) -> usize
    {
        self.code_points.len()
    }

    /// Returns whether no Unicode scalar values were supplied.
    #[must_use]
    pub const fn is_empty(&self) -> bool
    {
        self.code_points.is_empty()
    }

    /// Reserves storage for at least `additional` more code points.
    pub fn reserve(&mut self, additional: usize)
    {
        self.code_points.reserve(additional);
    }

    /// Appends one Unicode scalar value.
    pub fn push(&mut self, value: char)
    {
        self.code_points.push(u32::from(value));
    }

    /// Appends every Unicode scalar value in `text`.
    pub fn push_str(&mut self, text: impl AsRef<str>)
    {
        self.code_points.extend(text.as_ref().chars().map(u32::from));
    }

    /// Removes every accumulated code point while preserving byte order.
    pub fn clear(&mut self)
    {
        self.code_points.clear();
    }

    /// Serializes code points using the selected target byte order.
    #[must_use]
    pub fn encoded_bytes(&self) -> Vec<u8>
    {
        let mut bytes = Vec::with_capacity(self.code_points.len() * size_of::<u32>());
        for code_point in &self.code_points
        {
            let encoded = match self.encoding
            {
                Utf32Encoding::LittleEndian => code_point.to_le_bytes(),
                Utf32Encoding::BigEndian => code_point.to_be_bytes(),
            };
            bytes.extend_from_slice(&encoded);
        }
        bytes
    }

    /// Appends a copy of the converted value to the current XLIL builder block.
    pub fn emit(&self, builder: &mut Builder) -> Result<ValueId, BuildError>
    {
        builder.const_str(self.encoding, self.code_points.clone())
    }

    /// Consumes and appends the converted value to the current XLIL builder block.
    pub fn emit_owned(self, builder: &mut Builder) -> Result<ValueId, BuildError>
    {
        builder.const_str(self.encoding, self.code_points)
    }
}

impl Default for Utf32Builder
{
    fn default() -> Self
    {
        Self::empty()
    }
}

impl From<&str> for Utf32Builder
{
    fn from(value: &str) -> Self
    {
        Self::new(value)
    }
}

impl From<String> for Utf32Builder
{
    fn from(value: String) -> Self
    {
        Self::new(value)
    }
}

impl Extend<char> for Utf32Builder
{
    fn extend<T: IntoIterator<Item = char>>(&mut self, iterator: T)
    {
        self.code_points.extend(iterator.into_iter().map(u32::from));
    }
}

impl FromIterator<char> for Utf32Builder
{
    fn from_iter<T: IntoIterator<Item = char>>(iterator: T) -> Self
    {
        Self::from_code_points(iterator, Utf32Encoding::native())
    }
}

#[cfg(test)]
mod tests
{
    use crate::xlil::{Builder, Type, module_to_string};

    use super::*;

    #[test]
    fn builder_accumulates_scalars_without_retaining_utf8_source()
    {
        let mut text = Utf32Builder::new("A");
        text.push('🐺');
        text.push_str("Ω");
        assert_eq!(text.code_points(), &[0x41, 0x1f43a, 0x3a9]);
        assert_eq!(text.len(), 3);
    }

    #[test]
    fn byte_serialization_obeys_explicit_endianness()
    {
        let little = Utf32Builder::with_encoding("A", Utf32Encoding::LittleEndian);
        let big = Utf32Builder::with_encoding("A", Utf32Encoding::BigEndian);
        assert_eq!(little.encoded_bytes(), [0x41, 0x00, 0x00, 0x00]);
        assert_eq!(big.encoded_bytes(), [0x00, 0x00, 0x00, 0x41]);
    }

    #[test]
    fn xlil_text_contains_only_encoding_and_numeric_code_points()
    {
        let mut builder = Builder::new("Utf32");
        builder.begin_function("text", Type::STR, vec![]).unwrap();
        builder.append_block("entry").unwrap();
        let value = Utf32Builder::with_encoding("A🐺", Utf32Encoding::LittleEndian)
            .emit_owned(&mut builder)
            .unwrap();
        builder.return_value(Some(value)).unwrap();
        let text = module_to_string(&builder.finish().unwrap());
        assert!(text.contains("utf32le [0x00000041, 0x0001f43a]"));
        assert!(!text.contains("A🐺"));
    }

    #[test]
    fn clear_preserves_encoding_and_allows_reuse()
    {
        let mut text = Utf32Builder::with_encoding("discard", Utf32Encoding::BigEndian);
        text.clear();
        text.extend(['X', 'S']);
        assert_eq!(text.encoding(), Utf32Encoding::BigEndian);
        assert_eq!(text.code_points(), &[0x58, 0x53]);
    }
}
