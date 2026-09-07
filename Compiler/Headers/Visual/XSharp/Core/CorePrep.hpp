// SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
// SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.1
#pragma once

#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <utility>
#include <variant>
#include <vector>

#include "Visual/XSharp/Namespace.hpp"

namespace visual_xsharp::core
{
    using SymbolId = std::uint64_t;
    using BlockId = std::uint32_t;

    struct SymbolName final
    {
        SymbolId id{};
        std::u32string spelling;
        [[nodiscard]] auto
        operator==(const SymbolName &) const -> bool = default;
    };

    // Arbitrary-width values in specialization keys use the same canonical
    // sign/magnitude representation as runtime integer literals.  Keeping the
    // model ahead of Type lets a template argument refer to it without losing
    // host-independent integer width.
    struct IntegerLiteral final
    {
        bool negative{};
        std::vector<std::uint8_t> magnitude;
        [[nodiscard]] auto
        operator==(const IntegerLiteral &) const -> bool = default;
    };

    struct TemplateValue final
    {
        enum class Kind : std::uint8_t
        {
            Integer,
            Boolean,
            Character,
            Parameter
        };

        Kind kind{ Kind::Integer };
        IntegerLiteral integer;
        bool boolean{};
        SymbolName parameter;

        [[nodiscard]] static auto
        integer_value(IntegerLiteral value) -> TemplateValue;
        [[nodiscard]] static auto
        boolean_value(bool value) -> TemplateValue;
        [[nodiscard]] static auto
        character_value(IntegerLiteral value) -> TemplateValue;
        [[nodiscard]] static auto
        parameter_value(SymbolName value) -> TemplateValue;
        [[nodiscard]] auto
        operator==(const TemplateValue &) const -> bool = default;
    };

    struct Type;

    // shared_ptr breaks the recursive Type/TemplateArgument layout without
    // imposing pointer identity on equality. Constructors always allocate a
    // private immutable copy; operator== compares the pointed-to type value.
    struct TemplateArgument final
    {
        enum class Kind : std::uint8_t
        {
            Type,
            Value
        };

        Kind kind{ Kind::Type };
        std::shared_ptr<const Type> type;
        TemplateValue value;

        [[nodiscard]] static auto
        type_argument(Type value) -> TemplateArgument;
        [[nodiscard]] static auto
        value_argument(TemplateValue value) -> TemplateArgument;
        [[nodiscard]] auto
        operator==(const TemplateArgument &other) const -> bool;
    };

    struct Type final
    {
        enum class Kind : std::uint8_t
        {
            Unit,
            Bool,
            Character,
            Int8,
            Int16,
            Int64,
            Int32,
            Int128,
            UInt8,
            UInt16,
            UInt32,
            UInt64,
            UInt128,
            Float16,
            Float32,
            Float64,
            Float128,
            String,
            Function,
            Named,
            TypeVariable
        };
        Kind kind{ Kind::Unit };
        std::vector<std::u32string> name;
        // Function types keep parameters followed by the result in components.
        // Named types use ordered templateArguments exclusively.
        std::vector<Type> components;
        std::vector<TemplateArgument> templateArguments;
        SymbolName variable;

        [[nodiscard]] static auto
        unit() -> Type
        {
            return Type{ Kind::Unit, {}, {}, {}, {} };
        }
        [[nodiscard]] static auto
        boolean() -> Type
        {
            return Type{ Kind::Bool, {}, {}, {}, {} };
        }
        [[nodiscard]] static auto
        int64() -> Type
        {
            return Type{ Kind::Int64, {}, {}, {}, {} };
        }
        [[nodiscard]] static auto
        int32() -> Type
        {
            return Type{ Kind::Int32, {}, {}, {}, {} };
        }
        [[nodiscard]] static auto
        character() -> Type
        {
            return Type{ Kind::Character, {}, {}, {}, {} };
        }
        [[nodiscard]] static auto
        int8() -> Type
        {
            return Type{ Kind::Int8, {}, {}, {}, {} };
        }
        [[nodiscard]] static auto
        int16() -> Type
        {
            return Type{ Kind::Int16, {}, {}, {}, {} };
        }
        [[nodiscard]] static auto
        int128() -> Type
        {
            return Type{ Kind::Int128, {}, {}, {}, {} };
        }
        [[nodiscard]] static auto
        uint8() -> Type
        {
            return Type{ Kind::UInt8, {}, {}, {}, {} };
        }
        [[nodiscard]] static auto
        uint16() -> Type
        {
            return Type{ Kind::UInt16, {}, {}, {}, {} };
        }
        [[nodiscard]] static auto
        uint32() -> Type
        {
            return Type{ Kind::UInt32, {}, {}, {}, {} };
        }
        [[nodiscard]] static auto
        uint64() -> Type
        {
            return Type{ Kind::UInt64, {}, {}, {}, {} };
        }
        [[nodiscard]] static auto
        uint128() -> Type
        {
            return Type{ Kind::UInt128, {}, {}, {}, {} };
        }
        [[nodiscard]] static auto
        float16() -> Type
        {
            return Type{ Kind::Float16, {}, {}, {}, {} };
        }
        [[nodiscard]] static auto
        float32() -> Type
        {
            return Type{ Kind::Float32, {}, {}, {}, {} };
        }
        [[nodiscard]] static auto
        float64() -> Type
        {
            return Type{ Kind::Float64, {}, {}, {}, {} };
        }
        [[nodiscard]] static auto
        float128() -> Type
        {
            return Type{ Kind::Float128, {}, {}, {}, {} };
        }
        [[nodiscard]] static auto
        string() -> Type
        {
            return Type{ Kind::String, {}, {}, {}, {} };
        }
        [[nodiscard]] static auto
        function(std::vector<Type> parameters, Type result) -> Type
        {
            parameters.push_back(std::move(result));
            return Type{ Kind::Function, {}, std::move(parameters), {}, {} };
        }
        [[nodiscard]] static auto
        named(std::vector<std::u32string> qualified_name, std::vector<Type> arguments = {}) -> Type
        {
            std::vector<TemplateArgument> converted;
            converted.reserve(arguments.size());
            for (auto &argument : arguments)
                converted.push_back(TemplateArgument::type_argument(std::move(argument)));
            return Type{ Kind::Named, std::move(qualified_name), {}, std::move(converted), {} };
        }
        [[nodiscard]] static auto
        named_template(std::vector<std::u32string> qualified_name, std::vector<TemplateArgument> arguments) -> Type
        {
            return Type{ Kind::Named, std::move(qualified_name), {}, std::move(arguments), {} };
        }
        [[nodiscard]] static auto
        type_variable(SymbolName symbol) -> Type
        {
            return Type{ Kind::TypeVariable, {}, {}, {}, std::move(symbol) };
        }
        [[nodiscard]] auto
        operator==(const Type &) const -> bool = default;
    };

    inline auto
    TemplateValue::integer_value(IntegerLiteral value) -> TemplateValue
    {
        return TemplateValue{ Kind::Integer, std::move(value), false, {} };
    }

    inline auto
    TemplateValue::boolean_value(const bool value) -> TemplateValue
    {
        return TemplateValue{ Kind::Boolean, {}, value, {} };
    }

    inline auto
    TemplateValue::character_value(IntegerLiteral value) -> TemplateValue
    {
        return TemplateValue{ Kind::Character, std::move(value), false, {} };
    }

    inline auto
    TemplateValue::parameter_value(SymbolName value) -> TemplateValue
    {
        return TemplateValue{ Kind::Parameter, {}, false, std::move(value) };
    }

    inline auto
    TemplateArgument::type_argument(Type value) -> TemplateArgument
    {
        TemplateArgument result;
        result.kind = Kind::Type;
        result.type = std::make_shared<const Type>(std::move(value));
        return result;
    }

    inline auto
    TemplateArgument::value_argument(TemplateValue value) -> TemplateArgument
    {
        TemplateArgument result;
        result.kind = Kind::Value;
        result.value = std::move(value);
        return result;
    }

    inline auto
    TemplateArgument::operator==(const TemplateArgument &other) const -> bool
    {
        if (kind != other.kind)
            return false;
        if (kind == Kind::Value)
            return value == other.value;
        if (!type || !other.type)
            return type == other.type;
        return *type == *other.type;
    }

    // Floating-point literals retain their source-independent decimal spelling until LLVM
    // selects IEEE semantics for the declared scalar type.  The wire verifier accepts only
    // a deliberately small ASCII grammar, so this is not an unstructured text escape hatch.
    struct FloatingLiteral final
    {
        std::string spelling;
        [[nodiscard]] auto
        operator==(const FloatingLiteral &) const -> bool = default;
    };

    // int64_t and int32_t remain accepted for source compatibility with native clients that
    // constructed v2 modules directly. New decoders produce IntegerLiteral consistently.
    using Literal = std::variant<std::monostate, bool, std::int64_t, std::int32_t, IntegerLiteral, FloatingLiteral, std::u32string>;

    struct Atom final
    {
        enum class Kind : std::uint8_t
        {
            Variable,
            Literal
        } kind{ Kind::Literal };
        Type type{ Type::unit() };
        SymbolName symbol{};
        Literal literal{};

        [[nodiscard]] static auto
        variable(SymbolName name, Type value_type) -> Atom
        {
            return Atom{ Kind::Variable, std::move(value_type), std::move(name), {} };
        }
        [[nodiscard]] static auto
        variable(SymbolId id, Type value_type) -> Atom
        {
            return variable(SymbolName{ id, {} }, std::move(value_type));
        }
        [[nodiscard]] static auto
        constant(Literal value, Type value_type) -> Atom
        {
            return Atom{ Kind::Literal, std::move(value_type), {}, std::move(value) };
        }
        [[nodiscard]] auto
        operator==(const Atom &) const -> bool = default;
    };

    // Capture mode is part of the CorePrep contract because ownership cannot
    // be reconstructed once lexical bindings have been converted to an
    // environment layout.  The backend may optimize strong storage, but it
    // must preserve weak and unowned lifetime behavior.
    enum class CaptureMode : std::uint8_t
    {
        Strong,
        Weak,
        Unowned
    };

    struct Capture final
    {
        CaptureMode mode{ CaptureMode::Strong };
        SymbolName symbol{};
        Type type{ Type::unit() };
        Atom value{};
        [[nodiscard]] auto
        operator==(const Capture &) const -> bool = default;
    };

    enum class Operation : std::uint8_t
    {
        Copy,
        Call,
        Add,
        Subtract,
        Multiply,
        Divide,
        FloorDivide,
        Remainder,
        LessThan,
        LessEqual,
        GreaterThan,
        GreaterEqual,
        Equal,
        NotEqual,
        LogicalAnd,
        LogicalOr,
        Negate,
        LogicalNot,
        MakeClosure
    };

    struct Instruction final
    {
        enum class Kind : std::uint8_t
        {
            Bind,
            Assign,
            Evaluate
        } kind{ Kind::Evaluate };
        SymbolName destination{};
        Type type{ Type::unit() };
        bool mutable_binding{};
        Operation operation{ Operation::Copy };
        std::vector<Atom> operands;
        // Only MakeClosure uses these fields.  Keeping closure metadata next
        // to the operation avoids encoding a function symbol as a fake data
        // operand and makes ownership visible to Xpp verification.
        SymbolName closure_function{};
        std::vector<Capture> captures;
        [[nodiscard]] auto
        operator==(const Instruction &) const -> bool = default;
    };

    struct Terminator final
    {
        enum class Kind : std::uint8_t
        {
            Return,
            Branch,
            Jump,
            Unreachable
        } kind{ Kind::Unreachable };
        Atom value{};
        BlockId true_target{};
        BlockId false_target{};
        [[nodiscard]] auto
        operator==(const Terminator &) const -> bool = default;
    };

    struct Block final
    {
        BlockId id{};
        std::vector<Instruction> instructions;
        Terminator terminator;
        [[nodiscard]] auto
        operator==(const Block &) const -> bool = default;
    };

    struct Parameter final
    {
        SymbolName symbol{};
        Type type{ Type::unit() };
        [[nodiscard]] auto
        operator==(const Parameter &) const -> bool = default;
    };
    struct Function final
    {
        SymbolName symbol{};
        std::vector<Parameter> parameters;
        Type return_type{ Type::unit() };
        BlockId entry{};
        std::vector<Block> blocks;
        [[nodiscard]] auto
        operator==(const Function &) const -> bool = default;
    };
    struct CorePrepModule final
    {
        std::vector<std::u32string> name;
        std::vector<Function> functions;
        [[nodiscard]] auto
        operator==(const CorePrepModule &) const -> bool = default;
    };

} // namespace visual_xsharp::core
