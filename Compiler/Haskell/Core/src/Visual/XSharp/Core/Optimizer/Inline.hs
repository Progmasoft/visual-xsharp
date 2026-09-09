-- SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
-- SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.1

{- | Conservative expression inlining for verified Core.

Inlining is deliberately narrower than ordinary substitution. A candidate must
be a non-recursive pure function whose complete body is one return statement.
Call arguments must be variables or literals, so substituting a parameter zero,
one, or several times cannot erase or duplicate observable evaluation.
-}
module Visual.XSharp.Core.Optimizer.Inline
    ( InlineReport (..)
    , inlineFunctions
    ) where

import Data.Map.Strict (Map)
import Data.Map.Strict qualified as Map
import Visual.XSharp.AST (ResolvedName, SymbolId, Type, resolvedSymbol)
import Visual.XSharp.Core
import Visual.XSharp.Core.Optimizer.Analysis

data InlineCandidate = InlineCandidate
    { candidateParameters :: [(ResolvedName, Type)]
    , candidateResult :: CoreExpression
    , candidateExpressionNodes :: Int
    }
    deriving (Eq, Ord, Read, Show)

data InlineReport = InlineReport
    { inlineCandidateCount :: Int
    , inlineRewrittenCalls :: Int
    , inlineSkippedUnsafeArguments :: Int
    , inlineSkippedOversized :: Int
    }
    deriving (Eq, Ord, Read, Show)

data InlineState = InlineState
    { stateRewrittenCalls :: Int
    , stateSkippedUnsafeArguments :: Int
    , stateSkippedOversized :: Int
    }

emptyInlineState :: InlineState
emptyInlineState = InlineState 0 0 0

inlineFunctions :: Int -> [FunctionEffectReport] -> CoreModule -> (CoreModule, InlineReport)
inlineFunctions maximumNodes effectReports moduleValue =
    let candidates = discoverCandidates effectReports moduleValue
        (functions, finalState) = rewriteFunctions maximumNodes candidates (coreModuleFunctions moduleValue)
     in ( moduleValue {coreModuleFunctions = functions}
        , InlineReport
            { inlineCandidateCount = Map.size candidates
            , inlineRewrittenCalls = stateRewrittenCalls finalState
            , inlineSkippedUnsafeArguments = stateSkippedUnsafeArguments finalState
            , inlineSkippedOversized = stateSkippedOversized finalState
            }
        )

discoverCandidates :: [FunctionEffectReport] -> CoreModule -> Map SymbolId InlineCandidate
discoverCandidates reports moduleValue =
    Map.fromList
        [ (resolvedSymbol (coreFunctionName function), InlineCandidate parameters result nodeCount)
        | function <- coreModuleFunctions moduleValue
        , let symbol = resolvedSymbol (coreFunctionName function)
        , isPureNonRecursive symbol
        , [CoreReturn result] <- [coreFunctionBody function]
        , let parameters = coreFunctionParameters function
        , let nodeCount = expressionNodeCount result
        ]
    where
        reportMap = Map.fromList [(resolvedSymbol (effectFunctionName report), report) | report <- reports]
        isPureNonRecursive symbol = case Map.lookup symbol reportMap of
            Just report -> effectClassification report == PureEffect && not (effectIsRecursive report)
            Nothing -> False

rewriteFunctions :: Int -> Map SymbolId InlineCandidate -> [CoreFunction] -> ([CoreFunction], InlineState)
rewriteFunctions maximumNodes candidates = mapAccumulating rewriteFunction emptyInlineState
    where
        rewriteFunction state function =
            let (body, next) = rewriteStatements maximumNodes candidates state (coreFunctionBody function)
             in (function {coreFunctionBody = body}, next)

mapAccumulating :: (state -> value -> (result, state)) -> state -> [value] -> ([result], state)
mapAccumulating _ state [] = ([], state)
mapAccumulating transform state (value : remaining) =
    let (result, afterValue) = transform state value
        (results, finalState) = mapAccumulating transform afterValue remaining
     in (result : results, finalState)

rewriteStatements ::
    Int -> Map SymbolId InlineCandidate -> InlineState -> [CoreStatement] -> ([CoreStatement], InlineState)
rewriteStatements maximumNodes candidates = mapAccumulating rewrite
    where
        rewrite state statement = case statement of
            CoreBind binding ->
                let (value, next) = rewriteExpression maximumNodes candidates state (coreBindingValue binding)
                 in (CoreBind binding {coreBindingValue = value}, next)
            CoreAssign name value ->
                let (rewritten, next) = rewriteExpression maximumNodes candidates state value
                 in (CoreAssign name rewritten, next)
            CoreReturn value ->
                let (rewritten, next) = rewriteExpression maximumNodes candidates state value
                 in (CoreReturn rewritten, next)
            CoreEvaluate value ->
                let (rewritten, next) = rewriteExpression maximumNodes candidates state value
                 in (CoreEvaluate rewritten, next)
            CoreIf condition yes no ->
                let (rewrittenCondition, afterCondition) = rewriteExpression maximumNodes candidates state condition
                    (rewrittenYes, afterYes) = rewriteStatements maximumNodes candidates afterCondition yes
                    (rewrittenNo, finalState) = rewriteStatements maximumNodes candidates afterYes no
                 in (CoreIf rewrittenCondition rewrittenYes rewrittenNo, finalState)

rewriteExpression ::
    Int -> Map SymbolId InlineCandidate -> InlineState -> CoreExpression -> (CoreExpression, InlineState)
rewriteExpression maximumNodes candidates state expression = case expression of
    CoreVariable {} -> (expression, state)
    CoreLiteral {} -> (expression, state)
    CorePrimitive primitive arguments valueType ->
        let (rewritten, next) = mapAccumulating (rewriteExpression maximumNodes candidates) state arguments
         in (CorePrimitive primitive rewritten valueType, next)
    CoreClosure captures parameters returnType body valueType ->
        let (rewrittenCaptures, afterCaptures) = mapAccumulating rewriteCapture state captures
            (rewrittenBody, finalState) = rewriteStatements maximumNodes candidates afterCaptures body
         in (CoreClosure rewrittenCaptures parameters returnType rewrittenBody valueType, finalState)
        where
            rewriteCapture current capture =
                let (value, next) = rewriteExpression maximumNodes candidates current (coreCaptureValue capture)
                 in (capture {coreCaptureValue = value}, next)
    CoreApply callee arguments valueType ->
        let (rewrittenCallee, afterCallee) = rewriteExpression maximumNodes candidates state callee
            (rewrittenArguments, afterArguments) =
                mapAccumulating (rewriteExpression maximumNodes candidates) afterCallee arguments
            rebuilt = CoreApply rewrittenCallee rewrittenArguments valueType
         in tryInline maximumNodes candidates afterArguments rebuilt

tryInline :: Int -> Map SymbolId InlineCandidate -> InlineState -> CoreExpression -> (CoreExpression, InlineState)
tryInline maximumNodes candidates state call@(CoreApply callee arguments valueType) = case calleeSymbol callee >>= (`Map.lookup` candidates) of
    Nothing -> (call, state)
    Just candidate
        | length (candidateParameters candidate) /= length arguments -> (call, state)
        | not (all safeInlineArgument arguments) ->
            (call, state {stateSkippedUnsafeArguments = stateSkippedUnsafeArguments state + 1})
        | candidateExpressionNodes candidate > normalizedMaximum ->
            (call, state {stateSkippedOversized = stateSkippedOversized state + 1})
        | expandedNodes > normalizedMaximum ->
            (call, state {stateSkippedOversized = stateSkippedOversized state + 1})
        | expressionType substituted /= valueType -> (call, state)
        | otherwise ->
            ( substituted
            , state {stateRewrittenCalls = stateRewrittenCalls state + 1}
            )
        where
            substitutions =
                Map.fromList
                    [ (resolvedSymbol parameter, argument)
                    | ((parameter, _), argument) <- zip (candidateParameters candidate) arguments
                    ]
            substituted = substituteExpression substitutions (candidateResult candidate)
            expandedNodes = expressionNodeCount substituted
            normalizedMaximum = max 1 maximumNodes
tryInline _ _ state expression = (expression, state)

calleeSymbol :: CoreExpression -> Maybe SymbolId
calleeSymbol (CoreVariable name _) = Just (resolvedSymbol name)
calleeSymbol _ = Nothing

safeInlineArgument :: CoreExpression -> Bool
safeInlineArgument CoreVariable {} = True
safeInlineArgument CoreLiteral {} = True
safeInlineArgument _ = False

substituteExpression :: Map SymbolId CoreExpression -> CoreExpression -> CoreExpression
substituteExpression substitutions expression = case expression of
    CoreVariable name _ -> Map.findWithDefault expression (resolvedSymbol name) substitutions
    CoreLiteral {} -> expression
    CoreApply callee arguments valueType ->
        CoreApply
            (substituteExpression substitutions callee)
            (map (substituteExpression substitutions) arguments)
            valueType
    CorePrimitive primitive arguments valueType ->
        CorePrimitive primitive (map (substituteExpression substitutions) arguments) valueType
    CoreClosure captures parameters returnType body valueType ->
        let shadowed = foldr (Map.delete . resolvedSymbol . fst) substitutions parameters
         in CoreClosure
                [capture {coreCaptureValue = substituteExpression substitutions (coreCaptureValue capture)} | capture <- captures]
                parameters
                returnType
                (map (substituteStatement shadowed) body)
                valueType

substituteStatement :: Map SymbolId CoreExpression -> CoreStatement -> CoreStatement
substituteStatement substitutions statement = case statement of
    CoreBind binding ->
        CoreBind binding {coreBindingValue = substituteExpression substitutions (coreBindingValue binding)}
    CoreAssign name value -> CoreAssign name (substituteExpression substitutions value)
    CoreReturn value -> CoreReturn (substituteExpression substitutions value)
    CoreEvaluate value -> CoreEvaluate (substituteExpression substitutions value)
    CoreIf condition yes no ->
        CoreIf
            (substituteExpression substitutions condition)
            (map (substituteStatement substitutions) yes)
            (map (substituteStatement substitutions) no)

expressionNodeCount :: CoreExpression -> Int
expressionNodeCount expression = case expression of
    CoreVariable {} -> 1
    CoreLiteral {} -> 1
    CoreApply callee arguments _ -> 1 + sum (map expressionNodeCount (callee : arguments))
    CorePrimitive _ arguments _ -> 1 + sum (map expressionNodeCount arguments)
    CoreClosure captures _ _ body _ ->
        1
            + sum (map (expressionNodeCount . coreCaptureValue) captures)
            + sum (map statementNodeCount body)

statementNodeCount :: CoreStatement -> Int
statementNodeCount statement = case statement of
    CoreBind binding -> 1 + expressionNodeCount (coreBindingValue binding)
    CoreAssign _ value -> 1 + expressionNodeCount value
    CoreReturn value -> 1 + expressionNodeCount value
    CoreEvaluate value -> 1 + expressionNodeCount value
    CoreIf condition yes no ->
        1 + expressionNodeCount condition + sum (map statementNodeCount (yes ++ no))
