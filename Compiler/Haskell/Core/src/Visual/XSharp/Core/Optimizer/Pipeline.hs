-- SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
-- SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.1

module Visual.XSharp.Core.Optimizer.Pipeline (runOptimizationPipeline) where

import Visual.XSharp.Core
import Visual.XSharp.Core.Optimizer.Analysis (emptyEffectEnvironment)
import Visual.XSharp.Core.Optimizer.Constant
import Visual.XSharp.Core.Optimizer.ControlFlow
import Visual.XSharp.Core.Optimizer.EffectInference
import Visual.XSharp.Core.Optimizer.Liveness
import Visual.XSharp.Core.Optimizer.Types

runOptimizationPipeline :: OptimizerOptions -> CoreModule -> OptimizationResult
runOptimizationPipeline options original =
    let maximumIterations = max 1 (optimizerMaximumIterations options)
        (optimized, iterations, converged, reports) = iteratePipeline maximumIterations 1 original []
     in OptimizationResult
            { optimizedCore = optimized
            , optimizationBefore = measureModule original
            , optimizationAfter = measureModule optimized
            , optimizationIterations = iterations
            , optimizationConverged = converged
            , optimizationPassReports = reports
            , optimizationEffectReports = finalEffectReports optimized
            }
    where
        finalEffectReports value
            | optimizerInterproceduralEffects options = snd (inferFunctionEffects value)
            | otherwise = []
        iteratePipeline maximumIterations iteration current reports =
            let (next, currentReports) = runIteration options iteration current
                accumulated = reports ++ currentReports
             in if next == current
                    then (next, iteration, True, accumulated)
                    else
                        if iteration >= maximumIterations
                            then (next, iteration, False, accumulated)
                            else iteratePipeline maximumIterations (iteration + 1) next accumulated

runIteration :: OptimizerOptions -> Int -> CoreModule -> (CoreModule, [PassReport])
runIteration options iteration input =
    let (afterConstants, constantReports) =
            runEnabled
                (optimizerConstantPropagation options)
                ConstantPropagationPass
                propagateConstants
                input
        controlEnvironment = inferEnvironment afterConstants
        (afterControlFlow, controlFlowReports) =
            runEnabled
                (optimizerControlFlowSimplification options)
                ControlFlowSimplificationPass
                (simplifyControlFlowWith controlEnvironment)
                afterConstants
        deadCodeEnvironment = inferEnvironment afterControlFlow
        (afterDeadCode, deadCodeReports) =
            runEnabled
                (optimizerDeadCodeElimination options)
                DeadCodeEliminationPass
                (eliminateDeadCodeWith deadCodeEnvironment)
                afterControlFlow
     in (afterDeadCode, constantReports ++ controlFlowReports ++ deadCodeReports)
    where
        inferEnvironment value
            | optimizerInterproceduralEffects options = fst (inferFunctionEffects value)
            | otherwise = emptyEffectEnvironment
        runEnabled enabled passName pass before
            | not enabled = (before, [])
            | otherwise =
                let after = pass before
                 in ( after
                    ,
                        [ PassReport
                            { passReportIteration = iteration
                            , passReportPass = passName
                            , passReportBefore = measureModule before
                            , passReportAfter = measureModule after
                            , passReportChanged = before /= after
                            }
                        ]
                    )
