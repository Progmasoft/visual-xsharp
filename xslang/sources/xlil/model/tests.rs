/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 */

use super::super::{type_from_name, type_name};
use super::*;

#[test]
fn models_function_declaration_signature()
{
    let function = Function::declaration("xs$App$Main", Type::VOID, vec![Type::I64]);

    assert!(!function.is_definition);
    assert_eq!(function.name, "xs$App$Main");
    assert_eq!(function.parameters, vec![Type::I64]);
}

#[test]
fn definitions_allocate_parameter_values_before_instruction_results()
{
    let mut function = Function::definition("Identity", Type::I64, vec![Type::I64]);
    let block = function.append_block("entry");

    assert_eq!(function.parameter_value(0), Some(ValueId(0)));
    assert_eq!(function.add_const_i64(block, 42), Some(ValueId(1)));
}

#[test]
fn validates_return_value_type()
{
    let mut function = Function::definition("Value", Type::I64, vec![]);
    let block = function.append_block("entry");
    let value = function.add_const_i64(block, 42);

    assert!(function.set_return(block, value));
}

#[test]
fn adds_call_instruction_with_result()
{
    let mut function = Function::definition("Call", Type::I64, vec![]);
    let block = function.append_block("entry");
    let argument = function.add_const_i64(block, 7).expect("const should be added");
    let result = function
        .add_call(block, "callee", vec![argument], Type::I64)
        .expect("call should be added");

    assert_eq!(result, Some(ValueId(1)));
    assert!(function.set_return(block, result));
}

#[test]
fn models_typed_stack_slot_load_and_store()
{
    let mut function = Function::definition("Memory", Type::I32, vec![]);
    let entry = function.append_block("entry");
    let slot = function.add_slot(Type::I32).expect("slot should be added");
    let value = function.add_const_i32(entry, 7).expect("constant should be added");

    assert!(function.add_store(entry, slot, value));
    let loaded = function.add_load(entry, slot).expect("load should be added");
    assert!(function.set_return(entry, Some(loaded)));
}

#[test]
fn adds_i64_instruction_with_i64_operands()
{
    let mut function = Function::definition("Add", Type::I64, vec![]);
    let block = function.append_block("entry");
    let left = function.add_const_i64(block, 2).expect("left const should be added");
    let right = function.add_const_i64(block, 3).expect("right const should be added");
    let result = function.add_i64(block, left, right).expect("add should be added");

    assert_eq!(result, ValueId(2));
    assert!(function.set_return(block, Some(result)));
}

#[test]
fn subtracts_i64_instruction_with_i64_operands()
{
    let mut function = Function::definition("Sub", Type::I64, vec![]);
    let block = function.append_block("entry");
    let left = function.add_const_i64(block, 8).expect("left const should be added");
    let right = function.add_const_i64(block, 3).expect("right const should be added");
    let result = function.sub_i64(block, left, right).expect("sub should be added");

    assert_eq!(result, ValueId(2));
    assert!(function.set_return(block, Some(result)));
}

#[test]
fn multiplies_i64_instruction_with_i64_operands()
{
    let mut function = Function::definition("Mul", Type::I64, vec![]);
    let block = function.append_block("entry");
    let left = function.add_const_i64(block, 6).expect("left const should be added");
    let right = function.add_const_i64(block, 7).expect("right const should be added");
    let result = function.mul_i64(block, left, right).expect("mul should be added");

    assert_eq!(result, ValueId(2));
    assert!(function.set_return(block, Some(result)));
}

#[test]
fn adds_bool_const_and_i64_equality()
{
    let mut function = Function::definition("Eq", Type::BOOL, vec![]);
    let block = function.append_block("entry");
    let left = function.add_const_i64(block, 7).expect("left const should be added");
    let right = function.add_const_i64(block, 7).expect("right const should be added");
    let result = function.eq_i64(block, left, right).expect("eq should be added");

    assert_eq!(result, ValueId(2));
    assert!(function.set_return(block, Some(result)));
}

#[test]
fn adds_bool_const_instruction()
{
    let mut function = Function::definition("Truth", Type::BOOL, vec![]);
    let block = function.append_block("entry");
    let result = function
        .add_const_bool(block, true)
        .expect("bool const should be added");

    assert_eq!(result, ValueId(0));
    assert!(function.set_return(block, Some(result)));
}

#[test]
fn adds_i32_const_instruction()
{
    let mut function = Function::definition("main", Type::I32, vec![]);
    let block = function.append_block("entry");
    let result = function.add_const_i32(block, 0).expect("i32 const should be added");

    assert_eq!(result, ValueId(0));
    assert!(function.set_return(block, Some(result)));
}

#[test]
fn adds_i32_instruction_family()
{
    let mut function = Function::definition("Compare", Type::BOOL, vec![]);
    let block = function.append_block("entry");
    let left = function.add_const_i32(block, 2).expect("left const should be added");
    let right = function.add_const_i32(block, 3).expect("right const should be added");
    assert_eq!(function.add_i32(block, left, right), Some(ValueId(2)));
    assert_eq!(function.sub_i32(block, left, right), Some(ValueId(3)));
    assert_eq!(function.mul_i32(block, left, right), Some(ValueId(4)));
    assert_eq!(function.eq_i32(block, left, right), Some(ValueId(5)));
    assert_eq!(function.lt_i32(block, left, right), Some(ValueId(6)));
    assert_eq!(function.le_i32(block, left, right), Some(ValueId(7)));
    assert_eq!(function.gt_i32(block, left, right), Some(ValueId(8)));
    let result = function.ge_i32(block, left, right);

    assert_eq!(result, Some(ValueId(9)));
    assert!(function.set_return(block, result));
}

#[test]
fn sets_conditional_branch_with_bool_condition()
{
    let mut function = Function::definition("BranchIf", Type::VOID, vec![]);
    let entry = function.append_block("entry");
    let then_block = function.append_block("then");
    let else_block = function.append_block("else");
    let condition = function
        .add_const_bool(entry, true)
        .expect("bool const should be added");

    assert!(function.set_branch_if(entry, condition, then_block, else_block));
    assert_eq!(
        function.blocks[0].terminator,
        Some(Terminator::BranchIf {
            condition,
            then_block,
            else_block
        })
    );
}

#[test]
fn sets_panic_terminator()
{
    let mut function = Function::definition("Panic", Type::VOID, vec![]);
    let entry = function.append_block("entry");

    assert!(function.set_panic(entry));
    assert_eq!(function.blocks[0].terminator, Some(Terminator::Panic));
}

#[test]
fn rejects_void_return_from_non_void_function()
{
    let mut function = Function::definition("Value", Type::I64, vec![]);
    let block = function.append_block("entry");

    assert!(!function.set_return(block, None));
}

#[test]
fn sets_branch_to_existing_block()
{
    let mut function = Function::definition("Branch", Type::VOID, vec![]);
    let entry = function.append_block("entry");
    let exit = function.append_block("exit");

    assert!(function.set_branch(entry, exit));
    assert_eq!(function.blocks[0].terminator, Some(Terminator::Branch(exit)));
}

#[test]
fn reports_primitive_type_names()
{
    assert_eq!(type_name(Type::VOID), "void");
    assert_eq!(type_name(Type::I32), "i32");
    assert_eq!(type_name(Type::I64), "i64");
    assert_eq!(type_name(Type::STR), "str");
}

#[test]
fn parses_primitive_type_names()
{
    assert_eq!(type_from_name("void"), Some(Type::VOID));
    assert_eq!(type_from_name("i64"), Some(Type::I64));
    assert_eq!(type_from_name("i32"), Some(Type::I32));
    assert_eq!(type_from_name("str"), Some(Type::STR));
    assert_eq!(type_from_name("unknown"), None);
}
