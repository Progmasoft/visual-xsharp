/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 */

use std::{
    fmt,
    io::{self, Write},
};

/// Owned value accepted by the exported C-format `printf!` implementation.
#[doc(hidden)]
#[derive(Clone, Debug, PartialEq)]
pub enum PrintfValue
{
    /// Signed integer argument.
    Signed(i128),
    /// Unsigned integer argument.
    Unsigned(u128),
    /// Floating-point argument.
    Float(f64),
    /// UTF-8 Rust string argument.
    Text(String),
    /// Unicode scalar argument.
    Character(char),
    /// Pointer address argument.
    Pointer(usize),
}

/// Conversion boundary used by [`crate::printf!`].
#[doc(hidden)]
pub trait PrintfArgument
{
    /// Converts one borrowed Rust value to an owned formatting value.
    fn to_printf_value(&self) -> PrintfValue;
}

macro_rules! signed_arguments {
  ($($value_type:ty),+ $(,)?) => {
    $(impl PrintfArgument for $value_type
    {
      fn to_printf_value(&self) -> PrintfValue
      {
        PrintfValue::Signed(*self as i128)
      }
    })+
  };
}

macro_rules! unsigned_arguments {
  ($($value_type:ty),+ $(,)?) => {
    $(impl PrintfArgument for $value_type
    {
      fn to_printf_value(&self) -> PrintfValue
      {
        PrintfValue::Unsigned(*self as u128)
      }
    })+
  };
}

signed_arguments!(i8, i16, i32, i64, i128, isize);
unsigned_arguments!(u8, u16, u32, u64, u128, usize);

impl PrintfArgument for f32
{
    fn to_printf_value(&self) -> PrintfValue
    {
        PrintfValue::Float(f64::from(*self))
    }
}

impl PrintfArgument for f64
{
    fn to_printf_value(&self) -> PrintfValue
    {
        PrintfValue::Float(*self)
    }
}

impl PrintfArgument for str
{
    fn to_printf_value(&self) -> PrintfValue
    {
        PrintfValue::Text(self.to_owned())
    }
}

impl PrintfArgument for String
{
    fn to_printf_value(&self) -> PrintfValue
    {
        PrintfValue::Text(self.clone())
    }
}

impl PrintfArgument for char
{
    fn to_printf_value(&self) -> PrintfValue
    {
        PrintfValue::Character(*self)
    }
}

impl PrintfArgument for bool
{
    fn to_printf_value(&self) -> PrintfValue
    {
        PrintfValue::Unsigned(u128::from(*self))
    }
}

impl<T: PrintfArgument + ?Sized> PrintfArgument for &T
{
    fn to_printf_value(&self) -> PrintfValue
    {
        (*self).to_printf_value()
    }
}

impl<T> PrintfArgument for *const T
{
    fn to_printf_value(&self) -> PrintfValue
    {
        PrintfValue::Pointer(self.cast::<()>() as usize)
    }
}

impl<T> PrintfArgument for *mut T
{
    fn to_printf_value(&self) -> PrintfValue
    {
        PrintfValue::Pointer(self.cast::<()>() as usize)
    }
}

/// Captured C-style format string and its converted arguments.
#[doc(hidden)]
#[derive(Clone, Debug, PartialEq)]
pub struct PrintfArguments
{
    format: &'static str,
    values: Vec<PrintfValue>,
}

impl PrintfArguments
{
    /// Creates a formatting packet used by the exported macro.
    #[must_use]
    pub fn new(format: &'static str, values: Vec<PrintfValue>) -> Self
    {
        Self {
            format,
            values,
        }
    }

    /// Renders the packet without writing it.
    pub fn render(&self) -> io::Result<String>
    {
        render(self.format, &self.values)
    }
}

/// Writes already formatted arguments to standard output.
#[doc(hidden)]
pub fn _printf(arguments: fmt::Arguments<'_>) -> io::Result<()>
{
    let stdout = io::stdout();
    let mut lock = stdout.lock();
    lock.write_fmt(arguments)
}

/// Writes C-style formatted output to standard output.
///
/// The macro is implemented under `xslang::rust`, but `#[macro_export]`
/// intentionally exposes it at the crate root as `xslang::printf!`.
#[macro_export]
macro_rules! printf {
  ($format:literal $(, $argument:expr)* $(,)?) => {{
    let values = vec![$(
      (&$argument as &dyn $crate::rust::PrintfArgument).to_printf_value()
    ),*];
    match $crate::rust::PrintfArguments::new($format, values).render() {
      Ok(output) => $crate::rust::_printf(format_args!("{output}")),
      Err(error) => Err(error),
    }
  }};
}

#[derive(Clone, Copy, Debug, Default)]
struct FormatSpec
{
    left: bool,
    plus: bool,
    alternate: bool,
    zero: bool,
    width: Option<usize>,
    precision: Option<usize>,
    conversion: char,
}

fn invalid(message: &'static str) -> io::Error
{
    io::Error::new(io::ErrorKind::InvalidInput, message)
}

fn render(format: &str, values: &[PrintfValue]) -> io::Result<String>
{
    let characters: Vec<char> = format.chars().collect();
    let mut output = String::new();
    let mut cursor = 0;
    let mut argument = 0;
    while cursor < characters.len()
    {
        if characters[cursor] != '%'
        {
            output.push(characters[cursor]);
            cursor += 1;
            continue;
        }
        cursor += 1;
        if characters.get(cursor) == Some(&'%')
        {
            output.push('%');
            cursor += 1;
            continue;
        }
        let spec = parse_spec(&characters, &mut cursor)?;
        let value = values
            .get(argument)
            .ok_or_else(|| invalid("printf argument is missing"))?;
        output.push_str(&format_value(spec, value)?);
        argument += 1;
    }
    if argument != values.len()
    {
        return Err(invalid("printf received unused arguments"));
    }
    Ok(output)
}

fn parse_spec(characters: &[char], cursor: &mut usize) -> io::Result<FormatSpec>
{
    let mut spec = FormatSpec::default();
    while let Some(flag) = characters.get(*cursor)
    {
        match flag
        {
            '-' => spec.left = true,
            '+' => spec.plus = true,
            '#' => spec.alternate = true,
            '0' => spec.zero = true,
            ' ' => (),
            _ => break,
        }
        *cursor += 1;
    }
    spec.width = parse_number(characters, cursor);
    if characters.get(*cursor) == Some(&'.')
    {
        *cursor += 1;
        spec.precision = Some(parse_number(characters, cursor).unwrap_or(0));
    }
    while matches!(characters.get(*cursor), Some('h' | 'l' | 'j' | 'z' | 't' | 'L'))
    {
        *cursor += 1;
    }
    spec.conversion = *characters
        .get(*cursor)
        .ok_or_else(|| invalid("printf conversion is missing"))?;
    *cursor += 1;
    if !matches!(
        spec.conversion,
        'd' | 'i' | 'u' | 'x' | 'X' | 'o' | 'f' | 'F' | 'e' | 'E' | 'g' | 'G' | 's' | 'c' | 'p'
    )
    {
        return Err(invalid("printf conversion is unsupported"));
    }
    Ok(spec)
}

fn parse_number(characters: &[char], cursor: &mut usize) -> Option<usize>
{
    let start = *cursor;
    let mut value = 0usize;
    while let Some(digit) = characters.get(*cursor).and_then(|character| character.to_digit(10))
    {
        value = value.saturating_mul(10).saturating_add(digit as usize);
        *cursor += 1;
    }
    (*cursor != start).then_some(value)
}

fn format_value(spec: FormatSpec, value: &PrintfValue) -> io::Result<String>
{
    let body = match (spec.conversion, value)
    {
        ('d' | 'i', PrintfValue::Signed(value)) => signed_text(*value, spec.plus),
        ('u', PrintfValue::Unsigned(value)) => value.to_string(),
        ('x', PrintfValue::Unsigned(value)) => integer_text(format!("{value:x}"), spec.alternate, "0x"),
        ('X', PrintfValue::Unsigned(value)) => integer_text(format!("{value:X}"), spec.alternate, "0X"),
        ('o', PrintfValue::Unsigned(value)) => integer_text(format!("{value:o}"), spec.alternate, "0o"),
        ('f' | 'F', PrintfValue::Float(value)) =>
        {
            format!("{value:.precision$}", precision = spec.precision.unwrap_or(6))
        }
        ('e', PrintfValue::Float(value)) => format!("{value:.precision$e}", precision = spec.precision.unwrap_or(6)),
        ('E', PrintfValue::Float(value)) => format!("{value:.precision$E}", precision = spec.precision.unwrap_or(6)),
        ('g' | 'G', PrintfValue::Float(value)) =>
        {
            format!("{value:.precision$}", precision = spec.precision.unwrap_or(6))
        }
        ('s', PrintfValue::Text(value)) => truncate(value, spec.precision),
        ('c', PrintfValue::Character(value)) => value.to_string(),
        ('c', PrintfValue::Unsigned(value)) => u32::try_from(*value)
            .ok()
            .and_then(char::from_u32)
            .ok_or_else(|| invalid("printf %c value is invalid"))?
            .to_string(),
        ('p', PrintfValue::Pointer(value)) => format!("0x{value:x}"),
        _ => return Err(invalid("printf argument type does not match its conversion")),
    };
    Ok(apply_width(body, spec))
}

fn signed_text(value: i128, plus: bool) -> String
{
    if plus && value >= 0
    {
        format!("+{value}")
    }
    else
    {
        value.to_string()
    }
}

fn integer_text(body: String, alternate: bool, prefix: &str) -> String
{
    if alternate
    {
        format!("{prefix}{body}")
    }
    else
    {
        body
    }
}

fn truncate(value: &str, precision: Option<usize>) -> String
{
    precision.map_or_else(|| value.to_owned(), |length| value.chars().take(length).collect())
}

fn apply_width(body: String, spec: FormatSpec) -> String
{
    let Some(width) = spec.width
    else
    {
        return body;
    };
    let length = body.chars().count();
    if length >= width
    {
        return body;
    }
    let padding_length = width - length;
    if spec.left
    {
        let padding: String = std::iter::repeat_n(' ', padding_length).collect();
        format!("{body}{padding}")
    }
    else if spec.zero
    {
        zero_pad(body, padding_length)
    }
    else
    {
        let padding: String = std::iter::repeat_n(' ', padding_length).collect();
        format!("{padding}{body}")
    }
}

fn zero_pad(body: String, padding_length: usize) -> String
{
    let prefix_length = if body.starts_with(['+', '-'])
    {
        1
    }
    else if body.starts_with("0x") || body.starts_with("0X") || body.starts_with("0o")
    {
        2
    }
    else
    {
        0
    };
    let (prefix, value) = body.split_at(prefix_length);
    let padding: String = std::iter::repeat_n('0', padding_length).collect();
    format!("{prefix}{padding}{value}")
}

#[cfg(test)]
mod tests
{
    use super::*;

    #[test]
    fn renders_common_c_format_conversions()
    {
        let arguments = PrintfArguments::new("name=%s count=%+05d hex=%#x ratio=%.2f %%", vec![
            PrintfValue::Text("Alpha".to_owned()),
            PrintfValue::Signed(7),
            PrintfValue::Unsigned(255),
            PrintfValue::Float(1.25),
        ]);
        assert_eq!(
            arguments.render().unwrap(),
            "name=Alpha count=+0007 hex=0xff ratio=1.25 %"
        );
    }

    #[test]
    fn rejects_argument_count_and_type_mismatches()
    {
        assert!(PrintfArguments::new("%d", vec![]).render().is_err());
        assert!(
            PrintfArguments::new("%s", vec![PrintfValue::Signed(1)])
                .render()
                .is_err()
        );
        assert!(
            PrintfArguments::new("plain", vec![PrintfValue::Signed(1)])
                .render()
                .is_err()
        );
    }
}
