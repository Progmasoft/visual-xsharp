-- SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
-- SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.0

module Visual.XSharp.Core.Optimizer.ControlFlow (simplifyControlFlow) where

import Visual.XSharp.Core
import Visual.XSharp.Core.Optimizer.Analysis

simplifyControlFlow :: CoreModule -> CoreModule
simplifyControlFlow moduleValue =
    moduleValue {coreModuleFunctions = map simplifyFunction (coreModuleFunctions moduleValue)}

simplifyFunction :: CoreFunction -> CoreFunction
simplifyFunction function = function {coreFunctionBody = simplifyStatements (coreFunctionBody function)}

simplifyStatements :: [CoreStatement] -> [CoreStatement]
simplifyStatements [] = []
simplifyStatements (statement : remaining) =
    let current = simplifyStatement statement
     in if statementsAlwaysReturn current
            then current
            else current ++ simplifyStatements remaining

simplifyStatement :: CoreStatement -> [CoreStatement]
simplifyStatement statement = case statement of
    CoreBind binding ->
        [CoreBind binding {coreBindingValue = simplifyNestedExpression (coreBindingValue binding)}]
    CoreAssign name value -> [CoreAssign name (simplifyNestedExpression value)]
    CoreReturn value -> [CoreReturn (simplifyNestedExpression value)]
    CoreEvaluate value -> [CoreEvaluate (simplifyNestedExpression value)]
    CoreIf condition yes no ->
        let simplifiedCondition = simplifyNestedExpression condition
            simplifiedYes = simplifyStatements yes
            simplifiedNo = simplifyStatements no
         in case conditionTruth simplifiedCondition of
                Just True -> simplifiedYes
                Just False -> simplifiedNo
                Nothing
                    | simplifiedYes == simplifiedNo ->
                        preserveCondition simplifiedCondition simplifiedYes
                    | null simplifiedYes && null simplifiedNo ->
                        preserveCondition simplifiedCondition []
                    | otherwise -> [CoreIf simplifiedCondition simplifiedYes simplifiedNo]

preserveCondition :: CoreExpression -> [CoreStatement] -> [CoreStatement]
preserveCondition condition statements
    | discardableExpression condition = statements
    | otherwise = CoreEvaluate condition : statements

conditionTruth :: CoreExpression -> Maybe Bool
conditionTruth expression = case expression of
    CoreLiteral (CoreBoolean value) _ -> Just value
    CoreLiteral (CoreInteger value) _ -> Just (value /= 0)
    _ -> Nothing

simplifyNestedExpression :: CoreExpression -> CoreExpression
simplifyNestedExpression expression = case expression of
    CoreVariable {} -> expression
    CoreLiteral {} -> expression
    CoreApply callee arguments valueType ->
        CoreApply
            (simplifyNestedExpression callee)
            (map simplifyNestedExpression arguments)
            valueType
    CorePrimitive primitive arguments valueType ->
        CorePrimitive primitive (map simplifyNestedExpression arguments) valueType
    CoreClosure captures parameters returnType body valueType ->
        CoreClosure
            [capture {coreCaptureValue = simplifyNestedExpression (coreCaptureValue capture)} | capture <- captures]
            parameters
            returnType
            (simplifyStatements body)
            valueType
