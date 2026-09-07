-- SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
-- SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.1

{- | Exact evaluation for compile-time values embedded in type syntax.

This module intentionally does not reuse runtime numeric coercion. Template
values participate in specialization identity, so host overflow, floating
approximation, and truthy conversions would make artifacts non-portable.
-}
module Visual.XSharp.TemplateValue
    ( TemplateValueError (..)
    , evaluateTemplateValue
    , evaluateFixedArraySize
    , renderTemplateValueError
    , templateValueSyntaxSpan
    ) where

import Visual.XSharp.AST

data TemplateValueError
    = TemplateValueIsNotConstant QualifiedName
    | TemplateValueDivisionByZero
    | TemplateValueFloorDivisionByZero
    | TemplateValueRemainderByZero
    | TemplateValueRequiresInteger TemplateValue
    | TemplateValueNegativeArraySize Integer
    deriving (Eq, Ord, Read, Show)

data ExactValue
    = ExactInteger Integer
    | ExactBoolean Bool
    | ExactCharacter Integer
    deriving (Eq, Ord, Read, Show)

evaluateTemplateValue :: TemplateValueSyntax -> Either TemplateValueError TemplateValue
evaluateTemplateValue syntax = exactToTemplate <$> evaluateExact syntax

evaluateFixedArraySize :: TemplateValueSyntax -> Either TemplateValueError TemplateValue
evaluateFixedArraySize syntax = do
    value <- evaluateTemplateValue syntax
    case value of
        IntegerTemplateValue integer
            | integer < 0 -> Left (TemplateValueNegativeArraySize integer)
            | otherwise -> Right value
        CharacterTemplateValue scalar -> Right (IntegerTemplateValue scalar)
        other -> Left (TemplateValueRequiresInteger other)

evaluateExact :: TemplateValueSyntax -> Either TemplateValueError ExactValue
evaluateExact syntax = case syntax of
    TemplateIntegerSyntax _ value -> Right (ExactInteger value)
    TemplateCharacterSyntax _ value -> Right (ExactCharacter value)
    TemplateBooleanSyntax _ value -> Right (ExactBoolean value)
    TemplateNameSyntax _ name -> Left (TemplateValueIsNotConstant name)
    TemplateUnarySyntax _ operator operand -> do
        value <- evaluateExact operand
        evaluateUnary operator value
    TemplateBinarySyntax _ operator left right -> do
        leftValue <- evaluateExact left
        rightValue <- evaluateExact right
        evaluateBinary operator leftValue rightValue

evaluateUnary :: UnaryOperator -> ExactValue -> Either TemplateValueError ExactValue
evaluateUnary operator value = case operator of
    UnaryPlus -> ExactInteger <$> requireInteger value
    UnaryNegate -> ExactInteger . negate <$> requireInteger value
    LogicalNot -> ExactBoolean . not <$> requireBooleanContext value

evaluateBinary :: BinaryOperator -> ExactValue -> ExactValue -> Either TemplateValueError ExactValue
evaluateBinary operator left right = case operator of
    Add -> integerBinary (+)
    Subtract -> integerBinary (-)
    Multiply -> integerBinary (*)
    Divide -> do
        divisor <- requireInteger right
        if divisor == 0
            then Left TemplateValueDivisionByZero
            else ExactInteger . (`quot` divisor) <$> requireInteger left
    FloorDivide -> do
        divisor <- requireInteger right
        if divisor == 0
            then Left TemplateValueFloorDivisionByZero
            else ExactInteger . (`div` divisor) <$> requireInteger left
    Remainder -> do
        divisor <- requireInteger right
        if divisor == 0
            then Left TemplateValueRemainderByZero
            else ExactInteger . (`rem` divisor) <$> requireInteger left
    LessThan -> comparison (<)
    LessEqual -> comparison (<=)
    GreaterThan -> comparison (>)
    GreaterEqual -> comparison (>=)
    Equal -> Right (ExactBoolean (left == right))
    NotEqual -> Right (ExactBoolean (left /= right))
    LogicalAnd -> logical (&&)
    LogicalOr -> logical (||)
    where
        integerBinary operation = do
            lhs <- requireInteger left
            rhs <- requireInteger right
            Right (ExactInteger (operation lhs rhs))
        comparison operation = do
            lhs <- requireInteger left
            rhs <- requireInteger right
            Right (ExactBoolean (operation lhs rhs))
        logical operation = do
            lhs <- requireBooleanContext left
            rhs <- requireBooleanContext right
            Right (ExactBoolean (operation lhs rhs))

requireInteger :: ExactValue -> Either TemplateValueError Integer
requireInteger value = case value of
    ExactInteger integer -> Right integer
    ExactCharacter scalar -> Right scalar
    ExactBoolean boolean -> Left (TemplateValueRequiresInteger (BooleanTemplateValue boolean))

requireBooleanContext :: ExactValue -> Either TemplateValueError Bool
requireBooleanContext value = case value of
    ExactBoolean boolean -> Right boolean
    ExactInteger integer -> Right (integer /= 0)
    ExactCharacter scalar -> Right (scalar /= 0)

exactToTemplate :: ExactValue -> TemplateValue
exactToTemplate value = case value of
    ExactInteger integer -> IntegerTemplateValue integer
    ExactBoolean boolean -> BooleanTemplateValue boolean
    ExactCharacter scalar -> CharacterTemplateValue scalar

templateValueSyntaxSpan :: TemplateValueSyntax -> SourceSpan
templateValueSyntaxSpan syntax = case syntax of
    TemplateIntegerSyntax spanValue _ -> spanValue
    TemplateCharacterSyntax spanValue _ -> spanValue
    TemplateBooleanSyntax spanValue _ -> spanValue
    TemplateNameSyntax spanValue _ -> spanValue
    TemplateUnarySyntax spanValue _ _ -> spanValue
    TemplateBinarySyntax spanValue _ _ _ -> spanValue

renderTemplateValueError :: TemplateValueError -> String
renderTemplateValueError issue = case issue of
    TemplateValueIsNotConstant (QualifiedName parts) ->
        "template value " ++ joinQualified (map identifierText parts) ++ " is not a resolved compile-time constant"
    TemplateValueDivisionByZero -> "template value performs division by zero"
    TemplateValueFloorDivisionByZero -> "template value performs floor division by zero"
    TemplateValueRemainderByZero -> "template value performs remainder by zero"
    TemplateValueRequiresInteger _ -> "fixed System.Array size must evaluate to an integer"
    TemplateValueNegativeArraySize value -> "fixed System.Array size cannot be negative: " ++ show value

joinQualified :: [String] -> String
joinQualified [] = "<empty>"
joinQualified [part] = part
joinQualified (part : parts) = part ++ "." ++ joinQualified parts
