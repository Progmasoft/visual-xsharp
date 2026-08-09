-- SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
-- SPDX-License-Identifier: MPL-2.0

module Visual.XSharp.Core.Optimizer
    ( CoreOptimizer (..)
    , runCoreOptimizer
    ) where

import Visual.XSharp.Core (CoreModule)
import Visual.XSharp.Diagnostic (Diagnostic)

newtype CoreOptimizer = CoreOptimizer
    { optimizeCore :: CoreModule -> Either [Diagnostic] CoreModule
    }

runCoreOptimizer :: CoreOptimizer -> CoreModule -> Either [Diagnostic] CoreModule
runCoreOptimizer = optimizeCore
