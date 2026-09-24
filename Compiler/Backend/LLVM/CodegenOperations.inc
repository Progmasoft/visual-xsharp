[[nodiscard]] auto
CreateClosureThunk(llvm::StructType *payload,
                   const xmm::Instruction &instruction,
                   const std::vector<llvm::Type *> &captureTypes,
                   const FunctionState &target) -> llvm::Function *
{
    const auto signature
        = ::Visual::XSharp::Core::Callable::Decompose(instruction.result_type);
    if (!signature)
        return nullptr;

    auto *pointer = llvm::PointerType::get(context, 0);
    std::vector<llvm::Type *> parameterTypes{ pointer };
    parameterTypes.reserve(signature->parameters.size() + 1U);
    for (const auto &parameter : signature->parameters)
    {
        auto *lowered = types.Lower(parameter);
        if (lowered == nullptr || lowered->isVoidTy())
            return nullptr;
        parameterTypes.push_back(lowered);
    }
    auto *resultType = types.Lower(signature->result);
    if (resultType == nullptr)
        return nullptr;

    auto *thunkType
        = llvm::FunctionType::get(resultType, parameterTypes, false);
    const auto name
        = ".vxs.aarc.closure.invoke." + std::to_string(closure_index);
    auto *thunk = llvm::Function::Create(thunkType,
                                         llvm::GlobalValue::InternalLinkage,
                                         name,
                                         module);
    auto *entry = llvm::BasicBlock::Create(context, "entry", thunk);
    llvm::IRBuilder<> thunkBuilder(entry);
    auto *environment = thunk->getArg(0U);

    // The lifted target ABI starts with one parameter per capture.
    // Strong captures can be borrowed directly because invoking
    // code holds the closure alive. Weak and unowned slots must be
    // atomically upgraded and balanced around the call so
    // destruction cannot race the body.
    std::vector<llvm::Value *> arguments;
    arguments.reserve(instruction.operands.size()
                      + signature->parameters.size());
    std::vector<llvm::Value *> temporaryStrong;
    temporaryStrong.reserve(instruction.operands.size());
    for (std::size_t index = 0; index < instruction.operands.size(); ++index)
    {
        auto *slot
            = thunkBuilder.CreateStructGEP(payload,
                                           environment,
                                           static_cast<unsigned>(index + 1U));
        llvm::Value *captured = thunkBuilder.CreateLoad(captureTypes[index],
                                                        slot,
                                                        "capture.load");
        switch (instruction.capture_modes[index])
        {
            case core::CaptureMode::Strong:
                break;
            case core::CaptureMode::Weak:
                captured = thunkBuilder.CreateCall(
                    RuntimeFunction("vxs_aarc_lock_weak", pointer, { pointer }),
                    { captured },
                    "capture.locked");
                temporaryStrong.push_back(captured);
                break;
            case core::CaptureMode::Unowned:
                captured = thunkBuilder.CreateCall(
                    RuntimeFunction("vxs_aarc_load_unowned",
                                    pointer,
                                    { pointer }),
                    { captured },
                    "capture.loaded");
                temporaryStrong.push_back(captured);
                break;
        }
        arguments.push_back(captured);
    }
    for (std::size_t index = 1U; index < thunk->arg_size(); ++index)
        arguments.push_back(thunk->getArg(static_cast<unsigned>(index)));

    const auto *callName = signature->result.kind == core::Type::Kind::Unit
                               ? ""
                               : "closure.result";
    auto *result = thunkBuilder.CreateCall(target.type,
                                           target.value,
                                           arguments,
                                           callName);
    for (auto *temporary : temporaryStrong)
        thunkBuilder.CreateCall(RuntimeFunction("vxs_aarc_release_strong",
                                                llvm::Type::getVoidTy(context),
                                                { pointer }),
                                { temporary });

    if (signature->result.kind == core::Type::Kind::Unit)
        thunkBuilder.CreateRetVoid();
    else
        thunkBuilder.CreateRet(result);
    return thunk;
}

[[nodiscard]] auto
CreateClosureDestructor(llvm::StructType *payload,
                        const xmm::Instruction &instruction,
                        const std::vector<llvm::Type *> &captureTypes)
    -> llvm::Function *
{
    auto *pointer = llvm::PointerType::get(context, 0);
    auto *type = llvm::FunctionType::get(llvm::Type::getVoidTy(context),
                                         { pointer },
                                         false);
    const auto name
        = ".vxs.aarc.closure.destroy." + std::to_string(closure_index);
    auto *destructor
        = llvm::Function::Create(type,
                                 llvm::GlobalValue::InternalLinkage,
                                 name,
                                 module);
    auto *entry = llvm::BasicBlock::Create(context, "entry", destructor);
    llvm::IRBuilder<> builder(entry);
    auto *object = destructor->getArg(0);

    for (std::size_t index = 0; index < instruction.operands.size(); ++index)
    {
        const auto mode = instruction.capture_modes[index];
        const auto pointerCapture = captureTypes[index]->isPointerTy();
        if (mode == core::CaptureMode::Strong && !pointerCapture)
            continue;
        auto *slot = builder.CreateStructGEP(payload,
                                             object,
                                             static_cast<unsigned>(index + 1U));
        auto *captured = builder.CreateLoad(captureTypes[index], slot);
        if (mode == core::CaptureMode::Strong)
            builder.CreateCall(RuntimeFunction("vxs_aarc_release_strong",
                                               llvm::Type::getVoidTy(context),
                                               { pointer }),
                               { captured });
        else if (mode == core::CaptureMode::Weak)
            builder.CreateCall(RuntimeFunction("vxs_aarc_release_weak",
                                               llvm::Type::getVoidTy(context),
                                               { pointer }),
                               { captured });
        else
            builder.CreateCall(RuntimeFunction("vxs_aarc_release_unowned",
                                               llvm::Type::getVoidTy(context),
                                               { pointer }),
                               { captured });
    }
    builder.CreateRetVoid();
    return destructor;
}

[[nodiscard]] auto
LowerClosure(llvm::IRBuilder<> &builder,
             FunctionState &state,
             const xmm::Instruction &instruction) -> llvm::Value *
{
    const auto target = functions.find(instruction.closure_function);
    if (target == functions.end()
        || instruction.capture_modes.size() != instruction.operands.size())
        return nullptr;

    auto *pointer = llvm::PointerType::get(context, 0);
    std::vector<llvm::Type *> captureTypes;
    captureTypes.reserve(instruction.operands.size());
    std::vector<llvm::Type *> fields{ pointer };
    for (std::size_t index = 0; index < instruction.operands.size(); ++index)
    {
        auto *type
            = instruction.capture_modes[index] == core::CaptureMode::Strong
                  ? types.Lower(instruction.operands[index].type)
                  : pointer;
        if (type == nullptr || type->isVoidTy())
            return nullptr;
        captureTypes.push_back(type);
        fields.push_back(type);
    }
    auto *payload = llvm::StructType::create(
        context,
        fields,
        ".vxs.aarc.closure.payload." + std::to_string(closure_index));
    auto *thunk = CreateClosureThunk(payload,
                                     instruction,
                                     captureTypes,
                                     target->second);
    auto *destructor
        = CreateClosureDestructor(payload, instruction, captureTypes);
    if (thunk == nullptr || destructor == nullptr)
        return nullptr;

    // TypeMetadata mirrors the public runtime ABI. Size is
    // expressed as an LLVM constant expression so the target data
    // layout, not the host compiler, determines the closure payload
    // size.
    auto *sizeType = llvm::Triple(module.getTargetTriple()).isArch64Bit()
                         ? llvm::Type::getInt64Ty(context)
                         : llvm::Type::getInt32Ty(context);
    std::array<llvm::Type *, 7U> metadataFields{
        llvm::Type::getInt32Ty(context),
        llvm::Type::getInt32Ty(context),
        llvm::Type::getInt64Ty(context),
        sizeType,
        sizeType,
        pointer,
        pointer
    };
    auto *metadataType = llvm::StructType::get(context, metadataFields, false);
    auto *payloadSize = llvm::ConstantExpr::getSizeOf(payload);
    if (payloadSize->getType() != sizeType)
    {
        const auto sourceWidth
            = llvm::cast<llvm::IntegerType>(payloadSize->getType())
                  ->getBitWidth();
        const auto targetWidth
            = llvm::cast<llvm::IntegerType>(sizeType)->getBitWidth();
        payloadSize = llvm::ConstantExpr::getCast(
            sourceWidth < targetWidth ? llvm::Instruction::ZExt
                                      : llvm::Instruction::Trunc,
            payloadSize,
            sizeType);
    }
    std::array<llvm::Constant *, 7U> metadataValues{
        llvm::ConstantInt::get(llvm::Type::getInt32Ty(context), 2U),
        llvm::ConstantInt::get(llvm::Type::getInt32Ty(context), 0U),
        llvm::ConstantInt::get(llvm::Type::getInt64Ty(context), 0U),
        payloadSize,
        llvm::ConstantInt::get(sizeType, 16U),
        destructor,
        llvm::ConstantPointerNull::get(pointer)
    };
    auto *metadata = new llvm::GlobalVariable(
        module,
        metadataType,
        true,
        llvm::GlobalValue::PrivateLinkage,
        llvm::ConstantStruct::get(metadataType, metadataValues),
        ".vxs.aarc.closure.metadata." + std::to_string(closure_index++));
    auto *object = builder.CreateCall(
        RuntimeFunction("vxs_aarc_allocate", pointer, { pointer }),
        { metadata },
        "closure");
    builder.CreateStore(thunk, builder.CreateStructGEP(payload, object, 0U));

    for (std::size_t index = 0; index < instruction.operands.size(); ++index)
    {
        auto *captured = LoadValue(builder, state, instruction.operands[index]);
        if (captured == nullptr)
            return nullptr;
        const auto mode = instruction.capture_modes[index];
        if (mode == core::CaptureMode::Strong
            && captureTypes[index]->isPointerTy())
            captured = builder.CreateCall(
                RuntimeFunction("vxs_aarc_retain_strong", pointer, { pointer }),
                { captured },
                "capture.strong");
        else if (mode == core::CaptureMode::Weak)
            captured = builder.CreateCall(
                RuntimeFunction("vxs_aarc_make_weak", pointer, { pointer }),
                { captured },
                "capture.weak");
        else if (mode == core::CaptureMode::Unowned)
            captured = builder.CreateCall(
                RuntimeFunction("vxs_aarc_make_unowned", pointer, { pointer }),
                { captured },
                "capture.unowned");
        builder.CreateStore(
            captured,
            builder.CreateStructGEP(payload,
                                    object,
                                    static_cast<unsigned>(index + 1U)));
    }
    return object;
}

[[nodiscard]] auto
LowerInstruction(llvm::IRBuilder<> &builder,
                 FunctionState &state,
                 const xmm::Instruction &instruction) -> bool
{
    if (instruction.opcode == xmm::Opcode::Call)
    {
        auto *result = LowerCall(builder, state, instruction);
        if (result == nullptr)
            return false;
        if (instruction.has_result)
            builder.CreateStore(result,
                                state.slots.at(instruction.destination));
        return true;
    }
    if (instruction.opcode == xmm::Opcode::MakeClosure)
    {
        auto *result = LowerClosure(builder, state, instruction);
        if (result == nullptr)
            return false;
        if (instruction.has_result)
            builder.CreateStore(result,
                                state.slots.at(instruction.destination));
        else
        {
            // Closure construction retains every strong capture.  A
            // discarded source closure still performs those
            // observable ownership actions, but its temporary owner
            // must be released immediately or a dead expression
            // leaks both the environment and its captured values.
            builder.CreateCall(
                RuntimeFunction("vxs_aarc_release_strong",
                                llvm::Type::getVoidTy(context),
                                { llvm::PointerType::get(context, 0) }),
                { result });
        }
        return true;
    }
    if (instruction.opcode >= xmm::Opcode::RetainStrong
        && instruction.opcode <= xmm::Opcode::ReleaseUnowned)
        return LowerOwnership(builder, state, instruction);
    std::vector<llvm::Value *> operands;
    operands.reserve(instruction.operands.size());
    for (const auto &operand : instruction.operands)
        operands.push_back(LoadValue(builder, state, operand));
    if (std::ranges::any_of(operands, [](const llvm::Value *value) {
            return value == nullptr;
        }))
        return false;

    llvm::Value *result{};
    const auto &operandType = instruction.operands.empty()
                                  ? instruction.result_type
                                  : instruction.operands.front().type;
    const auto floating = core::is_floating(operandType);
    const auto unsignedInteger
        = core::is_unsigned_integer(operandType)
          || operandType.kind == core::Type::Kind::Character;
    switch (instruction.opcode)
    {
        case xmm::Opcode::LoadImmediate:
        case xmm::Opcode::Move:
            result = operands[0];
            break;
        case xmm::Opcode::Add:
            result = floating
                         ? builder.CreateFAdd(operands[0], operands[1], "add")
                         : builder.CreateAdd(operands[0], operands[1], "add");
            break;
        case xmm::Opcode::Subtract:
            result = floating
                         ? builder.CreateFSub(operands[0], operands[1], "sub")
                         : builder.CreateSub(operands[0], operands[1], "sub");
            break;
        case xmm::Opcode::Multiply:
            result = floating
                         ? builder.CreateFMul(operands[0], operands[1], "mul")
                         : builder.CreateMul(operands[0], operands[1], "mul");
            break;
        case xmm::Opcode::Divide:
            result = floating
                         ? builder.CreateFDiv(operands[0], operands[1], "div")
                     : unsignedInteger
                         ? builder.CreateUDiv(operands[0], operands[1], "div")
                         : builder.CreateSDiv(operands[0], operands[1], "div");
            break;
        case xmm::Opcode::FloorDivide:
            result = floating          ? LowerFloatingRoundedDiv(builder,
                                                                 operands[0],
                                                                 operands[1])
                     : unsignedInteger ? LowerUnsignedRoundedDiv(builder,
                                                                 operands[0],
                                                                 operands[1])
                                       : LowerSignedRoundedDiv(builder,
                                                               operands[0],
                                                               operands[1]);
            break;
        case xmm::Opcode::Remainder:
            result = floating
                         ? builder.CreateFRem(operands[0], operands[1], "rem")
                     : unsignedInteger
                         ? builder.CreateURem(operands[0], operands[1], "rem")
                         : builder.CreateSRem(operands[0], operands[1], "rem");
            break;
        case xmm::Opcode::Power:
            if (floating)
            {
                auto *intrinsic = llvm::Intrinsic::getOrInsertDeclaration(
                    &module,
                    llvm::Intrinsic::pow,
                    { operands[0]->getType() });
                result = builder.CreateCall(intrinsic,
                                            { operands[0], operands[1] },
                                            "power");
            }
            else
                result = builder.CreateCall(
                    IntegerPowerFunction(
                        llvm::cast<llvm::IntegerType>(operands[0]->getType())),
                    { operands[0], operands[1] },
                    "power");
            break;
        case xmm::Opcode::ShiftLeft:
            result = builder.CreateShl(operands[0], operands[1], "shift.left");
            break;
        case xmm::Opcode::ShiftRight:
            result = unsignedInteger ? builder.CreateLShr(operands[0],
                                                          operands[1],
                                                          "shift.right")
                                     : builder.CreateAShr(operands[0],
                                                          operands[1],
                                                          "shift.right");
            break;
        case xmm::Opcode::BitwiseAnd:
            result = builder.CreateAnd(operands[0], operands[1], "bitwise.and");
            break;
        case xmm::Opcode::BitwiseXor:
            result = builder.CreateXor(operands[0], operands[1], "bitwise.xor");
            break;
        case xmm::Opcode::BitwiseOr:
            result = builder.CreateOr(operands[0], operands[1], "bitwise.or");
            break;
        case xmm::Opcode::BitwiseNot:
            result = builder.CreateNot(operands[0], "bitwise.not");
            break;
        case xmm::Opcode::TypeIs:
            result = builder.CreateCall(
                RuntimeFunction("vxs_aarc_is_exact_type",
                                llvm::Type::getInt1Ty(context),
                                { llvm::PointerType::get(context, 0),
                                  llvm::Type::getInt64Ty(context) }),
                { operands[0], operands[1] },
                "type.is");
            break;
        case xmm::Opcode::CompareLess:
            result
                = floating
                      ? builder.CreateFCmpOLT(operands[0], operands[1], "less")
                  : unsignedInteger
                      ? builder.CreateICmpULT(operands[0], operands[1], "less")
                      : builder.CreateICmpSLT(operands[0], operands[1], "less");
            break;
        case xmm::Opcode::CompareLessEqual:
            result = floating          ? builder.CreateFCmpOLE(operands[0],
                                                               operands[1],
                                                               "less.equal")
                     : unsignedInteger ? builder.CreateICmpULE(operands[0],
                                                               operands[1],
                                                               "less.equal")
                                       : builder.CreateICmpSLE(operands[0],
                                                               operands[1],
                                                               "less.equal");
            break;
        case xmm::Opcode::CompareGreater:
            result = floating          ? builder.CreateFCmpOGT(operands[0],
                                                               operands[1],
                                                               "greater")
                     : unsignedInteger ? builder.CreateICmpUGT(operands[0],
                                                               operands[1],
                                                               "greater")
                                       : builder.CreateICmpSGT(operands[0],
                                                               operands[1],
                                                               "greater");
            break;
        case xmm::Opcode::CompareGreaterEqual:
            result = floating          ? builder.CreateFCmpOGE(operands[0],
                                                               operands[1],
                                                               "greater.equal")
                     : unsignedInteger ? builder.CreateICmpUGE(operands[0],
                                                               operands[1],
                                                               "greater.equal")
                                       : builder.CreateICmpSGE(operands[0],
                                                               operands[1],
                                                               "greater.equal");
            break;
        case xmm::Opcode::CompareEqual:
            result
                = floating
                      ? builder.CreateFCmpOEQ(operands[0], operands[1], "equal")
                      : builder.CreateICmpEQ(operands[0], operands[1], "equal");
            break;
        case xmm::Opcode::CompareNotEqual:
            result = floating ? builder.CreateFCmpUNE(operands[0],
                                                      operands[1],
                                                      "not.equal")
                              : builder.CreateICmpNE(operands[0],
                                                     operands[1],
                                                     "not.equal");
            break;
        case xmm::Opcode::AndBool:
            result = builder.CreateAnd(operands[0], operands[1], "logical.and");
            break;
        case xmm::Opcode::OrBool:
            result = builder.CreateOr(operands[0], operands[1], "logical.or");
            break;
        case xmm::Opcode::Negate:
            result = floating ? builder.CreateFNeg(operands[0], "negate")
                              : builder.CreateNeg(operands[0], "negate");
            break;
        case xmm::Opcode::NotBool:
            result = builder.CreateNot(operands[0], "logical.not");
            break;
        case xmm::Opcode::Call:
        case xmm::Opcode::MakeClosure:
        case xmm::Opcode::RetainStrong:
        case xmm::Opcode::ReleaseStrong:
        case xmm::Opcode::MakeWeak:
        case xmm::Opcode::LockWeak:
        case xmm::Opcode::ReleaseWeak:
        case xmm::Opcode::MakeUnowned:
        case xmm::Opcode::LoadUnowned:
        case xmm::Opcode::ReleaseUnowned:
            break;
    }
    if (instruction.has_result && result != nullptr)
        builder.CreateStore(result, state.slots.at(instruction.destination));
    return result != nullptr;
}

[[nodiscard]] auto
LowerTerminator(llvm::IRBuilder<> &builder,
                FunctionState &state,
                const xmm::Terminator &terminator) -> bool
{
    switch (terminator.kind)
    {
        case xmm::Terminator::Kind::Return:
            if (state.source->return_type.kind == core::Type::Kind::Unit)
                builder.CreateRetVoid();
            else
            {
                auto *value = LoadValue(builder, state, terminator.value);
                if (value == nullptr)
                    return false;
                builder.CreateRet(value);
            }
            return true;
        case xmm::Terminator::Kind::Branch:
        {
            auto *condition = LoadValue(builder, state, terminator.value);
            if (condition == nullptr)
                return false;
            builder.CreateCondBr(condition,
                                 state.blocks.at(terminator.true_target),
                                 state.blocks.at(terminator.false_target));
            return true;
        }
        case xmm::Terminator::Kind::Jump:
            builder.CreateBr(state.blocks.at(terminator.true_target));
            return true;
        case xmm::Terminator::Kind::Unreachable:
            builder.CreateUnreachable();
            return true;
    }
    return false;
}
