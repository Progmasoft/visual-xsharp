-- SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
-- SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.0

{- | Central numeric operator and contextual-literal policy.

Lexer and parser establish literal identity. This module answers semantic
questions without constructing diagnostics, which keeps it usable by the
TypeChecker, Analyzer, and future constant evaluator.
-}
module Visual.XSharp.NumericSemantics
    ( NumericContext (..)
    , NumericRuleError (..)
    , NumericRuleResult (..)
    , integerLiteralRule
    , floatingLiteralRule
    , unaryNumericRule
    , binaryNumericRule
    , acceptsBooleanContext
    , renderNumericRuleError
    ) where

import Visual.XSharp.AST
import Visual.XSharp.BuiltinTypes

data NumericContext
    = NoNumericContext
    | TargetNumericType Type
    | BooleanNumericContext
    deriving (Eq, Ord, Read, Show)

data NumericRuleError
    = IntegerLiteralOutsideTarget ScalarType Integer
    | UntargetedIntegerOutsideInt Integer
    | FloatingLiteralRequiresFloatingTarget Type
    | UnaryRequiresNumeric UnaryOperator Type
    | NegationRequiresSignedNumeric Type
    | LogicalRequiresBooleanContext UnaryOperator Type
    | BinaryRequiresMatchingTypes BinaryOperator Type Type
    | BinaryRequiresNumericType BinaryOperator Type
    | BinaryRequiresBooleanContext BinaryOperator Type Type
    deriving (Eq, Ord, Read, Show)

data NumericRuleResult = NumericRuleResult
    { numericRuleType :: Type
    , numericRuleError :: Maybe NumericRuleError
    }
    deriving (Eq, Ord, Read, Show)

integerLiteralRule :: NumericContext -> Integer -> NumericRuleResult
integerLiteralRule context value = case context of
    BooleanNumericContext -> success boolType
    TargetNumericType target -> case typeToScalarType target of
        Just BooleanScalar -> success boolType
        Just scalar
            | scalarTypeFamily scalar `elem` [SignedIntegerFamily, UnsignedIntegerFamily] ->
                if integerFits scalar value
                    then success (scalarTypeToType scalar)
                    else failure (scalarTypeToType scalar) (IntegerLiteralOutsideTarget scalar value)
        _ -> defaultInteger
    NoNumericContext -> defaultInteger
    where
        defaultInteger
            | integerFits defaultIntegerScalar value = success (scalarTypeToType defaultIntegerScalar)
            | otherwise = failure (scalarTypeToType defaultIntegerScalar) (UntargetedIntegerOutsideInt value)

floatingLiteralRule :: NumericContext -> NumericRuleResult
floatingLiteralRule context = case context of
    TargetNumericType target -> case typeToScalarType target of
        Just scalar | scalarTypeFamily scalar == FloatingFamily -> success (scalarTypeToType scalar)
        _ -> failure (scalarTypeToType defaultFloatingScalar) (FloatingLiteralRequiresFloatingTarget target)
    _ -> success (scalarTypeToType defaultFloatingScalar)

unaryNumericRule :: UnaryOperator -> Type -> NumericRuleResult
unaryNumericRule operator operandType = case operator of
    LogicalNot
        | acceptsBooleanContext operandType -> success boolType
        | otherwise -> failure boolType (LogicalRequiresBooleanContext operator operandType)
    UnaryPlus
        | isNumericType operandType -> success operandType
        | otherwise -> failure operandType (UnaryRequiresNumeric operator operandType)
    UnaryNegate
        | isSignedIntegerType operandType || isFloatingType operandType -> success operandType
        | otherwise -> failure operandType (NegationRequiresSignedNumeric operandType)

binaryNumericRule :: BinaryOperator -> Type -> Type -> NumericRuleResult
binaryNumericRule operator leftType rightType
    | operator `elem` [LogicalAnd, LogicalOr] =
        if acceptsBooleanContext leftType && acceptsBooleanContext rightType
            then success boolType
            else failure boolType (BinaryRequiresBooleanContext operator leftType rightType)
    | operator `elem` [Equal, NotEqual] =
        if leftType == rightType
            then success boolType
            else failure boolType (BinaryRequiresMatchingTypes operator leftType rightType)
    | leftType /= rightType =
        failure (resultFor operator leftType) (BinaryRequiresMatchingTypes operator leftType rightType)
    | not (isNumericType leftType) =
        failure (resultFor operator leftType) (BinaryRequiresNumericType operator leftType)
    | otherwise = success (resultFor operator leftType)

acceptsBooleanContext :: Type -> Bool
acceptsBooleanContext valueType = valueType == boolType || isNumericType valueType

resultFor :: BinaryOperator -> Type -> Type
resultFor operator operandType
    | operator `elem` [LessThan, LessEqual, GreaterThan, GreaterEqual, Equal, NotEqual] = boolType
    | otherwise = operandType

success :: Type -> NumericRuleResult
success valueType = NumericRuleResult valueType Nothing

failure :: Type -> NumericRuleError -> NumericRuleResult
failure valueType issue = NumericRuleResult valueType (Just issue)

renderNumericRuleError :: NumericRuleError -> String
renderNumericRuleError issue = case issue of
    IntegerLiteralOutsideTarget scalar value ->
        "integer literal " ++ show value ++ " does not fit target type " ++ scalarTypeName scalar
    UntargetedIntegerOutsideInt value ->
        "un-targeted integer literal " ++ show value ++ " does not fit int; provide an explicit wider target"
    FloatingLiteralRequiresFloatingTarget target ->
        "floating-point literal cannot use non-floating target " ++ show target
    UnaryRequiresNumeric operator operand ->
        show operator ++ " requires a numeric operand, found " ++ show operand
    NegationRequiresSignedNumeric operand ->
        "unary negation requires a signed integer or floating-point operand, found " ++ show operand
    LogicalRequiresBooleanContext operator operand ->
        show operator ++ " requires bool or numeric context, found " ++ show operand
    BinaryRequiresMatchingTypes operator left right ->
        show operator ++ " requires matching operand types, found " ++ show left ++ " and " ++ show right
    BinaryRequiresNumericType operator operand ->
        show operator ++ " requires numeric operands, found " ++ show operand
    BinaryRequiresBooleanContext operator left right ->
        show operator ++ " requires bool or numeric operands, found " ++ show left ++ " and " ++ show right
