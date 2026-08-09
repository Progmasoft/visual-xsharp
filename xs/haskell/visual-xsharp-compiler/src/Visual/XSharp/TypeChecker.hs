-- SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
-- SPDX-License-Identifier: MPL-2.0

module Visual.XSharp.TypeChecker
    ( TypeChecker (..)
    , runTypeChecker
    ) where

import Visual.XSharp.AST (ResolvedAST, TypedAST)
import Visual.XSharp.Diagnostic (Diagnostic)

-- | Checks a fully resolved tree and annotates every declaration with a type.
newtype TypeChecker = TypeChecker
    { checkResolvedAST :: ResolvedAST -> Either [Diagnostic] TypedAST
    }

runTypeChecker :: TypeChecker -> ResolvedAST -> Either [Diagnostic] TypedAST
runTypeChecker = checkResolvedAST
