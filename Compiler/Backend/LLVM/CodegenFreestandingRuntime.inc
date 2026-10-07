// SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
// SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.1

// The ownership runtime of a freestanding executable.
//
// A native executable is linked without a C runtime and without any library:
// it is the code of its own modules and nothing else. Generated code that
// creates a closure, or a callable that remembers its result, calls
// `vxs_aarc_allocate`, `vxs_aarc_retain_strong` and
// `vxs_aarc_release_strong`. In a process that hosts the JIT those are the
// entry points of the AARC runtime library. An executable has no such
// library to link, so the module that holds its entry defines the three
// functions itself, with the meaning the library gives them:
//
// - an object starts with one strong reference;
// - retaining adds one and releasing removes one;
// - when the last one is released the destructor of the object's type runs
//   and the object's memory is free again;
// - a null reference is retained and released without effect.
//
// The memory comes from one arena in the executable's zero-initialized data,
// because without a library there is no system allocator to ask. An object
// is laid out as a count, the address of its type metadata, and its
// payload, rounded up to sixteen bytes. Freed blocks of up to a kilobyte are
// kept in lists by size and reused before the arena grows, so a program that
// creates and releases objects in a loop stays within what it holds at one
// time. A program that holds more than the arena at once stops with a trap:
// it does not continue with memory it does not have.
//
// This runtime counts without atomic operations, because a freestanding
// executable has one thread. It implements strong ownership only. Weak and
// unowned handles, strings and type tests remain entry points of the
// library, and an executable that needs them does not link.

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

/// Defines the three ownership entry points in this module.
///
/// The functions keep their external linkage, so the other objects of a
/// project that is linked from several modules find them in this one.
void
DefineFreestandingRuntime()
{
    // What an executable may hold at one time, and the largest block that
    // is kept for reuse: sixty-four size classes of sixteen bytes.
    constexpr std::uint64_t kArenaBytes = 64ULL * 1024ULL * 1024ULL;
    constexpr std::uint64_t kSizeClasses = 64ULL;
    constexpr std::uint64_t kHeaderBytes = 16ULL;
    constexpr unsigned kInstanceSizeField = 3U;
    constexpr unsigned kDestructorField = 5U;

    auto *pointer = llvm::PointerType::get(context, 0);
    auto *word = llvm::Type::getInt64Ty(context);
    auto *byte = llvm::Type::getInt8Ty(context);
    auto *voidType = llvm::Type::getVoidTy(context);
    auto *metadataType = ObjectMetadataType();
    auto *sizeType = metadataType->getElementType(kInstanceSizeField);
    const auto constant = [&](std::uint64_t value) {
        return llvm::ConstantInt::get(word, value);
    };

    auto *arenaType = llvm::ArrayType::get(byte, kArenaBytes);
    auto *arena
        = new llvm::GlobalVariable(module,
                                   arenaType,
                                   false,
                                   llvm::GlobalValue::InternalLinkage,
                                   llvm::ConstantAggregateZero::get(arenaType),
                                   ".vxs.aarc.arena");
    arena->setAlignment(llvm::Align(16U));
    auto *used = new llvm::GlobalVariable(module,
                                          word,
                                          false,
                                          llvm::GlobalValue::InternalLinkage,
                                          constant(0U),
                                          ".vxs.aarc.arena.used");
    auto *listsType = llvm::ArrayType::get(pointer, kSizeClasses);
    auto *lists
        = new llvm::GlobalVariable(module,
                                   listsType,
                                   false,
                                   llvm::GlobalValue::InternalLinkage,
                                   llvm::ConstantAggregateZero::get(listsType),
                                   ".vxs.aarc.free");

    // The whole block of an object with the given payload size: header and
    // payload, rounded up to sixteen bytes.
    const auto blockBytes = [&](llvm::IRBuilder<> &builder,
                                llvm::Value *metadata) {
        auto *instanceSize = builder.CreateZExtOrTrunc(
            builder.CreateLoad(sizeType,
                               builder.CreateStructGEP(metadataType,
                                                       metadata,
                                                       kInstanceSizeField)),
            word,
            "instance.size");
        return builder.CreateAnd(
            builder.CreateAdd(instanceSize, constant(kHeaderBytes + 15U)),
            constant(~15ULL),
            "block.size");
    };
    const auto listOf = [&](llvm::IRBuilder<> &builder, llvm::Value *bytes) {
        return builder.CreateInBoundsGEP(
            listsType,
            lists,
            { constant(0U), builder.CreateLShr(bytes, constant(4U)) },
            "free.list");
    };
    const auto reusable = [&](llvm::IRBuilder<> &builder, llvm::Value *bytes) {
        return builder.CreateICmpULT(bytes,
                                     constant(kSizeClasses * 16ULL),
                                     "block.reusable");
    };
    const auto headerOf = [&](llvm::IRBuilder<> &builder, llvm::Value *object) {
        return builder.CreateGEP(byte,
                                 object,
                                 llvm::ConstantInt::getSigned(
                                     word,
                                     -static_cast<std::int64_t>(kHeaderBytes)),
                                 "block");
    };
    const auto define = [&](const char *name, llvm::FunctionType *type) {
        auto *function = llvm::cast<llvm::Function>(
            module.getOrInsertFunction(name, type).getCallee());
        function->setLinkage(llvm::GlobalValue::ExternalLinkage);
        return function;
    };

    {
        auto *function
            = define("vxs_aarc_allocate",
                     llvm::FunctionType::get(pointer, { pointer }, false));
        auto *entry = llvm::BasicBlock::Create(context, "entry", function);
        auto *fromList = llvm::BasicBlock::Create(context, "list", function);
        auto *reuse = llvm::BasicBlock::Create(context, "reuse", function);
        auto *grow = llvm::BasicBlock::Create(context, "grow", function);
        auto *take = llvm::BasicBlock::Create(context, "take", function);
        auto *exhausted
            = llvm::BasicBlock::Create(context, "exhausted", function);
        auto *ready = llvm::BasicBlock::Create(context, "ready", function);
        auto *metadata = function->getArg(0U);

        llvm::IRBuilder<> builder(entry);
        auto *bytes = blockBytes(builder, metadata);
        builder.CreateCondBr(reusable(builder, bytes), fromList, grow);

        builder.SetInsertPoint(fromList);
        auto *list = listOf(builder, bytes);
        auto *freed = builder.CreateLoad(pointer, list, "freed");
        builder.CreateCondBr(builder.CreateIsNull(freed), grow, reuse);

        // A freed block holds the next freed block of its size in its
        // first word.
        builder.SetInsertPoint(reuse);
        builder.CreateStore(builder.CreateLoad(pointer, freed), list);
        builder.CreateBr(ready);

        builder.SetInsertPoint(grow);
        auto *offset = builder.CreateLoad(word, used, "arena.offset");
        auto *end = builder.CreateAdd(offset, bytes, "arena.end");
        builder.CreateCondBr(builder.CreateICmpUGT(end, constant(kArenaBytes)),
                             exhausted,
                             take);

        builder.SetInsertPoint(exhausted);
        builder.CreateCall(
            llvm::Intrinsic::getOrInsertDeclaration(&module,
                                                    llvm::Intrinsic::trap));
        builder.CreateUnreachable();

        builder.SetInsertPoint(take);
        builder.CreateStore(end, used);
        auto *fresh = builder.CreateInBoundsGEP(arenaType,
                                                arena,
                                                { constant(0U), offset },
                                                "fresh");
        builder.CreateBr(ready);

        builder.SetInsertPoint(ready);
        auto *block = builder.CreatePHI(pointer, 2U, "block");
        block->addIncoming(freed, reuse);
        block->addIncoming(fresh, take);
        builder.CreateStore(constant(1U), block);
        builder.CreateStore(metadata,
                            builder.CreateGEP(byte, block, constant(8U)));
        builder.CreateRet(
            builder.CreateGEP(byte, block, constant(kHeaderBytes), "object"));
    }

    {
        auto *function
            = define("vxs_aarc_retain_strong",
                     llvm::FunctionType::get(pointer, { pointer }, false));
        auto *entry = llvm::BasicBlock::Create(context, "entry", function);
        auto *count = llvm::BasicBlock::Create(context, "count", function);
        auto *done = llvm::BasicBlock::Create(context, "done", function);
        auto *object = function->getArg(0U);

        llvm::IRBuilder<> builder(entry);
        builder.CreateCondBr(builder.CreateIsNull(object), done, count);

        builder.SetInsertPoint(count);
        auto *block = headerOf(builder, object);
        builder.CreateStore(
            builder.CreateAdd(builder.CreateLoad(word, block), constant(1U)),
            block);
        builder.CreateBr(done);

        builder.SetInsertPoint(done);
        builder.CreateRet(object);
    }

    {
        auto *function
            = define("vxs_aarc_release_strong",
                     llvm::FunctionType::get(voidType, { pointer }, false));
        auto *entry = llvm::BasicBlock::Create(context, "entry", function);
        auto *count = llvm::BasicBlock::Create(context, "count", function);
        auto *last = llvm::BasicBlock::Create(context, "last", function);
        auto *destroy = llvm::BasicBlock::Create(context, "destroy", function);
        auto *destroyed
            = llvm::BasicBlock::Create(context, "destroyed", function);
        auto *keep = llvm::BasicBlock::Create(context, "keep", function);
        auto *done = llvm::BasicBlock::Create(context, "done", function);
        auto *object = function->getArg(0U);

        llvm::IRBuilder<> builder(entry);
        builder.CreateCondBr(builder.CreateIsNull(object), done, count);

        builder.SetInsertPoint(count);
        auto *block = headerOf(builder, object);
        auto *remaining = builder.CreateSub(builder.CreateLoad(word, block),
                                            constant(1U),
                                            "remaining");
        builder.CreateStore(remaining, block);
        builder.CreateCondBr(builder.CreateICmpEQ(remaining, constant(0U)),
                             last,
                             done);

        // The destructor releases what the object holds. It runs before the
        // block is free, so nothing it does can receive this block.
        builder.SetInsertPoint(last);
        auto *metadata
            = builder.CreateLoad(pointer,
                                 builder.CreateGEP(byte, block, constant(8U)),
                                 "metadata");
        auto *destructor = builder.CreateLoad(
            pointer,
            builder.CreateStructGEP(metadataType, metadata, kDestructorField),
            "destructor");
        builder.CreateCondBr(builder.CreateIsNull(destructor),
                             destroyed,
                             destroy);

        builder.SetInsertPoint(destroy);
        builder.CreateCall(
            llvm::FunctionType::get(voidType, { pointer }, false),
            destructor,
            { object });
        builder.CreateBr(destroyed);

        builder.SetInsertPoint(destroyed);
        auto *bytes = blockBytes(builder, metadata);
        builder.CreateCondBr(reusable(builder, bytes), keep, done);

        builder.SetInsertPoint(keep);
        auto *list = listOf(builder, bytes);
        builder.CreateStore(builder.CreateLoad(pointer, list), block);
        builder.CreateStore(block, list);
        builder.CreateBr(done);

        builder.SetInsertPoint(done);
        builder.CreateRetVoid();
    }
}
