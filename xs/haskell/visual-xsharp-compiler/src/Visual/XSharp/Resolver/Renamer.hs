-- SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
-- SPDX-License-Identifier: MPL-2.0

module Visual.XSharp.Resolver.Renamer
    ( Renamer (..)
    , runRenamer
    ) where

import Visual.XSharp.AST (ParsedAST, RenamedAST)
import Visual.XSharp.Diagnostic (Diagnostic)

-- | Assigns stable local uniques without performing global name resolution.
newtype Renamer = Renamer
    { renameParsedAST :: ParsedAST -> Either [Diagnostic] RenamedAST
    }

runRenamer :: Renamer -> ParsedAST -> Either [Diagnostic] RenamedAST
runRenamer = renameParsedAST
