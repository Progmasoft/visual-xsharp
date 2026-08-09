-- SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
-- SPDX-License-Identifier: MPL-2.0

module Visual.XSharp.Desugarer
    ( Desugarer (..)
    , runDesugarer
    ) where

import Visual.XSharp.AST (TypedAST)
import Visual.XSharp.Core (CoreModule)
import Visual.XSharp.Diagnostic (Diagnostic)

-- | Lowers checked surface syntax to target-independent Core.
newtype Desugarer = Desugarer
    { desugarTypedAST :: TypedAST -> Either [Diagnostic] CoreModule
    }

runDesugarer :: Desugarer -> TypedAST -> Either [Diagnostic] CoreModule
runDesugarer = desugarTypedAST
