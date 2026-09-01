-- SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
-- SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.0

module Visual.XSharp.Core.Optimizer.Constant
    ( propagateConstants
    , simplifyExpression
    ) where

import Data.Map.Strict (Map)
import Data.Map.Strict qualified as Map
import Visual.XSharp.AST
import Visual.XSharp.Core
import Visual.XSharp.Core.Scalar

type ConstantEnvironment = Map SymbolId CoreExpression

propagateConstants :: CoreModule -> CoreModule
propagateConstants moduleValue =
    moduleValue {coreModuleFunctions = map simplifyFunction (coreModuleFunctions moduleValue)}

simplifyFunction :: CoreFunction -> CoreFunction
simplifyFunction function =
    function {coreFunctionBody = fst (simplifyStatements Map.empty (coreFunctionBody function))}

simplifyStatements :: ConstantEnvironment -> [CoreStatement] -> ([CoreStatement], ConstantEnvironment)
simplifyStatements environment [] = ([], environment)
simplifyStatements environment (statement : remaining) =
    let (simplified, nextEnvironment) = simplifyStatement environment statement
        (rest, finalEnvironment) = simplifyStatements nextEnvironment remaining
     in (simplified : rest, finalEnvironment)

simplifyStatement :: ConstantEnvironment -> CoreStatement -> (CoreStatement, ConstantEnvironment)
simplifyStatement environment statement = case statement of
    CoreBind binding ->
        let value = simplifyExpression environment (coreBindingValue binding)
            changed = binding {coreBindingValue = value}
            nextEnvironment
                | not (coreBindingMutable binding)
                , isPropagatable value =
                    Map.insert (resolvedSymbol (coreBindingName binding)) value environment
                | otherwise = Map.delete (resolvedSymbol (coreBindingName binding)) environment
         in (CoreBind changed, nextEnvironment)
    CoreAssign name value ->
        ( CoreAssign name (simplifyExpression environment value)
        , Map.delete (resolvedSymbol name) environment
        )
    CoreReturn value -> (CoreReturn (simplifyExpression environment value), environment)
    CoreEvaluate value -> (CoreEvaluate (simplifyExpression environment value), environment)
    CoreIf condition yes no ->
        let simplifiedCondition = simplifyExpression environment condition
            simplifiedYes = fst (simplifyStatements environment yes)
            simplifiedNo = fst (simplifyStatements environment no)
         in (CoreIf simplifiedCondition simplifiedYes simplifiedNo, environment)

-- Propagation is intentionally limited to literals. Duplicating calls,
-- closure allocations, or arbitrary primitive trees could change effects or
-- grow code. Algebraic simplification still operates on the use site.
isPropagatable :: CoreExpression -> Bool
isPropagatable CoreLiteral {} = True
isPropagatable _ = False

simplifyExpression :: ConstantEnvironment -> CoreExpression -> CoreExpression
simplifyExpression environment expression = case expression of
    CoreVariable name valueType ->
        case Map.lookup (resolvedSymbol name) environment of
            Just constant | expressionType constant == valueType -> constant
            _ -> expression
    CoreLiteral {} -> expression
    CoreApply callee arguments valueType ->
        CoreApply
            (simplifyExpression environment callee)
            (map (simplifyExpression environment) arguments)
            valueType
    CorePrimitive primitive arguments valueType ->
        foldPrimitive primitive (map (simplifyExpression environment) arguments) valueType
    CoreClosure captures parameters returnType body valueType ->
        let simplifiedCaptures = map simplifyCapture captures
            captureConstants =
                [ (resolvedSymbol (coreCaptureName capture), coreCaptureValue capture)
                | capture <- simplifiedCaptures
                , isPropagatable (coreCaptureValue capture)
                ]
            parameterSymbols = map (resolvedSymbol . fst) parameters
            bodyEnvironment = foldr Map.delete (Map.fromList captureConstants) parameterSymbols
            simplifiedBody = fst (simplifyStatements bodyEnvironment body)
         in CoreClosure simplifiedCaptures parameters returnType simplifiedBody valueType
    where
        simplifyCapture capture =
            capture {coreCaptureValue = simplifyExpression environment (coreCaptureValue capture)}

foldPrimitive :: CorePrimitive -> [CoreExpression] -> Type -> CoreExpression
foldPrimitive primitive arguments valueType =
    case evaluatePrimitive primitive arguments valueType of
        Just folded -> folded
        Nothing -> simplifyIdentity primitive arguments valueType

evaluatePrimitive :: CorePrimitive -> [CoreExpression] -> Type -> Maybe CoreExpression
evaluatePrimitive primitive arguments valueType =
    case (primitive, mapM integerLiteral arguments) of
        (CoreAdd, Just [a, b]) -> integer (a + b)
        (CoreSubtract, Just [a, b]) -> integer (a - b)
        (CoreMultiply, Just [a, b]) -> integer (a * b)
        (CoreDivide, Just [a, b]) | b /= 0 -> integer (a `quot` b)
        (CoreFloorDivide, Just [a, b]) | b /= 0 -> integer (a `div` b)
        (CoreRemainder, Just [a, b]) | b /= 0 -> integer (a `rem` b)
        (CoreNegate, Just [value]) -> integer (-value)
        (CoreLessThan, Just [a, b]) -> Just (boolean (a < b))
        (CoreLessEqual, Just [a, b]) -> Just (boolean (a <= b))
        (CoreGreaterThan, Just [a, b]) -> Just (boolean (a > b))
        (CoreGreaterEqual, Just [a, b]) -> Just (boolean (a >= b))
        (CoreEqual, Just [a, b]) -> Just (boolean (a == b))
        (CoreNotEqual, Just [a, b]) -> Just (boolean (a /= b))
        _ -> evaluateBoolean primitive arguments
    where
        integer result
            | integerFitsCoreType valueType result = Just (CoreLiteral (CoreInteger result) valueType)
            | otherwise = Nothing
        boolean result = CoreLiteral (CoreBoolean result) boolType

evaluateBoolean :: CorePrimitive -> [CoreExpression] -> Maybe CoreExpression
evaluateBoolean primitive arguments = case (primitive, mapM truthValue arguments) of
    (CoreLogicalNot, Just [value]) -> Just (boolean (not value))
    (CoreLogicalAnd, Just [left, right]) -> Just (boolean (left && right))
    (CoreLogicalOr, Just [left, right]) -> Just (boolean (left || right))
    _ -> Nothing
    where
        boolean result = CoreLiteral (CoreBoolean result) boolType

-- View patterns would obscure the data-flow rules in diagnostics, so literal
-- extraction remains explicit and total.
integerLiteral :: CoreExpression -> Maybe Integer
integerLiteral (CoreLiteral (CoreInteger value) _) = Just value
integerLiteral _ = Nothing

truthValue :: CoreExpression -> Maybe Bool
truthValue (CoreLiteral (CoreBoolean value) _) = Just value
truthValue (CoreLiteral (CoreInteger value) _) = Just (value /= 0)
truthValue _ = Nothing

simplifyIdentity :: CorePrimitive -> [CoreExpression] -> Type -> CoreExpression
simplifyIdentity primitive arguments valueType = case (primitive, arguments) of
    (CoreAdd, [value, zero]) | isIntegerLiteral 0 zero -> value
    (CoreAdd, [zero, value]) | isIntegerLiteral 0 zero -> value
    (CoreSubtract, [value, zero]) | isIntegerLiteral 0 zero -> value
    (CoreMultiply, [value, one]) | isIntegerLiteral 1 one -> value
    (CoreMultiply, [one, value]) | isIntegerLiteral 1 one -> value
    (CoreDivide, [value, one]) | isIntegerLiteral 1 one -> value
    (CoreFloorDivide, [value, one]) | isIntegerLiteral 1 one -> value
    (CoreRemainder, [value, one])
        | isIntegerLiteral 1 one, isSimpleValue value -> CoreLiteral (CoreInteger 0) valueType
    (CoreNegate, [CorePrimitive CoreNegate [value] innerType])
        | innerType == valueType -> value
    (CoreLogicalNot, [CorePrimitive CoreLogicalNot [value] innerType])
        | innerType == boolType, expressionType value == boolType -> value
    _ -> CorePrimitive primitive arguments valueType

-- Remainder-by-one may erase evaluation only for values which cannot carry
-- an effect. More general expressions remain intact until an effect-aware
-- rewrite can preserve their evaluation explicitly.
isSimpleValue :: CoreExpression -> Bool
isSimpleValue CoreVariable {} = True
isSimpleValue CoreLiteral {} = True
isSimpleValue _ = False

isIntegerLiteral :: Integer -> CoreExpression -> Bool
isIntegerLiteral expected expression = integerLiteral expression == Just expected
