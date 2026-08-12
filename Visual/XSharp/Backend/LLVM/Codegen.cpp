// SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
// SPDX-License-Identifier: MPL-2.0

#include "Visual/XSharp/Backend/LLVM.hpp"

#include <llvm-c/Analysis.h>
#include <llvm-c/BitWriter.h>
#include <llvm-c/Core.h>
#include <llvm-c/Error.h>
#include <llvm-c/TargetMachine.h>
#include <llvm-c/Transforms/PassBuilder.h>

#include <algorithm>
#include <array>
#include <limits>
#include <memory>
#include <ranges>
#include <string>
#include <unordered_map>
#include <utility>

namespace Visual::XSharp::Backend::LLVM
{
using namespace ::visual_xsharp;
namespace
{
// The LLVM C API exposes independent ownership rules for contexts, modules, builders,
// buffers and messages. Keeping each handle in a tiny owner makes every early error
// return safe and prevents LLVM implementation details from escaping the backend.
struct ContextOwner final
{
    LLVMContextRef value{LLVMContextCreate()};
    ~ContextOwner() { LLVMContextDispose(value); }
    ContextOwner(const ContextOwner &) = delete;
    auto operator=(const ContextOwner &) -> ContextOwner & = delete;
    ContextOwner() = default;
};

struct ModuleOwner final
{
    LLVMModuleRef value{};
    ~ModuleOwner()
    {
        if(value != nullptr)
            LLVMDisposeModule(value);
    }
    ModuleOwner(const ModuleOwner &) = delete;
    auto operator=(const ModuleOwner &) -> ModuleOwner & = delete;
    explicit ModuleOwner(LLVMModuleRef module) : value(module) {}
};

struct BuilderOwner final
{
    LLVMBuilderRef value{};
    ~BuilderOwner()
    {
        if(value != nullptr)
            LLVMDisposeBuilder(value);
    }
    BuilderOwner(const BuilderOwner &) = delete;
    auto operator=(const BuilderOwner &) -> BuilderOwner & = delete;
    explicit BuilderOwner(LLVMContextRef context) : value(LLVMCreateBuilderInContext(context)) {}
};

struct BufferOwner final
{
    LLVMMemoryBufferRef value{};
    ~BufferOwner()
    {
        if(value != nullptr)
            LLVMDisposeMemoryBuffer(value);
    }
    explicit BufferOwner(LLVMMemoryBufferRef buffer) : value(buffer) {}
};

struct MessageOwner final
{
    char *value{};
    ~MessageOwner()
    {
        if(value != nullptr)
            LLVMDisposeMessage(value);
    }
    explicit MessageOwner(char *message) : value(message) {}
};

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
    LLVMContextRef context{};
    LLVMTypeRef string_type{};

    explicit TypeLowerer(LLVMContextRef llvmContext) : context(llvmContext)
    {
        // String is { pointer-to-Unicode-scalar, scalar-count }. The count is i64 so
        // indexing capacity is target-independent, and it excludes the sentinel used
        // to make debugger inspection convenient.
        std::array<LLVMTypeRef, 2> fields{LLVMPointerType(LLVMInt32TypeInContext(context), 0),
                                          LLVMInt64TypeInContext(context)};
        string_type = LLVMStructTypeInContext(context, fields.data(), static_cast<unsigned>(fields.size()), false);
    }

    [[nodiscard]] auto Lower(const core::Type &type) const -> LLVMTypeRef
    {
        switch(type.kind)
        {
        case core::Type::Kind::Unit: return LLVMVoidTypeInContext(context);
        case core::Type::Kind::Bool: return LLVMInt1TypeInContext(context);
        case core::Type::Kind::Int64: return LLVMInt64TypeInContext(context);
        case core::Type::Kind::Int32: return LLVMInt32TypeInContext(context);
        case core::Type::Kind::String: return string_type;
        case core::Type::Kind::Function:
        {
            if(type.components.empty())
                return nullptr;
            std::vector<LLVMTypeRef> parameters;
            parameters.reserve(type.components.size() - 1U);
            for(std::size_t index = 0; index + 1U < type.components.size(); ++index)
                parameters.push_back(Lower(type.components[index]));
            if(std::ranges::any_of(parameters, [](LLVMTypeRef item) { return item == nullptr; }))
                return nullptr;
            auto *result = Lower(type.components.back());
            return result == nullptr ? nullptr
                                     : LLVMFunctionType(result, parameters.data(), static_cast<unsigned>(parameters.size()), false);
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
    LLVMValueRef value{};
    LLVMTypeRef type{};
    std::unordered_map<xmm::BlockId, LLVMBasicBlockRef> blocks;
    std::unordered_map<xmm::VirtualRegister, LLVMValueRef> slots;
    std::unordered_map<xmm::VirtualRegister, core::Type> register_types;
};

struct Generator final
{
    LLVMContextRef context{};
    LLVMModuleRef module{};
    TypeLowerer types;
    std::unordered_map<core::SymbolId, FunctionState> functions;
    std::uint64_t string_index{};
    std::optional<Error> error;

    Generator(LLVMContextRef llvmContext, LLVMModuleRef llvmModule) : context(llvmContext), module(llvmModule), types(context) {}

    void fail(ErrorKind kind, std::string code, std::string message)
    {
        if(!error)
            error = Error{kind, std::move(code), std::move(message), {}};
    }

    [[nodiscard]] auto FunctionType(const xmm::Function &function) -> LLVMTypeRef
    {
        std::vector<LLVMTypeRef> parameters;
        parameters.reserve(function.parameter_types.size());
        for(const auto &parameter : function.parameter_types)
            parameters.push_back(types.Lower(parameter));
        auto *result = types.Lower(function.return_type);
        if(result == nullptr || std::ranges::any_of(parameters, [](LLVMTypeRef item) { return item == nullptr; }))
            return nullptr;
        return LLVMFunctionType(result, parameters.data(), static_cast<unsigned>(parameters.size()), false);
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
                fail(ErrorKind::InvalidUnicode, "VXL2001", "module or function name contains an invalid Unicode scalar");
                return false;
            }
            if(type == nullptr)
            {
                fail(ErrorKind::UnsupportedType, "VXL2002", "function signature cannot be represented in LLVM");
                return false;
            }
            auto *value = LLVMAddFunction(module, name->c_str(), type);
            LLVMSetLinkage(value, LLVMExternalLinkage);
            functions.emplace(function.symbol.id, FunctionState{&function, value, type, {}, {}, {}});
        }
        return true;
    }

    void DiscoverRegisters(FunctionState &state)
    {
        for(std::size_t index = 0; index < state.source->parameter_registers.size(); ++index)
            state.register_types.emplace(state.source->parameter_registers[index], state.source->parameter_types[index]);
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
            state.blocks.emplace(block.id, LLVMAppendBasicBlockInContext(context, state.value, blockName.c_str()));
        }
        const auto entry = state.blocks.find(state.source->entry);
        if(entry == state.blocks.end())
            return false;

        BuilderOwner builder(context);
        LLVMPositionBuilderAtEnd(builder.value, entry->second);
        DiscoverRegisters(state);
        std::vector<xmm::VirtualRegister> ordered;
        ordered.reserve(state.register_types.size());
        for(const auto &[reg, _] : state.register_types)
            ordered.push_back(reg);
        std::ranges::sort(ordered);
        for(const auto reg : ordered)
        {
            auto *type = types.Lower(state.register_types.at(reg));
            if(type == nullptr || LLVMGetTypeKind(type) == LLVMVoidTypeKind)
                continue;
            const auto name = "r" + std::to_string(reg);
            state.slots.emplace(reg, LLVMBuildAlloca(builder.value, type, name.c_str()));
        }
        for(std::size_t index = 0; index < state.source->parameter_registers.size(); ++index)
        {
            const auto reg = state.source->parameter_registers[index];
            const auto slot = state.slots.find(reg);
            if(slot != state.slots.end())
                LLVMBuildStore(builder.value, LLVMGetParam(state.value, static_cast<unsigned>(index)), slot->second);
        }
        return true;
    }

    [[nodiscard]] auto StringLiteral(const std::u32string &text) -> LLVMValueRef
    {
        // Store one i32 per Unicode scalar. Visual X# String is intentionally not UTF-8:
        // scalar indexing must not depend on the encoded byte width of earlier text.
        // A trailing zero is storage convenience only and is excluded from the length.
        auto *i32 = LLVMInt32TypeInContext(context);
        std::vector<LLVMValueRef> units;
        units.reserve(text.size() + 1U);
        for(const auto point : text)
        {
            const auto scalar = static_cast<std::uint32_t>(point);
            if(scalar > 0x10ffffU || (scalar >= 0xd800U && scalar <= 0xdfffU))
            {
                fail(ErrorKind::InvalidUnicode, "VXL2003", "String literal contains an invalid Unicode scalar");
                return nullptr;
            }
            units.push_back(LLVMConstInt(i32, scalar, false));
        }
        units.push_back(LLVMConstInt(i32, 0, false));
        auto *array_type = LLVMArrayType2(i32, units.size());
        auto *initializer = LLVMConstArray2(i32, units.data(), units.size());
        const auto name = ".vxs.string." + std::to_string(string_index++);
        auto *global = LLVMAddGlobal(module, array_type, name.c_str());
        LLVMSetInitializer(global, initializer);
        LLVMSetGlobalConstant(global, true);
        LLVMSetLinkage(global, LLVMPrivateLinkage);
        LLVMSetUnnamedAddress(global, LLVMGlobalUnnamedAddr);
        std::array<LLVMValueRef, 2> fields{global, LLVMConstInt(LLVMInt64TypeInContext(context), text.size(), false)};
        return LLVMConstNamedStruct(types.string_type, fields.data(), static_cast<unsigned>(fields.size()));
    }

    [[nodiscard]] auto Immediate(const xmm::Value &value) -> LLVMValueRef
    {
        switch(value.type.kind)
        {
        case core::Type::Kind::Unit: return nullptr;
        case core::Type::Kind::Bool:
            return LLVMConstInt(LLVMInt1TypeInContext(context), std::get<bool>(value.immediate) ? 1U : 0U, false);
        case core::Type::Kind::Int64:
            return LLVMConstInt(LLVMInt64TypeInContext(context),
                                static_cast<std::uint64_t>(std::get<std::int64_t>(value.immediate)), true);
        case core::Type::Kind::Int32:
            return LLVMConstInt(LLVMInt32TypeInContext(context),
                                static_cast<std::uint32_t>(std::get<std::int32_t>(value.immediate)), true);
        case core::Type::Kind::String: return StringLiteral(std::get<std::u32string>(value.immediate));
        case core::Type::Kind::Function:
        case core::Type::Kind::Named:
        case core::Type::Kind::TypeVariable:
            return nullptr;
        }
        return nullptr;
    }

    [[nodiscard]] auto LoadValue(LLVMBuilderRef builder, FunctionState &state, const xmm::Value &value) -> LLVMValueRef
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
        return LLVMBuildLoad2(builder, types.Lower(value.type), slot->second, name.c_str());
    }

    [[nodiscard]] auto LowerFloorDiv(LLVMBuilderRef builder, LLVMValueRef left, LLVMValueRef right) -> LLVMValueRef
    {
        // LLVM sdiv truncates toward zero. Visual X# floor division rounds toward
        // negative infinity, so a non-exact quotient with unlike operand signs must be
        // reduced by one. XOR lets us test sign disagreement without branching.
        auto *quotient = LLVMBuildSDiv(builder, left, right, "floor.quotient");
        auto *remainder = LLVMBuildSRem(builder, left, right, "floor.remainder");
        auto *zero = LLVMConstNull(LLVMTypeOf(left));
        auto *has_remainder = LLVMBuildICmp(builder, LLVMIntNE, remainder, zero, "floor.has.remainder");
        auto *signs_differ = LLVMBuildICmp(builder, LLVMIntSLT, LLVMBuildXor(builder, left, right, "floor.sign.bits"), zero,
                                          "floor.signs.differ");
        auto *adjust = LLVMBuildAnd(builder, has_remainder, signs_differ, "floor.adjust");
        auto *adjustment = LLVMBuildZExt(builder, adjust, LLVMTypeOf(left), "floor.adjustment");
        return LLVMBuildSub(builder, quotient, adjustment, "floor.result");
    }

    [[nodiscard]] auto LowerCall(LLVMBuilderRef builder, FunctionState &state,
                                  const xmm::Instruction &instruction) -> LLVMValueRef
    {
        // Operand zero is a function identity, not a virtual register containing a
        // raw address. Resolving the preserved symbol id here keeps calls deterministic
        // and leaves room for a later linkage/mangling policy without changing Xmm.
        const auto target = functions.find(instruction.operands.front().symbol);
        if(target == functions.end())
            return nullptr;
        std::vector<LLVMValueRef> arguments;
        arguments.reserve(instruction.operands.size() - 1U);
        for(std::size_t index = 1; index < instruction.operands.size(); ++index)
            arguments.push_back(LoadValue(builder, state, instruction.operands[index]));
        if(std::ranges::any_of(arguments, [](LLVMValueRef value) { return value == nullptr; }))
            return nullptr;
        const auto *name = instruction.result_type.kind == core::Type::Kind::Unit ? "" : "call.result";
        return LLVMBuildCall2(builder, target->second.type, target->second.value, arguments.data(),
                              static_cast<unsigned>(arguments.size()), name);
    }

    [[nodiscard]] auto LowerInstruction(LLVMBuilderRef builder, FunctionState &state,
                                         const xmm::Instruction &instruction) -> bool
    {
        if(instruction.opcode == xmm::Opcode::Call)
        {
            auto *result = LowerCall(builder, state, instruction);
            if(result == nullptr)
                return false;
            if(instruction.has_result)
                LLVMBuildStore(builder, result, state.slots.at(instruction.destination));
            return true;
        }
        std::vector<LLVMValueRef> operands;
        operands.reserve(instruction.operands.size());
        for(const auto &operand : instruction.operands)
            operands.push_back(LoadValue(builder, state, operand));
        if(std::ranges::any_of(operands, [](LLVMValueRef value) { return value == nullptr; }))
            return false;

        LLVMValueRef result{};
        switch(instruction.opcode)
        {
        case xmm::Opcode::LoadImmediate:
        case xmm::Opcode::Move: result = operands[0]; break;
        case xmm::Opcode::AddI64: result = LLVMBuildAdd(builder, operands[0], operands[1], "add"); break;
        case xmm::Opcode::SubI64: result = LLVMBuildSub(builder, operands[0], operands[1], "sub"); break;
        case xmm::Opcode::MulI64: result = LLVMBuildMul(builder, operands[0], operands[1], "mul"); break;
        case xmm::Opcode::DivI64: result = LLVMBuildSDiv(builder, operands[0], operands[1], "div"); break;
        case xmm::Opcode::FloorDivI64: result = LowerFloorDiv(builder, operands[0], operands[1]); break;
        case xmm::Opcode::RemI64: result = LLVMBuildSRem(builder, operands[0], operands[1], "rem"); break;
        case xmm::Opcode::CompareLessI64:
            result = LLVMBuildICmp(builder, LLVMIntSLT, operands[0], operands[1], "less"); break;
        case xmm::Opcode::CompareLessEqualI64:
            result = LLVMBuildICmp(builder, LLVMIntSLE, operands[0], operands[1], "less.equal"); break;
        case xmm::Opcode::CompareGreaterI64:
            result = LLVMBuildICmp(builder, LLVMIntSGT, operands[0], operands[1], "greater"); break;
        case xmm::Opcode::CompareGreaterEqualI64:
            result = LLVMBuildICmp(builder, LLVMIntSGE, operands[0], operands[1], "greater.equal"); break;
        case xmm::Opcode::CompareEqual:
            result = LLVMBuildICmp(builder, LLVMIntEQ, operands[0], operands[1], "equal"); break;
        case xmm::Opcode::CompareNotEqual:
            result = LLVMBuildICmp(builder, LLVMIntNE, operands[0], operands[1], "not.equal"); break;
        case xmm::Opcode::AndBool: result = LLVMBuildAnd(builder, operands[0], operands[1], "logical.and"); break;
        case xmm::Opcode::OrBool: result = LLVMBuildOr(builder, operands[0], operands[1], "logical.or"); break;
        case xmm::Opcode::NegateI64: result = LLVMBuildNeg(builder, operands[0], "negate"); break;
        case xmm::Opcode::NotBool: result = LLVMBuildNot(builder, operands[0], "logical.not"); break;
        case xmm::Opcode::Call: break;
        }
        if(instruction.has_result && result != nullptr)
            LLVMBuildStore(builder, result, state.slots.at(instruction.destination));
        return result != nullptr;
    }

    [[nodiscard]] auto LowerTerminator(LLVMBuilderRef builder, FunctionState &state,
                                        const xmm::Terminator &terminator) -> bool
    {
        switch(terminator.kind)
        {
        case xmm::Terminator::Kind::Return:
            if(state.source->return_type.kind == core::Type::Kind::Unit)
                LLVMBuildRetVoid(builder);
            else
            {
                auto *value = LoadValue(builder, state, terminator.value);
                if(value == nullptr)
                    return false;
                LLVMBuildRet(builder, value);
            }
            return true;
        case xmm::Terminator::Kind::Branch:
        {
            auto *condition = LoadValue(builder, state, terminator.value);
            if(condition == nullptr)
                return false;
            LLVMBuildCondBr(builder, condition, state.blocks.at(terminator.true_target),
                            state.blocks.at(terminator.false_target));
            return true;
        }
        case xmm::Terminator::Kind::Jump:
            LLVMBuildBr(builder, state.blocks.at(terminator.true_target));
            return true;
        case xmm::Terminator::Kind::Unreachable:
            LLVMBuildUnreachable(builder);
            return true;
        }
        return false;
    }

    [[nodiscard]] auto DefineFunction(FunctionState &state) -> bool
    {
        if(!CreateBlocksAndSlots(state))
            return false;
        BuilderOwner builder(context);
        for(const auto &block : state.source->blocks)
        {
            LLVMPositionBuilderAtEnd(builder.value, state.blocks.at(block.id));
            for(const auto &instruction : block.instructions)
                if(!LowerInstruction(builder.value, state, instruction))
                    return false;
            if(!LowerTerminator(builder.value, state, block.terminator))
                return false;
        }
        return true;
    }
};

[[nodiscard]] auto PassPipeline(OptimizationLevel level) -> const char *
{
    switch(level)
    {
    case OptimizationLevel::Debug: return "default<O0>";
    case OptimizationLevel::Less: return "default<O1>";
    case OptimizationLevel::Default: return "default<O2>";
    case OptimizationLevel::Aggressive: return "default<O3>";
    }
    return "default<O2>";
}
} // namespace

auto Lower(const Xmm::Module &source, const Options &options) -> Result
{
    // Reject malformed Xmm before allocating LLVM state. Besides clearer diagnostics,
    // this keeps the construction code free to rely on verified block, type and symbol
    // invariants instead of duplicating defensive checks at every C API call.
    auto issues = Verify(source);
    if(!issues.empty())
    {
        Result result;
        result.error = Error{ErrorKind::InvalidXmm, "VXL2000", "Xmm verification failed before LLVM lowering",
                             std::move(issues)};
        return result;
    }

    ContextOwner context;
    const auto name = ModuleName(source);
    if(!name)
        return Failure(ErrorKind::InvalidUnicode, "VXL2001", "module name contains an invalid Unicode scalar");
    ModuleOwner module(LLVMModuleCreateWithNameInContext(name->c_str(), context.value));

    std::string triple = options.target_triple;
    if(triple.empty())
    {
        MessageOwner detected(LLVMGetDefaultTargetTriple());
        if(detected.value == nullptr)
            return Failure(ErrorKind::LlvmConstruction, "VXL2004", "LLVM did not provide a default target triple");
        triple = detected.value;
    }
    LLVMSetTarget(module.value, triple.c_str());

    Generator generator(context.value, module.value);
    if(!generator.DeclareFunctions(source))
        return Result{std::nullopt, std::move(generator.error)};
    for(auto &[_, function] : generator.functions)
        if(!generator.DefineFunction(function))
            return Failure(ErrorKind::LlvmConstruction, "VXL2005", "failed to lower an Xmm instruction or terminator");

    if(options.verify_module)
    {
        // LLVM verification runs before optimization so a backend construction defect
        // cannot be hidden or transformed into a less useful pass-manager failure.
        char *rawMessage{};
        if(LLVMVerifyModule(module.value, LLVMReturnStatusAction, &rawMessage) != 0)
        {
            MessageOwner message(rawMessage);
            return Failure(ErrorKind::LlvmVerification, "VXL2006",
                           message.value == nullptr ? "LLVM rejected the generated module" : message.value);
        }
    }

    auto *passOptions = LLVMCreatePassBuilderOptions();
    auto *passError = LLVMRunPasses(module.value, PassPipeline(options.optimization), nullptr, passOptions);
    LLVMDisposePassBuilderOptions(passOptions);
    if(passError != nullptr)
    {
        char *rawMessage = LLVMGetErrorMessage(passError);
        std::string message = rawMessage == nullptr ? "LLVM optimization pipeline failed" : rawMessage;
        if(rawMessage != nullptr)
            LLVMDisposeErrorMessage(rawMessage);
        return Failure(ErrorKind::LlvmConstruction, "VXL2007", std::move(message));
    }

    MessageOwner printed(LLVMPrintModuleToString(module.value));
    if(printed.value == nullptr)
        return Failure(ErrorKind::LlvmConstruction, "VXL2008", "LLVM could not print the generated module");
    BufferOwner bitcode(LLVMWriteBitcodeToMemoryBuffer(module.value));
    if(bitcode.value == nullptr)
        return Failure(ErrorKind::BitcodeEmission, "VXL2009", "LLVM could not serialize the module as bitcode");

    const auto *start = reinterpret_cast<const std::uint8_t *>(LLVMGetBufferStart(bitcode.value));
    const auto size = LLVMGetBufferSize(bitcode.value);
    Artifact artifact;
    // Copy both products before the local owners dispose the memory buffer, module and
    // context. Artifact consequently has ordinary C++ value semantics and no LLVM ABI
    // or lifetime obligation is imposed on CLI, tests or future object emitters.
    artifact.llvm_ir = printed.value;
    artifact.bitcode.assign(start, start + size);
    artifact.target_triple = std::move(triple);
    artifact.function_count = source.functions.size();
    return Result{std::move(artifact), std::nullopt};
}
} // namespace Visual::XSharp::Backend::LLVM
