// SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
// SPDX-License-Identifier: MPL-2.0

#include "Visual/XSharp/Core/CorePrep/Wire.hpp"

#include <limits>
#include <string_view>
#include <type_traits>

namespace visual_xsharp::core::wire
{
namespace
{
class Writer final
{
public:
    explicit Writer(const Limits &limits) : limits_(limits) {}

    [[nodiscard]] auto finish() && -> EncodeResult
    {
        if(!error_ && bytes_.size() > limits_.maximum_wire_bytes)
            fail(ErrorKind::LimitExceeded, "wire byte length", "encoded document exceeds configured byte limit");
        return EncodeResult{std::move(bytes_), std::move(error_)};
    }

    void document(const CorePrepModule &module)
    {
        for(const auto value : magic)
            byte(value);
        unsigned_integer(current_version);
        unsigned_integer<std::uint16_t>(0);
        qualified_name(module.name, "module name");
        vector(module.functions, limits_.maximum_functions, "function count",
               [this](const Function &function) { this->function(function); });
    }

private:
    const Limits &limits_;
    std::vector<std::uint8_t> bytes_;
    std::optional<Error> error_;

    void fail(ErrorKind kind, std::string context, std::string message)
    {
        if(!error_)
            error_ = Error{kind, bytes_.size(), std::move(context), std::move(message)};
    }

    void byte(std::uint8_t value)
    {
        if(!error_)
            bytes_.push_back(value);
    }

    template <typename Integer>
    void unsigned_integer(Integer value)
    {
        static_assert(std::is_unsigned_v<Integer>);
        for(std::size_t shift = 0; shift < sizeof(Integer) * 8U; shift += 8U)
            byte(static_cast<std::uint8_t>((value >> shift) & static_cast<Integer>(0xffU)));
    }

    void count(std::size_t value, std::size_t maximum, std::string_view context)
    {
        if(value > maximum || value > std::numeric_limits<std::uint32_t>::max())
        {
            fail(ErrorKind::LimitExceeded, std::string(context), "collection count exceeds wire limit");
            return;
        }
        unsigned_integer(static_cast<std::uint32_t>(value));
    }

    template <typename Value, typename Encode>
    void vector(const std::vector<Value> &values, std::size_t maximum, std::string_view context, Encode encode)
    {
        count(values.size(), maximum, context);
        if(error_)
            return;
        for(const auto &value : values)
        {
            encode(value);
            if(error_)
                return;
        }
    }

    void text(const std::u32string &value, std::string_view context)
    {
        count(value.size(), limits_.maximum_string_code_points, context);
        for(const auto code_point : value)
        {
            const auto numeric = static_cast<std::uint32_t>(code_point);
            if(numeric > 0x10ffffU || (numeric >= 0xd800U && numeric <= 0xdfffU))
            {
                fail(ErrorKind::InvalidCodePoint, std::string(context), "text contains a non-scalar Unicode code point");
                return;
            }
            unsigned_integer(numeric);
        }
    }

    void qualified_name(const std::vector<std::u32string> &parts, std::string_view context)
    {
        vector(parts, 65535U, context, [this, context](const std::u32string &part) { text(part, context); });
    }

    void symbol(const SymbolName &name, std::string_view context)
    {
        if(name.id == 0)
        {
            fail(ErrorKind::InvalidSymbol, std::string(context), "symbol id must be positive");
            return;
        }
        unsigned_integer(name.id);
        text(name.spelling, context);
    }

    void type(const Type &value, std::size_t depth = 0)
    {
        if(depth > limits_.maximum_type_depth)
        {
            fail(ErrorKind::LimitExceeded, "type", "type nesting exceeds configured limit");
            return;
        }
        byte(static_cast<std::uint8_t>(value.kind));
        switch(value.kind)
        {
        case Type::Kind::Unit:
        case Type::Kind::Bool:
        case Type::Kind::Int64:
        case Type::Kind::Int32:
        case Type::Kind::String: return;
        case Type::Kind::Function:
            if(value.components.empty())
            {
                fail(ErrorKind::UnsupportedType, "function type", "function type has no result component");
                return;
            }
            count(value.components.size() - 1U, 65535U, "function type parameter count");
            for(std::size_t index = 0; index + 1U < value.components.size(); ++index)
                type(value.components[index], depth + 1U);
            type(value.components.back(), depth + 1U);
            return;
        case Type::Kind::Named:
            qualified_name(value.name, "named type");
            vector(value.components, 65535U, "type argument count",
                   [this, depth](const Type &argument) { type(argument, depth + 1U); });
            return;
        case Type::Kind::TypeVariable:
            symbol(value.variable, "type variable symbol");
            return;
        }
    }

    void literal(const Atom &value)
    {
        switch(value.type.kind)
        {
        case Type::Kind::Unit:
            if(!std::holds_alternative<std::monostate>(value.literal))
                fail(ErrorKind::UnsupportedType, "unit literal", "literal payload does not match unit type");
            return;
        case Type::Kind::Bool:
            if(const auto *boolean = std::get_if<bool>(&value.literal))
                byte(*boolean ? 1U : 0U);
            else
                fail(ErrorKind::UnsupportedType, "bool literal", "literal payload does not match bool type");
            return;
        case Type::Kind::Int64:
            if(const auto *integer = std::get_if<std::int64_t>(&value.literal))
                unsigned_integer(static_cast<std::uint64_t>(*integer));
            else
                fail(ErrorKind::UnsupportedType, "int64 literal", "literal payload does not match int64 type");
            return;
        case Type::Kind::Int32:
            if(const auto *integer = std::get_if<std::int32_t>(&value.literal))
                unsigned_integer(static_cast<std::uint32_t>(*integer));
            else
                fail(ErrorKind::UnsupportedType, "int32 literal", "literal payload does not match int32 type");
            return;
        case Type::Kind::String:
            if(const auto *string = std::get_if<std::u32string>(&value.literal))
                text(*string, "string literal");
            else
                fail(ErrorKind::UnsupportedType, "string literal", "literal payload does not match string type");
            return;
        case Type::Kind::Function:
        case Type::Kind::Named:
        case Type::Kind::TypeVariable:
            fail(ErrorKind::UnsupportedType, "literal", "non-primitive literal types cannot cross CorePrep wire");
            return;
        }
    }

    void atom(const Atom &value)
    {
        byte(static_cast<std::uint8_t>(value.kind));
        type(value.type);
        if(value.kind == Atom::Kind::Variable)
            symbol(value.symbol, "variable symbol");
        else
            literal(value);
    }

    void operation(Operation operation, const std::vector<Atom> &operands)
    {
        byte(static_cast<std::uint8_t>(operation));
        vector(operands, limits_.maximum_operands_per_instruction, "operand count",
               [this](const Atom &operand) { atom(operand); });
    }

    void instruction(const Instruction &value)
    {
        byte(static_cast<std::uint8_t>(value.kind));
        switch(value.kind)
        {
        case Instruction::Kind::Bind:
            symbol(value.destination, "binding symbol");
            type(value.type);
            byte(value.mutable_binding ? 1U : 0U);
            operation(value.operation, value.operands);
            return;
        case Instruction::Kind::Assign:
            symbol(value.destination, "assignment symbol");
            if(value.operands.size() != 1U)
            {
                fail(ErrorKind::InvalidCount, "assignment", "assignment must contain exactly one atom");
                return;
            }
            atom(value.operands.front());
            return;
        case Instruction::Kind::Evaluate:
            operation(value.operation, value.operands);
            return;
        }
    }

    void terminator(const Terminator &value)
    {
        byte(static_cast<std::uint8_t>(value.kind));
        switch(value.kind)
        {
        case Terminator::Kind::Return: atom(value.value); return;
        case Terminator::Kind::Branch:
            atom(value.value);
            unsigned_integer(value.true_target);
            unsigned_integer(value.false_target);
            return;
        case Terminator::Kind::Jump: unsigned_integer(value.true_target); return;
        case Terminator::Kind::Unreachable: return;
        }
    }

    void block(const Block &value)
    {
        unsigned_integer(value.id);
        vector(value.instructions, limits_.maximum_instructions_per_block, "instruction count",
               [this](const Instruction &item) { instruction(item); });
        terminator(value.terminator);
    }

    void parameter(const Parameter &value)
    {
        symbol(value.symbol, "parameter symbol");
        type(value.type);
    }

    void function(const Function &value)
    {
        symbol(value.symbol, "function symbol");
        vector(value.parameters, limits_.maximum_parameters_per_function, "parameter count",
               [this](const Parameter &item) { parameter(item); });
        type(value.return_type);
        unsigned_integer(value.entry);
        vector(value.blocks, limits_.maximum_blocks_per_function, "block count",
               [this](const Block &item) { block(item); });
    }
};
} // namespace

auto encode(const CorePrepModule &module, const Limits &limits) -> EncodeResult
{
    Writer writer(limits);
    writer.document(module);
    return std::move(writer).finish();
}
} // namespace visual_xsharp::core::wire
