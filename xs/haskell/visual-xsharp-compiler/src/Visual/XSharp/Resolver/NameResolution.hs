-- SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
-- SPDX-License-Identifier: MPL-2.0

module Visual.XSharp.Resolver.NameResolution
    ( NameResolution (..)
    , runNameResolution
    ) where

import Visual.XSharp.AST (RenamedAST, ResolvedAST)
import Visual.XSharp.Diagnostic (Diagnostic)

-- | Resolves renamed references to stable symbol identities.
newtype NameResolution = NameResolution
    { resolveRenamedAST :: RenamedAST -> Either [Diagnostic] ResolvedAST
    }

runNameResolution :: NameResolution -> RenamedAST -> Either [Diagnostic] ResolvedAST
runNameResolution = resolveRenamedAST
