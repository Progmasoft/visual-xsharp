-- SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
-- SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.0
module Visual.XSharp.Core
    ( CoreLiteral (..), CorePrimitive (..), CoreExpression (..), CoreStatement (..)
    , CoreBinding (..), CoreFunction (..), CoreModule (..), expressionType ) where

import Visual.XSharp.AST (QualifiedName, ResolvedName, Type)

data CoreLiteral = CoreInteger Integer | CoreString String | CoreBoolean Bool | CoreUnit
    deriving (Eq, Ord, Read, Show)
data CorePrimitive
    = CoreAdd | CoreSubtract | CoreMultiply | CoreDivide | CoreFloorDivide | CoreRemainder
    | CoreLessThan | CoreLessEqual | CoreGreaterThan | CoreGreaterEqual | CoreEqual | CoreNotEqual
    | CoreLogicalAnd | CoreLogicalOr | CoreNegate | CoreLogicalNot
    deriving (Eq, Ord, Read, Show)
data CoreExpression
    = CoreVariable ResolvedName Type
    | CoreLiteral CoreLiteral Type
    | CoreApply CoreExpression [CoreExpression] Type
    | CorePrimitive CorePrimitive [CoreExpression] Type
    deriving (Eq, Ord, Read, Show)
data CoreStatement
    = CoreBind CoreBinding
    | CoreAssign ResolvedName CoreExpression
    | CoreReturn CoreExpression
    | CoreIf CoreExpression [CoreStatement] [CoreStatement]
    | CoreEvaluate CoreExpression
    deriving (Eq, Ord, Read, Show)
data CoreBinding = CoreBinding
    { coreBindingName :: ResolvedName, coreBindingType :: Type, coreBindingMutable :: Bool, coreBindingValue :: CoreExpression }
    deriving (Eq, Ord, Read, Show)
data CoreFunction = CoreFunction
    { coreFunctionName :: ResolvedName, coreFunctionParameters :: [(ResolvedName, Type)]
    , coreFunctionReturnType :: Type, coreFunctionBody :: [CoreStatement] }
    deriving (Eq, Ord, Read, Show)
data CoreModule = CoreModule { coreModuleName :: QualifiedName, coreModuleFunctions :: [CoreFunction] }
    deriving (Eq, Ord, Read, Show)

expressionType :: CoreExpression -> Type
expressionType expression = case expression of
    CoreVariable _ value -> value; CoreLiteral _ value -> value; CoreApply _ _ value -> value; CorePrimitive _ _ value -> value
