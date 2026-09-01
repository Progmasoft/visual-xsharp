-- SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
-- SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.0

{- | Public facade for the target-independent Core optimization pipeline.

The optimizer accepts only verified Core and verifies its final result again.
That invariant makes the facade safe for compiler orchestration and for tools
which load a Core artifact without running the normal frontend first.
-}
module Visual.XSharp.Core.Optimizer
    ( CoreOptimizer (..)
    , OptimizerOptions (..)
    , OptimizationPass (..)
    , OptimizationMetrics (..)
    , PassReport (..)
    , OptimizationResult (..)
    , defaultCoreOptimizer
    , defaultOptimizerOptions
    , runCoreOptimizer
    , optimizeCoreWith
    ) where

import Visual.XSharp.Core
import Visual.XSharp.Core.Optimizer.Pipeline
import Visual.XSharp.Core.Optimizer.Types
import Visual.XSharp.Core.Verifier (verifyCore)
import Visual.XSharp.Diagnostic (Diagnostic)

newtype CoreOptimizer = CoreOptimizer
    { optimizeCore :: CoreModule -> Either [Diagnostic] CoreModule
    }

runCoreOptimizer :: CoreOptimizer -> CoreModule -> Either [Diagnostic] CoreModule
runCoreOptimizer = optimizeCore

defaultCoreOptimizer :: CoreOptimizer
defaultCoreOptimizer =
    CoreOptimizer $ \moduleValue -> optimizedCore <$> optimizeCoreWith defaultOptimizerOptions moduleValue

optimizeCoreWith :: OptimizerOptions -> CoreModule -> Either [Diagnostic] OptimizationResult
optimizeCoreWith options moduleValue = do
    verified <- verifyCore moduleValue
    let result = runOptimizationPipeline options verified
    checked <- verifyCore (optimizedCore result)
    pure result {optimizedCore = checked}
