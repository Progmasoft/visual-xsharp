-- SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
-- SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.0

module Visual.XSharp.Core.Optimizer.Analysis
    ( Effect (..)
    , expressionEffect
    , expressionSymbols
    , statementSymbols
    , statementsAlwaysReturn
    , discardableExpression
    ) where

import Data.Set (Set)
import Data.Set qualified as Set
import Visual.XSharp.AST (SymbolId, resolvedSymbol)
import Visual.XSharp.Core

-- Calls and closure construction remain observable even when their result is
-- dead. This is conservative around allocation, AARC transitions, future
-- destructors, and user-defined call effects.
data Effect = PureEffect | AllocationEffect | CallEffect | WriteEffect
    deriving (Eq, Ord, Read, Show)

combineEffect :: Effect -> Effect -> Effect
combineEffect = max

expressionEffect :: CoreExpression -> Effect
expressionEffect expression = case expression of
    CoreVariable _ _ -> PureEffect
    CoreLiteral _ _ -> PureEffect
    CorePrimitive _ arguments _ -> foldl combineEffect PureEffect (map expressionEffect arguments)
    CoreApply callee arguments _ -> foldl combineEffect CallEffect (map expressionEffect (callee : arguments))
    CoreClosure captures _ _ _ _ ->
        foldl combineEffect AllocationEffect (map (expressionEffect . coreCaptureValue) captures)

discardableExpression :: CoreExpression -> Bool
discardableExpression expression = expressionEffect expression == PureEffect

expressionSymbols :: CoreExpression -> Set SymbolId
expressionSymbols expression = case expression of
    CoreVariable name _ -> Set.singleton (resolvedSymbol name)
    CoreLiteral _ _ -> Set.empty
    CoreApply callee arguments _ -> Set.unions (map expressionSymbols (callee : arguments))
    CorePrimitive _ arguments _ -> Set.unions (map expressionSymbols arguments)
    CoreClosure captures _ _ _ _ -> Set.unions (map (expressionSymbols . coreCaptureValue) captures)

statementSymbols :: CoreStatement -> Set SymbolId
statementSymbols statement = case statement of
    CoreBind binding -> expressionSymbols (coreBindingValue binding)
    CoreAssign _ value -> expressionSymbols value
    CoreReturn value -> expressionSymbols value
    CoreIf condition yes no -> Set.unions (expressionSymbols condition : map statementSymbols (yes ++ no))
    CoreEvaluate value -> expressionSymbols value

statementsAlwaysReturn :: [CoreStatement] -> Bool
statementsAlwaysReturn [] = False
statementsAlwaysReturn (statement : remaining) = case statement of
    CoreReturn _ -> True
    CoreIf _ yes no ->
        (not (null no) && statementsAlwaysReturn yes && statementsAlwaysReturn no)
            || statementsAlwaysReturn remaining
    _ -> statementsAlwaysReturn remaining
