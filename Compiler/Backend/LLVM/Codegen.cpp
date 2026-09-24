// SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
// SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.1

#include <algorithm>
#include <array>
#include <limits>
#include <llvm/ADT/APInt.h>
#include <llvm/ADT/SmallVector.h>
#include <llvm/Bitcode/BitcodeWriter.h>
#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/GlobalVariable.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Intrinsics.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/LegacyPassManager.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Verifier.h>
#include <llvm/MC/TargetRegistry.h>
#include <llvm/Passes/OptimizationLevel.h>
#include <llvm/Passes/PassBuilder.h>
#include <llvm/Support/CodeGen.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/Target/TargetMachine.h>
#include <llvm/Target/TargetOptions.h>
#include <llvm/TargetParser/Host.h>
#include <llvm/TargetParser/Triple.h>
#include <memory>
#include <mutex>
#include <ranges>
#include <string>
#include <unordered_map>
#include <utility>

#include "Visual/XSharp/Backend/LLVM.hpp"
#include "Visual/XSharp/Core/Callable.hpp"
#include "Visual/XSharp/Core/Ownership.hpp"
#include "Visual/XSharp/Core/Scalar.hpp"

namespace Visual::XSharp::Backend::LLVM
{
    namespace core = ::visual_xsharp::core;
    namespace xmm = ::visual_xsharp::xmm;

    namespace
    {
        [[nodiscard]] auto
        Failure(ErrorKind kind, std::string code, std::string message) -> Result
        {
            Result result;
            result.error
                = Error{ kind, std::move(code), std::move(message), {} };
            return result;
        }

        [[nodiscard]] auto
        AppendUtf8(std::string &result, char32_t point) -> bool
        {
            // UTF-8 is used here only for LLVM identifiers and
            // diagnostic-facing metadata. It is not the in-memory
            // representation of Visual X# String values; those retain their
            // Unicode-scalar ABI and are lowered separately by StringLiteral.
            const auto value = static_cast<std::uint32_t>(point);
            if (value > 0x10ffffU || (value >= 0xd800U && value <= 0xdfffU))
                return false;
            if (value <= 0x7fU)
                result.push_back(static_cast<char>(value));
            else if (value <= 0x7ffU)
            {
                result.push_back(static_cast<char>(0xc0U | value >> 6U));
                result.push_back(static_cast<char>(0x80U | (value & 0x3fU)));
            }
            else if (value <= 0xffffU)
            {
                result.push_back(static_cast<char>(0xe0U | value >> 12U));
                result.push_back(
                    static_cast<char>(0x80U | ((value >> 6U) & 0x3fU)));
                result.push_back(static_cast<char>(0x80U | (value & 0x3fU)));
            }
            else
            {
                result.push_back(static_cast<char>(0xf0U | value >> 18U));
                result.push_back(
                    static_cast<char>(0x80U | ((value >> 12U) & 0x3fU)));
                result.push_back(
                    static_cast<char>(0x80U | ((value >> 6U) & 0x3fU)));
                result.push_back(static_cast<char>(0x80U | (value & 0x3fU)));
            }
            return true;
        }

        [[nodiscard]] auto
        Utf8(std::u32string_view text) -> std::optional<std::string>
        {
            std::string result;
            result.reserve(text.size());
            for (const auto point : text)
                if (!AppendUtf8(result, point))
                    return std::nullopt;
            return result;
        }

        [[nodiscard]] auto
        ModuleName(const xmm::Module &module) -> std::optional<std::string>
        {
            std::string result;
            for (const auto &part : module.name)
            {
                const auto encoded = Utf8(part);
                if (!encoded)
                    return std::nullopt;
                if (!result.empty())
                    result.push_back('.');
                result += *encoded;
            }
            return result;
        }

        [[nodiscard]] auto
        SymbolName(const xmm::Module &module, const xmm::Function &function)
            -> std::optional<std::string>
        {
            auto result = ModuleName(module);
            const auto spelling = Utf8(function.symbol.spelling);
            if (!result || !spelling)
                return std::nullopt;
            result->append(".");
            result->append(*spelling);
            result->append(".");
            result->append(std::to_string(function.symbol.id));
            return result;
        }

        struct TypeLowerer final
        {
            llvm::LLVMContext &context;

            explicit TypeLowerer(llvm::LLVMContext &llvmContext)
                : context(llvmContext)
            {}

            [[nodiscard]] auto
            Lower(const core::Type &type) const -> llvm::Type *
            {
                switch (type.kind)
                {
                    case core::Type::Kind::Unit:
                        return llvm::Type::getVoidTy(context);
                    case core::Type::Kind::Bool:
                        return llvm::Type::getInt1Ty(context);
                    case core::Type::Kind::Character:
                        return llvm::Type::getInt32Ty(context);
                    case core::Type::Kind::Int8:
                    case core::Type::Kind::UInt8:
                        return llvm::Type::getInt8Ty(context);
                    case core::Type::Kind::Int16:
                    case core::Type::Kind::UInt16:
                        return llvm::Type::getInt16Ty(context);
                    case core::Type::Kind::Int64:
                    case core::Type::Kind::UInt64:
                        return llvm::Type::getInt64Ty(context);
                    case core::Type::Kind::Int32:
                    case core::Type::Kind::UInt32:
                        return llvm::Type::getInt32Ty(context);
                    case core::Type::Kind::Int128:
                    case core::Type::Kind::UInt128:
                        return llvm::IntegerType::get(context, 128U);
                    case core::Type::Kind::Float16:
                        return llvm::Type::getHalfTy(context);
                    case core::Type::Kind::Float32:
                        return llvm::Type::getFloatTy(context);
                    case core::Type::Kind::Float64:
                        return llvm::Type::getDoubleTy(context);
                    case core::Type::Kind::Float128:
                        return llvm::Type::getFP128Ty(context);
                    case core::Type::Kind::String:
                        return llvm::PointerType::get(context, 0);
                    case core::Type::Kind::Function:
                        // A function value is an AARC closure pointer. Direct
                        // function declarations build their LLVM FunctionType
                        // in FunctionType().
                        return llvm::PointerType::get(context, 0);
                    case core::Type::Kind::Named:
                        // Nominal AARC values are opaque at this boundary.
                        // Concrete field layout belongs to the type metadata
                        // and allocation lowering.
                        return llvm::PointerType::get(context, 0);
                    case core::Type::Kind::TypeVariable:
                        return nullptr;
                }
                return nullptr;
            }
        };

        struct FunctionState final
        {
            // Xmm virtual registers model typed mutable storage, not LLVM SSA
            // definitions. Slots preserve assignments across control-flow joins
            // without manufacturing phi nodes whose semantics have not yet been
            // established by an Xmm analysis pass.
            const xmm::Function *source{};
            llvm::Function *value{};
            llvm::FunctionType *type{};
            std::unordered_map<xmm::BlockId, llvm::BasicBlock *> blocks;
            std::unordered_map<xmm::VirtualRegister, llvm::AllocaInst *> slots;
            std::unordered_map<xmm::VirtualRegister, core::Type> register_types;
        };

        struct Generator final
        {
            llvm::LLVMContext &context;
            llvm::Module &module;
            TypeLowerer types;
            std::unordered_map<core::SymbolId, FunctionState> functions;
            std::uint64_t string_index{};
            std::uint64_t closure_index{};
            std::optional<Error> error;

            Generator(llvm::LLVMContext &llvmContext, llvm::Module &llvmModule)
                : context(llvmContext)
                , module(llvmModule)
                , types(context)
            {}

            void
            fail(ErrorKind kind, std::string code, std::string message)
            {
                if (!error)
                    error = Error{ kind,
                                   std::move(code),
                                   std::move(message),
                                   {} };
            }

            [[nodiscard]] auto
            FunctionType(const xmm::Function &function) -> llvm::FunctionType *
            {
                std::vector<llvm::Type *> parameters;
                parameters.reserve(function.parameter_types.size());
                for (const auto &parameter : function.parameter_types)
                    parameters.push_back(types.Lower(parameter));
                auto *result = types.Lower(function.return_type);
                if (result == nullptr
                    || std::ranges::any_of(parameters,
                                           [](const llvm::Type *item) {
                                               return item == nullptr;
                                           }))
                    return nullptr;
                return llvm::FunctionType::get(result, parameters, false);
            }

            [[nodiscard]] auto
            DeclareFunctions(const xmm::Module &source) -> bool
            {
                // Declare every function before emitting any body. Calls
                // therefore resolve by stable symbol id regardless of source
                // order, and direct recursion needs no special case during
                // instruction lowering.
                for (const auto &function : source.functions)
                {
                    const auto name = SymbolName(source, function);
                    auto *type = FunctionType(function);
                    if (!name)
                    {
                        fail(ErrorKind::InvalidUnicode,
                             "VXL2001",
                             "module or function name contains an invalid "
                             "Unicode scalar");
                        return false;
                    }
                    if (type == nullptr)
                    {
                        fail(
                            ErrorKind::UnsupportedType,
                            "VXL2002",
                            "function signature cannot be represented in LLVM");
                        return false;
                    }
                    auto *value = llvm::Function::Create(
                        type,
                        llvm::GlobalValue::ExternalLinkage,
                        *name,
                        module);
                    functions.emplace(
                        function.symbol.id,
                        FunctionState{ &function, value, type, {}, {}, {} });
                }
                return true;
            }

            void
            DiscoverRegisters(FunctionState &state)
            {
                for (std::size_t index = 0;
                     index < state.source->parameter_registers.size();
                     ++index)
                    state.register_types.emplace(
                        state.source->parameter_registers[index],
                        state.source->parameter_types[index]);
                for (const auto &block : state.source->blocks)
                    for (const auto &instruction : block.instructions)
                        if (instruction.has_result)
                            state.register_types.emplace(
                                instruction.destination,
                                instruction.result_type);
            }

            [[nodiscard]] auto
            CreateBlocksAndSlots(FunctionState &state) -> bool
            {
                // All allocas belong to the entry block even when the first
                // write appears in a later block. This gives each Xmm register
                // one address for the whole function and lets LLVM's
                // optimization pipeline promote eligible slots back to SSA.
                for (const auto &block : state.source->blocks)
                {
                    const auto blockName = "block." + std::to_string(block.id);
                    state.blocks.emplace(block.id,
                                         llvm::BasicBlock::Create(context,
                                                                  blockName,
                                                                  state.value));
                }
                const auto entry = state.blocks.find(state.source->entry);
                if (entry == state.blocks.end())
                    return false;

                llvm::IRBuilder<> builder(entry->second);
                DiscoverRegisters(state);
                std::vector<xmm::VirtualRegister> ordered;
                ordered.reserve(state.register_types.size());
                for (const auto &[reg, _] : state.register_types)
                    ordered.push_back(reg);
                std::ranges::sort(ordered);
                for (const auto reg : ordered)
                {
                    auto *type = types.Lower(state.register_types.at(reg));
                    if (type == nullptr || type->isVoidTy())
                        continue;
                    const auto name = "r" + std::to_string(reg);
                    state.slots.emplace(
                        reg,
                        builder.CreateAlloca(type, nullptr, name));
                }
                for (std::size_t index = 0;
                     index < state.source->parameter_registers.size();
                     ++index)
                {
                    const auto reg = state.source->parameter_registers[index];
                    const auto slot = state.slots.find(reg);
                    if (slot != state.slots.end())
                        builder.CreateStore(
                            state.value->getArg(static_cast<unsigned>(index)),
                            slot->second);
                }
                return true;
            }

            [[nodiscard]] auto
            StringLiteral(llvm::IRBuilder<> &builder,
                          const std::u32string &text) -> llvm::Value *
            {
                // Store one i32 per Unicode scalar. Visual X# String is
                // intentionally not UTF-8: scalar indexing must not depend on
                // the encoded byte width of earlier text. A trailing zero is
                // storage convenience only and is excluded from the length.
                auto *i32 = llvm::Type::getInt32Ty(context);
                std::vector<llvm::Constant *> units;
                units.reserve(text.size() + 1U);
                for (const auto point : text)
                {
                    const auto scalar = static_cast<std::uint32_t>(point);
                    if (scalar > 0x10ffffU
                        || (scalar >= 0xd800U && scalar <= 0xdfffU))
                    {
                        fail(ErrorKind::InvalidUnicode,
                             "VXL2003",
                             "String literal contains an invalid Unicode "
                             "scalar");
                        return nullptr;
                    }
                    units.push_back(llvm::ConstantInt::get(i32, scalar, false));
                }
                units.push_back(llvm::ConstantInt::get(i32, 0, false));
                auto *arrayType = llvm::ArrayType::get(i32, units.size());
                auto *initializer = llvm::ConstantArray::get(arrayType, units);
                const auto name
                    = ".vxs.string." + std::to_string(string_index++);
                auto *global = new llvm::GlobalVariable(
                    module,
                    arrayType,
                    true,
                    llvm::GlobalValue::PrivateLinkage,
                    initializer,
                    name);
                global->setUnnamedAddr(llvm::GlobalValue::UnnamedAddr::Global);
                auto *pointer = llvm::PointerType::get(context, 0);
                auto *sizeType
                    = llvm::Triple(module.getTargetTriple()).isArch64Bit()
                          ? llvm::Type::getInt64Ty(context)
                          : llvm::Type::getInt32Ty(context);
                auto literalFactory = module.getOrInsertFunction(
                    "vxs_aarc_string_literal",
                    llvm::FunctionType::get(pointer,
                                            { pointer, sizeType },
                                            false));
                return builder.CreateCall(
                    literalFactory,
                    { global,
                      llvm::ConstantInt::get(sizeType, text.size(), false) },
                    "string.literal");
            }

            [[nodiscard]] auto
            Immediate(llvm::IRBuilder<> &builder, const xmm::Value &value)
                -> llvm::Value *
            {
                const auto integer_constant
                    = [this,
                       &value](core::IntegerLiteral integer) -> llvm::Value * {
                    integer = core::normalize_integer(std::move(integer));
                    const auto description = core::describe_scalar(value.type);
                    if (!description
                        || (description->family != core::ScalarFamily::Character
                            && !description->is_integer()))
                        return nullptr;
                    llvm::APInt bits(description->bit_width,
                                     core::integer_hex_magnitude(integer),
                                     16);
                    if (integer.negative)
                        bits = -bits;
                    return llvm::ConstantInt::get(context, bits);
                };
                switch (value.type.kind)
                {
                    case core::Type::Kind::Unit:
                        return nullptr;
                    case core::Type::Kind::Bool:
                        return llvm::ConstantInt::get(
                            llvm::Type::getInt1Ty(context),
                            std::get<bool>(value.immediate));
                    case core::Type::Kind::Character:
                    case core::Type::Kind::Int8:
                    case core::Type::Kind::Int16:
                    case core::Type::Kind::Int64:
                    case core::Type::Kind::Int32:
                    case core::Type::Kind::Int128:
                    case core::Type::Kind::UInt8:
                    case core::Type::Kind::UInt16:
                    case core::Type::Kind::UInt32:
                    case core::Type::Kind::UInt64:
                    case core::Type::Kind::UInt128:
                        if (const auto *integer
                            = std::get_if<core::IntegerLiteral>(
                                &value.immediate))
                            return integer_constant(*integer);
                        if (const auto *integer64
                            = std::get_if<std::int64_t>(&value.immediate))
                            return integer_constant(
                                core::integer_from_signed(*integer64));
                        if (const auto *integer32
                            = std::get_if<std::int32_t>(&value.immediate))
                            return integer_constant(
                                core::integer_from_signed(*integer32));
                        return nullptr;
                    case core::Type::Kind::Float16:
                    case core::Type::Kind::Float32:
                    case core::Type::Kind::Float64:
                    case core::Type::Kind::Float128:
                        if (const auto *floating
                            = std::get_if<core::FloatingLiteral>(
                                &value.immediate))
                            return llvm::ConstantFP::get(
                                types.Lower(value.type),
                                floating->spelling);
                        return nullptr;
                    case core::Type::Kind::String:
                        if (std::holds_alternative<std::monostate>(
                                value.immediate))
                            return llvm::ConstantPointerNull::get(
                                llvm::PointerType::get(context, 0));
                        return StringLiteral(
                            builder,
                            std::get<std::u32string>(value.immediate));
                    case core::Type::Kind::Function:
                    case core::Type::Kind::Named:
                        if (std::holds_alternative<std::monostate>(
                                value.immediate))
                            return llvm::ConstantPointerNull::get(
                                llvm::PointerType::get(context, 0));
                        return nullptr;
                    case core::Type::Kind::TypeVariable:
                        return nullptr;
                }
                return nullptr;
            }

            [[nodiscard]] auto
            LoadValue(llvm::IRBuilder<> &builder,
                      FunctionState &state,
                      const xmm::Value &value) -> llvm::Value *
            {
                if (value.kind == xmm::Value::Kind::Immediate)
                    return Immediate(builder, value);
                if (value.kind == xmm::Value::Kind::Function)
                {
                    const auto found = functions.find(value.symbol);
                    return found == functions.end() ? nullptr
                                                    : found->second.value;
                }
                const auto slot = state.slots.find(value.reg);
                if (slot == state.slots.end())
                    return nullptr;
                const auto name = "r" + std::to_string(value.reg) + ".load";
                return builder.CreateLoad(types.Lower(value.type),
                                          slot->second,
                                          name);
            }

            [[nodiscard]] auto
            LowerSignedRoundedDiv(llvm::IRBuilder<> &builder,
                                  llvm::Value *left,
                                  llvm::Value *right) -> llvm::Value *
            {
                // `//` rounds to the nearest integer and sends an exact half
                // away from zero. Keep every comparison in the original integer
                // width: unsigned two's-complement magnitude represents
                // abs(MIN) without introducing an overflowing signed
                // absolute-value call.
                auto *quotient
                    = builder.CreateSDiv(left, right, "rounded.quotient");
                auto *remainder
                    = builder.CreateSRem(left, right, "rounded.remainder");
                auto *zero = llvm::Constant::getNullValue(left->getType());
                auto *one = llvm::ConstantInt::get(left->getType(), 1U);
                auto magnitude = [&](llvm::Value *value, const char *name) {
                    auto *negative = builder.CreateICmpSLT(value, zero);
                    return builder.CreateSelect(negative,
                                                builder.CreateNeg(value),
                                                value,
                                                name);
                };
                auto *remainderMagnitude
                    = magnitude(remainder, "rounded.remainder.magnitude");
                auto *divisorMagnitude
                    = magnitude(right, "rounded.divisor.magnitude");
                auto *half
                    = builder.CreateLShr(divisorMagnitude, one, "rounded.half");
                auto *odd = builder.CreateAnd(divisorMagnitude,
                                              one,
                                              "rounded.divisor.odd");
                auto *threshold
                    = builder.CreateAdd(half, odd, "rounded.threshold");
                auto *adjust = builder.CreateICmpUGE(remainderMagnitude,
                                                     threshold,
                                                     "rounded.adjust");
                auto *signBits
                    = builder.CreateXor(left, right, "rounded.sign.bits");
                auto *sameSign = builder.CreateICmpSGE(signBits,
                                                       zero,
                                                       "rounded.same.sign");
                auto *direction = builder.CreateSelect(
                    sameSign,
                    one,
                    llvm::ConstantInt::getSigned(
                        llvm::cast<llvm::IntegerType>(left->getType()),
                        -1));
                auto *adjusted = builder.CreateAdd(quotient,
                                                   direction,
                                                   "rounded.adjusted");
                return builder.CreateSelect(adjust,
                                            adjusted,
                                            quotient,
                                            "rounded.result");
            }

            [[nodiscard]] auto
            LowerUnsignedRoundedDiv(llvm::IRBuilder<> &builder,
                                    llvm::Value *left,
                                    llvm::Value *right) -> llvm::Value *
            {
                auto *one = llvm::ConstantInt::get(left->getType(), 1U);
                auto *quotient
                    = builder.CreateUDiv(left, right, "rounded.quotient");
                auto *remainder
                    = builder.CreateURem(left, right, "rounded.remainder");
                auto *half = builder.CreateLShr(right, one, "rounded.half");
                auto *odd
                    = builder.CreateAnd(right, one, "rounded.divisor.odd");
                auto *threshold
                    = builder.CreateAdd(half, odd, "rounded.threshold");
                auto *adjust = builder.CreateICmpUGE(remainder,
                                                     threshold,
                                                     "rounded.adjust");
                auto *adjusted
                    = builder.CreateAdd(quotient, one, "rounded.adjusted");
                return builder.CreateSelect(adjust,
                                            adjusted,
                                            quotient,
                                            "rounded.result");
            }

            [[nodiscard]] auto
            LowerFloatingRoundedDiv(llvm::IRBuilder<> &builder,
                                    llvm::Value *left,
                                    llvm::Value *right) -> llvm::Value *
            {
                auto *quotient
                    = builder.CreateFDiv(left, right, "rounded.quotient");
                auto *roundIntrinsic = llvm::Intrinsic::getOrInsertDeclaration(
                    &module,
                    llvm::Intrinsic::round,
                    { left->getType() });
                auto *rounded = builder.CreateCall(roundIntrinsic,
                                                   { quotient },
                                                   "rounded.integral");
                return builder.CreateFPToSI(rounded,
                                            llvm::Type::getInt64Ty(context),
                                            "rounded.result");
            }

            [[nodiscard]] auto
            IntegerPowerFunction(llvm::IntegerType *type) -> llvm::Function *
            {
                const auto name
                    = "__vxs_pow_i" + std::to_string(type->getBitWidth());
                if (auto *existing = module.getFunction(name))
                    return existing;

                auto *functionType
                    = llvm::FunctionType::get(type, { type, type }, false);
                auto *function
                    = llvm::Function::Create(functionType,
                                             llvm::GlobalValue::InternalLinkage,
                                             name,
                                             module);
                auto argument = function->arg_begin();
                auto *baseArgument = &*argument++;
                auto *exponentArgument = &*argument;
                baseArgument->setName("base");
                exponentArgument->setName("exponent");

                auto *entry
                    = llvm::BasicBlock::Create(context, "entry", function);
                auto *loop
                    = llvm::BasicBlock::Create(context, "loop", function);
                auto *body
                    = llvm::BasicBlock::Create(context, "body", function);
                auto *exit
                    = llvm::BasicBlock::Create(context, "exit", function);
                llvm::IRBuilder<> powerBuilder(entry);
                powerBuilder.CreateBr(loop);

                powerBuilder.SetInsertPoint(loop);
                auto *result = powerBuilder.CreatePHI(type, 2U, "result");
                auto *base = powerBuilder.CreatePHI(type, 2U, "factor");
                auto *exponent = powerBuilder.CreatePHI(type, 2U, "remaining");
                auto *zero = llvm::ConstantInt::get(type, 0U);
                auto *one = llvm::ConstantInt::get(type, 1U);
                result->addIncoming(one, entry);
                base->addIncoming(baseArgument, entry);
                exponent->addIncoming(exponentArgument, entry);
                powerBuilder.CreateCondBr(
                    powerBuilder.CreateICmpNE(exponent, zero),
                    body,
                    exit);

                powerBuilder.SetInsertPoint(body);
                auto *odd = powerBuilder.CreateICmpNE(
                    powerBuilder.CreateAnd(exponent, one),
                    zero,
                    "odd");
                auto *multiplied
                    = powerBuilder.CreateMul(result, base, "multiplied");
                auto *nextResult = powerBuilder.CreateSelect(odd,
                                                             multiplied,
                                                             result,
                                                             "next.result");
                auto *nextBase
                    = powerBuilder.CreateMul(base, base, "next.factor");
                auto *nextExponent
                    = powerBuilder.CreateLShr(exponent, one, "next.remaining");
                powerBuilder.CreateBr(loop);
                result->addIncoming(nextResult, body);
                base->addIncoming(nextBase, body);
                exponent->addIncoming(nextExponent, body);

                powerBuilder.SetInsertPoint(exit);
                powerBuilder.CreateRet(result);
                return function;
            }

            [[nodiscard]] auto
            LowerCall(llvm::IRBuilder<> &builder,
                      FunctionState &state,
                      const xmm::Instruction &instruction) -> llvm::Value *
            {
                if (instruction.operands.empty())
                    return nullptr;
                std::vector<llvm::Value *> arguments;
                arguments.reserve(instruction.operands.size() - 1U);
                for (std::size_t index = 1; index < instruction.operands.size();
                     ++index)
                    arguments.push_back(
                        LoadValue(builder, state, instruction.operands[index]));
                if (std::ranges::any_of(arguments,
                                        [](const llvm::Value *value) {
                                            return value == nullptr;
                                        }))
                    return nullptr;
                const auto *name
                    = instruction.result_type.kind == core::Type::Kind::Unit
                          ? ""
                          : "call.result";

                if (instruction.operands.front().kind
                    == xmm::Value::Kind::Function)
                {
                    // Direct functions retain symbol identity through Xmm.
                    // Resolving that identity here keeps recursion and forward
                    // calls independent of source order without allocating a
                    // closure for ordinary declarations.
                    const auto target
                        = functions.find(instruction.operands.front().symbol);
                    if (target == functions.end())
                        return nullptr;
                    return builder.CreateCall(target->second.type,
                                              target->second.value,
                                              arguments,
                                              name);
                }

                if (instruction.operands.front().kind
                    != xmm::Value::Kind::Register)
                    return nullptr;
                const auto signature
                    = ::Visual::XSharp::Core::Callable::Decompose(
                        instruction.operands.front().type);
                if (!signature)
                    return nullptr;
                auto *closure
                    = LoadValue(builder, state, instruction.operands.front());
                if (closure == nullptr || !closure->getType()->isPointerTy())
                    return nullptr;

                auto *pointer = llvm::PointerType::get(context, 0);
                std::vector<llvm::Type *> thunkParameters{ pointer };
                thunkParameters.reserve(signature->parameters.size() + 1U);
                for (const auto &parameter : signature->parameters)
                {
                    auto *lowered = types.Lower(parameter);
                    if (lowered == nullptr || lowered->isVoidTy())
                        return nullptr;
                    thunkParameters.push_back(lowered);
                }
                auto *resultType = types.Lower(signature->result);
                if (resultType == nullptr)
                    return nullptr;
                auto *thunkType = llvm::FunctionType::get(resultType,
                                                          thunkParameters,
                                                          false);

                // Every closure payload begins with its invoke thunk. The call
                // site can load this stable prefix without knowing capture
                // count or layout; only the private thunk interprets the
                // remainder of the environment.
                auto *thunk
                    = builder.CreateLoad(pointer, closure, "closure.invoke");
                std::vector<llvm::Value *> thunkArguments{ closure };
                thunkArguments.insert(thunkArguments.end(),
                                      arguments.begin(),
                                      arguments.end());
                return builder.CreateCall(thunkType,
                                          thunk,
                                          thunkArguments,
                                          name);
            }

            [[nodiscard]] auto
            RuntimeFunction(std::string_view name,
                            llvm::Type *result,
                            std::initializer_list<llvm::Type *> arguments)
                -> llvm::FunctionCallee
            {
                return module.getOrInsertFunction(
                    std::string(name),
                    llvm::FunctionType::get(result, arguments, false));
            }

            [[nodiscard]] auto
            IsPointerAarcType(const core::Type &type) const -> bool
            {
                // String is semantically AARC but still uses the scalar-buffer
                // bridge in this backend revision. Ownership instructions are
                // emitted only after a value has the opaque pointer
                // representation used by Named/callable values.
                return core::UsesAarc(type)
                       || type.kind == core::Type::Kind::Named;
            }

            [[nodiscard]] auto
            LowerOwnership(llvm::IRBuilder<> &builder,
                           FunctionState &state,
                           const xmm::Instruction &instruction) -> bool
            {
                if (instruction.operands.size() != 1U
                    || !IsPointerAarcType(instruction.operands.front().type))
                    return false;
                auto *value
                    = LoadValue(builder, state, instruction.operands.front());
                if (value == nullptr || !value->getType()->isPointerTy())
                    return false;

                auto *pointer = llvm::PointerType::get(context, 0);
                llvm::Value *result{};
                switch (instruction.opcode)
                {
                    case xmm::Opcode::RetainStrong:
                        result = builder.CreateCall(
                            RuntimeFunction("vxs_aarc_retain_strong",
                                            pointer,
                                            { pointer }),
                            { value },
                            "aarc.strong");
                        break;
                    case xmm::Opcode::ReleaseStrong:
                        builder.CreateCall(
                            RuntimeFunction("vxs_aarc_release_strong",
                                            llvm::Type::getVoidTy(context),
                                            { pointer }),
                            { value });
                        return true;
                    case xmm::Opcode::MakeWeak:
                        result = builder.CreateCall(
                            RuntimeFunction("vxs_aarc_make_weak",
                                            pointer,
                                            { pointer }),
                            { value },
                            "aarc.weak");
                        break;
                    case xmm::Opcode::LockWeak:
                        result = builder.CreateCall(
                            RuntimeFunction("vxs_aarc_lock_weak",
                                            pointer,
                                            { pointer }),
                            { value },
                            "aarc.locked");
                        break;
                    case xmm::Opcode::ReleaseWeak:
                        builder.CreateCall(
                            RuntimeFunction("vxs_aarc_release_weak",
                                            llvm::Type::getVoidTy(context),
                                            { pointer }),
                            { value });
                        return true;
                    case xmm::Opcode::MakeUnowned:
                        result = builder.CreateCall(
                            RuntimeFunction("vxs_aarc_make_unowned",
                                            pointer,
                                            { pointer }),
                            { value },
                            "aarc.unowned");
                        break;
                    case xmm::Opcode::LoadUnowned:
                        result = builder.CreateCall(
                            RuntimeFunction("vxs_aarc_load_unowned",
                                            pointer,
                                            { pointer }),
                            { value },
                            "aarc.borrowed");
                        break;
                    case xmm::Opcode::ReleaseUnowned:
                        builder.CreateCall(
                            RuntimeFunction("vxs_aarc_release_unowned",
                                            llvm::Type::getVoidTy(context),
                                            { pointer }),
                            { value });
                        return true;
                    default:
                        return false;
                }
                if (result == nullptr || !instruction.has_result)
                    return false;
                builder.CreateStore(result,
                                    state.slots.at(instruction.destination));
                return true;
            }

            // Keep the instruction and closure lowering methods together while
            // the owning generator retains one shared LLVM context and state.
#include "CodegenOperations.inc"

            [[nodiscard]] auto
            DefineFunction(FunctionState &state) -> bool
            {
                if (!CreateBlocksAndSlots(state))
                    return false;
                llvm::IRBuilder<> builder(context);
                for (const auto &block : state.source->blocks)
                {
                    builder.SetInsertPoint(state.blocks.at(block.id));
                    for (const auto &instruction : block.instructions)
                        if (!LowerInstruction(builder, state, instruction))
                            return false;
                    if (!LowerTerminator(builder, state, block.terminator))
                        return false;
                }
                return true;
            }

            [[nodiscard]] auto
            CreateExecutableEntry(const llvm::Triple &triple) -> bool
            {
                // Project selection has already narrowed compilation to the
                // requested Core module. Guard the ABI-critical method shape
                // once more before an operating-system entry symbol is
                // synthesized.
                const FunctionState *entry{};
                for (const auto &[_, candidate] : functions)
                {
                    if (candidate.source->symbol.spelling != U"Main"
                        || !candidate.source->parameter_types.empty()
                        || candidate.source->return_type.kind
                               != core::Type::Kind::Unit)
                        continue;
                    if (entry != nullptr)
                    {
                        fail(ErrorKind::InvalidEntryPoint,
                             "VXL2008",
                             "native executable contains more than one "
                             "parameterless void Main function");
                        return false;
                    }
                    entry = &candidate;
                }
                if (entry == nullptr)
                {
                    fail(ErrorKind::InvalidEntryPoint,
                         "VXL2007",
                         "native executable requires one parameterless void "
                         "Main function");
                    return false;
                }

                // The source entry remains a normal Visual X# function. A tiny
                // platform ABI bridge is synthesized only for executable
                // emission, so object/library builds never acquire an
                // accidental process entry symbol.
                const auto entryName
                    = triple.isOSWindows() ? "mainCRTStartup" : "main";
                auto *entryType
                    = llvm::FunctionType::get(llvm::Type::getInt32Ty(context),
                                              false);
                auto *bridge
                    = llvm::Function::Create(entryType,
                                             llvm::GlobalValue::ExternalLinkage,
                                             entryName,
                                             module);
                auto *block
                    = llvm::BasicBlock::Create(context, "entry", bridge);
                llvm::IRBuilder<> builder(block);
                builder.CreateCall(entry->type, entry->value);
                builder.CreateRet(
                    llvm::ConstantInt::get(llvm::Type::getInt32Ty(context), 0));
                return true;
            }
        };

        [[nodiscard]] auto
        PassOptimizationLevel(OptimizationLevel level)
            -> llvm::OptimizationLevel
        {
            switch (level)
            {
                case OptimizationLevel::Debug:
                    return llvm::OptimizationLevel::O0;
                case OptimizationLevel::Less:
                    return llvm::OptimizationLevel::O1;
                case OptimizationLevel::Default:
                    return llvm::OptimizationLevel::O2;
                case OptimizationLevel::Aggressive:
                    return llvm::OptimizationLevel::O3;
            }
            return llvm::OptimizationLevel::O2;
        }

        void
        Optimize(llvm::Module &module, OptimizationLevel level)
        {
            // Build the standard per-module pipeline through LLVM's C++ pass
            // manager. Keeping every analysis manager local makes ownership
            // explicit and prevents global pass state from leaking between
            // compiler invocations in the same process.
            llvm::LoopAnalysisManager loopAnalyses;
            llvm::FunctionAnalysisManager functionAnalyses;
            llvm::CGSCCAnalysisManager cgsccAnalyses;
            llvm::ModuleAnalysisManager moduleAnalyses;
            llvm::PassBuilder passBuilder;
            passBuilder.registerModuleAnalyses(moduleAnalyses);
            passBuilder.registerCGSCCAnalyses(cgsccAnalyses);
            passBuilder.registerFunctionAnalyses(functionAnalyses);
            passBuilder.registerLoopAnalyses(loopAnalyses);
            passBuilder.crossRegisterProxies(loopAnalyses,
                                             functionAnalyses,
                                             cgsccAnalyses,
                                             moduleAnalyses);
            auto pipeline = passBuilder.buildPerModuleDefaultPipeline(
                PassOptimizationLevel(level));
            pipeline.run(module, moduleAnalyses);
        }

        [[nodiscard]] auto
        CodeGenerationLevel(OptimizationLevel level) -> llvm::CodeGenOptLevel
        {
            switch (level)
            {
                case OptimizationLevel::Debug:
                    return llvm::CodeGenOptLevel::None;
                case OptimizationLevel::Less:
                    return llvm::CodeGenOptLevel::Less;
                case OptimizationLevel::Default:
                    return llvm::CodeGenOptLevel::Default;
                case OptimizationLevel::Aggressive:
                    return llvm::CodeGenOptLevel::Aggressive;
            }
            return llvm::CodeGenOptLevel::Default;
        }

        [[nodiscard]] auto
        NativeTargetsAvailable() -> bool
        {
            // LLVM target initialization mutates process-global registries.
            // Serialize it once even when several compiler sessions lower
            // modules concurrently.
            static std::once_flag once;
            static bool available{};
            std::call_once(once, [] {
                available = !llvm::InitializeNativeTarget()
                            && !llvm::InitializeNativeTargetAsmPrinter();
            });
            return available;
        }

        [[nodiscard]] auto
        ArtifactObjectFormat(const llvm::Triple &triple) -> ObjectFormat
        {
            switch (triple.getObjectFormat())
            {
                case llvm::Triple::COFF:
                    return ObjectFormat::Coff;
                case llvm::Triple::ELF:
                    return ObjectFormat::Elf;
                case llvm::Triple::MachO:
                    return ObjectFormat::MachO;
                case llvm::Triple::Wasm:
                    return ObjectFormat::Wasm;
                default:
                    return ObjectFormat::Unknown;
            }
        }

        [[nodiscard]] auto
        CreateTargetMachine(llvm::Module &module,
                            OptimizationLevel optimization,
                            std::optional<Error> &error)
            -> std::unique_ptr<llvm::TargetMachine>
        {
            if (!NativeTargetsAvailable())
            {
                error = Error{ ErrorKind::TargetMachine,
                               "VXL2009",
                               "LLVM native target initialization failed",
                               {} };
                return {};
            }
            std::string lookupError;
            const auto triple = llvm::Triple(module.getTargetTriple());
            const auto *target
                = llvm::TargetRegistry::lookupTarget(triple, lookupError);
            if (target == nullptr)
            {
                error = Error{
                    ErrorKind::TargetMachine,
                    "VXL2010",
                    lookupError.empty()
                        ? "LLVM has no code generator for the selected target"
                        : lookupError,
                    {}
                };
                return {};
            }
            llvm::TargetOptions targetOptions;
            // `generic` avoids silently selecting host-only CPU extensions that
            // could make a produced executable fail on another machine of the
            // same target.
            auto machine = std::unique_ptr<llvm::TargetMachine>(
                target->createTargetMachine(triple,
                                            "generic",
                                            "",
                                            targetOptions,
                                            std::nullopt,
                                            std::nullopt,
                                            CodeGenerationLevel(optimization)));
            if (!machine)
            {
                error = Error{
                    ErrorKind::TargetMachine,
                    "VXL2011",
                    "LLVM could not create the selected target machine",
                    {}
                };
                return {};
            }
            module.setDataLayout(machine->createDataLayout());
            return machine;
        }

        [[nodiscard]] auto
        EmitMachineCode(llvm::Module &module,
                        llvm::TargetMachine &machine,
                        MachineCodeEmission emission,
                        Artifact &artifact) -> std::optional<Error>
        {
            // TargetMachine still exposes emission through the legacy
            // pass-manager adapter. Optimization above remains on LLVM's new
            // pass manager.
            llvm::SmallVector<char, 0> bytes;
            llvm::raw_svector_ostream stream(bytes);
            llvm::legacy::PassManager passes;
            const auto fileType = emission == MachineCodeEmission::Assembly
                                      ? llvm::CodeGenFileType::AssemblyFile
                                      : llvm::CodeGenFileType::ObjectFile;
            if (machine.addPassesToEmitFile(passes, stream, nullptr, fileType))
                return Error{ ErrorKind::MachineCodeEmission,
                              "VXL2012",
                              emission == MachineCodeEmission::Assembly
                                  ? "target cannot emit assembly"
                                  : "target cannot emit an object file",
                              {} };
            passes.run(module);
            if (bytes.empty())
                return Error{ ErrorKind::MachineCodeEmission,
                              "VXL2013",
                              "LLVM emitted an empty machine-code artifact",
                              {} };
            if (emission == MachineCodeEmission::Assembly)
                artifact.assembly.assign(bytes.begin(), bytes.end());
            else
                artifact.object.assign(
                    reinterpret_cast<const std::uint8_t *>(bytes.data()),
                    reinterpret_cast<const std::uint8_t *>(bytes.data()
                                                           + bytes.size()));
            return std::nullopt;
        }
    } // namespace

    auto
    Lower(const Xmm::Module &source, const Options &options) -> Result
    {
        // Reject malformed Xmm before allocating LLVM state. Besides clearer
        // diagnostics, this keeps construction free to rely on verified block,
        // type and symbol invariants instead of duplicating defensive checks at
        // every IRBuilder call.
        auto issues = Verify(source);
        if (!issues.empty())
        {
            Result result;
            result.error
                = Error{ ErrorKind::InvalidXmm,
                         "VXL2000",
                         "Xmm verification failed before LLVM lowering",
                         std::move(issues) };
            return result;
        }

        llvm::LLVMContext context;
        const auto name = ModuleName(source);
        if (!name)
            return Failure(ErrorKind::InvalidUnicode,
                           "VXL2001",
                           "module name contains an invalid Unicode scalar");
        llvm::Module module(*name, context);

        std::string triple = options.target_triple;
        if (triple.empty())
            triple = llvm::sys::getDefaultTargetTriple();
        if (triple.empty())
            return Failure(ErrorKind::LlvmConstruction,
                           "VXL2004",
                           "LLVM did not provide a default target triple");
        module.setTargetTriple(llvm::Triple(triple));

        std::optional<Error> targetError;
        std::unique_ptr<llvm::TargetMachine> targetMachine;
        if (options.machineCode != MachineCodeEmission::None)
        {
            // Fix the data layout before generating functions and before
            // optimization reasons about pointer widths, alignment, or calling
            // conventions.
            targetMachine = CreateTargetMachine(module,
                                                options.optimization,
                                                targetError);
            if (!targetMachine)
                return Result{ std::nullopt, std::move(targetError) };
        }

        Generator generator(context, module);
        if (!generator.DeclareFunctions(source))
            return Result{ std::nullopt, std::move(generator.error) };
        for (auto &[_, function] : generator.functions)
            if (!generator.DefineFunction(function))
                return Failure(
                    ErrorKind::LlvmConstruction,
                    "VXL2005",
                    "failed to lower an Xmm instruction or terminator");
        if (options.executableEntry
            && !generator.CreateExecutableEntry(llvm::Triple(triple)))
            return Result{ std::nullopt, std::move(generator.error) };

        if (options.verify_module)
        {
            // LLVM verification runs before optimization so a backend
            // construction defect cannot be hidden or transformed into a less
            // useful pass-manager failure.
            std::string message;
            llvm::raw_string_ostream diagnostics(message);
            if (llvm::verifyModule(module, &diagnostics))
            {
                diagnostics.flush();
                return Failure(ErrorKind::LlvmVerification,
                               "VXL2006",
                               message.empty()
                                   ? "LLVM rejected the generated module"
                                   : std::move(message));
            }
        }

        Optimize(module, options.optimization);

        std::string printed;
        llvm::raw_string_ostream printedStream(printed);
        module.print(printedStream, nullptr);
        printedStream.flush();
        llvm::SmallVector<char, 0> bitcode;
        llvm::raw_svector_ostream bitcodeStream(bitcode);
        llvm::WriteBitcodeToFile(module, bitcodeStream);
        Artifact artifact;
        // Copy both products before the local module and context are destroyed.
        // Artifact consequently has ordinary C++ value semantics and no LLVM
        // ABI or lifetime obligation is imposed on CLI, tests or future object
        // emitters.
        artifact.llvm_ir = std::move(printed);
        artifact.bitcode.assign(
            reinterpret_cast<const std::uint8_t *>(bitcode.data()),
            reinterpret_cast<const std::uint8_t *>(bitcode.data()
                                                   + bitcode.size()));
        artifact.target_triple = std::move(triple);
        // Preserve the format beside its bytes. The linker can reject
        // mismatched input without guessing from a filename or reparsing an
        // object header.
        artifact.objectFormat
            = ArtifactObjectFormat(llvm::Triple(artifact.target_triple));
        artifact.function_count = source.functions.size();
        if (targetMachine)
            if (auto error = EmitMachineCode(module,
                                             *targetMachine,
                                             options.machineCode,
                                             artifact))
                return Result{ std::nullopt, std::move(error) };
        return Result{ std::move(artifact), std::nullopt };
    }
} // namespace Visual::XSharp::Backend::LLVM
