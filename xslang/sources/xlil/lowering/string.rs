/*
 * SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
 * SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.0
 */

use super::*;
use crate::xlil::StrComparisonOperation;

impl MirToXlilLowerer
{
    #[allow(clippy::too_many_arguments)]
    pub(super) fn lower_str_comparison(
        &mut self,
        operation: StrComparisonOperation,
        result: mir::LocalId,
        left: mir::LocalId,
        right: mir::LocalId,
        span: Span,
        block: BlockId,
        values: &mut HashMap<mir::LocalId, ValueId>,
        function: &mut Function,
    )
    {
        self.lower_float_operation(
            result,
            left,
            right,
            span,
            "Str comparison",
            block,
            values,
            function,
            |function, block, left, right| function.compare_str(block, operation, left, right),
        );
    }
}
