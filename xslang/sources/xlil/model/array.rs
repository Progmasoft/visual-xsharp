/*
 * SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
 * SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.0
 */

use super::*;

impl Function
{
    pub(crate) fn add_array_length(&mut self, block: BlockId, array: ValueId) -> Option<ValueId>
    {
        if self.value(array)?.value_type.kind != TypeKind::Array
        {
            return None;
        }
        let result = ValueId(self.values.len() as u32);
        self.values.push(Value {
            id: result,
            value_type: Type::I64,
        });
        self.block_mut(block)?.instructions.push(Instruction::ArrayLength {
            result,
            array,
        });
        Some(result)
    }

    pub(crate) fn add_array_get(
        &mut self,
        block: BlockId,
        array: ValueId,
        index: ValueId,
        element_type: Type,
    ) -> Option<ValueId>
    {
        if self.value(array)?.value_type.kind != TypeKind::Array ||
            self.value(index)?.value_type != Type::I64 ||
            element_type == Type::VOID
        {
            return None;
        }
        let result = ValueId(self.values.len() as u32);
        self.values.push(Value {
            id: result,
            value_type: element_type,
        });
        self.block_mut(block)?.instructions.push(Instruction::ArrayGet {
            result,
            array,
            index,
        });
        Some(result)
    }

    pub(crate) fn add_array_set(
        &mut self,
        block: BlockId,
        array: ValueId,
        index: ValueId,
        value: ValueId,
    ) -> Option<ValueId>
    {
        let array_type = self.value(array)?.value_type;
        if array_type.kind != TypeKind::Array ||
            self.value(index)?.value_type != Type::I64 ||
            self.value(value).is_none()
        {
            return None;
        }
        let result = ValueId(self.values.len() as u32);
        self.values.push(Value {
            id: result,
            value_type: array_type,
        });
        self.block_mut(block)?.instructions.push(Instruction::ArraySet {
            result,
            array,
            index,
            value,
        });
        Some(result)
    }
}
