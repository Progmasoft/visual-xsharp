/*
 * SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
 * SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.0
 */

use crate::xlil::{Type, types::XlilType};

/// Owned function signature used by [`super::TypedBuilder`].
#[derive(Clone, Debug, Eq, PartialEq)]
pub struct Signature
{
    name: String,
    return_type: Type,
    parameters: Vec<Type>,
}

impl Signature
{
    /// Creates a value-producing signature.
    #[must_use]
    pub fn returning<T: XlilType>(name: impl Into<String>) -> Self
    {
        Self {
            name: name.into(),
            return_type: T::XLIL_TYPE,
            parameters: Vec::new(),
        }
    }

    /// Creates a void signature.
    #[must_use]
    pub fn void(name: impl Into<String>) -> Self
    {
        Self {
            name: name.into(),
            return_type: Type::VOID,
            parameters: Vec::new(),
        }
    }

    /// Appends one typed parameter.
    #[must_use]
    pub fn parameter<T: XlilType>(mut self) -> Self
    {
        self.parameters.push(T::XLIL_TYPE);
        self
    }

    /// Appends one runtime-selected parameter type.
    #[must_use]
    pub fn dynamic_parameter(mut self, value_type: Type) -> Self
    {
        self.parameters.push(value_type);
        self
    }

    /// Returns the function symbol.
    #[must_use]
    pub fn name(&self) -> &str
    {
        &self.name
    }

    /// Returns the XLIL result type.
    #[must_use]
    pub const fn return_type(&self) -> Type
    {
        self.return_type
    }

    /// Returns parameter types in declaration order.
    #[must_use]
    pub fn parameters(&self) -> &[Type]
    {
        &self.parameters
    }

    /// Returns signature arity.
    #[must_use]
    pub const fn arity(&self) -> usize
    {
        self.parameters.len()
    }

    pub(crate) fn into_parts(self) -> (String, Type, Vec<Type>)
    {
        (self.name, self.return_type, self.parameters)
    }
}

#[cfg(test)]
mod tests
{
    use super::*;

    #[test]
    fn signature_builder_preserves_order_and_result()
    {
        let signature = Signature::returning::<i64>("sum").parameter::<i32>().parameter::<i64>();
        assert_eq!(signature.name(), "sum");
        assert_eq!(signature.return_type(), Type::I64);
        assert_eq!(signature.parameters(), &[Type::I32, Type::I64]);
        assert_eq!(signature.arity(), 2);
    }

    #[test]
    fn void_signature_has_no_implicit_parameters()
    {
        let signature = Signature::void("sink");
        assert_eq!(signature.return_type(), Type::VOID);
        assert_eq!(signature.arity(), 0);
    }
}
