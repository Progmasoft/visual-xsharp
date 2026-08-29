/*
 * SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
 * SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.0
 */

use std::{error::Error, fmt, ops::Deref};

use super::{Module, ParseDiagnostic, VerifyDiagnostic, module_to_string, parse_module, verify_module};

/// Failure while parsing or validating an XLIL module.
#[derive(Clone, Debug, Eq, PartialEq)]
pub enum ModuleError
{
    /// Text parsing failed.
    Parse(Vec<ParseDiagnostic>),
    /// Whole-module invariant verification failed.
    Verify(Vec<VerifyDiagnostic>),
}

impl ModuleError
{
    /// Returns the number of diagnostics carried by this error.
    #[must_use]
    pub fn diagnostic_count(&self) -> usize
    {
        match self
        {
            Self::Parse(diagnostics) => diagnostics.len(),
            Self::Verify(diagnostics) => diagnostics.len(),
        }
    }
}

impl fmt::Display for ModuleError
{
    fn fmt(&self, formatter: &mut fmt::Formatter<'_>) -> fmt::Result
    {
        let stage = match self
        {
            Self::Parse(_) => "parsing",
            Self::Verify(_) => "verification",
        };
        write!(
            formatter,
            "XLIL {stage} failed with {} diagnostic(s)",
            self.diagnostic_count()
        )
    }
}

impl Error for ModuleError {}

/// An XLIL module proven to satisfy the current model verifier.
///
/// Mutating the model requires consuming this wrapper and validating it again.
#[derive(Clone, Debug, Eq, PartialEq)]
pub struct VerifiedModule(Module);

impl VerifiedModule
{
    /// Verifies and wraps `module`.
    pub fn new(module: Module) -> Result<Self, ModuleError>
    {
        let diagnostics = verify_module(&module);
        if diagnostics.is_empty()
        {
            Ok(Self(module))
        }
        else
        {
            Err(ModuleError::Verify(diagnostics))
        }
    }

    /// Returns the verified read-only model.
    #[must_use]
    pub const fn as_module(&self) -> &Module
    {
        &self.0
    }

    /// Consumes the proof wrapper and returns the model.
    #[must_use]
    pub fn into_module(self) -> Module
    {
        self.0
    }

    /// Emits canonical XLIL text.
    #[must_use]
    pub fn to_text(&self) -> String
    {
        module_to_string(&self.0)
    }
}

impl Deref for VerifiedModule
{
    type Target = Module;

    fn deref(&self) -> &Self::Target
    {
        &self.0
    }
}

impl TryFrom<Module> for VerifiedModule
{
    type Error = ModuleError;

    fn try_from(module: Module) -> Result<Self, Self::Error>
    {
        Self::new(module)
    }
}

/// Parses and verifies an XLIL text module in one operation.
pub fn parse_verified(text: &str) -> Result<VerifiedModule, ModuleError>
{
    let module = parse_module(text).map_err(ModuleError::Parse)?;
    VerifiedModule::new(module)
}
