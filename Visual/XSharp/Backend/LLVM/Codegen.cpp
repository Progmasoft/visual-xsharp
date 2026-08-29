// SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
// SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.0

#include "Visual/XSharp/Backend/LLVM.hpp"

#include <llvm/ADT/SmallVector.h>
#include <llvm/Bitcode/BitcodeWriter.h>
#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/GlobalVariable.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Verifier.h>
#include <llvm/Passes/OptimizationLevel.h>
#include <llvm/Passes/PassBuilder.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/TargetParser/Host.h>
#include <llvm/TargetParser/Triple.h>

#include <algorithm>
#include <array>
#include <limits>
#include <ranges>
#include <string>
#include <unordered_map>
#include <utility>

namespace Visual::XSharp::Backend::LLVM
{
using namespace ::visual_xsharp;
namespace
{
[[nodiscard]] auto Failure(ErrorKind kind, std::string code, std::string message) -> Result
{
    Result result;
    result.error = Error{kind, std::move(code), std::move(message), {}};
    return result;
}

[[nodiscard]] auto AppendUtf8(std::string &result, char32_t point) -> bool
{
    // UTF-8 is used here only for LLVM identifiers and diagnostic-facing metadata.
    // It is not the in-memory representation of Visual X# String values; those retain
    // their Unicode-scalar ABI and are lowered separately by StringLiteral.
    const auto value = static_cast<std::uint32_t>(point);
    if(value > 0x10ffffU || (value >= 0xd800U && value <= 0xdfffU))
        return false;
    if(value <= 0x7fU)
        result.push_back(static_cast<char>(value));
    else if(value <= 0x7ffU)
    {
        result.push_back(static_cast<char>(0xc0U | value >> 6U));
        result.push_back(static_cast<char>(0x80U | (value & 0x3fU)));
    }
    else if(value <= 0xffffU)
    {
        result.push_back(static_cast<char>(0xe0U | value >> 12U));
        result.push_back(static_cast<char>(0x80U | ((value >> 6U) & 0x3fU)));
        result.push_back(static_cast<char>(0x80U | (value & 0x3fU)));
    }
    else
    {
        result.push_back(static_cast<char>(0xf0U | value >> 18U));
        result.push_back(static_cast<char>(0x80U | ((value >> 12U) & 0x3fU)));
        result.push_back(static_cast<char>(0x80U | ((value >> 6U) & 0x3fU)));
        result.push_back(static_cast<char>(0x80U | (value & 0x3fU)));
    }
    return true;
}

[[nodiscard]] auto Utf8(std::u32string_view text) -> std::optional<std::string>
{
    std::string result;
    result.reserve(text.size());
    for(const auto point : text)
        if(!AppendUtf8(result, point))
            return std::nullopt;
    return result;
}

[[nodiscard]] auto ModuleName(const xmm::Module &module) -> std::optional<std::string>
{
    std::string result;
    for(const auto &part : module.name)
    {
        const auto encoded = Utf8(part);
        if(!encoded)
            return std::nullopt;
        if(!result.empty())
            result.push_back('.');
        result += *encoded;
    }
    return result;
}

[[nodiscard]] auto SymbolName(const xmm::Module &module, const xmm::Function &function) -> std::optional<std::string>
{
    auto result = ModuleName(module);
    const auto spelling = Utf8(function.symbol.spelling);
    if(!result || !spelling)
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
    llvm::StructType *stringType{};

    explicit TypeLowerer(llvm::LLVMContext &llvmContext) : context(llvmContext)
    {
        // String is { pointer-to-Unicode-scalar, scalar-count }. The count is i64 so
        // indexing capacity is target-independent, and it excludes the sentinel used
        // to make debugger inspection convenient.
        std::array<llvm::Type *, 2> fields{llvm::PointerType::get(context, 0), llvm::Type::getInt64Ty(context)};
        stringType = llvm::StructType::get(context, fields, false);
    }

    [[nodiscard]] auto Lower(const core::Type &type) const -> llvm::Type *
    {
        switch(type.kind)
        {
        case core::Type::Kind::Unit:
            return llvm::Type::getVoidTy(context);
        case core::Type::Kind::Bool:
            return llvm::Type::getInt1Ty(context);
        case core::Type::Kind::Int64:
            return llvm::Type::getInt64Ty(context);
        case core::Type::Kind::Int32:
            return llvm::Type::getInt32Ty(context);
        case core::Type::Kind::String:
            return stringType;
        case core::Type::Kind::Function:
        {
            if(type.components.empty())
                return nullptr;
            std::vector<llvm::Type *> parameters;
            parameters.reserve(type.components.size() - 1U);
            for(std::size_t index = 0; index + 1U < type.components.size(); ++index)
                parameters.push_back(Lower(type.components[index]));
            if(std::ranges::any_of(parameters, [](const llvm::Type *item) { return item == nullptr; }))
                return nullptr;
            auto *result = Lower(type.components.back());
            return result == nullptr ? nullptr : llvm::FunctionType::get(result, parameters, false);
        }
        case core::Type::Kind::Named:
        case core::Type::Kind::TypeVariable:
            return nullptr;
        }
        return nullptr;
    }
};

struct FunctionState final
{
    // Xmm virtual registers model typed mutable storage, not LLVM SSA definitions.
    // Slots preserve assignments across control-flow joins without manufacturing phi
    // nodes whose semantics have not yet been established by an Xmm analysis pass.
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
    std::optional<Error> error;

    Generator(llvm::LLVMContext &llvmContext, llvm::Module &llvmModule)
        : context(llvmContext), module(llvmModule), types(context)
    {
    }

    void fail(ErrorKind kind, std::string code, std::string message)
    {
        if(!error)
            error = Error{kind, std::move(code), std::move(message), {}};
    }

    [[nodiscard]] auto FunctionType(const xmm::Function &function) -> llvm::FunctionType *
    {
        std::vector<llvm::Type *> parameters;
        parameters.reserve(function.parameter_types.size());
        for(const auto &parameter : function.parameter_types)
            parameters.push_back(types.Lower(parameter));
        auto *result = types.Lower(function.return_type);
        if(result == nullptr || std::ranges::any_of(parameters, [](const llvm::Type *item) { return item == nullptr; }))
            return nullptr;
        return llvm::FunctionType::get(result, parameters, false);
    }

    [[nodiscard]] auto DeclareFunctions(const xmm::Module &source) -> bool
    {
        // Declare every function before emitting any body. Calls therefore resolve by
        // stable symbol id regardless of source order, and direct recursion needs no
        // special case during instruction lowering.
        for(const auto &function : source.functions)
        {
            const auto name = SymbolName(source, function);
            auto *type = FunctionType(function);
            if(!name)
            {
                fail(ErrorKind::InvalidUnicode, "VXL2001",
                     "module or function name contains an invalid Unicode scalar");
                return false;
            }
            if(type == nullptr)
            {
                fail(ErrorKind::UnsupportedType, "VXL2002", "function signature cannot be represented in LLVM");
                return false;
            }
            auto *value = llvm::Function::Create(type, llvm::GlobalValue::ExternalLinkage, *name, module);
            functions.emplace(function.symbol.id, FunctionState{&function, value, type, {}, {}, {}});
        }
        return true;
    }

    void DiscoverRegisters(FunctionState &state)
    {
        for(std::size_t index = 0; index < state.source->parameter_registers.size(); ++index)
            state.register_types.emplace(state.source->parameter_registers[index],
                                         state.source->parameter_types[index]);
        for(const auto &block : state.source->blocks)
            for(const auto &instruction : block.instructions)
                if(instruction.has_result)
                    state.register_types.emplace(instruction.destination, instruction.result_type);
    }

    [[nodiscard]] auto CreateBlocksAndSlots(FunctionState &state) -> bool
    {
        // All allocas belong to the entry block even when the first write appears in a
        // later block. This gives each Xmm register one address for the whole function
        // and lets LLVM's optimization pipeline promote eligible slots back to SSA.
        for(const auto &block : state.source->blocks)
        {
            const auto blockName = "block." + std::to_string(block.id);
            state.blocks.emplace(block.id, llvm::BasicBlock::Create(context, blockName, state.value));
        }
        const auto entry = state.blocks.find(state.source->entry);
        if(entry == state.blocks.end())
            return false;

        llvm::IRBuilder<> builder(entry->second);
        DiscoverRegisters(state);
        std::vector<xmm::VirtualRegister> ordered;
        ordered.reserve(state.register_types.size());
        for(const auto &[reg, _] : state.register_types)
            ordered.push_back(reg);
        std::ranges::sort(ordered);
        for(const auto reg : ordered)
        {
            auto *type = types.Lower(state.register_types.at(reg));
            if(type == nullptr || type->isVoidTy())
                continue;
            const auto name = "r" + std::to_string(reg);
            state.slots.emplace(reg, builder.CreateAlloca(type, nullptr, name));
        }
        for(std::size_t index = 0; index < state.source->parameter_registers.size(); ++index)
        {
            const auto reg = state.source->parameter_registers[index];
            const auto slot = state.slots.find(reg);
            if(slot != state.slots.end())
                builder.CreateStore(state.value->getArg(static_cast<unsigned>(index)), slot->second);
        }
        return true;
    }

    [[nodiscard]] auto StringLiteral(const std::u32string &text) -> llvm::Constant *
    {
        // Store one i32 per Unicode scalar. Visual X# String is intentionally not UTF-8:
        // scalar indexing must not depend on the encoded byte width of earlier text.
        // A trailing zero is storage convenience only and is excluded from the length.
        auto *i32 = llvm::Type::getInt32Ty(context);
        std::vector<llvm::Constant *> units;
        units.reserve(text.size() + 1U);
        for(const auto point : text)
        {
            const auto scalar = static_cast<std::uint32_t>(point);
            if(scalar > 0x10ffffU || (scalar >= 0xd800U && scalar <= 0xdfffU))
            {
                fail(ErrorKind::InvalidUnicode, "VXL2003", "String literal contains an invalid Unicode scalar");
                return nullptr;
            }
            units.push_back(llvm::ConstantInt::get(i32, scalar, false));
        }
        units.push_back(llvm::ConstantInt::get(i32, 0, false));
        auto *arrayType = llvm::ArrayType::get(i32, units.size());
        auto *initializer = llvm::ConstantArray::get(arrayType, units);
        const auto name = ".vxs.string." + std::to_string(string_index++);
        auto *global =
            new llvm::GlobalVariable(module, arrayType, true, llvm::GlobalValue::PrivateLinkage, initializer, name);
        global->setUnnamedAddr(llvm::GlobalValue::UnnamedAddr::Global);
        std::array<llvm::Constant *, 2> fields{
            global, llvm::ConstantInt::get(llvm::Type::getInt64Ty(context), text.size(), false)};
        return llvm::ConstantStruct::get(types.stringType, fields);
    }

    [[nodiscard]] auto Immediate(const xmm::Value &value) -> llvm::Value *
    {
        switch(value.type.kind)
        {
        case core::Type::Kind::Unit:
            return nullptr;
        case core::Type::Kind::Bool:
            return llvm::ConstantInt::get(llvm::Type::getInt1Ty(context), std::get<bool>(value.immediate));
        case core::Type::Kind::Int64:
            return llvm::ConstantInt::getSigned(llvm::Type::getInt64Ty(context),
                                                std::get<std::int64_t>(value.immediate));
        case core::Type::Kind::Int32:
            return llvm::ConstantInt::getSigned(llvm::Type::getInt32Ty(context),
                                                std::get<std::int32_t>(value.immediate));
        case core::Type::Kind::String:
            return StringLiteral(std::get<std::u32string>(value.immediate));
        case core::Type::Kind::Function:
        case core::Type::Kind::Named:
        case core::Type::Kind::TypeVariable:
            return nullptr;
        }
        return nullptr;
    }

    [[nodiscard]] auto LoadValue(llvm::IRBuilder<> &builder, FunctionState &state, const xmm::Value &value)
        -> llvm::Value *
    {
        if(value.kind == xmm::Value::Kind::Immediate)
            return Immediate(value);
        if(value.kind == xmm::Value::Kind::Function)
        {
            const auto found = functions.find(value.symbol);
            return found == functions.end() ? nullptr : found->second.value;
        }
        const auto slot = state.slots.find(value.reg);
        if(slot == state.slots.end())
            return nullptr;
        const auto name = "r" + std::to_string(value.reg) + ".load";
        return builder.CreateLoad(types.Lower(value.type), slot->second, name);
    }

    [[nodiscard]] auto LowerFloorDiv(llvm::IRBuilder<> &builder, llvm::Value *left, llvm::Value *right) -> llvm::Value *
    {
        // LLVM sdiv truncates toward zero. Visual X# floor division rounds toward
        // negative infinity, so a non-exact quotient with unlike operand signs must be
        // reduced by one. XOR lets us test sign disagreement without branching.
        auto *quotient = builder.CreateSDiv(left, right, "floor.quotient");
        auto *remainder = builder.CreateSRem(left, right, "floor.remainder");
        auto *zero = llvm::Constant::getNullValue(left->getType());
        auto *hasRemainder = builder.CreateICmpNE(remainder, zero, "floor.has.remainder");
        auto *signBits = builder.CreateXor(left, right, "floor.sign.bits");
        auto *signsDiffer = builder.CreateICmpSLT(signBits, zero, "floor.signs.differ");
        auto *adjust = builder.CreateAnd(hasRemainder, signsDiffer, "floor.adjust");
        auto *adjustment = builder.CreateZExt(adjust, left->getType(), "floor.adjustment");
        return builder.CreateSub(quotient, adjustment, "floor.result");
    }

    [[nodiscard]] auto LowerCall(llvm::IRBuilder<> &builder, FunctionState &state, const xmm::Instruction &instruction)
        -> llvm::Value *
    {
        // Operand zero is a function identity, not a virtual register containing a
        // raw address. Resolving the preserved symbol id here keeps calls deterministic
        // and leaves room for a later linkage/mangling policy without changing Xmm.
        const auto target = functions.find(instruction.operands.front().symbol);
        if(target == functions.end())
            return nullptr;
        std::vector<llvm::Value *> arguments;
        arguments.reserve(instruction.operands.size() - 1U);
        for(std::size_t index = 1; index < instruction.operands.size(); ++index)
            arguments.push_back(LoadValue(builder, state, instruction.operands[index]));
        if(std::ranges::any_of(arguments, [](const llvm::Value *value) { return value == nullptr; }))
            return nullptr;
        const auto *name = instruction.result_type.kind == core::Type::Kind::Unit ? "" : "call.result";
        return builder.CreateCall(target->second.type, target->second.value, arguments, name);
    }

    [[nodiscard]] auto LowerInstruction(llvm::IRBuilder<> &builder, FunctionState &state,
                                        const xmm::Instruction &instruction) -> bool
    {
        if(instruction.opcode == xmm::Opcode::Call)
        {
            auto *result = LowerCall(builder, state, instruction);
            if(result == nullptr)
                return false;
            if(instruction.has_result)
                builder.CreateStore(result, state.slots.at(instruction.destination));
            return true;
        }
        std::vector<llvm::Value *> operands;
        operands.reserve(instruction.operands.size());
        for(const auto &operand : instruction.operands)
            operands.push_back(LoadValue(builder, state, operand));
        if(std::ranges::any_of(operands, [](const llvm::Value *value) { return value == nullptr; }))
            return false;

        llvm::Value *result{};
        switch(instruction.opcode)
        {
        case xmm::Opcode::LoadImmediate:
        case xmm::Opcode::Move:
            result = operands[0];
            break;
        case xmm::Opcode::AddI64:
            result = builder.CreateAdd(operands[0], operands[1], "add");
            break;
        case xmm::Opcode::SubI64:
            result = builder.CreateSub(operands[0], operands[1], "sub");
            break;
        case xmm::Opcode::MulI64:
            result = builder.CreateMul(operands[0], operands[1], "mul");
            break;
        case xmm::Opcode::DivI64:
            result = builder.CreateSDiv(operands[0], operands[1], "div");
            break;
        case xmm::Opcode::FloorDivI64:
            result = LowerFloorDiv(builder, operands[0], operands[1]);
            break;
        case xmm::Opcode::RemI64:
            result = builder.CreateSRem(operands[0], operands[1], "rem");
            break;
        case xmm::Opcode::CompareLessI64:
            result = builder.CreateICmpSLT(operands[0], operands[1], "less");
            break;
        case xmm::Opcode::CompareLessEqualI64:
            result = builder.CreateICmpSLE(operands[0], operands[1], "less.equal");
            break;
        case xmm::Opcode::CompareGreaterI64:
            result = builder.CreateICmpSGT(operands[0], operands[1], "greater");
            break;
        case xmm::Opcode::CompareGreaterEqualI64:
            result = builder.CreateICmpSGE(operands[0], operands[1], "greater.equal");
            break;
        case xmm::Opcode::CompareEqual:
            result = builder.CreateICmpEQ(operands[0], operands[1], "equal");
            break;
        case xmm::Opcode::CompareNotEqual:
            result = builder.CreateICmpNE(operands[0], operands[1], "not.equal");
            break;
        case xmm::Opcode::AndBool:
            result = builder.CreateAnd(operands[0], operands[1], "logical.and");
            break;
        case xmm::Opcode::OrBool:
            result = builder.CreateOr(operands[0], operands[1], "logical.or");
            break;
        case xmm::Opcode::NegateI64:
            result = builder.CreateNeg(operands[0], "negate");
            break;
        case xmm::Opcode::NotBool:
            result = builder.CreateNot(operands[0], "logical.not");
            break;
        case xmm::Opcode::Call:
            break;
        }
        if(instruction.has_result && result != nullptr)
            builder.CreateStore(result, state.slots.at(instruction.destination));
        return result != nullptr;
    }

    [[nodiscard]] auto LowerTerminator(llvm::IRBuilder<> &builder, FunctionState &state,
                                       const xmm::Terminator &terminator) -> bool
    {
        switch(terminator.kind)
        {
        case xmm::Terminator::Kind::Return:
            if(state.source->return_type.kind == core::Type::Kind::Unit)
                builder.CreateRetVoid();
            else
            {
                auto *value = LoadValue(builder, state, terminator.value);
                if(value == nullptr)
                    return false;
                builder.CreateRet(value);
            }
            return true;
        case xmm::Terminator::Kind::Branch:
        {
            auto *condition = LoadValue(builder, state, terminator.value);
            if(condition == nullptr)
                return false;
            builder.CreateCondBr(condition, state.blocks.at(terminator.true_target),
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

    [[nodiscard]] auto DefineFunction(FunctionState &state) -> bool
    {
        if(!CreateBlocksAndSlots(state))
            return false;
        llvm::IRBuilder<> builder(context);
        for(const auto &block : state.source->blocks)
        {
            builder.SetInsertPoint(state.blocks.at(block.id));
            for(const auto &instruction : block.instructions)
                if(!LowerInstruction(builder, state, instruction))
                    return false;
            if(!LowerTerminator(builder, state, block.terminator))
                return false;
        }
        return true;
    }
};

[[nodiscard]] auto PassOptimizationLevel(OptimizationLevel level) -> llvm::OptimizationLevel
{
    switch(level)
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

void Optimize(llvm::Module &module, OptimizationLevel level)
{
    // Build the standard per-module pipeline through LLVM's C++ pass manager. Keeping
    // every analysis manager local makes ownership explicit and prevents global pass
    // state from leaking between compiler invocations in the same process.
    llvm::LoopAnalysisManager loopAnalyses;
    llvm::FunctionAnalysisManager functionAnalyses;
    llvm::CGSCCAnalysisManager cgsccAnalyses;
    llvm::ModuleAnalysisManager moduleAnalyses;
    llvm::PassBuilder passBuilder;
    passBuilder.registerModuleAnalyses(moduleAnalyses);
    passBuilder.registerCGSCCAnalyses(cgsccAnalyses);
    passBuilder.registerFunctionAnalyses(functionAnalyses);
    passBuilder.registerLoopAnalyses(loopAnalyses);
    passBuilder.crossRegisterProxies(loopAnalyses, functionAnalyses, cgsccAnalyses, moduleAnalyses);
    auto pipeline = passBuilder.buildPerModuleDefaultPipeline(PassOptimizationLevel(level));
    pipeline.run(module, moduleAnalyses);
}
} // namespace

auto Lower(const Xmm::Module &source, const Options &options) -> Result
{
    // Reject malformed Xmm before allocating LLVM state. Besides clearer diagnostics,
    // this keeps construction free to rely on verified block, type and symbol
    // invariants instead of duplicating defensive checks at every IRBuilder call.
    auto issues = Verify(source);
    if(!issues.empty())
    {
        Result result;
        result.error =
            Error{ErrorKind::InvalidXmm, "VXL2000", "Xmm verification failed before LLVM lowering", std::move(issues)};
        return result;
    }

    llvm::LLVMContext context;
    const auto name = ModuleName(source);
    if(!name)
        return Failure(ErrorKind::InvalidUnicode, "VXL2001", "module name contains an invalid Unicode scalar");
    llvm::Module module(*name, context);

    std::string triple = options.target_triple;
    if(triple.empty())
        triple = llvm::sys::getDefaultTargetTriple();
    if(triple.empty())
        return Failure(ErrorKind::LlvmConstruction, "VXL2004", "LLVM did not provide a default target triple");
    module.setTargetTriple(llvm::Triple(triple));

    Generator generator(context, module);
    if(!generator.DeclareFunctions(source))
        return Result{std::nullopt, std::move(generator.error)};
    for(auto &[_, function] : generator.functions)
        if(!generator.DefineFunction(function))
            return Failure(ErrorKind::LlvmConstruction, "VXL2005", "failed to lower an Xmm instruction or terminator");

    if(options.verify_module)
    {
        // LLVM verification runs before optimization so a backend construction defect
        // cannot be hidden or transformed into a less useful pass-manager failure.
        std::string message;
        llvm::raw_string_ostream diagnostics(message);
        if(llvm::verifyModule(module, &diagnostics))
        {
            diagnostics.flush();
            return Failure(ErrorKind::LlvmVerification, "VXL2006",
                           message.empty() ? "LLVM rejected the generated module" : std::move(message));
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
    // Copy both products before the local module and context are destroyed. Artifact
    // consequently has ordinary C++ value semantics and no LLVM ABI or lifetime
    // obligation is imposed on CLI, tests or future object emitters.
    artifact.llvm_ir = std::move(printed);
    artifact.bitcode.assign(reinterpret_cast<const std::uint8_t *>(bitcode.data()),
                            reinterpret_cast<const std::uint8_t *>(bitcode.data() + bitcode.size()));
    artifact.target_triple = std::move(triple);
    artifact.function_count = source.functions.size();
    return Result{std::move(artifact), std::nullopt};
}
} // namespace Visual::XSharp::Backend::LLVM
