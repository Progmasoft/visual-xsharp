-- SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
-- SPDX-License-Identifier: MPL-2.0

module Visual.XSharp.Core
    ( CoreLiteral (..)
    , CoreExpression (..)
    , CoreBinding (..)
    , CoreModule (..)
    ) where

import Visual.XSharp.AST (QualifiedName, ResolvedName, Type)

data CoreLiteral
    = CoreInteger Integer
    | CoreDecimal Rational
    | CoreString String
    | CoreBoolean Bool
    | CoreUnit
    deriving (Eq, Ord, Read, Show)

data CoreExpression
    = CoreVariable ResolvedName
    | CoreLiteral CoreLiteral
    | CoreApply CoreExpression [CoreExpression]
    | CoreLambda [(ResolvedName, Type)] CoreExpression
    | CoreLet [CoreBinding] CoreExpression
    deriving (Eq, Ord, Read, Show)

data CoreBinding = CoreBinding
    { coreBindingName :: ResolvedName
    , coreBindingType :: Type
    , coreBindingValue :: CoreExpression
    }
    deriving (Eq, Ord, Read, Show)

data CoreModule = CoreModule
    { coreModuleName :: QualifiedName
    , coreModuleBindings :: [CoreBinding]
    }
    deriving (Eq, Ord, Read, Show)
