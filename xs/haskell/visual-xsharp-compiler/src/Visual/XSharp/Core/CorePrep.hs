-- SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
-- SPDX-License-Identifier: MPL-2.0

module Visual.XSharp.Core.CorePrep
    ( CorePrepAtom (..)
    , CorePrepExpression (..)
    , CorePrepBinding (..)
    , CorePrepModule (..)
    ) where

import Visual.XSharp.AST (QualifiedName, ResolvedName, Type)
import Visual.XSharp.Core (CoreLiteral)

-- | CorePrep atoms make evaluation order explicit before Xpp lowering.
data CorePrepAtom
    = CorePrepVariable ResolvedName
    | CorePrepLiteral CoreLiteral
    deriving (Eq, Ord, Read, Show)

data CorePrepExpression
    = CorePrepAtom CorePrepAtom
    | CorePrepCall CorePrepAtom [CorePrepAtom]
    | CorePrepLet CorePrepBinding CorePrepExpression
    deriving (Eq, Ord, Read, Show)

data CorePrepBinding = CorePrepBinding
    { corePrepBindingName :: ResolvedName
    , corePrepBindingType :: Type
    , corePrepBindingValue :: CorePrepExpression
    }
    deriving (Eq, Ord, Read, Show)

data CorePrepModule = CorePrepModule
    { corePrepModuleName :: QualifiedName
    , corePrepModuleBindings :: [CorePrepBinding]
    }
    deriving (Eq, Ord, Read, Show)
