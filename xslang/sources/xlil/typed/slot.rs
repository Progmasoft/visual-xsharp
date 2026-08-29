/*
 * SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
 * SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.0
 */

use std::{fmt, marker::PhantomData};

use crate::xlil::{SlotId, types::XlilType};

/// Stack-slot identifier carrying a compile-time XLIL primitive type.
#[derive(Eq, Hash, PartialEq)]
pub struct Slot<T: XlilType>
{
    id: SlotId,
    marker: PhantomData<fn() -> T>,
}

impl<T: XlilType> Slot<T>
{
    pub(crate) const fn trusted(id: SlotId) -> Self
    {
        Self {
            id,
            marker: PhantomData,
        }
    }

    /// Returns the underlying XLIL stack-slot identifier.
    #[must_use]
    pub const fn id(self) -> SlotId
    {
        self.id
    }
}

impl<T: XlilType> Clone for Slot<T>
{
    fn clone(&self) -> Self
    {
        *self
    }
}

impl<T: XlilType> Copy for Slot<T> {}

impl<T: XlilType> fmt::Debug for Slot<T>
{
    fn fmt(&self, formatter: &mut fmt::Formatter<'_>) -> fmt::Result
    {
        formatter
            .debug_struct("Slot")
            .field("id", &self.id)
            .field("type", &T::NAME)
            .finish()
    }
}
