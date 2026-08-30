-- SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
-- SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.0
module Visual.XSharp.Core.Optimizer (CoreOptimizer (..), defaultCoreOptimizer, runCoreOptimizer) where

import Visual.XSharp.AST (Type)
import Visual.XSharp.Core
import Visual.XSharp.Diagnostic (Diagnostic)

newtype CoreOptimizer = CoreOptimizer {optimizeCore :: CoreModule -> Either [Diagnostic] CoreModule}
runCoreOptimizer :: CoreOptimizer -> CoreModule -> Either [Diagnostic] CoreModule
runCoreOptimizer = optimizeCore
defaultCoreOptimizer :: CoreOptimizer
defaultCoreOptimizer = CoreOptimizer (Right . optimizeModule)

optimizeModule :: CoreModule -> CoreModule
optimizeModule moduleValue = moduleValue {coreModuleFunctions = map optimizeFunction (coreModuleFunctions moduleValue)}
optimizeFunction :: CoreFunction -> CoreFunction
optimizeFunction function = function {coreFunctionBody = optimizeStatements (coreFunctionBody function)}
optimizeStatements :: [CoreStatement] -> [CoreStatement]
optimizeStatements [] = []
optimizeStatements (statement : remaining) = case optimizeStatement statement of
    value@(CoreReturn _) : _ -> [value]
    values -> values ++ optimizeStatements remaining
optimizeStatement :: CoreStatement -> [CoreStatement]
optimizeStatement statement = case statement of
    CoreBind binding -> [CoreBind binding {coreBindingValue = foldExpression (coreBindingValue binding)}]
    CoreAssign name value -> [CoreAssign name (foldExpression value)]
    CoreReturn value -> [CoreReturn (foldExpression value)]
    CoreEvaluate value -> [CoreEvaluate (foldExpression value)]
    CoreIf condition trueBranch falseBranch -> case foldExpression condition of
        CoreLiteral (CoreBoolean True) _ -> optimizeStatements trueBranch
        CoreLiteral (CoreBoolean False) _ -> optimizeStatements falseBranch
        folded -> [CoreIf folded (optimizeStatements trueBranch) (optimizeStatements falseBranch)]

foldExpression :: CoreExpression -> CoreExpression
foldExpression expression = case expression of
    CoreApply callee arguments valueType -> CoreApply (foldExpression callee) (map foldExpression arguments) valueType
    CorePrimitive primitive arguments valueType -> foldPrimitive primitive (map foldExpression arguments) valueType
    CoreClosure captures parameters returnType body valueType ->
        CoreClosure
            [capture {coreCaptureValue = foldExpression (coreCaptureValue capture)} | capture <- captures]
            parameters
            returnType
            (optimizeStatements body)
            valueType
    value -> value
foldPrimitive :: CorePrimitive -> [CoreExpression] -> Type -> CoreExpression
foldPrimitive primitive values valueType = case (primitive, values) of
    (CoreAdd, [CoreLiteral (CoreInteger a) _, CoreLiteral (CoreInteger b) _]) -> integer (a + b)
    (CoreSubtract, [CoreLiteral (CoreInteger a) _, CoreLiteral (CoreInteger b) _]) -> integer (a - b)
    (CoreMultiply, [CoreLiteral (CoreInteger a) _, CoreLiteral (CoreInteger b) _]) -> integer (a * b)
    (CoreDivide, [CoreLiteral (CoreInteger a) _, CoreLiteral (CoreInteger b) _]) | b /= 0 -> integer (a `quot` b)
    (CoreFloorDivide, [CoreLiteral (CoreInteger a) _, CoreLiteral (CoreInteger b) _]) | b /= 0 -> integer (a `div` b)
    (CoreRemainder, [CoreLiteral (CoreInteger a) _, CoreLiteral (CoreInteger b) _]) | b /= 0 -> integer (a `rem` b)
    (CoreNegate, [CoreLiteral (CoreInteger a) _]) -> integer (-a)
    (CoreLogicalNot, [CoreLiteral (CoreBoolean a) _]) -> boolean (not a)
    (CoreLessThan, [CoreLiteral (CoreInteger a) _, CoreLiteral (CoreInteger b) _]) -> boolean (a < b)
    (CoreLessEqual, [CoreLiteral (CoreInteger a) _, CoreLiteral (CoreInteger b) _]) -> boolean (a <= b)
    (CoreGreaterThan, [CoreLiteral (CoreInteger a) _, CoreLiteral (CoreInteger b) _]) -> boolean (a > b)
    (CoreGreaterEqual, [CoreLiteral (CoreInteger a) _, CoreLiteral (CoreInteger b) _]) -> boolean (a >= b)
    (CoreEqual, [CoreLiteral (CoreInteger a) _, CoreLiteral (CoreInteger b) _]) -> boolean (a == b)
    (CoreNotEqual, [CoreLiteral (CoreInteger a) _, CoreLiteral (CoreInteger b) _]) -> boolean (a /= b)
    (CoreLogicalAnd, [CoreLiteral (CoreBoolean a) _, CoreLiteral (CoreBoolean b) _]) -> boolean (a && b)
    (CoreLogicalOr, [CoreLiteral (CoreBoolean a) _, CoreLiteral (CoreBoolean b) _]) -> boolean (a || b)
    _ -> CorePrimitive primitive values valueType
    where
        integer value = CoreLiteral (CoreInteger value) valueType; boolean value = CoreLiteral (CoreBoolean value) valueType
