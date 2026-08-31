// SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
// SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.0

#include <limits>
#include <string_view>
#include <type_traits>

#include "Visual/XSharp/Core/Scalar.hpp"
#include "Visual/XSharp/Core/Wire.hpp"

namespace Visual::XSharp::Core::Wire
{
    namespace
    {
        inline constexpr std::uint8_t kMagic[] = { 'V', 'X', 'C', 'R' };

        class Reader final
        {
        public:
            Reader(std::span<const std::uint8_t> bytes, const Limits &limits)
                : bytes_(bytes)
                , limits_(limits)
            {}

            [[nodiscard]] auto
            Document() -> DecodeResult
            {
                if (bytes_.size() > limits_.maximumWireBytes)
                    return Failure(ErrorKind::LimitExceeded, "wire byte length", "input exceeds configured byte limit");
                for (const auto expected : kMagic)
                    if (Byte("magic") != expected)
                        return Failure(ErrorKind::InvalidMagic, "magic", "input is not a Visual X# Core document");
                const auto version = Unsigned<std::uint16_t>("version");
                if (!error_ && version != kCurrentVersion)
                    Fail(ErrorKind::UnsupportedVersion, "version", "unsupported Core wire version");
                const auto flags = Unsigned<std::uint16_t>("flags");
                if (!error_ && flags != 0U)
                    Fail(ErrorKind::InvalidTag, "flags", "reserved flags must be zero");
                if (error_)
                    return Result();

                Module module;
                module.name = QualifiedName("module name");
                module.functions = Vector<Function>(limits_.maximumFunctions, "function count", [this] {
                    return ReadFunction();
                });
                if (!error_ && offset_ != bytes_.size())
                    Fail(ErrorKind::TrailingInput, "document", "bytes remain after Core module");
                if (!error_)
                    module_ = std::move(module);
                return Result();
            }

        private:
            std::span<const std::uint8_t> bytes_;
            const Limits &limits_;
            std::size_t offset_{};
            std::optional<Module> module_;
            std::optional<Error> error_;

            [[nodiscard]] auto
            Result() -> DecodeResult
            {
                return DecodeResult{ std::move(module_), std::move(error_) };
            }
            [[nodiscard]] auto
            Failure(ErrorKind kind, std::string context, std::string message) -> DecodeResult
            {
                Fail(kind, std::move(context), std::move(message));
                return Result();
            }
            void
            Fail(ErrorKind kind, std::string context, std::string message)
            {
                if (!error_)
                    error_ = Error{ kind, offset_, std::move(context), std::move(message) };
            }
            [[nodiscard]] auto
            Byte(std::string_view context) -> std::uint8_t
            {
                if (offset_ >= bytes_.size())
                {
                    Fail(ErrorKind::TruncatedInput, std::string(context), "input ended before field was complete");
                    return 0U;
                }
                return bytes_[offset_++];
            }
            template<typename Integer>
            [[nodiscard]] auto
            Unsigned(std::string_view context) -> Integer
            {
                static_assert(std::is_unsigned_v<Integer>);
                Integer result{};
                for (std::size_t shift = 0; shift < sizeof(Integer) * 8U; shift += 8U)
                    result |= static_cast<Integer>(Byte(context)) << shift;
                return result;
            }
            [[nodiscard]] auto
            Count(std::size_t maximum, std::string_view context) -> std::size_t
            {
                const auto value = Unsigned<std::uint32_t>(context);
                if (!error_ && value > maximum)
                    Fail(ErrorKind::LimitExceeded, std::string(context), "collection count exceeds configured limit");
                return error_ ? 0U : static_cast<std::size_t>(value);
            }
            template<typename Value, typename Decode>
            [[nodiscard]] auto
            Vector(std::size_t maximum, std::string_view context, Decode decode) -> std::vector<Value>
            {
                const auto size = Count(maximum, context);
                std::vector<Value> values;
                values.reserve(size);
                for (std::size_t index = 0; index < size && !error_; ++index)
                    values.push_back(decode());
                return values;
            }
            [[nodiscard]] auto
            Text(std::string_view context) -> std::u32string
            {
                const auto size = Count(limits_.maximumTextScalars, context);
                std::u32string value;
                value.reserve(size);
                for (std::size_t index = 0; index < size && !error_; ++index)
                {
                    const auto scalar = Unsigned<std::uint32_t>(context);
                    if (scalar > 0x10ffffU || (scalar >= 0xd800U && scalar <= 0xdfffU))
                    {
                        Fail(ErrorKind::InvalidScalar, std::string(context), "wire text contains a non-scalar Unicode value");
                        break;
                    }
                    value.push_back(static_cast<char32_t>(scalar));
                }
                return value;
            }
            [[nodiscard]] auto
            QualifiedName(std::string_view context) -> std::vector<std::u32string>
            {
                return Vector<std::u32string>(65535U, context, [this, context] {
                    return Text(context);
                });
            }
            [[nodiscard]] auto
            Symbol(std::string_view context) -> SymbolName
            {
                const auto id = Unsigned<SymbolId>(context);
                if (!error_ && id == 0U)
                    Fail(ErrorKind::InvalidSymbol, std::string(context), "symbol id must be positive");
                return SymbolName{ id, Text(context) };
            }
            [[nodiscard]] auto
            ReadType(std::size_t depth = 0U) -> Type
            {
                if (depth > limits_.maximumTypeDepth)
                {
                    Fail(ErrorKind::LimitExceeded, "type", "type nesting exceeds configured limit");
                    return Type::unit();
                }
                switch (Byte("type tag"))
                {
                    case 0:
                        return Type::unit();
                    case 1:
                        return Type::boolean();
                    case 2:
                        return Type::int64();
                    case 3:
                        return Type::string();
                    case 4:
                        return Type::named(QualifiedName("named type"),
                                           Vector<Type>(limits_.maximumOperands, "type argument count", [this, depth] {
                                               return ReadType(depth + 1U);
                                           }));
                    case 5:
                    {
                        auto parameters = Vector<Type>(limits_.maximumParameters, "function type parameter count", [this, depth] {
                            return ReadType(depth + 1U);
                        });
                        return Type::function(std::move(parameters), ReadType(depth + 1U));
                    }
                    case 6:
                        return Type::type_variable(Symbol("type variable symbol"));
                    case 7:
                        return Type::character();
                    case 8:
                        return Type::int8();
                    case 9:
                        return Type::int16();
                    case 10:
                        return Type::int32();
                    case 11:
                        return Type::int128();
                    case 12:
                        return Type::uint8();
                    case 13:
                        return Type::uint16();
                    case 14:
                        return Type::uint32();
                    case 15:
                        return Type::uint64();
                    case 16:
                        return Type::uint128();
                    case 17:
                        return Type::float16();
                    case 18:
                        return Type::float32();
                    case 19:
                        return Type::float64();
                    case 20:
                        return Type::float128();
                    default:
                        Fail(ErrorKind::InvalidTag, "type tag", "unknown Core type tag");
                        return Type::unit();
                }
            }
            [[nodiscard]] auto
            Boolean(std::string_view context) -> bool
            {
                const auto value = Byte(context);
                if (value > 1U)
                    Fail(ErrorKind::InvalidBoolean, std::string(context), "boolean byte must be zero or one");
                return value == 1U;
            }
            [[nodiscard]] auto
            ReadLiteral() -> Literal
            {
                switch (Byte("literal tag"))
                {
                    case 0:
                        return std::monostate{};
                    case 1:
                        return Boolean("boolean literal");
                    case 2:
                        return static_cast<std::int64_t>(Unsigned<std::uint64_t>("integer literal"));
                    case 3:
                        return Text("string literal");
                    case 4:
                    {
                        ::visual_xsharp::core::IntegerLiteral value;
                        value.negative = Boolean("integer sign");
                        value.magnitude = Vector<std::uint8_t>(limits_.maximumNumericBytes, "integer magnitude", [this] {
                            return Byte("integer magnitude");
                        });
                        if (!::visual_xsharp::core::integer_is_canonical(value))
                            Fail(ErrorKind::InvalidInteger, "integer literal", "integer magnitude/sign is not canonical");
                        return value;
                    }
                    case 5:
                    {
                        const auto size = Count(limits_.maximumNumericBytes, "floating literal length");
                        std::string spelling;
                        spelling.reserve(size);
                        for (std::size_t index = 0; index < size && !error_; ++index)
                        {
                            const auto value = Byte("floating literal");
                            if (value > 0x7fU)
                                Fail(ErrorKind::InvalidInteger, "floating literal", "floating spelling must be ASCII");
                            else
                                spelling.push_back(static_cast<char>(value));
                        }
                        if (!error_ && !::visual_xsharp::core::floating_spelling_is_valid(spelling))
                            Fail(ErrorKind::InvalidInteger, "floating literal", "floating spelling is not canonical");
                        return ::visual_xsharp::core::FloatingLiteral{ std::move(spelling) };
                    }
                    default:
                        Fail(ErrorKind::InvalidTag, "literal tag", "unknown Core literal tag");
                        return std::monostate{};
                }
            }
            [[nodiscard]] auto
            ReadExpression(std::size_t depth = 0U) -> Expression
            {
                if (depth > limits_.maximumExpressionDepth)
                {
                    Fail(ErrorKind::LimitExceeded, "expression", "expression nesting exceeds configured limit");
                    return {};
                }
                const auto tag = Byte("expression tag");
                const auto primitiveTag = tag == 3U ? Byte("primitive tag") : 0U;
                auto valueType = ReadType();
                switch (tag)
                {
                    case 0:
                        return Expression::Variable(Symbol("variable symbol"), std::move(valueType));
                    case 1:
                        return Expression::Constant(ReadLiteral(), std::move(valueType));
                    case 2:
                    {
                        auto callee = ReadExpression(depth + 1U);
                        auto arguments = Vector<Expression>(limits_.maximumOperands, "call argument count", [this, depth] {
                            return ReadExpression(depth + 1U);
                        });
                        return Expression::Apply(std::move(callee), std::move(arguments), std::move(valueType));
                    }
                    case 3:
                    {
                        if (primitiveTag > static_cast<std::uint8_t>(Primitive::LogicalNot))
                            Fail(ErrorKind::InvalidTag, "primitive tag", "unknown Core primitive tag");
                        auto arguments = Vector<Expression>(limits_.maximumOperands, "primitive operand count", [this, depth] {
                            return ReadExpression(depth + 1U);
                        });
                        return Expression::InvokePrimitive(static_cast<Primitive>(primitiveTag), std::move(arguments), std::move(valueType));
                    }
                    default:
                        Fail(ErrorKind::InvalidTag, "expression tag", "unknown Core expression tag");
                        return {};
                }
            }
            [[nodiscard]] auto
            ReadStatement() -> Statement
            {
                switch (Byte("statement tag"))
                {
                    case 0:
                    {
                        auto symbol = Symbol("binding symbol");
                        auto type = ReadType();
                        const auto mutableBinding = Boolean("binding mutability");
                        auto value = ReadExpression();
                        return Statement::Bind(Binding{ std::move(symbol), std::move(type), mutableBinding, std::move(value) });
                    }
                    case 1:
                    {
                        auto symbol = Symbol("assignment symbol");
                        return Statement::Assign(std::move(symbol), ReadExpression());
                    }
                    case 2:
                        return Statement::Return(ReadExpression());
                    case 3:
                    {
                        auto condition = ReadExpression();
                        auto whenTrue = Vector<Statement>(limits_.maximumStatements, "true branch statement count", [this] {
                            return ReadStatement();
                        });
                        auto whenFalse = Vector<Statement>(limits_.maximumStatements, "false branch statement count", [this] {
                            return ReadStatement();
                        });
                        return Statement::If(std::move(condition), std::move(whenTrue), std::move(whenFalse));
                    }
                    case 4:
                        return Statement::Evaluate(ReadExpression());
                    default:
                        Fail(ErrorKind::InvalidTag, "statement tag", "unknown Core statement tag");
                        return {};
                }
            }
            [[nodiscard]] auto
            ReadParameter() -> Parameter
            {
                return Parameter{ Symbol("parameter symbol"), ReadType() };
            }
            [[nodiscard]] auto
            ReadFunction() -> Function
            {
                Function function;
                function.symbol = Symbol("function symbol");
                function.parameters = Vector<Parameter>(limits_.maximumParameters, "parameter count", [this] {
                    return ReadParameter();
                });
                function.returnType = ReadType();
                function.body = Vector<Statement>(limits_.maximumStatements, "statement count", [this] {
                    return ReadStatement();
                });
                return function;
            }
        };

        class Writer final
        {
        public:
            explicit Writer(const Limits &limits)
                : limits_(limits)
            {}
            void
            Document(const Module &module)
            {
                for (const auto value : kMagic)
                    Byte(value);
                Unsigned(kCurrentVersion);
                Unsigned<std::uint16_t>(0U);
                QualifiedName(module.name, "module name");
                Vector(module.functions, limits_.maximumFunctions, "function count", [this](const Function &function) {
                    WriteFunction(function);
                });
            }
            [[nodiscard]] auto
            Finish() && -> EncodeResult
            {
                if (!error_ && bytes_.size() > limits_.maximumWireBytes)
                    Fail(ErrorKind::LimitExceeded, "wire byte length", "encoded document exceeds configured limit");
                return EncodeResult{ std::move(bytes_), std::move(error_) };
            }

        private:
            const Limits &limits_;
            std::vector<std::uint8_t> bytes_;
            std::optional<Error> error_;

            void
            Fail(ErrorKind kind, std::string context, std::string message)
            {
                if (!error_)
                    error_ = Error{ kind, bytes_.size(), std::move(context), std::move(message) };
            }
            void
            Byte(std::uint8_t value)
            {
                if (!error_)
                    bytes_.push_back(value);
            }
            template<typename Integer>
            void
            Unsigned(Integer value)
            {
                static_assert(std::is_unsigned_v<Integer>);
                for (std::size_t shift = 0; shift < sizeof(Integer) * 8U; shift += 8U)
                    Byte(static_cast<std::uint8_t>((value >> shift) & static_cast<Integer>(0xffU)));
            }
            void
            Count(std::size_t value, std::size_t maximum, std::string_view context)
            {
                if (value > maximum || value > std::numeric_limits<std::uint32_t>::max())
                    Fail(ErrorKind::LimitExceeded, std::string(context), "collection count exceeds wire limit");
                else
                    Unsigned(static_cast<std::uint32_t>(value));
            }
            template<typename Value, typename Encode>
            void
            Vector(const std::vector<Value> &values, std::size_t maximum, std::string_view context, Encode encode)
            {
                Count(values.size(), maximum, context);
                for (const auto &value : values)
                {
                    if (error_)
                        return;
                    encode(value);
                }
            }
            void
            Text(const std::u32string &value, std::string_view context)
            {
                Count(value.size(), limits_.maximumTextScalars, context);
                for (const auto scalar : value)
                {
                    const auto numeric = static_cast<std::uint32_t>(scalar);
                    if (numeric > 0x10ffffU || (numeric >= 0xd800U && numeric <= 0xdfffU))
                    {
                        Fail(ErrorKind::InvalidScalar, std::string(context), "text contains a non-scalar Unicode value");
                        return;
                    }
                    Unsigned(numeric);
                }
            }
            void
            QualifiedName(const std::vector<std::u32string> &parts, std::string_view context)
            {
                Vector(parts, 65535U, context, [this, context](const auto &part) {
                    Text(part, context);
                });
            }
            void
            Symbol(const SymbolName &symbol, std::string_view context)
            {
                if (symbol.id == 0U)
                {
                    Fail(ErrorKind::InvalidSymbol, std::string(context), "symbol id must be positive");
                    return;
                }
                Unsigned(symbol.id);
                Text(symbol.spelling, context);
            }
            void
            WriteType(const Type &type, std::size_t depth = 0U)
            {
                if (depth > limits_.maximumTypeDepth)
                {
                    Fail(ErrorKind::LimitExceeded, "type", "type nesting exceeds configured limit");
                    return;
                }
                switch (type.kind)
                {
                    case Type::Kind::Unit:
                        Byte(0);
                        return;
                    case Type::Kind::Bool:
                        Byte(1);
                        return;
                    case Type::Kind::Int64:
                        Byte(2);
                        return;
                    case Type::Kind::String:
                        Byte(3);
                        return;
                    case Type::Kind::Named:
                        Byte(4);
                        QualifiedName(type.name, "named type");
                        Vector(type.components, limits_.maximumOperands, "type argument count", [this, depth](const Type &argument) {
                            WriteType(argument, depth + 1U);
                        });
                        return;
                    case Type::Kind::Function:
                        Byte(5);
                        if (type.components.empty())
                        {
                            Fail(ErrorKind::UnsupportedType, "function type", "function type has no result component");
                            return;
                        }
                        Count(type.components.size() - 1U, limits_.maximumParameters, "function type parameter count");
                        for (std::size_t index = 0; index + 1U < type.components.size(); ++index)
                            WriteType(type.components[index], depth + 1U);
                        WriteType(type.components.back(), depth + 1U);
                        return;
                    case Type::Kind::TypeVariable:
                        Byte(6);
                        Symbol(type.variable, "type variable symbol");
                        return;
                    case Type::Kind::Character:
                        Byte(7);
                        return;
                    case Type::Kind::Int8:
                        Byte(8);
                        return;
                    case Type::Kind::Int16:
                        Byte(9);
                        return;
                    case Type::Kind::Int32:
                        Byte(10);
                        return;
                    case Type::Kind::Int128:
                        Byte(11);
                        return;
                    case Type::Kind::UInt8:
                        Byte(12);
                        return;
                    case Type::Kind::UInt16:
                        Byte(13);
                        return;
                    case Type::Kind::UInt32:
                        Byte(14);
                        return;
                    case Type::Kind::UInt64:
                        Byte(15);
                        return;
                    case Type::Kind::UInt128:
                        Byte(16);
                        return;
                    case Type::Kind::Float16:
                        Byte(17);
                        return;
                    case Type::Kind::Float32:
                        Byte(18);
                        return;
                    case Type::Kind::Float64:
                        Byte(19);
                        return;
                    case Type::Kind::Float128:
                        Byte(20);
                        return;
                }
            }
            void
            WriteLiteral(const Literal &literal)
            {
                if (std::holds_alternative<std::monostate>(literal))
                    Byte(0);
                else if (const auto *boolean = std::get_if<bool>(&literal))
                {
                    Byte(1);
                    Byte(*boolean ? 1U : 0U);
                }
                else if (const auto *integer = std::get_if<std::int64_t>(&literal))
                {
                    Byte(2);
                    Unsigned(static_cast<std::uint64_t>(*integer));
                }
                else if (const auto *string = std::get_if<std::u32string>(&literal))
                {
                    Byte(3);
                    Text(*string, "string literal");
                }
                else if (const auto *integer = std::get_if<::visual_xsharp::core::IntegerLiteral>(&literal))
                {
                    if (!::visual_xsharp::core::integer_is_canonical(*integer))
                    {
                        Fail(ErrorKind::InvalidInteger, "integer literal", "integer magnitude/sign is not canonical");
                        return;
                    }
                    Byte(4);
                    Byte(integer->negative ? 1U : 0U);
                    Vector(integer->magnitude, limits_.maximumNumericBytes, "integer magnitude", [this](const std::uint8_t octet) {
                        Byte(octet);
                    });
                }
                else if (const auto *floating = std::get_if<::visual_xsharp::core::FloatingLiteral>(&literal))
                {
                    if (!::visual_xsharp::core::floating_spelling_is_valid(floating->spelling))
                    {
                        Fail(ErrorKind::InvalidInteger, "floating literal", "floating spelling is not canonical");
                        return;
                    }
                    Byte(5);
                    Count(floating->spelling.size(), limits_.maximumNumericBytes, "floating literal length");
                    for (const auto character : floating->spelling)
                        Byte(static_cast<std::uint8_t>(character));
                }
                else if (const auto *integer = std::get_if<std::int32_t>(&literal))
                {
                    Byte(4);
                    const auto normalized = ::visual_xsharp::core::integer_from_signed(*integer);
                    Byte(normalized.negative ? 1U : 0U);
                    Vector(normalized.magnitude, limits_.maximumNumericBytes, "integer magnitude", [this](const std::uint8_t octet) {
                        Byte(octet);
                    });
                }
                else
                    Fail(ErrorKind::UnsupportedType, "literal", "literal cannot cross the Core v3 boundary");
            }
            void
            WriteExpression(const Expression &expression, std::size_t depth = 0U)
            {
                if (depth > limits_.maximumExpressionDepth)
                {
                    Fail(ErrorKind::LimitExceeded, "expression", "expression nesting exceeds configured limit");
                    return;
                }
                Byte(static_cast<std::uint8_t>(expression.kind));
                if (expression.kind == Expression::Kind::Primitive)
                    Byte(static_cast<std::uint8_t>(expression.primitive));
                WriteType(expression.type);
                switch (expression.kind)
                {
                    case Expression::Kind::Variable:
                        Symbol(expression.symbol, "variable symbol");
                        return;
                    case Expression::Kind::Literal:
                        WriteLiteral(expression.literal);
                        return;
                    case Expression::Kind::Apply:
                        if (!expression.callee)
                        {
                            Fail(ErrorKind::InvalidCount, "callee", "Core call must contain a callee");
                            return;
                        }
                        WriteExpression(*expression.callee, depth + 1U);
                        Vector(expression.operands, limits_.maximumOperands, "call argument count", [this, depth](const Expression &value) {
                            WriteExpression(value, depth + 1U);
                        });
                        return;
                    case Expression::Kind::Primitive:
                        Vector(expression.operands, limits_.maximumOperands, "primitive operand count", [this, depth](const Expression &value) {
                            WriteExpression(value, depth + 1U);
                        });
                        return;
                }
            }
            void
            WriteStatement(const Statement &statement)
            {
                Byte(static_cast<std::uint8_t>(statement.kind));
                switch (statement.kind)
                {
                    case Statement::Kind::Bind:
                        Symbol(statement.binding.symbol, "binding symbol");
                        WriteType(statement.binding.type);
                        Byte(statement.binding.mutableBinding ? 1U : 0U);
                        WriteExpression(statement.binding.value);
                        return;
                    case Statement::Kind::Assign:
                        Symbol(statement.destination, "assignment symbol");
                        WriteExpression(statement.expression);
                        return;
                    case Statement::Kind::Return:
                        WriteExpression(statement.expression);
                        return;
                    case Statement::Kind::If:
                        WriteExpression(statement.expression);
                        Vector(statement.trueBranch, limits_.maximumStatements, "true branch statement count", [this](const Statement &value) {
                            WriteStatement(value);
                        });
                        Vector(statement.falseBranch, limits_.maximumStatements, "false branch statement count", [this](const Statement &value) {
                            WriteStatement(value);
                        });
                        return;
                    case Statement::Kind::Evaluate:
                        WriteExpression(statement.expression);
                        return;
                }
            }
            void
            WriteFunction(const Function &function)
            {
                Symbol(function.symbol, "function symbol");
                Vector(function.parameters, limits_.maximumParameters, "parameter count", [this](const Parameter &parameter) {
                    Symbol(parameter.symbol, "parameter symbol");
                    WriteType(parameter.type);
                });
                WriteType(function.returnType);
                Vector(function.body, limits_.maximumStatements, "statement count", [this](const Statement &statement) {
                    WriteStatement(statement);
                });
            }
        };
    } // namespace

    auto
    Encode(const Module &module, const Limits &limits) -> EncodeResult
    {
        Writer writer(limits);
        writer.Document(module);
        return std::move(writer).Finish();
    }

    auto
    Decode(std::span<const std::uint8_t> bytes, const Limits &limits) -> DecodeResult
    {
        return Reader(bytes, limits).Document();
    }
} // namespace Visual::XSharp::Core::Wire
