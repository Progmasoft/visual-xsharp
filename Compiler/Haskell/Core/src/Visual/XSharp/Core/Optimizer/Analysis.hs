-- SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
-- SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.1

module Visual.XSharp.Core.Optimizer.Analysis
    ( Effect (..)
    , EffectEnvironment
    , FunctionEffectReport (..)
    , emptyEffectEnvironment
    , effectEnvironment
    , lookupFunctionEffect
    , combineEffect
    , expressionEffect
    , expressionEffectWith
    , expressionSymbols
    , statementSymbols
    , statementsAlwaysReturn
    , discardableExpression
    , discardableExpressionWith
    ) where

import Data.Map.Strict (Map)
import Data.Map.Strict qualified as Map
import Data.Set (Set)
import Data.Set qualified as Set
import Visual.XSharp.AST (ResolvedName, SymbolId, resolvedSymbol)
import Visual.XSharp.Core
import Visual.XSharp.Core.Scalar (isCoreFloatingType, isCoreIntegerType)

-- Effect ordering is deliberately conservative. The optimizer currently needs
-- one decisive property: only PureEffect may disappear when its value is dead.
-- The remaining constructors retain the strongest reason in diagnostics and
-- reports without pretending that allocation, failure and divergence are
-- interchangeable language effects.
data Effect
    = PureEffect
    | FailureEffect
    | AllocationEffect
    | CallEffect
    | DivergenceEffect
    deriving (Eq, Ord, Read, Show)

-- The environment is immutable for one optimizer pass. Recomputing it after a
-- structural rewrite is cheap relative to parsing and avoids stale call facts
-- when control-flow simplification removes a call site.
newtype EffectEnvironment = EffectEnvironment (Map SymbolId Effect)
    deriving (Eq, Ord, Read, Show)

data FunctionEffectReport = FunctionEffectReport
    { effectFunctionName :: ResolvedName
    , effectClassification :: Effect
    , effectDirectCallees :: [SymbolId]
    , effectHasUnknownCall :: Bool
    , effectIsRecursive :: Bool
    }
    deriving (Eq, Ord, Read, Show)

emptyEffectEnvironment :: EffectEnvironment
emptyEffectEnvironment = EffectEnvironment Map.empty

effectEnvironment :: [(SymbolId, Effect)] -> EffectEnvironment
effectEnvironment = EffectEnvironment . Map.fromList

lookupFunctionEffect :: EffectEnvironment -> SymbolId -> Maybe Effect
lookupFunctionEffect (EffectEnvironment values) symbol = Map.lookup symbol values

combineEffect :: Effect -> Effect -> Effect
combineEffect = max

expressionEffect :: CoreExpression -> Effect
expressionEffect = expressionEffectWith emptyEffectEnvironment

expressionEffectWith :: EffectEnvironment -> CoreExpression -> Effect
expressionEffectWith environment expression = case expression of
    CoreVariable _ _ -> PureEffect
    CoreLiteral _ _ -> PureEffect
    CorePrimitive primitive arguments _ ->
        foldl combineEffect (primitiveEffect primitive arguments) (map (expressionEffectWith environment) arguments)
    CoreApply callee arguments _ ->
        let nested = foldl combineEffect PureEffect (map (expressionEffectWith environment) (callee : arguments))
            invoked = case callee of
                CoreVariable name _ ->
                    maybe CallEffect id (lookupFunctionEffect environment (resolvedSymbol name))
                _ -> CallEffect
         in combineEffect invoked nested
    CoreClosure captures _ _ _ _ ->
        foldl
            combineEffect
            AllocationEffect
            (map (expressionEffectWith environment . coreCaptureValue) captures)

-- Integer division can fail even though it has no externally visible write.
-- Treating a variable divisor as pure would let dead-code elimination erase a
-- required divide-by-zero failure. Floating division follows its IEEE target
-- semantics and does not use this failure classification.
primitiveEffect :: CorePrimitive -> [CoreExpression] -> Effect
primitiveEffect primitive arguments
    | primitive `elem` [CoreDivide, CoreFloorDivide, CoreRemainder]
    , firstTypeIsInteger arguments
    , not (knownNonzeroDivisor arguments) =
        FailureEffect
    | otherwise = PureEffect

firstTypeIsInteger :: [CoreExpression] -> Bool
firstTypeIsInteger (first : _) = isCoreIntegerType (expressionType first)
firstTypeIsInteger [] = False

knownNonzeroDivisor :: [CoreExpression] -> Bool
knownNonzeroDivisor [_, CoreLiteral (CoreInteger value) _] = value /= 0
knownNonzeroDivisor [_, CoreLiteral (CoreFloating _) valueType] = isCoreFloatingType valueType
knownNonzeroDivisor _ = False

discardableExpression :: CoreExpression -> Bool
discardableExpression = discardableExpressionWith emptyEffectEnvironment

discardableExpressionWith :: EffectEnvironment -> CoreExpression -> Bool
discardableExpressionWith environment expression = expressionEffectWith environment expression == PureEffect

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
