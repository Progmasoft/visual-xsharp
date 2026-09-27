-- SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
-- SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.1
{-# LANGUAGE PatternSynonyms #-}

module Visual.XSharp.Core
    ( CoreLiteral (..)
    , CorePrimitive (..)
    , CoreCapture (..)
    , CoreExpression (..)
    , CoreStatement (..)
    , CoreBinding (..)
    , CoreFunction (..)
    , CoreModule (..)
    , pattern CoreModule
    , expressionType
    ) where

import Visual.XSharp.AST (CaptureMode, QualifiedName, ResolvedName, Type)

data CoreLiteral = CoreInteger Integer | CoreFloating String | CoreString String | CoreBoolean Bool | CoreUnit | CoreNull
    deriving (Eq, Ord, Read, Show)
data CorePrimitive
    = CoreAdd
    | CoreSubtract
    | CoreMultiply
    | CoreDivide
    | CoreFloorDivide
    | CoreRemainder
    | CoreLessThan
    | CoreLessEqual
    | CoreGreaterThan
    | CoreGreaterEqual
    | CoreEqual
    | CoreNotEqual
    | CoreLogicalAnd
    | CoreLogicalOr
    | CoreNegate
    | CoreLogicalNot
    | CorePower
    | CoreShiftLeft
    | CoreShiftRight
    | CoreBitwiseAnd
    | CoreBitwiseXor
    | CoreBitwiseOr
    | CoreBitwiseNot
    | CoreTypeIs
    deriving (Eq, Ord, Read, Show)
data CoreExpression
    = CoreVariable ResolvedName Type
    | CoreLiteral CoreLiteral Type
    | CoreApply CoreExpression [CoreExpression] Type
    | CorePrimitive CorePrimitive [CoreExpression] Type
    | -- CoreLet is expression-local sequencing. Pattern lowering uses it to
      -- evaluate a potentially effectful subject exactly once before testing
      -- several alternatives.
      CoreLet ResolvedName Type CoreExpression CoreExpression Type
    | CoreClosure
        [CoreCapture]
        [(ResolvedName, Type)]
        Type
        [CoreStatement]
        Type
    deriving (Eq, Ord, Read, Show)
data CoreCapture = CoreCapture
    { coreCaptureMode :: CaptureMode
    , coreCaptureName :: ResolvedName
    , coreCaptureType :: Type
    , coreCaptureValue :: CoreExpression
    }
    deriving (Eq, Ord, Read, Show)
data CoreStatement
    = CoreBind CoreBinding
    | CoreAssign ResolvedName CoreExpression
    | CoreReturn CoreExpression
    | CoreIf CoreExpression [CoreStatement] [CoreStatement]
    | CoreEvaluate CoreExpression
    deriving (Eq, Ord, Read, Show)
data CoreBinding = CoreBinding
    { coreBindingName :: ResolvedName
    , coreBindingType :: Type
    , coreBindingMutable :: Bool
    , coreBindingValue :: CoreExpression
    }
    deriving (Eq, Ord, Read, Show)
data CoreFunction = CoreFunction
    { coreFunctionName :: ResolvedName
    , coreFunctionParameters :: [(ResolvedName, Type)]
    , coreFunctionReturnType :: Type
    , coreFunctionBody :: [CoreStatement]
    }
    deriving (Eq, Ord, Read, Show)

{- | Source ownership is deliberately side metadata rather than syntax or
semantics.  Function identities remain stable while the optimizer rewrites
bodies, and the native backend can partition definitions without guessing
from declaration spelling or filesystem layout.
The named data constructor carries compiler provenance.  The two-argument
pattern below preserves the long-standing source-level construction API for
hand-written Core fixtures; project compilation uses CoreModuleWithSources.
-}
data CoreModule = CoreModuleWithSources
    { coreModuleName :: QualifiedName
    , coreModuleFunctions :: [CoreFunction]
    , coreModuleSourceFiles :: [FilePath]
    , coreModuleFunctionSources :: [(Int, FilePath)]
    }
    deriving (Eq, Ord, Read, Show)

pattern CoreModule :: QualifiedName -> [CoreFunction] -> CoreModule
pattern CoreModule name functions <- CoreModuleWithSources name functions _ _
    where
        CoreModule name functions = CoreModuleWithSources name functions [] []

{-# COMPLETE CoreModule #-}

expressionType :: CoreExpression -> Type
expressionType expression = case expression of
    CoreVariable _ value -> value
    CoreLiteral _ value -> value
    CoreApply _ _ value -> value
    CorePrimitive _ _ value -> value
    CoreLet _ _ _ _ value -> value
    CoreClosure _ _ _ _ value -> value
