/*
 * SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
 * SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.0
 */

use std::fmt::{self, Write};

use crate::xlil::{
    Block, Function, Instruction, Module, SUPPORTED_XLIL_VERSION, Terminator, ValueId, type_name, type_text,
};

/// Writes one module in canonical human-readable XLIL v0 form.
pub fn write_module(module: &Module, output: &mut impl Write) -> fmt::Result
{
    writeln!(output, ".xlil version {SUPPORTED_XLIL_VERSION}")?;
    writeln!(output, ".xlil module {}", module.name)?;
    for aggregate in &module.aggregate_types
    {
        write!(output, ".type %t{} {} : (", aggregate.id, aggregate.name)?;
        for (index, field) in aggregate.fields.iter().enumerate()
        {
            if index != 0
            {
                write!(output, ", ")?;
            }
            write!(output, "{}", type_text(*field))?;
        }
        writeln!(output, ")")?;
    }
    for array in &module.array_types
    {
        match array.length
        {
            Some(length) => writeln!(
                output,
                ".array %a{} : {} x {}",
                array.id,
                type_text(array.element_type),
                length
            )?,
            None => writeln!(output, ".array %a{} : {}", array.id, type_text(array.element_type))?,
        }
    }
    for function in &module.functions
    {
        write_function(function, output)?;
    }
    Ok(())
}

/// Returns canonical human-readable XLIL v0 text for `module`.
#[must_use]
pub fn module_to_string(module: &Module) -> String
{
    let mut output = String::new();
    write_module(module, &mut output).expect("writing XLIL to String cannot fail");
    output
}

fn write_function(function: &Function, output: &mut impl Write) -> fmt::Result
{
    if function.is_definition
    {
        write!(output, ".func ")?;
    }
    else
    {
        write!(output, ".extern ")?;
    }
    write_signature(function, output)?;
    if !function.is_definition
    {
        return Ok(());
    }
    for (index, parameter) in function.parameters.iter().enumerate()
    {
        writeln!(output, ".param %r{}:{}", index, type_text(*parameter))?;
    }
    for slot in &function.slots
    {
        writeln!(output, ".slot %s{}:{}", slot.id.0, type_text(slot.value_type))?;
    }
    for block in &function.blocks
    {
        write_block(function, block, output)?;
    }
    writeln!(output, ".end")
}

fn write_signature(function: &Function, output: &mut impl Write) -> fmt::Result
{
    write!(output, "{} : (", function.name)?;
    for (index, parameter) in function.parameters.iter().enumerate()
    {
        if index != 0
        {
            write!(output, ", ")?;
        }
        write!(output, "{}", type_text(*parameter))?;
    }
    writeln!(output, ") -> {}", type_text(function.return_type))
}

fn write_block(function: &Function, block: &Block, output: &mut impl Write) -> fmt::Result
{
    writeln!(output, "bb{}.{}:", block.id.0, block.label)?;
    for instruction in &block.instructions
    {
        write_instruction(function, instruction, output)?;
    }
    match block.terminator
    {
        Some(Terminator::Return(value)) => write_return(value, output),
        Some(Terminator::Branch(target)) => writeln!(output, "  br bb{}", target.0),
        Some(Terminator::BranchIf {
            condition,
            then_block,
            else_block,
        }) => writeln!(
            output,
            "  br_if %r{}, bb{}, bb{}",
            condition.0, then_block.0, else_block.0
        ),
        Some(Terminator::Panic) => writeln!(output, "  panic"),
        None => writeln!(output, "  .missing_terminator"),
    }
}

fn write_instruction(function: &Function, instruction: &Instruction, output: &mut impl Write) -> fmt::Result
{
    match *instruction
    {
        Instruction::ConstI64 {
            result,
            value,
        } => writeln!(output, "  %r{}:i64 = const.i64 {}", result.0, value),
        Instruction::ConstI32 {
            result,
            value,
        } => writeln!(output, "  %r{}:i32 = const.i32 {}", result.0, value),
        Instruction::ConstU16 {
            result,
            value,
        } => writeln!(output, "  %r{}:u16 = const.u16 0x{value:04x}", result.0),
        Instruction::ConstInteger {
            result,
            value,
        } => writeln!(
            output,
            "  %r{}:{} = const.{} 0x{:0width$x}",
            result.0,
            type_name(value.value_type),
            type_name(value.value_type),
            value.bits,
            width = value.hexadecimal_digits()
        ),
        Instruction::ConstF32 {
            result,
            bits,
        } => writeln!(output, "  %r{}:f32 = const.f32 0x{bits:08x}", result.0),
        Instruction::ConstF64 {
            result,
            bits,
        } => writeln!(output, "  %r{}:f64 = const.f64 0x{bits:016x}", result.0),
        Instruction::ConstStr {
            result,
            encoding,
            ref units,
        } => writeln!(
            output,
            "  %r{}:str = const.str {}",
            result.0,
            crate::text::format_encoded(encoding, units)
        ),
        Instruction::ConstBool {
            result,
            value,
        } => writeln!(output, "  %r{}:bool = const.bool {}", result.0, value),
        Instruction::BinaryInteger {
            operation,
            value_type,
            result,
            left,
            right,
        } => writeln!(
            output,
            "  %r{}:{} = {}.{} %r{}, %r{}",
            result.0,
            if operation.is_comparison()
            {
                "bool"
            }
            else
            {
                type_name(value_type)
            },
            operation.text_stem(),
            type_name(value_type),
            left.0,
            right.0
        ),
        Instruction::BinaryFloat {
            operation,
            value_type,
            result,
            left,
            right,
        } => writeln!(
            output,
            "  %r{}:{} = {}.{} %r{}, %r{}",
            result.0,
            type_name(value_type),
            operation.text_stem(),
            type_name(value_type),
            left.0,
            right.0
        ),
        Instruction::CompareFloat {
            operation,
            value_type,
            result,
            left,
            right,
        } => writeln!(
            output,
            "  %r{}:bool = {}.{} %r{}, %r{}",
            result.0,
            operation.text_stem(),
            type_name(value_type),
            left.0,
            right.0
        ),
        Instruction::CompareStr {
            operation,
            result,
            left,
            right,
        } => writeln!(
            output,
            "  %r{}:bool = {}.str %r{}, %r{}",
            result.0,
            operation.text_stem(),
            left.0,
            right.0
        ),
        Instruction::AddI64 {
            result,
            left,
            right,
        } => writeln!(output, "  %r{}:i64 = add.i64 %r{}, %r{}", result.0, left.0, right.0),
        Instruction::SubI64 {
            result,
            left,
            right,
        } => writeln!(output, "  %r{}:i64 = sub.i64 %r{}, %r{}", result.0, left.0, right.0),
        Instruction::MulI64 {
            result,
            left,
            right,
        } => writeln!(output, "  %r{}:i64 = mul.i64 %r{}, %r{}", result.0, left.0, right.0),
        Instruction::EqI64 {
            result,
            left,
            right,
        } => writeln!(output, "  %r{}:bool = eq.i64 %r{}, %r{}", result.0, left.0, right.0),
        Instruction::BinaryI64 {
            operation,
            result,
            left,
            right,
        } => writeln!(
            output,
            "  %r{}:i64 = {} %r{}, %r{}",
            result.0,
            operation.text_name(),
            left.0,
            right.0
        ),
        Instruction::CompareI64 {
            operation,
            result,
            left,
            right,
        } => writeln!(
            output,
            "  %r{}:bool = {} %r{}, %r{}",
            result.0,
            operation.text_name(),
            left.0,
            right.0
        ),
        Instruction::AddI32 {
            result,
            left,
            right,
        } => writeln!(output, "  %r{}:i32 = add.i32 %r{}, %r{}", result.0, left.0, right.0),
        Instruction::SubI32 {
            result,
            left,
            right,
        } => writeln!(output, "  %r{}:i32 = sub.i32 %r{}, %r{}", result.0, left.0, right.0),
        Instruction::MulI32 {
            result,
            left,
            right,
        } => writeln!(output, "  %r{}:i32 = mul.i32 %r{}, %r{}", result.0, left.0, right.0),
        Instruction::BinaryI32 {
            operation,
            result,
            left,
            right,
        } => writeln!(
            output,
            "  %r{}:i32 = {} %r{}, %r{}",
            result.0,
            operation.text_name(),
            left.0,
            right.0
        ),
        Instruction::EqI32 {
            result,
            left,
            right,
        } => writeln!(output, "  %r{}:bool = eq.i32 %r{}, %r{}", result.0, left.0, right.0),
        Instruction::LtI32 {
            result,
            left,
            right,
        } => writeln!(output, "  %r{}:bool = lt.i32 %r{}, %r{}", result.0, left.0, right.0),
        Instruction::LeI32 {
            result,
            left,
            right,
        } => writeln!(output, "  %r{}:bool = le.i32 %r{}, %r{}", result.0, left.0, right.0),
        Instruction::GtI32 {
            result,
            left,
            right,
        } => writeln!(output, "  %r{}:bool = gt.i32 %r{}, %r{}", result.0, left.0, right.0),
        Instruction::GeI32 {
            result,
            left,
            right,
        } => writeln!(output, "  %r{}:bool = ge.i32 %r{}, %r{}", result.0, left.0, right.0),
        Instruction::NotBool {
            result,
            operand,
        } => writeln!(output, "  %r{}:bool = not.bool %r{}", result.0, operand.0),
        Instruction::Call {
            result,
            ref function,
            ref arguments,
            return_type,
        } =>
        {
            if let Some(result) = result
            {
                write!(output, "  %r{}:{} = ", result.0, type_text(return_type))?;
            }
            else
            {
                write!(output, "  ")?;
            }
            write!(output, "call {function}(")?;
            for (index, argument) in arguments.iter().enumerate()
            {
                if index != 0
                {
                    write!(output, ", ")?;
                }
                write!(output, "%r{}", argument.0)?;
            }
            writeln!(output, ")")
        }
        Instruction::Aggregate {
            result,
            value_type,
            ref fields,
        } =>
        {
            let instruction = if value_type.kind == crate::xlil::TypeKind::Array
            {
                "array"
            }
            else
            {
                "aggregate"
            };
            write!(output, "  %r{}:{} = {instruction} ", result.0, type_text(value_type))?;
            for (index, field) in fields.iter().enumerate()
            {
                if index != 0
                {
                    write!(output, ", ")?;
                }
                write!(output, "%r{}", field.0)?;
            }
            writeln!(output)
        }
        Instruction::Extract {
            result,
            aggregate,
            field,
        } =>
        {
            let instruction = if function.values[aggregate.0 as usize].value_type.kind == crate::xlil::TypeKind::Array
            {
                "extract.array"
            }
            else
            {
                "extract"
            };
            writeln!(
                output,
                "  %r{}:{} = {instruction} %r{}, {}",
                result.0,
                type_text(function.values[result.0 as usize].value_type),
                aggregate.0,
                field
            )
        }
        Instruction::ArrayGet {
            result,
            array,
            index,
        } => writeln!(
            output,
            "  %r{}:{} = array.get %r{}, %r{}",
            result.0,
            type_text(function.values[result.0 as usize].value_type),
            array.0,
            index.0
        ),
        Instruction::ArraySet {
            result,
            array,
            index,
            value,
        } => writeln!(
            output,
            "  %r{}:{} = array.set %r{}, %r{}, %r{}",
            result.0,
            type_text(function.values[result.0 as usize].value_type),
            array.0,
            index.0,
            value.0
        ),
        Instruction::ArrayLength {
            result,
            array,
        } => writeln!(output, "  %r{}:i64 = len.array %r{}", result.0, array.0),
        Instruction::Load {
            result,
            slot,
        } => writeln!(
            output,
            "  %r{}:{} = load %s{}",
            result.0,
            type_text(function.values[result.0 as usize].value_type),
            slot.0
        ),
        Instruction::Store {
            slot,
            value,
        } => writeln!(output, "  store %r{}, %s{}", value.0, slot.0),
    }
}

fn write_return(value: Option<ValueId>, output: &mut impl Write) -> fmt::Result
{
    match value
    {
        Some(value) => writeln!(output, "  ret %r{}", value.0),
        None => writeln!(output, "  ret"),
    }
}

#[cfg(test)]
mod tests
{
    use super::*;
    use crate::xlil::{Function, Module, Type};

    #[test]
    fn writes_function_declaration()
    {
        let mut module = Module::new("App");
        module.add_function(Function::declaration("xs$App$External", Type::I64, vec![Type::I64]));

        assert_eq!(
            module_to_string(&module),
            ".xlil version 1\n.xlil module App\n.extern xs$App$External : (i64) -> i64\n"
        );
    }

    #[test]
    fn writes_void_function_definition()
    {
        let mut module = Module::new("App");
        let mut function = Function::definition("xs$App$Main", Type::VOID, vec![]);
        let entry = function.append_block("entry");
        assert!(function.set_return(entry, None));
        module.add_function(function);

        assert_eq!(
            module_to_string(&module),
            ".xlil version 1\n.xlil module App\n.func xs$App$Main : () -> void\nbb0.entry:\n  ret\n.end\n"
        );
    }

    #[test]
    fn writes_const_i64_and_return_value()
    {
        let mut module = Module::new("App");
        let mut function = Function::definition("xs$App$Value", Type::I64, vec![]);
        let entry = function.append_block("entry");
        let value = function.add_const_i64(entry, 42);
        assert!(function.set_return(entry, value));
        module.add_function(function);

        assert_eq!(
            module_to_string(&module),
            ".xlil version 1\n.xlil module App\n.func xs$App$Value : () -> i64\nbb0.entry:\n  %r0:i64 = const.i64 \
             42\n  ret %r0\n.end\n"
        );
    }

    #[test]
    fn writes_const_i32_and_return_value()
    {
        let mut module = Module::new("App");
        let mut function = Function::definition("main", Type::I32, vec![]);
        let entry = function.append_block("entry");
        let value = function.add_const_i32(entry, 0);
        assert!(function.set_return(entry, value));
        module.add_function(function);

        assert_eq!(
            module_to_string(&module),
            ".xlil version 1\n.xlil module App\n.func main : () -> i32\nbb0.entry:\n  %r0:i32 = const.i32 0\n  ret \
             %r0\n.end\n"
        );
    }

    #[test]
    fn writes_branch_terminator()
    {
        let mut module = Module::new("App");
        let mut function = Function::definition("xs$App$Branch", Type::VOID, vec![]);
        let entry = function.append_block("entry");
        let exit = function.append_block("exit");
        assert!(function.set_branch(entry, exit));
        assert!(function.set_return(exit, None));
        module.add_function(function);

        assert_eq!(
            module_to_string(&module),
            ".xlil version 1\n.xlil module App\n.func xs$App$Branch : () -> void\nbb0.entry:\n  br bb1\nbb1.exit:\n  \
             ret\n.end\n"
        );
    }

    #[test]
    fn writes_branch_if_terminator()
    {
        let mut module = Module::new("App");
        let mut function = Function::definition("xs$App$BranchIf", Type::VOID, vec![]);
        let entry = function.append_block("entry");
        let then_block = function.append_block("then");
        let else_block = function.append_block("else");
        let condition = function
            .add_const_bool(entry, true)
            .expect("bool const should be added");
        assert!(function.set_branch_if(entry, condition, then_block, else_block));
        assert!(function.set_return(then_block, None));
        assert!(function.set_return(else_block, None));
        module.add_function(function);

        assert_eq!(
            module_to_string(&module),
            ".xlil version 1\n.xlil module App\n.func xs$App$BranchIf : () -> void\nbb0.entry:\n  %r0:bool = \
             const.bool true\n  br_if %r0, bb1, bb2\nbb1.then:\n  ret\nbb2.else:\n  ret\n.end\n"
        );
    }

    #[test]
    fn writes_panic_terminator()
    {
        let mut module = Module::new("App");
        let mut function = Function::definition("xs$App$Panic", Type::VOID, vec![]);
        let entry = function.append_block("entry");
        assert!(function.set_panic(entry));
        module.add_function(function);

        assert_eq!(
            module_to_string(&module),
            ".xlil version 1\n.xlil module App\n.func xs$App$Panic : () -> void\nbb0.entry:\n  panic\n.end\n"
        );
    }

    #[test]
    fn writes_call_instruction()
    {
        let mut module = Module::new("App");
        let mut function = Function::definition("xs$App$Call", Type::I64, vec![]);
        let entry = function.append_block("entry");
        let argument = function.add_const_i64(entry, 7).expect("const should be added");
        let result = function
            .add_call(entry, "xs$App$Callee", vec![argument], Type::I64)
            .expect("call should be added");
        assert!(function.set_return(entry, result));
        module.add_function(function);

        assert_eq!(
            module_to_string(&module),
            ".xlil version 1\n.xlil module App\n.func xs$App$Call : () -> i64\nbb0.entry:\n  %r0:i64 = const.i64 7\n  \
             %r1:i64 = call xs$App$Callee(%r0)\n  ret %r1\n.end\n"
        );
    }

    #[test]
    fn writes_explicit_parameter_records()
    {
        let mut module = Module::new("App");
        let mut function = Function::definition("Identity", Type::I64, vec![Type::I64]);
        let entry = function.append_block("entry");
        assert!(function.set_return(entry, function.parameter_value(0)));
        module.add_function(function);

        assert_eq!(
            module_to_string(&module),
            ".xlil version 1\n.xlil module App\n.func Identity : (i64) -> i64\n.param %r0:i64\nbb0.entry:\n  ret \
             %r0\n.end\n"
        );
    }

    #[test]
    fn writes_add_i64_instruction()
    {
        let mut module = Module::new("App");
        let mut function = Function::definition("xs$App$Add", Type::I64, vec![]);
        let entry = function.append_block("entry");
        let left = function.add_const_i64(entry, 2).expect("left const should be added");
        let right = function.add_const_i64(entry, 3).expect("right const should be added");
        let result = function.add_i64(entry, left, right).expect("add should be added");
        assert!(function.set_return(entry, Some(result)));
        module.add_function(function);

        assert_eq!(
            module_to_string(&module),
            ".xlil version 1\n.xlil module App\n.func xs$App$Add : () -> i64\nbb0.entry:\n  %r0:i64 = const.i64 2\n  \
             %r1:i64 = const.i64 3\n  %r2:i64 = add.i64 %r0, %r1\n  ret %r2\n.end\n"
        );
    }

    #[test]
    fn writes_sub_i64_instruction()
    {
        let mut module = Module::new("App");
        let mut function = Function::definition("xs$App$Sub", Type::I64, vec![]);
        let entry = function.append_block("entry");
        let left = function.add_const_i64(entry, 8).expect("left const should be added");
        let right = function.add_const_i64(entry, 3).expect("right const should be added");
        let result = function.sub_i64(entry, left, right).expect("sub should be added");
        assert!(function.set_return(entry, Some(result)));
        module.add_function(function);

        assert_eq!(
            module_to_string(&module),
            ".xlil version 1\n.xlil module App\n.func xs$App$Sub : () -> i64\nbb0.entry:\n  %r0:i64 = const.i64 8\n  \
             %r1:i64 = const.i64 3\n  %r2:i64 = sub.i64 %r0, %r1\n  ret %r2\n.end\n"
        );
    }

    #[test]
    fn writes_mul_i64_instruction()
    {
        let mut module = Module::new("App");
        let mut function = Function::definition("xs$App$Mul", Type::I64, vec![]);
        let entry = function.append_block("entry");
        let left = function.add_const_i64(entry, 6).expect("left const should be added");
        let right = function.add_const_i64(entry, 7).expect("right const should be added");
        let result = function.mul_i64(entry, left, right).expect("mul should be added");
        assert!(function.set_return(entry, Some(result)));
        module.add_function(function);

        assert_eq!(
            module_to_string(&module),
            ".xlil version 1\n.xlil module App\n.func xs$App$Mul : () -> i64\nbb0.entry:\n  %r0:i64 = const.i64 6\n  \
             %r1:i64 = const.i64 7\n  %r2:i64 = mul.i64 %r0, %r1\n  ret %r2\n.end\n"
        );
    }

    #[test]
    fn writes_const_bool_and_eq_i64_instruction()
    {
        let mut module = Module::new("App");
        let mut function = Function::definition("xs$App$Eq", Type::BOOL, vec![]);
        let entry = function.append_block("entry");
        let left = function.add_const_i64(entry, 7).expect("left const should be added");
        let right = function.add_const_i64(entry, 7).expect("right const should be added");
        let result = function.eq_i64(entry, left, right).expect("eq should be added");
        assert!(function.set_return(entry, Some(result)));
        module.add_function(function);

        assert_eq!(
            module_to_string(&module),
            ".xlil version 1\n.xlil module App\n.func xs$App$Eq : () -> bool\nbb0.entry:\n  %r0:i64 = const.i64 7\n  \
             %r1:i64 = const.i64 7\n  %r2:bool = eq.i64 %r0, %r1\n  ret %r2\n.end\n"
        );
    }

    #[test]
    fn writes_const_bool_instruction()
    {
        let mut module = Module::new("App");
        let mut function = Function::definition("xs$App$Truth", Type::BOOL, vec![]);
        let entry = function.append_block("entry");
        let result = function
            .add_const_bool(entry, true)
            .expect("bool const should be added");
        assert!(function.set_return(entry, Some(result)));
        module.add_function(function);

        assert_eq!(
            module_to_string(&module),
            ".xlil version 1\n.xlil module App\n.func xs$App$Truth : () -> bool\nbb0.entry:\n  %r0:bool = const.bool \
             true\n  ret %r0\n.end\n"
        );
    }

    #[test]
    fn writes_i32_instruction_family()
    {
        let mut module = Module::new("App");
        let mut function = Function::definition("xs$App$I32", Type::BOOL, vec![]);
        let entry = function.append_block("entry");
        let left = function.add_const_i32(entry, 2).expect("left const should be added");
        let right = function.add_const_i32(entry, 3).expect("right const should be added");
        assert_eq!(function.add_i32(entry, left, right), Some(ValueId(2)));
        assert_eq!(function.sub_i32(entry, left, right), Some(ValueId(3)));
        assert_eq!(function.mul_i32(entry, left, right), Some(ValueId(4)));
        assert_eq!(function.eq_i32(entry, left, right), Some(ValueId(5)));
        assert_eq!(function.lt_i32(entry, left, right), Some(ValueId(6)));
        assert_eq!(function.le_i32(entry, left, right), Some(ValueId(7)));
        assert_eq!(function.gt_i32(entry, left, right), Some(ValueId(8)));
        let result = function.ge_i32(entry, left, right);
        assert!(function.set_return(entry, result));
        module.add_function(function);

        assert!(module_to_string(&module).contains("  %r2:i32 = add.i32 %r0, %r1\n"));
        assert!(module_to_string(&module).contains("  %r9:bool = ge.i32 %r0, %r1\n"));
    }
}
