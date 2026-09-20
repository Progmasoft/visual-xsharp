-- SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
-- SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.1

{- | Capture-avoiding linear-body inlining for verified Core.

Pure, non-recursive functions may contain immutable bindings and evaluations
before their single final return. The pass converts that straight-line body to
nested 'CoreLet' expressions. Non-trivial arguments are let-bound too, so they
are evaluated exactly once even when the callee reads a parameter repeatedly
or not at all. Every copied binder receives a fresh module-wide SymbolId.
-}
module Visual.XSharp.Core.Optimizer.Inline
    ( InlineReport (..)
    , inlineFunctions
    ) where

import Data.Map.Strict (Map)
import Data.Map.Strict qualified as Map
import Visual.XSharp.AST
import Visual.XSharp.Core
import Visual.XSharp.Core.Optimizer.Analysis
import Visual.XSharp.Core.Symbols (nextCoreSymbolValue)

data InlineStep
    = InlineBinding ResolvedName Type CoreExpression
    | InlineEvaluation CoreExpression
    deriving (Eq, Ord, Read, Show)

data InlineCandidate = InlineCandidate
    { candidateParameters :: [(ResolvedName, Type)]
    , candidateSteps :: [InlineStep]
    , candidateResult :: CoreExpression
    , candidateExpressionNodes :: Int
    }
    deriving (Eq, Ord, Read, Show)

data CandidateDiscovery = CandidateDiscovery
    { discoveredCandidates :: Map SymbolId InlineCandidate
    , discoveredExpressionCandidates :: Int
    , discoveredStatementCandidates :: Int
    , rejectedNonLinearBodies :: Int
    }

data InlineReport = InlineReport
    { inlineCandidateCount :: Int
    , inlineExpressionCandidateCount :: Int
    , inlineStatementCandidateCount :: Int
    , inlineRejectedNonLinearBodies :: Int
    , inlineRewrittenCalls :: Int
    , inlineSkippedOversized :: Int
    , inlineGeneratedParameterLets :: Int
    , inlineGeneratedLocalLets :: Int
    , inlineGeneratedEvaluationLets :: Int
    , inlineAlphaRenamedSymbols :: Int
    }
    deriving (Eq, Ord, Read, Show)

data InlineState = InlineState
    { stateNextSymbol :: Int
    , stateRewrittenCalls :: Int
    , stateSkippedOversized :: Int
    , stateGeneratedParameterLets :: Int
    , stateGeneratedLocalLets :: Int
    , stateGeneratedEvaluationLets :: Int
    , stateAlphaRenamedSymbols :: Int
    }

initialInlineState :: CoreModule -> InlineState
initialInlineState moduleValue =
    InlineState (nextCoreSymbolValue moduleValue) 0 0 0 0 0 0

inlineFunctions :: Int -> [FunctionEffectReport] -> CoreModule -> (CoreModule, InlineReport)
inlineFunctions maximumNodes effectReports moduleValue =
    let discovery = discoverCandidates effectReports moduleValue
        candidates = discoveredCandidates discovery
        (functions, finalState) =
            rewriteFunctions maximumNodes candidates (initialInlineState moduleValue) (coreModuleFunctions moduleValue)
     in ( moduleValue {coreModuleFunctions = functions}
        , InlineReport
            { inlineCandidateCount = Map.size candidates
            , inlineExpressionCandidateCount = discoveredExpressionCandidates discovery
            , inlineStatementCandidateCount = discoveredStatementCandidates discovery
            , inlineRejectedNonLinearBodies = rejectedNonLinearBodies discovery
            , inlineRewrittenCalls = stateRewrittenCalls finalState
            , inlineSkippedOversized = stateSkippedOversized finalState
            , inlineGeneratedParameterLets = stateGeneratedParameterLets finalState
            , inlineGeneratedLocalLets = stateGeneratedLocalLets finalState
            , inlineGeneratedEvaluationLets = stateGeneratedEvaluationLets finalState
            , inlineAlphaRenamedSymbols = stateAlphaRenamedSymbols finalState
            }
        )

discoverCandidates :: [FunctionEffectReport] -> CoreModule -> CandidateDiscovery
discoverCandidates reports moduleValue = foldl' discover emptyDiscovery (coreModuleFunctions moduleValue)
    where
        reportMap = Map.fromList [(resolvedSymbol (effectFunctionName report), report) | report <- reports]
        emptyDiscovery = CandidateDiscovery Map.empty 0 0 0
        discover discovery function =
            let symbol = resolvedSymbol (coreFunctionName function)
             in case Map.lookup symbol reportMap of
                    Just report
                        | effectClassification report == PureEffect
                        , not (effectIsRecursive report) ->
                            case linearizeBody (coreFunctionBody function) of
                                Just (steps, result) ->
                                    let candidate =
                                            InlineCandidate
                                                (coreFunctionParameters function)
                                                steps
                                                result
                                                (linearizedNodeCount (length (coreFunctionParameters function)) steps result)
                                        statementDelta = if null steps then 0 else 1
                                     in discovery
                                            { discoveredCandidates = Map.insert symbol candidate (discoveredCandidates discovery)
                                            , discoveredExpressionCandidates = discoveredExpressionCandidates discovery + 1 - statementDelta
                                            , discoveredStatementCandidates = discoveredStatementCandidates discovery + statementDelta
                                            }
                                Nothing -> discovery {rejectedNonLinearBodies = rejectedNonLinearBodies discovery + 1}
                    _ -> discovery

-- Only a final return is accepted. Mutable state, branches, early returns, and
-- fallthrough retain their call boundary until a dedicated CFG inliner exists.
linearizeBody :: [CoreStatement] -> Maybe ([InlineStep], CoreExpression)
linearizeBody = go []
    where
        go steps [CoreReturn result] = Just (reverse steps, result)
        go steps (CoreBind binding : remaining)
            | not (coreBindingMutable binding) =
                go (InlineBinding (coreBindingName binding) (coreBindingType binding) (coreBindingValue binding) : steps) remaining
        go steps (CoreEvaluate value : remaining) = go (InlineEvaluation value : steps) remaining
        go _ _ = Nothing

rewriteFunctions ::
    Int -> Map SymbolId InlineCandidate -> InlineState -> [CoreFunction] -> ([CoreFunction], InlineState)
rewriteFunctions maximumNodes candidates = mapAccumulating rewriteFunction
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
    CoreLet name bindingType value body valueType ->
        let (rewrittenValue, afterValue) = rewriteExpression maximumNodes candidates state value
            (rewrittenBody, afterBody) = rewriteExpression maximumNodes candidates afterValue body
         in (CoreLet name bindingType rewrittenValue rewrittenBody valueType, afterBody)
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
            (rewrittenArguments, afterArguments) = mapAccumulating (rewriteExpression maximumNodes candidates) afterCallee arguments
            rebuilt = CoreApply rewrittenCallee rewrittenArguments valueType
         in tryInline maximumNodes candidates afterArguments rebuilt

tryInline :: Int -> Map SymbolId InlineCandidate -> InlineState -> CoreExpression -> (CoreExpression, InlineState)
tryInline maximumNodes candidates state call@(CoreApply callee arguments valueType) =
    case calleeSymbol callee >>= (`Map.lookup` candidates) of
        Nothing -> (call, state)
        Just candidate
            | length (candidateParameters candidate) /= length arguments -> (call, state)
            | candidateExpressionNodes candidate > normalizedMaximum -> skipped
            | otherwise ->
                let (expanded, afterExpansion) = expandCandidate candidate arguments state
                 in if expressionNodeCount expanded > normalizedMaximum || expressionType expanded /= valueType
                        then skipped
                        else (expanded, afterExpansion {stateRewrittenCalls = stateRewrittenCalls afterExpansion + 1})
            where
                normalizedMaximum = max 1 maximumNodes
                skipped = (call, state {stateSkippedOversized = stateSkippedOversized state + 1})
tryInline _ _ state expression = (expression, state)

calleeSymbol :: CoreExpression -> Maybe SymbolId
calleeSymbol (CoreVariable name _) = Just (resolvedSymbol name)
calleeSymbol _ = Nothing

-- A substitution environment carries complete expressions. Variables and
-- literals can be copied safely; every other argument first receives a let.
expandCandidate :: InlineCandidate -> [CoreExpression] -> InlineState -> (CoreExpression, InlineState)
expandCandidate candidate arguments initialState =
    let (environment, parameterLets, afterParameters) =
            prepareParameters (candidateParameters candidate) arguments initialState
        (body, afterBody) = cloneLinearBody environment (candidateSteps candidate) (candidateResult candidate) afterParameters
     in (foldr applyLet body parameterLets, afterBody)
    where
        applyLet (name, valueType, value) body = CoreLet name valueType value body (expressionType body)

type Substitution = Map SymbolId CoreExpression

prepareParameters ::
    [(ResolvedName, Type)] ->
    [CoreExpression] ->
    InlineState ->
    (Substitution, [(ResolvedName, Type, CoreExpression)], InlineState)
prepareParameters parameters arguments = go Map.empty [] (zip parameters arguments)
    where
        go environment bindings [] state = (environment, reverse bindings, state)
        go environment bindings (((parameter, parameterType), argument) : remaining) state
            | trivialArgument argument =
                go (Map.insert (resolvedSymbol parameter) argument environment) bindings remaining state
            | otherwise =
                let (fresh, next) = freshName "argument" parameter state
                    reference = CoreVariable fresh parameterType
                    counted = next {stateGeneratedParameterLets = stateGeneratedParameterLets next + 1}
                 in go
                        (Map.insert (resolvedSymbol parameter) reference environment)
                        ((fresh, parameterType, argument) : bindings)
                        remaining
                        counted

trivialArgument :: CoreExpression -> Bool
trivialArgument CoreVariable {} = True
trivialArgument CoreLiteral {} = True
trivialArgument _ = False

cloneLinearBody :: Substitution -> [InlineStep] -> CoreExpression -> InlineState -> (CoreExpression, InlineState)
cloneLinearBody initialEnvironment steps result initialState =
    let (environment, builders, afterSteps) = cloneSteps initialEnvironment [] steps initialState
        (clonedResult, afterResult) = cloneExpression environment result afterSteps
     in (foldr ($) clonedResult builders, afterResult)

cloneSteps ::
    Substitution ->
    [CoreExpression -> CoreExpression] ->
    [InlineStep] ->
    InlineState ->
    (Substitution, [CoreExpression -> CoreExpression], InlineState)
cloneSteps environment builders [] state = (environment, reverse builders, state)
cloneSteps environment builders (step : remaining) state = case step of
    InlineBinding oldName valueType value ->
        let (clonedValue, afterValue) = cloneExpression environment value state
            (fresh, afterName) = freshName "local" oldName afterValue
            reference = CoreVariable fresh valueType
            environment' = Map.insert (resolvedSymbol oldName) reference environment
            builder body = CoreLet fresh valueType clonedValue body (expressionType body)
            counted = afterName {stateGeneratedLocalLets = stateGeneratedLocalLets afterName + 1}
         in cloneSteps environment' (builder : builders) remaining counted
    InlineEvaluation value ->
        let (clonedValue, afterValue) = cloneExpression environment value state
            (fresh, afterName) = freshSynthetic "evaluation" afterValue
            builder body = CoreLet fresh (expressionType clonedValue) clonedValue body (expressionType body)
            counted = afterName {stateGeneratedEvaluationLets = stateGeneratedEvaluationLets afterName + 1}
         in cloneSteps environment (builder : builders) remaining counted

cloneExpression :: Substitution -> CoreExpression -> InlineState -> (CoreExpression, InlineState)
cloneExpression environment expression state = case expression of
    CoreVariable name _ -> (Map.findWithDefault expression (resolvedSymbol name) environment, state)
    CoreLiteral {} -> (expression, state)
    CoreApply callee arguments valueType ->
        let (clonedCallee, afterCallee) = cloneExpression environment callee state
            (clonedArguments, afterArguments) =
                mapAccumulating (\current value -> cloneExpression environment value current) afterCallee arguments
         in (CoreApply clonedCallee clonedArguments valueType, afterArguments)
    CorePrimitive primitive arguments valueType ->
        let (clonedArguments, afterArguments) =
                mapAccumulating (\current value -> cloneExpression environment value current) state arguments
         in (CorePrimitive primitive clonedArguments valueType, afterArguments)
    CoreLet oldName bindingType value body valueType ->
        let (clonedValue, afterValue) = cloneExpression environment value state
            (fresh, afterName) = freshName "let" oldName afterValue
            bodyEnvironment = Map.insert (resolvedSymbol oldName) (CoreVariable fresh bindingType) environment
            (clonedBody, afterBody) = cloneExpression bodyEnvironment body afterName
         in (CoreLet fresh bindingType clonedValue clonedBody valueType, afterBody)
    CoreClosure captures parameters returnType body valueType ->
        let (clonedValues, afterValues) = mapAccumulating cloneCaptureValue state captures
            (captureEnvironment, clonedCaptures, afterCaptures) = cloneCaptureNames environment clonedValues afterValues
            (closureEnvironment, clonedParameters, afterParameters) = cloneNames "closure-parameter" captureEnvironment parameters afterCaptures
            (clonedBody, _, afterBody) = cloneStatements closureEnvironment body afterParameters
         in (CoreClosure clonedCaptures clonedParameters returnType clonedBody valueType, afterBody)
        where
            cloneCaptureValue current capture =
                let (value, next) = cloneExpression environment (coreCaptureValue capture) current
                 in (capture {coreCaptureValue = value}, next)

cloneCaptureNames :: Substitution -> [CoreCapture] -> InlineState -> (Substitution, [CoreCapture], InlineState)
cloneCaptureNames = go []
    where
        go changed environment [] state = (environment, reverse changed, state)
        go changed environment (capture : remaining) state =
            let oldName = coreCaptureName capture
                valueType = coreCaptureType capture
                (fresh, next) = freshName "capture" oldName state
                environment' = Map.insert (resolvedSymbol oldName) (CoreVariable fresh valueType) environment
             in go (capture {coreCaptureName = fresh} : changed) environment' remaining next

cloneNames ::
    String -> Substitution -> [(ResolvedName, Type)] -> InlineState -> (Substitution, [(ResolvedName, Type)], InlineState)
cloneNames role = go []
    where
        go changed environment [] state = (environment, reverse changed, state)
        go changed environment ((oldName, valueType) : remaining) state =
            let (fresh, next) = freshName role oldName state
                environment' = Map.insert (resolvedSymbol oldName) (CoreVariable fresh valueType) environment
             in go ((fresh, valueType) : changed) environment' remaining next

cloneStatements :: Substitution -> [CoreStatement] -> InlineState -> ([CoreStatement], Substitution, InlineState)
cloneStatements environment [] state = ([], environment, state)
cloneStatements environment (statement : remaining) state =
    let (cloned, nextEnvironment, afterStatement) = cloneStatement environment statement state
        (rest, finalEnvironment, finalState) = cloneStatements nextEnvironment remaining afterStatement
     in (cloned : rest, finalEnvironment, finalState)

cloneStatement :: Substitution -> CoreStatement -> InlineState -> (CoreStatement, Substitution, InlineState)
cloneStatement environment statement state = case statement of
    CoreBind binding ->
        let (value, afterValue) = cloneExpression environment (coreBindingValue binding) state
            oldName = coreBindingName binding
            valueType = coreBindingType binding
            (fresh, afterName) = freshName "closure-local" oldName afterValue
            changed = binding {coreBindingName = fresh, coreBindingValue = value}
            environment' = Map.insert (resolvedSymbol oldName) (CoreVariable fresh valueType) environment
         in (CoreBind changed, environment', afterName)
    CoreAssign oldName value ->
        let (clonedValue, next) = cloneExpression environment value state
            changedName = case Map.lookup (resolvedSymbol oldName) environment of
                Just (CoreVariable name _) -> name
                _ -> oldName
         in (CoreAssign changedName clonedValue, environment, next)
    CoreReturn value ->
        let (clonedValue, next) = cloneExpression environment value state
         in (CoreReturn clonedValue, environment, next)
    CoreEvaluate value ->
        let (clonedValue, next) = cloneExpression environment value state
         in (CoreEvaluate clonedValue, environment, next)
    CoreIf condition whenTrue whenFalse ->
        let (clonedCondition, afterCondition) = cloneExpression environment condition state
            (clonedTrue, _, afterTrue) = cloneStatements environment whenTrue afterCondition
            (clonedFalse, _, afterFalse) = cloneStatements environment whenFalse afterTrue
         in (CoreIf clonedCondition clonedTrue clonedFalse, environment, afterFalse)

freshName :: String -> ResolvedName -> InlineState -> (ResolvedName, InlineState)
freshName role original state =
    let value = stateNextSymbol state
        Identifier spelling = resolvedSpelling original
        fresh = ResolvedName (SymbolId value) (Identifier ("$inline." ++ role ++ "." ++ spelling ++ "." ++ show value))
     in (fresh, state {stateNextSymbol = value + 1, stateAlphaRenamedSymbols = stateAlphaRenamedSymbols state + 1})

freshSynthetic :: String -> InlineState -> (ResolvedName, InlineState)
freshSynthetic role state = freshName role (ResolvedName (SymbolId 1) (Identifier role)) state

linearizedNodeCount :: Int -> [InlineStep] -> CoreExpression -> Int
linearizedNodeCount parameterCount steps result = parameterCount + sum (map stepNodeCount steps) + expressionNodeCount result
    where
        stepNodeCount step = case step of
            InlineBinding _ _ value -> 1 + expressionNodeCount value
            InlineEvaluation value -> 1 + expressionNodeCount value

expressionNodeCount :: CoreExpression -> Int
expressionNodeCount expression = case expression of
    CoreVariable {} -> 1
    CoreLiteral {} -> 1
    CoreApply callee arguments _ -> 1 + sum (map expressionNodeCount (callee : arguments))
    CorePrimitive _ arguments _ -> 1 + sum (map expressionNodeCount arguments)
    CoreLet _ _ value body _ -> 1 + expressionNodeCount value + expressionNodeCount body
    CoreClosure captures _ _ body _ -> 1 + sum (map (expressionNodeCount . coreCaptureValue) captures) + sum (map statementNodeCount body)

statementNodeCount :: CoreStatement -> Int
statementNodeCount statement = case statement of
    CoreBind binding -> 1 + expressionNodeCount (coreBindingValue binding)
    CoreAssign _ value -> 1 + expressionNodeCount value
    CoreReturn value -> 1 + expressionNodeCount value
    CoreEvaluate value -> 1 + expressionNodeCount value
    CoreIf condition yes no -> 1 + expressionNodeCount condition + sum (map statementNodeCount (yes ++ no))
