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

/// Whether destroying a closure has anything to release: a strong capture
/// of an object, or a weak or unowned handle. A closure over numbers and
/// Booleans alone owns nothing.
[[nodiscard]] static auto
ClosureOwnsCaptures(const xmm::Instruction &instruction,
                    const std::vector<llvm::Type *> &captureTypes) -> bool
{
    for (std::size_t index = 0; index < instruction.operands.size(); ++index)
        if (instruction.capture_modes[index] != core::CaptureMode::Strong
            || captureTypes[index]->isPointerTy())
            return true;
    return false;
}

/// The destructor of a closure, or null for a closure that owns nothing.
/// The runtime runs no destructor for an object whose metadata names none,
/// so such a closure costs no function of its own.
[[nodiscard]] auto
CreateClosureDestructor(llvm::StructType *payload,
                        const xmm::Instruction &instruction,
                        const std::vector<llvm::Type *> &captureTypes)
    -> llvm::Function *
{
    if (!ClosureOwnsCaptures(instruction, captureTypes))
        return nullptr;
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

/// The LLVM type of the metadata record an allocation is described by; it
/// mirrors `VxsAarcTypeMetadata` of the public runtime ABI.
[[nodiscard]] auto
ObjectMetadataType() -> llvm::StructType *
{
    auto *pointer = llvm::PointerType::get(context, 0);
    auto *sizeType = llvm::Triple(module.getTargetTriple()).isArch64Bit()
                         ? llvm::Type::getInt64Ty(context)
                         : llvm::Type::getInt32Ty(context);
    std::array<llvm::Type *, 7U> fields{ llvm::Type::getInt32Ty(context),
                                         llvm::Type::getInt32Ty(context),
                                         llvm::Type::getInt64Ty(context),
                                         sizeType,
                                         sizeType,
                                         pointer,
                                         pointer };
    return llvm::StructType::get(context, fields, false);
}

/// Allocates an AARC object with the given payload and destructor.
///
/// TypeMetadata mirrors the public runtime ABI. Size is expressed as an
/// LLVM constant expression so the target data layout, not the host
/// compiler, determines the payload size. A null destructor describes an
/// object that owns nothing. A metadata record that the module already
/// holds under the given name is used again.
[[nodiscard]] auto
AllocateObject(llvm::IRBuilder<> &builder,
               llvm::StructType *payload,
               llvm::Function *destructor,
               const std::string &metadataName,
               const char *valueName) -> llvm::Value *
{
    auto *pointer = llvm::PointerType::get(context, 0);
    if (auto *known = module.getNamedGlobal(metadataName))
        return builder.CreateCall(
            RuntimeFunction("vxs_aarc_allocate", pointer, { pointer }),
            { known },
            valueName);
    auto *sizeType = llvm::Triple(module.getTargetTriple()).isArch64Bit()
                         ? llvm::Type::getInt64Ty(context)
                         : llvm::Type::getInt32Ty(context);
    auto *metadataType = ObjectMetadataType();
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
        destructor != nullptr ? static_cast<llvm::Constant *>(destructor)
                              : static_cast<llvm::Constant *>(
                                    llvm::ConstantPointerNull::get(pointer)),
        llvm::ConstantPointerNull::get(pointer)
    };
    auto *metadata = new llvm::GlobalVariable(
        module,
        metadataType,
        true,
        llvm::GlobalValue::PrivateLinkage,
        llvm::ConstantStruct::get(metadataType, metadataValues),
        metadataName);
    return builder.CreateCall(
        RuntimeFunction("vxs_aarc_allocate", pointer, { pointer }),
        { metadata },
        valueName);
}

/// Lowers a callable that remembers its result.
///
/// The value is an object of its own around the callable that computes the
/// result. Like every closure it starts with its invoke entry, so a call
/// site calls it as it calls any callable. Behind the entry it keeps
/// whether it has a result, the result, and the callable. The entry calls
/// the callable when there is no result yet, keeps what it returned, and
/// returns the kept result from then on. Every holder of the object sees
/// the same result, which is what lets a caller and the method it calls
/// share one computation.
///
/// The object owns the callable and releases it when it is destroyed. The
/// result owns nothing: the verifiers admit only Bool and numeric results.
/// A call that does not return leaves the object without a result.
///
/// Nothing in the object's code depends on which computation it holds, so a
/// module has one payload type, one entry, one destructor and one metadata
/// record for each result type, however many values it suspends.
[[nodiscard]] auto
LowerMemoize(llvm::IRBuilder<> &builder,
             FunctionState &state,
             const xmm::Instruction &instruction) -> llvm::Value *
{
    const auto signature
        = ::Visual::XSharp::Core::Callable::Decompose(instruction.result_type);
    if (!signature || !signature->parameters.empty()
        || instruction.operands.size() != 1U)
        return nullptr;
    auto *resultType = types.Lower(signature->result);
    if (resultType == nullptr || resultType->isVoidTy())
        return nullptr;
    auto *computation = LoadValue(builder, state, instruction.operands.front());
    if (computation == nullptr || !computation->getType()->isPointerTy())
        return nullptr;

    constexpr unsigned kInvoke = 0U;
    constexpr unsigned kKnown = 1U;
    constexpr unsigned kResult = 2U;
    constexpr unsigned kComputation = 3U;
    std::string index;
    {
        llvm::raw_string_ostream spelling(index);
        resultType->print(spelling);
    }
    auto *pointer = llvm::PointerType::get(context, 0);
    auto *flagType = llvm::Type::getInt8Ty(context);
    auto *payload
        = llvm::StructType::getTypeByName(context,
                                          ".vxs.aarc.memo.payload." + index);
    if (payload == nullptr)
        payload = llvm::StructType::create(
            context,
            { pointer, flagType, resultType, pointer },
            ".vxs.aarc.memo.payload." + index);

    auto *invokeType = llvm::FunctionType::get(resultType, { pointer }, false);
    auto *invoke = module.getFunction(".vxs.aarc.memo.invoke." + index);
    if (invoke == nullptr)
    {
        invoke = llvm::Function::Create(invokeType,
                                        llvm::GlobalValue::InternalLinkage,
                                        ".vxs.aarc.memo.invoke." + index,
                                        module);
        auto *entry = llvm::BasicBlock::Create(context, "entry", invoke);
        auto *compute = llvm::BasicBlock::Create(context, "compute", invoke);
        auto *done = llvm::BasicBlock::Create(context, "done", invoke);
        auto *self = invoke->getArg(0U);
        llvm::IRBuilder<> body(entry);
        auto *known
            = body.CreateLoad(flagType,
                              body.CreateStructGEP(payload, self, kKnown),
                              "memo.known");
        body.CreateCondBr(
            body.CreateICmpNE(known, llvm::ConstantInt::get(flagType, 0U)),
            done,
            compute);

        body.SetInsertPoint(compute);
        auto *held
            = body.CreateLoad(pointer,
                              body.CreateStructGEP(payload, self, kComputation),
                              "memo.computation");
        // The callable is a closure: its payload begins with its own invoke
        // entry, which takes the closure and returns the result.
        auto *target = body.CreateLoad(pointer, held, "memo.target");
        auto *computed
            = body.CreateCall(invokeType, target, { held }, "memo.computed");
        body.CreateStore(computed,
                         body.CreateStructGEP(payload, self, kResult));
        body.CreateStore(llvm::ConstantInt::get(flagType, 1U),
                         body.CreateStructGEP(payload, self, kKnown));
        body.CreateBr(done);

        body.SetInsertPoint(done);
        body.CreateRet(
            body.CreateLoad(resultType,
                            body.CreateStructGEP(payload, self, kResult),
                            "memo.result"));
    }

    auto *destructor = module.getFunction(".vxs.aarc.memo.destroy." + index);
    if (destructor == nullptr)
    {
        destructor = llvm::Function::Create(
            llvm::FunctionType::get(llvm::Type::getVoidTy(context),
                                    { pointer },
                                    false),
            llvm::GlobalValue::InternalLinkage,
            ".vxs.aarc.memo.destroy." + index,
            module);
        llvm::IRBuilder<> body(
            llvm::BasicBlock::Create(context, "entry", destructor));
        auto *held
            = body.CreateLoad(pointer,
                              body.CreateStructGEP(payload,
                                                   destructor->getArg(0U),
                                                   kComputation));
        body.CreateCall(RuntimeFunction("vxs_aarc_release_strong",
                                        llvm::Type::getVoidTy(context),
                                        { pointer }),
                        { held });
        body.CreateRetVoid();
    }

    auto *object = AllocateObject(builder,
                                  payload,
                                  destructor,
                                  ".vxs.aarc.memo.metadata." + index,
                                  "memo");
    builder.CreateStore(invoke,
                        builder.CreateStructGEP(payload, object, kInvoke));
    builder.CreateStore(llvm::ConstantInt::get(flagType, 0U),
                        builder.CreateStructGEP(payload, object, kKnown));
    builder.CreateStore(llvm::Constant::getNullValue(resultType),
                        builder.CreateStructGEP(payload, object, kResult));
    // The operand stays with its own owner; the object takes a reference
    // of its own.
    auto *owned = builder.CreateCall(
        RuntimeFunction("vxs_aarc_retain_strong", pointer, { pointer }),
        { computation },
        "memo.owned");
    builder.CreateStore(owned,
                        builder.CreateStructGEP(payload, object, kComputation));
    return object;
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
    if (thunk == nullptr)
        return nullptr;

    auto *object = AllocateObject(builder,
                                  payload,
                                  destructor,
                                  ".vxs.aarc.closure.metadata."
                                      + std::to_string(closure_index++),
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

// Values that cannot be computed.
//
// An integer quotient or remainder by zero, and a shift by an amount that is
// negative or not less than the width of the shifted value, have no value.
// A program that needs one fails: it stops there. LLVM gives the same
// instructions no meaning at all, so without a check an optimizer may remove
// the computation, or the code after it, as if the program had never needed
// the value. Each such instruction is therefore preceded by a check that
// stops the program, and the instruction itself only ever runs on operands
// it has a value for.

/// The function that stops the program when its two operands are equal or,
/// with `below` unset, when the first is not below the second as unsigned
/// numbers. It is small and always inlined, so that the check costs a
/// comparison where the operand is not known and nothing where it is.
[[nodiscard]] auto
ComputabilityCheck(llvm::IntegerType *type, bool below) -> llvm::Function *
{
    const auto name
        = std::string(below ? ".vxs.check.below.i" : ".vxs.check.differs.i")
          + std::to_string(type->getBitWidth());
    if (auto *existing = module.getFunction(name))
        return existing;
    auto *function = llvm::Function::Create(
        llvm::FunctionType::get(llvm::Type::getVoidTy(context),
                                { type, type },
                                false),
        llvm::GlobalValue::InternalLinkage,
        name,
        module);
    function->addFnAttr(llvm::Attribute::AlwaysInline);
    function->addFnAttr(llvm::Attribute::NoUnwind);
    auto *entry = llvm::BasicBlock::Create(context, "entry", function);
    auto *fails = llvm::BasicBlock::Create(context, "fails", function);
    auto *computable
        = llvm::BasicBlock::Create(context, "computable", function);
    llvm::IRBuilder<> body(entry);
    auto *holds
        = below ? body.CreateICmpULT(function->getArg(0U), function->getArg(1U))
                : body.CreateICmpNE(function->getArg(0U), function->getArg(1U));
    body.CreateCondBr(holds, computable, fails);
    body.SetInsertPoint(fails);
    body.CreateCall(
        llvm::Intrinsic::getOrInsertDeclaration(&module,
                                                llvm::Intrinsic::trap));
    body.CreateUnreachable();
    body.SetInsertPoint(computable);
    body.CreateRetVoid();
    return function;
}

/// Stops the program when an integer divisor is zero, and gives the divisor
/// the division is carried out with.
///
/// The least value of a signed type divided by minus one has a quotient one
/// above the greatest value of the type. That is a result that does not fit,
/// not a value that cannot be computed, and generated code lets every other
/// integer result that does not fit wrap. Wrapped, the quotient is the least
/// value again and the remainder is zero, which is what dividing by one
/// gives; the division is carried out with one. The processor's own
/// instruction would stop the program instead.
[[nodiscard]] auto
ComputableDivisor(llvm::IRBuilder<> &builder,
                  llvm::Value *dividend,
                  llvm::Value *divisor,
                  bool isSigned) -> llvm::Value *
{
    auto *type = llvm::cast<llvm::IntegerType>(divisor->getType());
    builder.CreateCall(ComputabilityCheck(type, false),
                       { divisor, llvm::ConstantInt::get(type, 0U) });
    if (!isSigned)
        return divisor;
    auto *least = llvm::ConstantInt::get(
        type,
        llvm::APInt::getSignedMinValue(type->getBitWidth()));
    auto *wraps = builder.CreateAnd(
        builder.CreateICmpEQ(dividend, least),
        builder.CreateICmpEQ(divisor, llvm::ConstantInt::getSigned(type, -1)),
        "quotient.wraps");
    return builder.CreateSelect(wraps,
                                llvm::ConstantInt::get(type, 1U),
                                divisor,
                                "divisor");
}

/// Stops the program when a shift amount is negative or not less than the
/// width of the shifted value.
void
RequireShiftAmount(llvm::IRBuilder<> &builder, llvm::Value *amount)
{
    auto *type = llvm::cast<llvm::IntegerType>(amount->getType());
    builder.CreateCall(
        ComputabilityCheck(type, true),
        { amount, llvm::ConstantInt::get(type, type->getBitWidth()) });
}

/// Lowers a call of a function of the runtime.
///
/// The verifiers have checked the call against its row of the catalog. An
/// argument that the row gives a family of types is widened here to the one
/// representation the runtime function is written for: a signed integer is
/// sign-extended and an unsigned one zero-extended to sixty-four bits, and a
/// floating-point number is extended to a `double`. None of these changes
/// the value. A string result arrives with a reference the caller owns; when
/// nothing takes it, it is released here. A Boolean result arrives as the
/// byte the C ABI returns and is narrowed to the one bit a Bool is here.
[[nodiscard]] auto
LowerRuntimeCall(llvm::IRBuilder<> &builder,
                 FunctionState &state,
                 const xmm::Instruction &instruction) -> bool
{
    namespace runtime = core::runtime;
    if (instruction.operands.empty()
        || instruction.operands.front().kind != xmm::Value::Kind::Immediate)
        return false;
    const auto identity
        = runtime::IdentityOf(instruction.operands.front().immediate,
                              instruction.operands.front().type);
    const auto *signature = identity ? runtime::Find(*identity) : nullptr;
    if (signature == nullptr
        || instruction.operands.size() != signature->parameters.size() + 1U)
        return false;

    auto *pointer = llvm::PointerType::get(context, 0);
    auto *word = llvm::Type::getInt64Ty(context);
    std::vector<llvm::Type *> parameterTypes;
    std::vector<llvm::Value *> arguments;
    parameterTypes.reserve(signature->parameters.size());
    arguments.reserve(signature->parameters.size());
    for (std::size_t index = 0U; index < signature->parameters.size(); ++index)
    {
        auto *value
            = LoadValue(builder, state, instruction.operands[index + 1U]);
        if (value == nullptr)
            return false;
        llvm::Type *expected = word;
        switch (signature->parameters[index])
        {
            case runtime::Parameter::Signed:
            case runtime::Parameter::Count:
                if (!value->getType()->isIntegerTy())
                    return false;
                value
                    = builder.CreateSExtOrTrunc(value, word, "runtime.signed");
                break;
            case runtime::Parameter::Unsigned:
                if (!value->getType()->isIntegerTy())
                    return false;
                value = builder.CreateZExtOrTrunc(value,
                                                  word,
                                                  "runtime.unsigned");
                break;
            case runtime::Parameter::Floating:
                expected = llvm::Type::getDoubleTy(context);
                if (!value->getType()->isFloatingPointTy())
                    return false;
                if (value->getType() != expected)
                    value = builder.CreateFPExt(value,
                                                expected,
                                                "runtime.floating");
                break;
            case runtime::Parameter::Bool:
                // The C ABI passes a `bool` as a byte that is zero or one.
                expected = llvm::Type::getInt8Ty(context);
                if (!value->getType()->isIntegerTy())
                    return false;
                value = builder.CreateZExtOrTrunc(value,
                                                  expected,
                                                  "runtime.bool");
                break;
            case runtime::Parameter::Char:
                expected = llvm::Type::getInt32Ty(context);
                if (!value->getType()->isIntegerTy())
                    return false;
                value = builder.CreateZExtOrTrunc(value,
                                                  expected,
                                                  "runtime.char");
                break;
            case runtime::Parameter::Text:
                expected = pointer;
                if (!value->getType()->isPointerTy())
                    return false;
                break;
        }
        parameterTypes.push_back(expected);
        arguments.push_back(value);
    }

    const auto yieldsText = signature->result == runtime::Result::Text;
    const auto yieldsTruth = signature->result == runtime::Result::Truth;
    auto callee = module.getOrInsertFunction(
        std::string(signature->symbol),
        llvm::FunctionType::get(yieldsText    ? pointer
                                : yieldsTruth ? llvm::Type::getInt8Ty(context)
                                              : llvm::Type::getVoidTy(context),
                                parameterTypes,
                                false));
    auto *result = builder.CreateCall(callee,
                                      arguments,
                                      yieldsText    ? "runtime.text"
                                      : yieldsTruth ? "runtime.truth"
                                                    : "");
    if (yieldsTruth)
    {
        if (instruction.has_result)
            builder.CreateStore(
                builder.CreateICmpNE(
                    result,
                    llvm::ConstantInt::get(result->getType(), 0U),
                    "runtime.bool"),
                state.slots.at(instruction.destination));
        return true;
    }
    if (!yieldsText)
        return true;
    if (instruction.has_result)
        builder.CreateStore(result, state.slots.at(instruction.destination));
    else
        builder.CreateCall(RuntimeFunction("vxs_aarc_release_strong",
                                           llvm::Type::getVoidTy(context),
                                           { pointer }),
                           { result });
    return true;
}

[[nodiscard]] auto
LowerInstruction(llvm::IRBuilder<> &builder,
                 FunctionState &state,
                 const xmm::Instruction &instruction) -> bool
{
    if (instruction.opcode == xmm::Opcode::RuntimeCall)
        return LowerRuntimeCall(builder, state, instruction);
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
    if (instruction.opcode == xmm::Opcode::MakeClosure
        || instruction.opcode == xmm::Opcode::Memoize)
    {
        auto *result = instruction.opcode == xmm::Opcode::Memoize
                           ? LowerMemoize(builder, state, instruction)
                           : LowerClosure(builder, state, instruction);
        if (result == nullptr)
            return false;
        if (instruction.has_result)
            builder.CreateStore(result,
                                state.slots.at(instruction.destination));
        else
        {
            // Closure construction retains every strong capture, and a
            // remembering callable retains its computation.  A discarded
            // one still performs those observable ownership actions, but
            // its temporary owner must be released immediately or a dead
            // expression leaks both the object and what it holds.
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
        case xmm::Opcode::Divide:
        case xmm::Opcode::FloorDivide:
        case xmm::Opcode::Remainder:
            if (!floating)
                operands[1] = ComputableDivisor(builder,
                                                operands[0],
                                                operands[1],
                                                !unsignedInteger);
            break;
        case xmm::Opcode::ShiftLeft:
        case xmm::Opcode::ShiftRight:
            RequireShiftAmount(builder, operands[1]);
            break;
        default:
            break;
    }
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
        case xmm::Opcode::Memoize:
        case xmm::Opcode::RuntimeCall:
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
