impl HirToMirLowerer
{
    fn lower_binary_into(
        &mut self,
        target: mir::LocalId,
        operator: BinaryOperator,
        left: &Expression,
        right: &Expression,
        span: Span,
        lowered: &mut mir::Function,
    )
    {
        let Some(target_type) = self.local_value_type(target, lowered)
        else
        {
            self.report(
                DiagnosticCode::UnsupportedType,
                "binary expression target local has no MIR value type",
                span,
            );
            return;
        };
        let Some(operand_type) = self.binary_operand_type(operator, target_type, left, right, lowered)
        else
        {
            self.report(
                DiagnosticCode::UnsupportedExpression,
                "HIR binary expression cannot lower to MIR for these operand/result types",
                span,
            );
            return;
        };
        let Some(left) = self.lower_expression_to_local(left, operand_type, lowered)
        else
        {
            return;
        };
        let Some(right) = self.lower_expression_to_local(right, operand_type, lowered)
        else
        {
            return;
        };
        if self.lower_enum_comparison(target, operator, operand_type, (left, right), span, lowered)
        {
            return;
        }
        match (operator, target_type, operand_type)
        {
            (BinaryOperator::Add, XlilType::I32, XlilType::I32) =>
            {
                self.current_block_mut(lowered).statements.push(mir::Statement::AddI32 {
                    result: target,
                    left,
                    right,
                    span,
                })
            }
            (BinaryOperator::Sub, XlilType::I32, XlilType::I32) =>
            {
                self.current_block_mut(lowered).statements.push(mir::Statement::SubI32 {
                    result: target,
                    left,
                    right,
                    span,
                })
            }
            (BinaryOperator::Mul, XlilType::I32, XlilType::I32) =>
            {
                self.current_block_mut(lowered).statements.push(mir::Statement::MulI32 {
                    result: target,
                    left,
                    right,
                    span,
                })
            }
            (operator, XlilType::I32, XlilType::I32) if binary_i32_operation(operator).is_some() =>
            {
                self.current_block_mut(lowered)
                    .statements
                    .push(mir::Statement::BinaryI32 {
                        operation: binary_i32_operation(operator).expect("guarded i32 operation must exist"),
                        result: target,
                        left,
                        right,
                        span,
                    });
            }
            (BinaryOperator::Equal, XlilType::BOOL, XlilType::I32) =>
            {
                self.current_block_mut(lowered).statements.push(mir::Statement::EqI32 {
                    result: target,
                    left,
                    right,
                    span,
                })
            }
            (BinaryOperator::Less, XlilType::BOOL, XlilType::I32) =>
            {
                self.current_block_mut(lowered).statements.push(mir::Statement::LtI32 {
                    result: target,
                    left,
                    right,
                    span,
                })
            }
            (BinaryOperator::LessEqual, XlilType::BOOL, XlilType::I32) =>
            {
                self.current_block_mut(lowered).statements.push(mir::Statement::LeI32 {
                    result: target,
                    left,
                    right,
                    span,
                })
            }
            (BinaryOperator::Greater, XlilType::BOOL, XlilType::I32) =>
            {
                self.current_block_mut(lowered).statements.push(mir::Statement::GtI32 {
                    result: target,
                    left,
                    right,
                    span,
                })
            }
            (BinaryOperator::GreaterEqual, XlilType::BOOL, XlilType::I32) =>
            {
                self.current_block_mut(lowered).statements.push(mir::Statement::GeI32 {
                    result: target,
                    left,
                    right,
                    span,
                })
            }
            (BinaryOperator::Add, XlilType::I64, XlilType::I64) =>
            {
                self.current_block_mut(lowered).statements.push(mir::Statement::AddI64 {
                    result: target,
                    left,
                    right,
                    span,
                })
            }
            (BinaryOperator::Sub, XlilType::I64, XlilType::I64) =>
            {
                self.current_block_mut(lowered).statements.push(mir::Statement::SubI64 {
                    result: target,
                    left,
                    right,
                    span,
                })
            }
            (BinaryOperator::Mul, XlilType::I64, XlilType::I64) =>
            {
                self.current_block_mut(lowered).statements.push(mir::Statement::MulI64 {
                    result: target,
                    left,
                    right,
                    span,
                })
            }
            (operator, XlilType::I64, XlilType::I64) if binary_i64_operation(operator).is_some() =>
            {
                self.current_block_mut(lowered)
                    .statements
                    .push(mir::Statement::BinaryI64 {
                        operation: binary_i64_operation(operator).expect("guarded i64 operation must exist"),
                        result: target,
                        left,
                        right,
                        span,
                    });
            }
            (BinaryOperator::Equal, XlilType::BOOL, XlilType::I64) =>
            {
                self.current_block_mut(lowered).statements.push(mir::Statement::EqI64 {
                    result: target,
                    left,
                    right,
                    span,
                })
            }
            (operator, XlilType::BOOL, XlilType::I64) if comparison_i64_operation(operator).is_some() =>
            {
                self.current_block_mut(lowered)
                    .statements
                    .push(mir::Statement::CompareI64 {
                        operation: comparison_i64_operation(operator).expect("guarded i64 comparison must exist"),
                        result: target,
                        left,
                        right,
                        span,
                    });
            }
            (operator, value_type, operand_type)
                if value_type == operand_type && value_type.is_integer() && integer_operation(operator).is_some() =>
            {
                self.current_block_mut(lowered)
                    .statements
                    .push(mir::Statement::BinaryInteger {
                        operation: integer_operation(operator).expect("guarded integer operation must exist"),
                        value_type,
                        result: target,
                        left,
                        right,
                        span,
                    });
            }
            (operator, XlilType::BOOL, operand_type)
                if operand_type.is_integer() &&
                    integer_operation(operator).is_some_and(|operation| operation.is_comparison()) =>
            {
                self.current_block_mut(lowered)
                    .statements
                    .push(mir::Statement::BinaryInteger {
                        operation: integer_operation(operator).expect("guarded integer comparison must exist"),
                        value_type: operand_type,
                        result: target,
                        left,
                        right,
                        span,
                    });
            }
            (operator, value_type, operand_type)
                if value_type == operand_type &&
                    matches!(value_type, XlilType::F32 | XlilType::F64) &&
                    binary_float_operation(operator).is_some() =>
            {
                self.current_block_mut(lowered)
                    .statements
                    .push(mir::Statement::BinaryFloat {
                        operation: binary_float_operation(operator).expect("guarded float operation must exist"),
                        value_type,
                        result: target,
                        left,
                        right,
                        span,
                    });
            }
            (operator, XlilType::BOOL, operand_type)
                if matches!(operand_type, XlilType::F32 | XlilType::F64) &&
                    comparison_float_operation(operator).is_some() =>
            {
                self.current_block_mut(lowered)
                    .statements
                    .push(mir::Statement::CompareFloat {
                        operation: comparison_float_operation(operator).expect("guarded float comparison must exist"),
                        value_type: operand_type,
                        result: target,
                        left,
                        right,
                        span,
                    });
            }
            (operator, XlilType::BOOL, XlilType::STR) if comparison_str_operation(operator).is_some() =>
            {
                self.current_block_mut(lowered)
                    .statements
                    .push(mir::Statement::CompareStr {
                        operation: comparison_str_operation(operator).expect("guarded Str comparison"),
                        result: target,
                        left,
                        right,
                        span,
                    });
            }
            _ => self.report(
                DiagnosticCode::UnsupportedExpression,
                "HIR binary expression has no MIR instruction for this type combination",
                span,
            ),
        }
    }

    fn local_value_type(&self, local: mir::LocalId, lowered: &mir::Function) -> Option<XlilType>
    {
        lowered
            .parameters
            .iter()
            .find(|candidate| candidate.local == local)
            .map(|candidate| candidate.value_type)
            .or_else(|| {
                lowered
                    .locals
                    .iter()
                    .find(|candidate| candidate.id == local)
                    .and_then(|candidate| candidate.value_type)
            })
    }

    fn unsupported_expression(&mut self, expression: &Expression)
    {
        self.report(
            DiagnosticCode::UnsupportedExpression,
            "HIR expression cannot lower to MIR yet",
            expression_span(expression),
        );
    }

    fn current_block_mut<'a>(&self, lowered: &'a mut mir::Function) -> &'a mut mir::BasicBlock
    {
        lowered
            .blocks
            .iter_mut()
            .find(|block| block.id == self.current_block)
            .expect("HIR lowering current MIR block must exist")
    }

    fn append_block(&mut self, span: Span, lowered: &mut mir::Function) -> mir::BlockId
    {
        let id = mir::BlockId(u32::try_from(lowered.blocks.len()).expect("MIR block count must fit u32"));
        lowered.blocks.push(mir::BasicBlock {
            id,
            statements: Vec::new(),
            terminator: None,
            span,
        });
        id
    }

    fn switch_to(&mut self, block: mir::BlockId)
    {
        self.current_block = block;
    }

    fn set_terminator(&mut self, terminator: mir::Terminator, span: Span, lowered: &mut mir::Function)
    {
        let block = self.current_block_mut(lowered);
        if block.terminator.is_none()
        {
            block.terminator = Some(terminator);
            block.span = span;
        }
    }
}

#[must_use]
pub const fn primitive_to_xlil(primitive: PrimitiveType) -> Option<XlilType>
{
    let kind = match primitive
    {
        PrimitiveType::Bool => TypeKind::Bool,
        PrimitiveType::Byte => TypeKind::U8,
        PrimitiveType::SByte => TypeKind::I8,
        PrimitiveType::UShort => TypeKind::U16,
        PrimitiveType::Char => TypeKind::U32,
        PrimitiveType::Short => TypeKind::I16,
        PrimitiveType::Long => TypeKind::I32,
        PrimitiveType::Int => TypeKind::I64,
        PrimitiveType::Integer => TypeKind::I128,
        PrimitiveType::ULong => TypeKind::U32,
        PrimitiveType::UInt => TypeKind::U64,
        PrimitiveType::UInteger => TypeKind::U128,
        PrimitiveType::SFloat => TypeKind::F16,
        PrimitiveType::LFloat => TypeKind::F32,
        PrimitiveType::Float => TypeKind::F64,
        PrimitiveType::Double => TypeKind::F128,
        PrimitiveType::Str => TypeKind::Str,
        PrimitiveType::String => TypeKind::String,
    };
    Some(XlilType {
        kind,
        registry_id: 0,
    })
}

const fn expression_span(expression: &Expression) -> Span
{
    match expression
    {
        Expression::Literal {
            span, ..
        } |
        Expression::Local {
            span, ..
        } |
        Expression::Object {
            span, ..
        } |
        Expression::EnumData {
            span, ..
        } |
        Expression::Array {
            span, ..
        } |
        Expression::Set {
            span, ..
        } |
        Expression::Map {
            span, ..
        } |
        Expression::Tuple {
            span, ..
        } |
        Expression::TupleElement {
            span, ..
        } |
        Expression::Index {
            span, ..
        } |
        Expression::ArrayLength {
            span, ..
        } |
        Expression::Assign {
            span, ..
        } |
        Expression::AssignField {
            span, ..
        } |
        Expression::Update {
            span, ..
        } |
        Expression::Binary {
            span, ..
        } |
        Expression::Unary {
            span, ..
        } |
        Expression::OptionalUnwrap {
            span, ..
        } |
        Expression::OptionalCoalesceAssign {
            span, ..
        } |
        Expression::OptionalMember {
            span, ..
        } |
        Expression::ResultPropagation {
            span, ..
        } => *span,
        Expression::Field {
            path,
        } => path.span,
        Expression::Member {
            span, ..
        } => *span,
        Expression::Call {
            span, ..
        } |
        Expression::If {
            span, ..
        } |
        Expression::Match {
            span, ..
        } => *span,
    }
}
#[path = "binary_operations.rs"]
mod binary_operations;
#[path = "binary_types.rs"]
mod binary_types;
#[path = "call_lowering.rs"]
mod call_lowering;
#[path = "collection.rs"]
mod collection;
#[cfg(test)]
#[path = "collection_tests.rs"]
mod collection_tests;
#[path = "control_flow.rs"]
mod control_flow;
#[cfg(test)]
#[path = "control_flow_tests.rs"]
mod control_flow_tests;
#[cfg(test)]
#[path = "desugar_tests.rs"]
mod desugar_tests;
#[path = "desugared.rs"]
mod desugared;
#[path = "function_lowering.rs"]
mod function_lowering;
#[path = "integer_literal.rs"]
mod integer_literal;
#[cfg(test)]
#[path = "match_tests.rs"]
mod match_tests;
#[path = "short_circuit.rs"]
mod short_circuit;
#[cfg(test)]
#[path = "short_circuit_tests.rs"]
mod short_circuit_tests;
#[path = "tuple.rs"]
mod tuple;
#[cfg(test)]
#[path = "tuple_tests.rs"]
mod tuple_tests;
#[path = "update.rs"]
mod update;
#[path = "value_lowering.rs"]
mod value_lowering;
#[cfg(test)]
#[path = "value_tests.rs"]
mod value_tests;

#[cfg(test)]
#[path = "tests.rs"]
mod tests;
