/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 */

use std::{
    error::Error,
    fmt,
    io::{self, Read, Write},
};

use super::{Module, ModuleError, VerifiedModule, module_to_string, parse_module, parse_verified};

/// I/O, parse, or verification failure from the stream-oriented XLIL API.
#[derive(Debug)]
pub enum ModuleIoError
{
    /// Reading or writing the stream failed.
    Io(io::Error),
    /// The text could not be parsed or did not verify.
    Module(ModuleError),
}

impl fmt::Display for ModuleIoError
{
    fn fmt(&self, formatter: &mut fmt::Formatter<'_>) -> fmt::Result
    {
        match self
        {
            Self::Io(error) => write!(formatter, "XLIL stream I/O failed: {error}"),
            Self::Module(error) => error.fmt(formatter),
        }
    }
}

impl Error for ModuleIoError
{
    fn source(&self) -> Option<&(dyn Error + 'static)>
    {
        match self
        {
            Self::Io(error) => Some(error),
            Self::Module(error) => Some(error),
        }
    }
}

impl From<io::Error> for ModuleIoError
{
    fn from(error: io::Error) -> Self
    {
        Self::Io(error)
    }
}

impl From<ModuleError> for ModuleIoError
{
    fn from(error: ModuleError) -> Self
    {
        Self::Module(error)
    }
}

/// Reads and parses an XLIL module without running the model verifier.
pub fn read_module(mut input: impl Read) -> Result<Module, ModuleIoError>
{
    let mut text = String::new();
    input.read_to_string(&mut text)?;
    parse_module(&text).map_err(|diagnostics| ModuleError::Parse(diagnostics).into())
}

/// Reads, parses, and verifies an XLIL module.
pub fn read_verified(mut input: impl Read) -> Result<VerifiedModule, ModuleIoError>
{
    let mut text = String::new();
    input.read_to_string(&mut text)?;
    parse_verified(&text).map_err(Into::into)
}

/// Writes canonical text for a model to a byte stream.
pub fn write_module_io(module: &Module, mut output: impl Write) -> Result<(), ModuleIoError>
{
    output.write_all(module_to_string(module).as_bytes())?;
    Ok(())
}

/// Writes canonical text for a verified module to a byte stream.
pub fn write_verified(module: &VerifiedModule, output: impl Write) -> Result<(), ModuleIoError>
{
    write_module_io(module.as_module(), output)
}
