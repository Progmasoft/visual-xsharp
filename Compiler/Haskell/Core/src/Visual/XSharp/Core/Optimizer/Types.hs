-- SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
-- SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.0

module Visual.XSharp.Core.Optimizer.Types
    ( OptimizerOptions (..)
    , OptimizationPass (..)
    , OptimizationMetrics (..)
    , PassReport (..)
    , OptimizationResult (..)
    , defaultOptimizerOptions
    , emptyMetrics
    , addMetrics
    , measureModule
    ) where

import Visual.XSharp.Core

data OptimizerOptions = OptimizerOptions
    { optimizerMaximumIterations :: Int
    , optimizerConstantPropagation :: Bool
    , optimizerControlFlowSimplification :: Bool
    , optimizerDeadCodeElimination :: Bool
    }
    deriving (Eq, Ord, Read, Show)

defaultOptimizerOptions :: OptimizerOptions
defaultOptimizerOptions = OptimizerOptions 12 True True True

-- Pass names are data rather than display strings so compiler drivers can
-- render reports without parsing human-oriented output. The order of these
-- constructors does not define execution order; Pipeline owns that policy.
data OptimizationPass
    = ConstantPropagationPass
    | ControlFlowSimplificationPass
    | DeadCodeEliminationPass
    deriving (Eq, Ord, Read, Show)

data OptimizationMetrics = OptimizationMetrics
    { metricFunctions :: Int
    , metricStatements :: Int
    , metricExpressions :: Int
    , metricBindings :: Int
    , metricAssignments :: Int
    , metricBranches :: Int
    , metricCalls :: Int
    , metricClosures :: Int
    }
    deriving (Eq, Ord, Read, Show)

data OptimizationResult = OptimizationResult
    { optimizedCore :: CoreModule
    , optimizationBefore :: OptimizationMetrics
    , optimizationAfter :: OptimizationMetrics
    , optimizationIterations :: Int
    , optimizationConverged :: Bool
    , optimizationPassReports :: [PassReport]
    }
    deriving (Eq, Ord, Read, Show)

-- A report describes one enabled pass invocation in one fixed-point
-- iteration. Keeping the complete trace makes an optimizer decision
-- inspectable without exposing an unstable textual dump as a public API.
data PassReport = PassReport
    { passReportIteration :: Int
    , passReportPass :: OptimizationPass
    , passReportBefore :: OptimizationMetrics
    , passReportAfter :: OptimizationMetrics
    , passReportChanged :: Bool
    }
    deriving (Eq, Ord, Read, Show)

emptyMetrics :: OptimizationMetrics
emptyMetrics = OptimizationMetrics 0 0 0 0 0 0 0 0

addMetrics :: OptimizationMetrics -> OptimizationMetrics -> OptimizationMetrics
addMetrics left right =
    OptimizationMetrics
        { metricFunctions = metricFunctions left + metricFunctions right
        , metricStatements = metricStatements left + metricStatements right
        , metricExpressions = metricExpressions left + metricExpressions right
        , metricBindings = metricBindings left + metricBindings right
        , metricAssignments = metricAssignments left + metricAssignments right
        , metricBranches = metricBranches left + metricBranches right
        , metricCalls = metricCalls left + metricCalls right
        , metricClosures = metricClosures left + metricClosures right
        }

measureModule :: CoreModule -> OptimizationMetrics
measureModule moduleValue = foldl addMetrics emptyMetrics (map measureFunction (coreModuleFunctions moduleValue))

measureFunction :: CoreFunction -> OptimizationMetrics
measureFunction function = (measureStatements (coreFunctionBody function)) {metricFunctions = 1}

measureStatements :: [CoreStatement] -> OptimizationMetrics
measureStatements = foldl (\total statement -> addMetrics total (measureStatement statement)) emptyMetrics

measureStatement :: CoreStatement -> OptimizationMetrics
measureStatement statement =
    let nested = case statement of
            CoreBind binding -> (measureExpression (coreBindingValue binding)) {metricBindings = 1}
            CoreAssign _ value -> (measureExpression value) {metricAssignments = 1}
            CoreReturn value -> measureExpression value
            CoreIf condition yes no ->
                (addMetrics (measureExpression condition) (addMetrics (measureStatements yes) (measureStatements no)))
                    { metricBranches = 1
                    }
            CoreEvaluate value -> measureExpression value
     in nested {metricStatements = metricStatements nested + 1}

measureExpression :: CoreExpression -> OptimizationMetrics
measureExpression expression =
    let nested = case expression of
            CoreVariable _ _ -> emptyMetrics
            CoreLiteral _ _ -> emptyMetrics
            CoreApply callee arguments _ ->
                (foldl addMetrics (measureExpression callee) (map measureExpression arguments)) {metricCalls = 1}
            CorePrimitive _ arguments _ -> foldl addMetrics emptyMetrics (map measureExpression arguments)
            CoreClosure captures _ _ body _ ->
                ( addMetrics
                    (foldl addMetrics emptyMetrics (map (measureExpression . coreCaptureValue) captures))
                    (measureStatements body)
                )
                    { metricClosures = 1
                    }
     in nested {metricExpressions = metricExpressions nested + 1}
