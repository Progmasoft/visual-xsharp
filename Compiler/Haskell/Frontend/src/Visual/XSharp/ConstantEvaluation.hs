-- SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
-- SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.0

-- | Side-effect-free constant evaluation used for target-range diagnostics.
module Visual.XSharp.ConstantEvaluation
    ( ConstantIntegerError (..)
    , evaluateConstantInteger
    , renderConstantIntegerError
    ) where

import Visual.XSharp.AST

data ConstantIntegerError
    = ConstantDivisionByZero
    | ConstantFloorDivisionByZero
    | ConstantRemainderByZero
    deriving (Eq, Ord, Read, Show)

-- Non-constant expressions return Nothing. A definite arithmetic failure is
-- retained as Left so callers can diagnose it without pretending the entire
-- expression ceased to be constant.
evaluateConstantInteger :: Expression name annotation -> Either ConstantIntegerError (Maybe Integer)
evaluateConstantInteger expression = case expression of
    LiteralExpression _ literal _ -> case literal of
        IntegerLiteral value -> pure (Just value)
        CharacterLiteral value -> pure (Just value)
        BooleanLiteral value -> pure (Just (if value then 1 else 0))
        _ -> pure Nothing
    UnaryExpression _ operator value _ -> do
        operand <- evaluateConstantInteger value
        pure $ case (operator, operand) of
            (UnaryPlus, Just number) -> Just number
            (UnaryNegate, Just number) -> Just (-number)
            (LogicalNot, Just number) -> Just (if number == 0 then 1 else 0)
            _ -> Nothing
    BinaryExpression _ operator left right _ -> do
        leftValue <- evaluateConstantInteger left
        rightValue <- evaluateConstantInteger right
        evaluateBinary operator leftValue rightValue
    _ -> pure Nothing

evaluateBinary :: BinaryOperator -> Maybe Integer -> Maybe Integer -> Either ConstantIntegerError (Maybe Integer)
evaluateBinary _ Nothing _ = pure Nothing
evaluateBinary _ _ Nothing = pure Nothing
evaluateBinary operator (Just left) (Just right) = case operator of
    Add -> value (left + right)
    Subtract -> value (left - right)
    Multiply -> value (left * right)
    Divide
        | right == 0 -> Left ConstantDivisionByZero
        | otherwise -> value (left `quot` right)
    FloorDivide
        | right == 0 -> Left ConstantFloorDivisionByZero
        | otherwise -> value (left `div` right)
    Remainder
        | right == 0 -> Left ConstantRemainderByZero
        | otherwise -> value (left `rem` right)
    LessThan -> boolean (left < right)
    LessEqual -> boolean (left <= right)
    GreaterThan -> boolean (left > right)
    GreaterEqual -> boolean (left >= right)
    Equal -> boolean (left == right)
    NotEqual -> boolean (left /= right)
    LogicalAnd -> boolean (left /= 0 && right /= 0)
    LogicalOr -> boolean (left /= 0 || right /= 0)
    where
        value = pure . Just
        boolean result = value (if result then 1 else 0)

renderConstantIntegerError :: ConstantIntegerError -> String
renderConstantIntegerError issue = case issue of
    ConstantDivisionByZero -> "constant division by zero"
    ConstantFloorDivisionByZero -> "constant floor division by zero"
    ConstantRemainderByZero -> "constant remainder by zero"
