-- SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
-- SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.1

{- | Classic enums: their members, their values and their rules.

A classic enum is a value type whose members are named integers of one
underlying type. Members are numbered from zero, a member without a written
value takes the value after that of the member before it, and two members
may have the same value. An enum is a type of its own: its values are
compared with @==@ and @\\=@ and with nothing else, and there is no
conversion between an enum and an integer in either direction.

The checker reads the enums of a source set into a table before it checks
any body, so that a use may precede the declaration.
-}
module Visual.XSharp.TypeChecker.Enums
    ( EnumInfo (..)
    , enumInfos
    , enumBySymbol
    , enumBySpelling
    , enumMemberValue
    , enumDeclarationProblems
    , isEnumType
    ) where

import Data.List (nub, sort)
import Visual.XSharp.AST
import Visual.XSharp.BuiltinTypes
import Visual.XSharp.Diagnostic
import Visual.XSharp.TypeChecker.Literals (problem)

-- | What the checker knows of one enum.
data EnumInfo = EnumInfo
    { enumInfoName :: ResolvedName
    -- ^ The declared name and its symbol.
    , enumInfoType :: Type
    -- ^ The type of its values, as 'enumType' encodes it.
    , enumInfoMembers :: [(Identifier, Integer)]
    -- ^ Its members in source order, each with its value.
    }

-- | The enums among the declarations of a source set.
enumInfos :: [Declaration ResolvedName ()] -> [EnumInfo]
enumInfos declarations =
    [ EnumInfo name (enumType (resolvedSpelling name) (underlyingType underlying) (sort (nub (map snd members)))) members
    | EnumDeclaration _ name _ underlying cases <- declarations
    , let members = numbered cases
    ]

-- | Each member with its value: the written one, or one more than the last.
numbered :: [EnumCase] -> [(Identifier, Integer)]
numbered = go 0
    where
        go _ [] = []
        go next (member : remaining) =
            let value = maybe next id (enumCaseValue member)
             in (enumCaseName member, value) : go (value + 1) remaining

{- | The underlying type an enum declares, @int@ when it declares none. A
type that is not an integer type is reported by 'enumDeclarationProblems';
the enum is then checked as if it had declared none.
-}
underlyingType :: Maybe TypeSyntax -> Type
underlyingType syntax = maybe intType scalarTypeToType (syntax >>= integerScalar)

integerScalar :: TypeSyntax -> Maybe ScalarType
integerScalar syntax = case syntax of
    ExplicitType (Identifier spelling) ->
        case [scalar | scalar <- scalarTypes, scalarTypeName scalar == spelling, isInteger scalar] of
            scalar : _ -> Just scalar
            [] -> Nothing
    _ -> Nothing
    where
        isInteger scalar = scalarTypeFamily scalar `elem` [SignedIntegerFamily, UnsignedIntegerFamily]

-- | The enum a name in an expression refers to.
enumBySymbol :: [EnumInfo] -> SymbolId -> Maybe EnumInfo
enumBySymbol enums symbol = case [info | info <- enums, resolvedSymbol (enumInfoName info) == symbol] of
    info : _ -> Just info
    [] -> Nothing

-- | The enum a name in a type refers to.
enumBySpelling :: [EnumInfo] -> Identifier -> Maybe EnumInfo
enumBySpelling enums spelling = case [info | info <- enums, resolvedSpelling (enumInfoName info) == spelling] of
    info : _ -> Just info
    [] -> Nothing

-- | The value of a member of the enum with the given type.
enumMemberValue :: [EnumInfo] -> Type -> Identifier -> Maybe Integer
enumMemberValue enums valueType member =
    case [value | info <- enums, enumInfoType info == valueType, (name, value) <- enumInfoMembers info, name == member] of
        value : _ -> Just value
        [] -> Nothing

-- | Whether a type is the type of an enum.
isEnumType :: Type -> Bool
isEnumType valueType = case enumUnderlyingType valueType of
    Just _ -> True
    Nothing -> False

{- | The problems of an enum declaration by itself: an underlying type that
is not an integer type, a member named twice, and a value its underlying
type cannot hold.
-}
enumDeclarationProblems :: Declaration ResolvedName () -> [Diagnostic]
enumDeclarationProblems declaration = case declaration of
    EnumDeclaration spanValue _ _ underlying cases ->
        let scalar = maybe (Just defaultScalar) integerScalar underlying
            members = zip cases (map snd (numbered cases))
         in [ problem spanValue "VXT0066" "the underlying type of an enum must be an integer type"
            | Nothing <- [scalar]
            ]
                ++ [ problem
                        (enumCaseSpan member)
                        "VXT0067"
                        ("the enum already has a member named " ++ identifierText (enumCaseName member))
                   | (index, member) <- zip [0 :: Int ..] cases
                   , enumCaseName member `elem` map enumCaseName (take index cases)
                   ]
                ++ [ problem
                        (enumCaseSpan member)
                        "VXT0068"
                        ( "the value "
                            ++ show value
                            ++ " of this enum member does not fit "
                            ++ scalarTypeName (maybe defaultScalar id scalar)
                        )
                   | (member, value) <- members
                   , not (integerFits (maybe defaultScalar id scalar) value)
                   ]
    _ -> []
    where
        defaultScalar = defaultIntegerScalar
