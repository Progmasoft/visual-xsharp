-- SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
-- SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.0

module Visual.XSharp.Core.Optimizer.Liveness (eliminateDeadCode) where

import Data.Set (Set)
import Data.Set qualified as Set
import Visual.XSharp.AST (ResolvedName, SymbolId, resolvedSymbol)
import Visual.XSharp.Core
import Visual.XSharp.Core.Optimizer.Analysis

-- Value liveness and declaration retention are deliberately separate. A
-- retained assignment kills the previous stored value, but it still requires
-- the CoreBind which declares its storage. Using one set for both concerns
-- either drops that declaration or falsely keeps every earlier assignment.
data LiveState = LiveState
    { liveValues :: Set SymbolId
    , requiredDeclarations :: Set SymbolId
    }

emptyLiveState :: LiveState
emptyLiveState = LiveState Set.empty Set.empty

stateForSymbols :: Set SymbolId -> LiveState
stateForSymbols symbols = LiveState symbols symbols

addRequiredSymbols :: Set SymbolId -> LiveState -> LiveState
addRequiredSymbols symbols state =
    LiveState
        { liveValues = Set.union symbols (liveValues state)
        , requiredDeclarations = Set.union symbols (requiredDeclarations state)
        }

mergeLiveStates :: [LiveState] -> LiveState
mergeLiveStates = foldl merge emptyLiveState
    where
        merge left right =
            LiveState
                { liveValues = Set.union (liveValues left) (liveValues right)
                , requiredDeclarations =
                    Set.union (requiredDeclarations left) (requiredDeclarations right)
                }

eliminateDeadCode :: CoreModule -> CoreModule
eliminateDeadCode moduleValue =
    moduleValue {coreModuleFunctions = map eliminateFunction (coreModuleFunctions moduleValue)}

eliminateFunction :: CoreFunction -> CoreFunction
eliminateFunction function =
    function {coreFunctionBody = fst (eliminateStatements emptyLiveState (coreFunctionBody function))}

-- Liveness runs backwards. The returned state describes storage and values
-- required before the optimized statement list executes. Function symbols
-- can occur in both sets; because they have no local CoreBind, they simply
-- flow to the function boundary and are ignored by this local pass.
eliminateStatements :: LiveState -> [CoreStatement] -> ([CoreStatement], LiveState)
eliminateStatements liveAfter statements = foldr eliminateOne ([], liveAfter) statements

eliminateOne :: CoreStatement -> ([CoreStatement], LiveState) -> ([CoreStatement], LiveState)
eliminateOne statement (remaining, liveAfter) = case statement of
    CoreReturn value ->
        let optimized = optimizeExpression value
         in ([CoreReturn optimized], stateForSymbols (expressionSymbols optimized))
    CoreEvaluate value ->
        let optimized = optimizeExpression value
         in if discardableExpression optimized
                then (remaining, liveAfter)
                else
                    ( CoreEvaluate optimized : remaining
                    , addRequiredSymbols (expressionSymbols optimized) liveAfter
                    )
    CoreBind binding -> eliminateBinding binding remaining liveAfter
    CoreAssign name value -> eliminateAssignment name value remaining liveAfter
    CoreIf condition yes no -> eliminateBranch condition yes no remaining liveAfter

eliminateBinding :: CoreBinding -> [CoreStatement] -> LiveState -> ([CoreStatement], LiveState)
eliminateBinding binding remaining liveAfter =
    let symbol = resolvedSymbol (coreBindingName binding)
        value = optimizeExpression (coreBindingValue binding)
        valueIsNeeded = Set.member symbol (liveValues liveAfter)
        declarationIsNeeded = Set.member symbol (requiredDeclarations liveAfter)
        beforeDefinition =
            LiveState
                { liveValues = Set.delete symbol (liveValues liveAfter)
                , requiredDeclarations = Set.delete symbol (requiredDeclarations liveAfter)
                }
     in if valueIsNeeded || declarationIsNeeded
            then
                ( CoreBind binding {coreBindingValue = value} : remaining
                , addRequiredSymbols (expressionSymbols value) beforeDefinition
                )
            else preserveDeadValue value remaining beforeDefinition

eliminateAssignment ::
    ResolvedName ->
    CoreExpression ->
    [CoreStatement] ->
    LiveState ->
    ([CoreStatement], LiveState)
eliminateAssignment name source remaining liveAfter =
    let symbol = resolvedSymbol name
        value = optimizeExpression source
        beforeWrite = liveAfter {liveValues = Set.delete symbol (liveValues liveAfter)}
     in if Set.member symbol (liveValues liveAfter)
            then
                -- The write supplies the value required later, so an earlier
                -- value of the target is dead. Its declaration is different:
                -- the retained assignment still needs storage to exist.
                let withSource = addRequiredSymbols (expressionSymbols value) beforeWrite
                    withDeclaration =
                        withSource
                            { requiredDeclarations =
                                Set.insert symbol (requiredDeclarations withSource)
                            }
                 in (CoreAssign name value : remaining, withDeclaration)
            else preserveDeadValue value remaining beforeWrite

eliminateBranch ::
    CoreExpression ->
    [CoreStatement] ->
    [CoreStatement] ->
    [CoreStatement] ->
    LiveState ->
    ([CoreStatement], LiveState)
eliminateBranch condition yes no remaining liveAfter =
    let optimizedCondition = optimizeExpression condition
        (optimizedYes, liveYes) = eliminateStatements liveAfter yes
        (optimizedNo, liveNo) = eliminateStatements liveAfter no
        branchState = mergeLiveStates [liveYes, liveNo]
        liveBefore = addRequiredSymbols (expressionSymbols optimizedCondition) branchState
     in (CoreIf optimizedCondition optimizedYes optimizedNo : remaining, liveBefore)

preserveDeadValue :: CoreExpression -> [CoreStatement] -> LiveState -> ([CoreStatement], LiveState)
preserveDeadValue value remaining liveAfter
    | discardableExpression value = (remaining, liveAfter)
    | otherwise =
        ( CoreEvaluate value : remaining
        , addRequiredSymbols (expressionSymbols value) liveAfter
        )

-- Closure bodies are independent liveness regions. Capture initializers are
-- evaluated outside the closure, while the body is analyzed from its own
-- return and effect roots. Parameter and capture declarations belong to the
-- closure environment and therefore need no CoreBind in the nested body.
optimizeExpression :: CoreExpression -> CoreExpression
optimizeExpression expression = case expression of
    CoreVariable {} -> expression
    CoreLiteral {} -> expression
    CoreApply callee arguments valueType ->
        CoreApply (optimizeExpression callee) (map optimizeExpression arguments) valueType
    CorePrimitive primitive arguments valueType ->
        CorePrimitive primitive (map optimizeExpression arguments) valueType
    CoreClosure captures parameters returnType body valueType ->
        CoreClosure
            [capture {coreCaptureValue = optimizeExpression (coreCaptureValue capture)} | capture <- captures]
            parameters
            returnType
            (fst (eliminateStatements emptyLiveState body))
            valueType
