-- SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
-- SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.1

{- | Interprocedural effect inference for verified Core.

The analysis proves only effects needed for safe dead-result elimination. A
direct call is removable when its complete transitive call graph is known,
non-recursive, allocation-free and failure-free. Unknown and indirect calls
remain observable. Recursive SCCs remain observable because removing a call
could remove nontermination even when every expression is otherwise pure.
-}
module Visual.XSharp.Core.Optimizer.EffectInference
    ( inferFunctionEffects
    ) where

import Data.Graph (SCC (..), stronglyConnComp)
import Data.List (sort)
import Data.Map.Strict (Map)
import Data.Map.Strict qualified as Map
import Data.Set (Set)
import Data.Set qualified as Set
import Visual.XSharp.AST (SymbolId, resolvedSymbol)
import Visual.XSharp.Core
import Visual.XSharp.Core.Optimizer.Analysis

data DirectFacts = DirectFacts
    { directLocalEffect :: Effect
    , directCallees :: Set SymbolId
    , directHasUnknownCall :: Bool
    }
    deriving (Eq, Ord, Read, Show)

emptyDirectFacts :: DirectFacts
emptyDirectFacts = DirectFacts PureEffect Set.empty False

combineFacts :: DirectFacts -> DirectFacts -> DirectFacts
combineFacts left right =
    DirectFacts
        { directLocalEffect = combineEffect (directLocalEffect left) (directLocalEffect right)
        , directCallees = Set.union (directCallees left) (directCallees right)
        , directHasUnknownCall = directHasUnknownCall left || directHasUnknownCall right
        }

inferFunctionEffects :: CoreModule -> (EffectEnvironment, [FunctionEffectReport])
inferFunctionEffects moduleValue =
    let functions = coreModuleFunctions moduleValue
        catalog = Map.fromList [(resolvedSymbol (coreFunctionName function), function) | function <- functions]
        knownSymbols = Map.keysSet catalog
        pureKnown = effectEnvironment [(symbol, PureEffect) | symbol <- Map.keys catalog]
        facts = Map.map (functionFacts knownSymbols pureKnown) catalog
        recursive = recursiveSymbols facts
        initial = Map.mapWithKey (initialEffect recursive) facts
        solved = solveEffects recursive facts initial
        reports = map (functionReport facts recursive solved) functions
     in (effectEnvironment (Map.toAscList solved), reports)

initialEffect :: Set SymbolId -> SymbolId -> DirectFacts -> Effect
initialEffect recursive symbol facts
    | Set.member symbol recursive = DivergenceEffect
    | directHasUnknownCall facts = combineEffect CallEffect (directLocalEffect facts)
    | otherwise = directLocalEffect facts

solveEffects :: Set SymbolId -> Map SymbolId DirectFacts -> Map SymbolId Effect -> Map SymbolId Effect
solveEffects recursive facts initial = go 0 initial
    where
        maximumIterations = Map.size facts + 1
        go iteration current
            | iteration >= maximumIterations = current
            | next == current = current
            | otherwise = go (iteration + 1) next
            where
                next = Map.mapWithKey (resolveOne current) facts
        resolveOne current symbol direct =
            foldl'
                combineEffect
                (initialEffect recursive symbol direct)
                [Map.findWithDefault CallEffect callee current | callee <- Set.toAscList (directCallees direct)]

functionReport ::
    Map SymbolId DirectFacts -> Set SymbolId -> Map SymbolId Effect -> CoreFunction -> FunctionEffectReport
functionReport facts recursive solved function =
    let name = coreFunctionName function
        symbol = resolvedSymbol name
        direct = Map.findWithDefault emptyDirectFacts symbol facts
     in FunctionEffectReport
            { effectFunctionName = name
            , effectClassification = Map.findWithDefault CallEffect symbol solved
            , effectDirectCallees = Set.toAscList (directCallees direct)
            , effectHasUnknownCall = directHasUnknownCall direct
            , effectIsRecursive = Set.member symbol recursive
            }

recursiveSymbols :: Map SymbolId DirectFacts -> Set SymbolId
recursiveSymbols facts =
    Set.fromList
        [ symbol
        | component <- stronglyConnComp graphNodes
        , symbol <- cyclicMembers component
        ]
    where
        graphNodes =
            [ (symbol, symbol, Set.toAscList (directCallees direct))
            | (symbol, direct) <- Map.toAscList facts
            ]
        cyclicMembers (CyclicSCC symbols) = sort symbols
        cyclicMembers (AcyclicSCC _) = []

functionFacts :: Set SymbolId -> EffectEnvironment -> CoreFunction -> DirectFacts
functionFacts knownSymbols pureKnown = statementsFacts knownSymbols pureKnown . coreFunctionBody

statementsFacts :: Set SymbolId -> EffectEnvironment -> [CoreStatement] -> DirectFacts
statementsFacts _ _ [] = emptyDirectFacts
statementsFacts knownSymbols pureKnown (statement : remaining) =
    let current = statementFacts knownSymbols pureKnown statement
     in if statementAlwaysReturns statement
            then current
            else combineFacts current (statementsFacts knownSymbols pureKnown remaining)

statementAlwaysReturns :: CoreStatement -> Bool
statementAlwaysReturns (CoreReturn _) = True
statementAlwaysReturns (CoreIf _ yes no) = not (null no) && statementsAlwaysReturn yes && statementsAlwaysReturn no
statementAlwaysReturns _ = False

statementFacts :: Set SymbolId -> EffectEnvironment -> CoreStatement -> DirectFacts
statementFacts knownSymbols pureKnown statement = case statement of
    CoreBind binding -> expressionFacts knownSymbols pureKnown (coreBindingValue binding)
    CoreAssign _ value -> expressionFacts knownSymbols pureKnown value
    CoreReturn value -> expressionFacts knownSymbols pureKnown value
    CoreEvaluate value -> expressionFacts knownSymbols pureKnown value
    CoreIf condition yes no ->
        combineFacts
            (expressionFacts knownSymbols pureKnown condition)
            (combineFacts (statementsFacts knownSymbols pureKnown yes) (statementsFacts knownSymbols pureKnown no))

expressionFacts :: Set SymbolId -> EffectEnvironment -> CoreExpression -> DirectFacts
expressionFacts knownSymbols pureKnown expression =
    let nested = case expression of
            CoreVariable _ _ -> emptyDirectFacts
            CoreLiteral _ _ -> emptyDirectFacts
            CorePrimitive _ arguments _ -> foldFacts (map (expressionFacts knownSymbols pureKnown) arguments)
            CoreApply callee arguments _ ->
                let children = foldFacts (map (expressionFacts knownSymbols pureKnown) (callee : arguments))
                 in case directCallee callee of
                        Just symbol
                            | Set.member symbol knownSymbols ->
                                children {directCallees = Set.insert symbol (directCallees children)}
                        _ -> children {directHasUnknownCall = True}
            -- A closure body executes only when the callable is invoked. Its
            -- construction is observable allocation, while capture initializers
            -- execute now and therefore contribute their own direct facts.
            CoreClosure captures _ _ _ _ ->
                foldFacts (map (expressionFacts knownSymbols pureKnown . coreCaptureValue) captures)
        local = expressionEffectWith pureKnown expression
     in nested {directLocalEffect = combineEffect local (directLocalEffect nested)}

foldFacts :: [DirectFacts] -> DirectFacts
foldFacts = foldl' combineFacts emptyDirectFacts

directCallee :: CoreExpression -> Maybe SymbolId
directCallee (CoreVariable name _) = Just (resolvedSymbol name)
directCallee _ = Nothing
