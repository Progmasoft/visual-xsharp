/*
 * SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
 * SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.0
 */

use std::{error::Error, fmt};

use crate::xlil::{BuildError, Type};

/// Error reported by the typed XLIL producer facade.
#[derive(Clone, Debug, Eq, PartialEq)]
pub enum TypedBuildError
{
    /// The raw XLIL builder rejected the requested operation.
    Raw(BuildError),
    /// An operation required an open function definition.
    NoCurrentFunction,
    /// A typed parameter request used an invalid index.
    ParameterOutOfRange
    {
        /// Requested zero-based index.
        index: usize,
        /// Number of parameters in the current signature.
        count: usize,
    },
    /// A Rust marker disagreed with the current XLIL signature or register.
    TypeMismatch
    {
        /// Operation being checked.
        operation: &'static str,
        /// Type required by the typed API.
        expected: Type,
        /// Type stored in the XLIL model.
        actual: Type,
    },
    /// A value-producing call resolved to a void signature.
    MissingCallResult
    {
        /// Callee symbol.
        function: String,
    },
    /// A void call resolved to a value-producing signature.
    UnexpectedCallResult
    {
        /// Callee symbol.
        function: String,
        /// Registered result type.
        actual: Type,
    },
    /// An exact-width integer constant did not fit its selected type.
    IntegerConstantOutOfRange
    {
        /// Selected XLIL integer type.
        value_type: Type,
        /// Requested unsigned bit pattern.
        bits: u128,
    },
}

impl fmt::Display for TypedBuildError
{
    fn fmt(&self, formatter: &mut fmt::Formatter<'_>) -> fmt::Result
    {
        match self
        {
            Self::Raw(error) => error.fmt(formatter),
            Self::NoCurrentFunction => formatter.write_str("typed XLIL builder has no current function"),
            Self::ParameterOutOfRange {
                index,
                count,
            } =>
            {
                write!(
                    formatter,
                    "XLIL parameter index {index} is outside signature arity {count}"
                )
            }
            Self::TypeMismatch {
                operation,
                expected,
                actual,
            } => write!(
                formatter,
                "typed XLIL {operation} expects {expected:?}, found {actual:?}"
            ),
            Self::MissingCallResult {
                function,
            } => write!(formatter, "XLIL call to '{function}' returns no value"),
            Self::UnexpectedCallResult {
                function,
                actual,
            } =>
            {
                write!(formatter, "XLIL void call to '{function}' returns {actual:?}")
            }
            Self::IntegerConstantOutOfRange {
                value_type,
                bits,
            } =>
            {
                write!(
                    formatter,
                    "integer bit pattern {bits:#x} does not fit XLIL {value_type:?}"
                )
            }
        }
    }
}

impl Error for TypedBuildError
{
    fn source(&self) -> Option<&(dyn Error + 'static)>
    {
        match self
        {
            Self::Raw(error) => Some(error),
            _ => None,
        }
    }
}

impl From<BuildError> for TypedBuildError
{
    fn from(value: BuildError) -> Self
    {
        Self::Raw(value)
    }
}
