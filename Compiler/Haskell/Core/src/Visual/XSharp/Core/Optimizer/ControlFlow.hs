-- SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
-- SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.1

module Visual.XSharp.Core.Optimizer.ControlFlow
    ( simplifyControlFlow
    , simplifyControlFlowWith
    ) where

import Visual.XSharp.Core
import Visual.XSharp.Core.Optimizer.Analysis

simplifyControlFlow :: CoreModule -> CoreModule
simplifyControlFlow = simplifyControlFlowWith emptyEffectEnvironment

simplifyControlFlowWith :: EffectEnvironment -> CoreModule -> CoreModule
simplifyControlFlowWith environment moduleValue =
    moduleValue {coreModuleFunctions = map (simplifyFunction environment) (coreModuleFunctions moduleValue)}

simplifyFunction :: EffectEnvironment -> CoreFunction -> CoreFunction
simplifyFunction environment function =
    function {coreFunctionBody = simplifyStatements environment (coreFunctionBody function)}

simplifyStatements :: EffectEnvironment -> [CoreStatement] -> [CoreStatement]
simplifyStatements _ [] = []
simplifyStatements environment (statement : remaining) =
    let current = simplifyStatement environment statement
     in if statementsAlwaysReturn current
            then current
            else current ++ simplifyStatements environment remaining

simplifyStatement :: EffectEnvironment -> CoreStatement -> [CoreStatement]
simplifyStatement environment statement = case statement of
    CoreBind binding ->
        [CoreBind binding {coreBindingValue = simplifyNestedExpression environment (coreBindingValue binding)}]
    CoreAssign name value -> [CoreAssign name (simplifyNestedExpression environment value)]
    CoreReturn value -> [CoreReturn (simplifyNestedExpression environment value)]
    CoreEvaluate value -> [CoreEvaluate (simplifyNestedExpression environment value)]
    CoreIf condition yes no ->
        let simplifiedCondition = simplifyNestedExpression environment condition
            simplifiedYes = simplifyStatements environment yes
            simplifiedNo = simplifyStatements environment no
         in case conditionTruth simplifiedCondition of
                Just True -> simplifiedYes
                Just False -> simplifiedNo
                Nothing
                    | simplifiedYes == simplifiedNo ->
                        preserveCondition environment simplifiedCondition simplifiedYes
                    | null simplifiedYes && null simplifiedNo ->
                        preserveCondition environment simplifiedCondition []
                    | otherwise -> [CoreIf simplifiedCondition simplifiedYes simplifiedNo]

preserveCondition :: EffectEnvironment -> CoreExpression -> [CoreStatement] -> [CoreStatement]
preserveCondition environment condition statements
    | discardableExpressionWith environment condition = statements
    | otherwise = CoreEvaluate condition : statements

conditionTruth :: CoreExpression -> Maybe Bool
conditionTruth expression = case expression of
    CoreLiteral (CoreBoolean value) _ -> Just value
    CoreLiteral (CoreInteger value) _ -> Just (value /= 0)
    _ -> Nothing

simplifyNestedExpression :: EffectEnvironment -> CoreExpression -> CoreExpression
simplifyNestedExpression environment expression = case expression of
    CoreVariable {} -> expression
    CoreLiteral {} -> expression
    CoreApply callee arguments valueType ->
        CoreApply
            (simplifyNestedExpression environment callee)
            (map (simplifyNestedExpression environment) arguments)
            valueType
    CorePrimitive primitive arguments valueType ->
        CorePrimitive primitive (map (simplifyNestedExpression environment) arguments) valueType
    CoreClosure captures parameters returnType body valueType ->
        CoreClosure
            [ capture {coreCaptureValue = simplifyNestedExpression environment (coreCaptureValue capture)}
            | capture <- captures
            ]
            parameters
            returnType
            (simplifyStatements environment body)
            valueType
