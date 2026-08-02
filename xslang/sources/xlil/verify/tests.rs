#[cfg(test)]
mod tests
{
    use super::*;
    use crate::xlil::{Block, BlockId, Function, Instruction, Module, Terminator, Type, Value, ValueId};

    fn valid_module() -> Module
    {
        let mut module = Module::new("App");
        let mut function = Function::definition("xs$App$Main", Type::VOID, vec![]);
        let block = function.append_block("entry");
        assert!(function.set_return(block, None));
        module.add_function(function);
        module
    }

    #[test]
    fn accepts_valid_void_function()
    {
        assert!(verify_module(&valid_module()).is_empty());
    }

    #[test]
    fn rejects_duplicate_function_names()
    {
        let mut module = Module::new("App");
        module.add_function(Function::declaration("same", Type::VOID, vec![]));
        module.add_function(Function::declaration("same", Type::VOID, vec![]));

        let diagnostics = verify_module(&module);

        assert!(
            diagnostics
                .iter()
                .any(|diagnostic| diagnostic.code == DiagnosticCode::DuplicateFunctionName)
        );
    }

    #[test]
    fn rejects_definition_without_blocks()
    {
        let mut module = Module::new("App");
        module.add_function(Function::definition("empty", Type::VOID, vec![]));

        let diagnostics = verify_module(&module);

        assert_eq!(diagnostics[0].code, DiagnosticCode::DefinitionHasNoBlocks);
    }

    #[test]
    fn rejects_missing_block_terminator()
    {
        let mut module = Module::new("App");
        let mut function = Function::definition("bad", Type::VOID, vec![]);
        function.append_block("entry");
        module.add_function(function);

        let diagnostics = verify_module(&module);

        assert_eq!(diagnostics[0].code, DiagnosticCode::MissingTerminator);
    }

    #[test]
    fn rejects_unknown_return_value()
    {
        let mut module = Module::new("App");
        let mut function = Function::definition("bad", Type::I64, vec![]);
        function.blocks.push(Block {
            id: BlockId(0),
            label: "entry".to_string(),
            instructions: vec![],
            terminator: Some(Terminator::Return(Some(ValueId(99)))),
        });
        module.add_function(function);

        let diagnostics = verify_module(&module);

        assert_eq!(diagnostics[0].code, DiagnosticCode::ReturnValueUnknown);
    }

    #[test]
    fn rejects_instruction_result_without_value_record()
    {
        let mut module = Module::new("App");
        let mut function = Function::definition("bad", Type::VOID, vec![]);
        function.blocks.push(Block {
            id: BlockId(0),
            label: "entry".to_string(),
            instructions: vec![Instruction::ConstI64 {
                result: ValueId(0),
                value: 42,
            }],
            terminator: Some(Terminator::Return(None)),
        });
        module.add_function(function);

        let diagnostics = verify_module(&module);

        assert_eq!(diagnostics[0].code, DiagnosticCode::InstructionResultUnknown);
    }

    #[test]
    fn accepts_add_i64_instruction()
    {
        let mut module = Module::new("App");
        let mut function = Function::definition("add", Type::I64, vec![]);
        let block = function.append_block("entry");
        let left = function.add_const_i64(block, 2).expect("left const should be added");
        let right = function.add_const_i64(block, 3).expect("right const should be added");
        let result = function.add_i64(block, left, right).expect("add should be added");
        assert!(function.set_return(block, Some(result)));
        module.add_function(function);

        assert!(verify_module(&module).is_empty());
    }

    #[test]
    fn accepts_sub_i64_instruction()
    {
        let mut module = Module::new("App");
        let mut function = Function::definition("sub", Type::I64, vec![]);
        let block = function.append_block("entry");
        let left = function.add_const_i64(block, 8).expect("left const should be added");
        let right = function.add_const_i64(block, 3).expect("right const should be added");
        let result = function.sub_i64(block, left, right).expect("sub should be added");
        assert!(function.set_return(block, Some(result)));
        module.add_function(function);

        assert!(verify_module(&module).is_empty());
    }

    #[test]
    fn accepts_mul_i64_instruction()
    {
        let mut module = Module::new("App");
        let mut function = Function::definition("mul", Type::I64, vec![]);
        let block = function.append_block("entry");
        let left = function.add_const_i64(block, 6).expect("left const should be added");
        let right = function.add_const_i64(block, 7).expect("right const should be added");
        let result = function.mul_i64(block, left, right).expect("mul should be added");
        assert!(function.set_return(block, Some(result)));
        module.add_function(function);

        assert!(verify_module(&module).is_empty());
    }

    #[test]
    fn accepts_eq_i64_instruction()
    {
        let mut module = Module::new("App");
        let mut function = Function::definition("eq", Type::BOOL, vec![]);
        let block = function.append_block("entry");
        let left = function.add_const_i64(block, 7).expect("left const should be added");
        let right = function.add_const_i64(block, 7).expect("right const should be added");
        let result = function.eq_i64(block, left, right).expect("eq should be added");
        assert!(function.set_return(block, Some(result)));
        module.add_function(function);

        assert!(verify_module(&module).is_empty());
    }

    #[test]
    fn accepts_i32_instruction_family()
    {
        let mut module = Module::new("App");
        let mut function = Function::definition("i32_ops", crate::xlil::Type::BOOL, vec![]);
        let block = function.append_block("entry");
        let left = function.add_const_i32(block, 2).expect("left const should be added");
        let right = function.add_const_i32(block, 3).expect("right const should be added");
        assert!(function.add_i32(block, left, right).is_some());
        assert!(function.sub_i32(block, left, right).is_some());
        assert!(function.mul_i32(block, left, right).is_some());
        assert!(function.eq_i32(block, left, right).is_some());
        assert!(function.lt_i32(block, left, right).is_some());
        assert!(function.le_i32(block, left, right).is_some());
        assert!(function.gt_i32(block, left, right).is_some());
        let result = function.ge_i32(block, left, right);
        assert!(function.set_return(block, result));
        module.add_function(function);

        assert!(verify_module(&module).is_empty());
    }

    #[test]
    fn accepts_forward_call_with_matching_signature()
    {
        let mut module = Module::new("App");
        let mut caller = Function::definition("Caller", Type::I64, vec![]);
        let block = caller.append_block("entry");
        let argument = caller.add_const_i64(block, 7).expect("argument should be created");
        let result = caller
            .add_call(block, "Callee", vec![argument], Type::I64)
            .expect("call should be created");
        assert!(caller.set_return(block, result));
        module.add_function(caller);
        module.add_function(Function::declaration("Callee", Type::I64, vec![Type::I64]));

        assert!(verify_module(&module).is_empty());
    }

    #[test]
    fn rejects_unknown_call_target()
    {
        let mut module = Module::new("App");
        let mut caller = Function::definition("Caller", Type::VOID, vec![]);
        let block = caller.append_block("entry");
        assert!(caller.add_call(block, "Missing", vec![], Type::VOID).is_some());
        assert!(caller.set_return(block, None));
        module.add_function(caller);

        let diagnostics = verify_module(&module);

        assert!(
            diagnostics
                .iter()
                .any(|diagnostic| diagnostic.code == DiagnosticCode::CallTargetUnknown)
        );
    }

    #[test]
    fn rejects_call_argument_and_return_type_mismatches()
    {
        let mut module = Module::new("App");
        let mut caller = Function::definition("Caller", Type::VOID, vec![]);
        let block = caller.append_block("entry");
        let argument = caller.add_const_bool(block, true).expect("argument should be created");
        assert!(caller.add_call(block, "Callee", vec![argument], Type::BOOL).is_some());
        assert!(caller.set_return(block, None));
        module.add_function(caller);
        module.add_function(Function::declaration("Callee", Type::I64, vec![Type::I64]));

        let diagnostics = verify_module(&module);

        assert!(
            diagnostics
                .iter()
                .any(|diagnostic| diagnostic.code == DiagnosticCode::CallArgumentTypeMismatch)
        );
        assert!(
            diagnostics
                .iter()
                .any(|diagnostic| diagnostic.code == DiagnosticCode::CallResultTypeMismatch)
        );
    }

    #[test]
    fn rejects_void_call_with_result()
    {
        let mut module = Module::new("App");
        let mut caller = Function::definition("Caller", Type::I64, vec![]);
        let block = caller.append_block("entry");
        let result = caller
            .add_call(block, "Sink", vec![], Type::I64)
            .expect("call should be created");
        assert!(caller.set_return(block, result));
        module.add_function(caller);
        module.add_function(Function::declaration("Sink", Type::VOID, vec![]));

        let diagnostics = verify_module(&module);

        assert!(
            diagnostics
                .iter()
                .any(|diagnostic| diagnostic.code == DiagnosticCode::CallVoidResultMismatch)
        );
    }

    #[test]
    fn accepts_branch_if_with_bool_condition()
    {
        let mut module = Module::new("App");
        let mut function = Function::definition("branch_if", Type::VOID, vec![]);
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

        assert!(verify_module(&module).is_empty());
    }

    #[test]
    fn accepts_panic_terminator()
    {
        let mut module = Module::new("App");
        let mut function = Function::definition("panic", Type::VOID, vec![]);
        let entry = function.append_block("entry");
        assert!(function.set_panic(entry));
        module.add_function(function);

        assert!(verify_module(&module).is_empty());
    }

    #[test]
    fn rejects_branch_if_non_bool_and_unknown_targets()
    {
        let mut module = Module::new("App");
        let mut function = Function::definition("bad_branch_if", Type::VOID, vec![]);
        function.values.push(Value {
            id: ValueId(0),
            value_type: Type::I64,
        });
        function.blocks.push(Block {
            id: BlockId(0),
            label: "entry".to_string(),
            instructions: vec![],
            terminator: Some(Terminator::BranchIf {
                condition: ValueId(0),
                then_block: BlockId(1),
                else_block: BlockId(2),
            }),
        });
        module.add_function(function);

        let diagnostics = verify_module(&module);

        assert_eq!(diagnostics[0].code, DiagnosticCode::InstructionResultUnknown);
        assert_eq!(diagnostics[1].code, DiagnosticCode::BranchTargetUnknown);
        assert_eq!(diagnostics[2].code, DiagnosticCode::BranchTargetUnknown);
    }

    #[test]
    fn rejects_return_type_mismatch()
    {
        let mut module = Module::new("App");
        let mut function = Function::definition("bad", Type::VOID, vec![]);
        function.values.push(Value {
            id: ValueId(0),
            value_type: Type::I64,
        });
        function.blocks.push(Block {
            id: BlockId(0),
            label: "entry".to_string(),
            instructions: vec![],
            terminator: Some(Terminator::Return(Some(ValueId(0)))),
        });
        module.add_function(function);

        let diagnostics = verify_module(&module);

        assert_eq!(diagnostics[0].code, DiagnosticCode::VoidReturnValue);
    }

    #[test]
    fn rejects_unknown_branch_target()
    {
        let mut module = Module::new("App");
        let mut function = Function::definition("bad", Type::VOID, vec![]);
        function.blocks.push(Block {
            id: BlockId(0),
            label: "entry".to_string(),
            instructions: vec![],
            terminator: Some(Terminator::Branch(BlockId(99))),
        });
        module.add_function(function);

        let diagnostics = verify_module(&module);

        assert_eq!(diagnostics[0].code, DiagnosticCode::BranchTargetUnknown);
    }
}
