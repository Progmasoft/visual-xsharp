-- SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
-- SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.0

{- | Canonical built-in scalar descriptions shared by type checking and Core.
Widths here are language widths, never host-machine widths.
-}
module Visual.XSharp.BuiltinTypes
    ( ScalarFamily (..)
    , ScalarType (..)
    , scalarTypes
    , scalarTypeName
    , scalarTypeWidth
    , scalarTypeFamily
    , scalarTypeSigned
    , scalarWrapperName
    , wrapperNameToScalarType
    , scalarTypeRank
    , widerScalarType
    , scalarTypeToType
    , typeToScalarType
    , isIntegerType
    , isSignedIntegerType
    , isUnsignedIntegerType
    , isFloatingType
    , isNumericType
    , integerMinimum
    , integerMaximum
    , integerFits
    , defaultIntegerScalar
    , defaultFloatingScalar
    ) where

import Visual.XSharp.AST

data ScalarFamily = CharacterFamily | BooleanFamily | SignedIntegerFamily | UnsignedIntegerFamily | FloatingFamily
    deriving (Bounded, Enum, Eq, Ord, Read, Show)

data ScalarType
    = CharacterScalar
    | BooleanScalar
    | ByteScalar
    | ShortScalar
    | LongScalar
    | IntScalar
    | LongIntScalar
    | UByteScalar
    | UShortScalar
    | ULongScalar
    | UIntScalar
    | ULongIntScalar
    | SFloatScalar
    | LFloatScalar
    | FloatScalar
    | DoubleScalar
    deriving (Bounded, Enum, Eq, Ord, Read, Show)

scalarTypes :: [ScalarType]
scalarTypes = [minBound .. maxBound]

scalarTypeName :: ScalarType -> String
scalarTypeName scalar = case scalar of
    CharacterScalar -> "char"
    BooleanScalar -> "bool"
    ByteScalar -> "byte"
    ShortScalar -> "short"
    LongScalar -> "long"
    IntScalar -> "int"
    LongIntScalar -> "longint"
    UByteScalar -> "ubyte"
    UShortScalar -> "ushort"
    ULongScalar -> "ulong"
    UIntScalar -> "uint"
    ULongIntScalar -> "ulongint"
    SFloatScalar -> "sfloat"
    LFloatScalar -> "lfloat"
    FloatScalar -> "float"
    DoubleScalar -> "double"

scalarTypeWidth :: ScalarType -> Int
scalarTypeWidth scalar = case scalar of
    CharacterScalar -> 32
    BooleanScalar -> 8
    ByteScalar -> 8
    ShortScalar -> 16
    LongScalar -> 32
    IntScalar -> 64
    LongIntScalar -> 128
    UByteScalar -> 8
    UShortScalar -> 16
    ULongScalar -> 32
    UIntScalar -> 64
    ULongIntScalar -> 128
    SFloatScalar -> 16
    LFloatScalar -> 32
    FloatScalar -> 64
    DoubleScalar -> 128

scalarTypeFamily :: ScalarType -> ScalarFamily
scalarTypeFamily scalar = case scalar of
    CharacterScalar -> CharacterFamily
    BooleanScalar -> BooleanFamily
    ByteScalar -> SignedIntegerFamily
    ShortScalar -> SignedIntegerFamily
    LongScalar -> SignedIntegerFamily
    IntScalar -> SignedIntegerFamily
    LongIntScalar -> SignedIntegerFamily
    UByteScalar -> UnsignedIntegerFamily
    UShortScalar -> UnsignedIntegerFamily
    ULongScalar -> UnsignedIntegerFamily
    UIntScalar -> UnsignedIntegerFamily
    ULongIntScalar -> UnsignedIntegerFamily
    SFloatScalar -> FloatingFamily
    LFloatScalar -> FloatingFamily
    FloatScalar -> FloatingFamily
    DoubleScalar -> FloatingFamily

scalarTypeSigned :: ScalarType -> Bool
scalarTypeSigned scalar = scalarTypeFamily scalar == SignedIntegerFamily

-- Wrapper names are canonical system types used by contextual Of(value: ...)
-- insertion. Keeping the table beside scalar widths prevents wrapper and
-- primitive catalogs from drifting as the standard library grows.
scalarWrapperName :: ScalarType -> QualifiedName
scalarWrapperName scalar = QualifiedName [Identifier "System", Identifier (wrapperLeaf scalar)]
    where
        wrapperLeaf value = case value of
            CharacterScalar -> "Character"
            BooleanScalar -> "Boolean"
            ByteScalar -> "Byte"
            ShortScalar -> "Short"
            LongScalar -> "Long"
            IntScalar -> "Integer"
            LongIntScalar -> "LongInteger"
            UByteScalar -> "UByte"
            UShortScalar -> "UShort"
            ULongScalar -> "ULong"
            UIntScalar -> "UInteger"
            ULongIntScalar -> "ULongInteger"
            SFloatScalar -> "SFloat"
            LFloatScalar -> "LFloat"
            FloatScalar -> "Float"
            DoubleScalar -> "Double"

wrapperNameToScalarType :: QualifiedName -> Maybe ScalarType
wrapperNameToScalarType name = lookup name [(scalarWrapperName scalar, scalar) | scalar <- scalarTypes]

-- Rank is meaningful only inside one family. It is not an implicit-conversion
-- rule; it exists for range analysis and explicit conversion diagnostics.
scalarTypeRank :: ScalarType -> Int
scalarTypeRank scalar = case scalar of
    CharacterScalar -> 0
    BooleanScalar -> 0
    ByteScalar -> 0
    ShortScalar -> 1
    LongScalar -> 2
    IntScalar -> 3
    LongIntScalar -> 4
    UByteScalar -> 0
    UShortScalar -> 1
    ULongScalar -> 2
    UIntScalar -> 3
    ULongIntScalar -> 4
    SFloatScalar -> 0
    LFloatScalar -> 1
    FloatScalar -> 2
    DoubleScalar -> 3

widerScalarType :: ScalarType -> ScalarType -> Maybe ScalarType
widerScalarType left right
    | scalarTypeFamily left /= scalarTypeFamily right = Nothing
    | scalarTypeRank left >= scalarTypeRank right = Just left
    | otherwise = Just right

scalarTypeToType :: ScalarType -> Type
scalarTypeToType = named . scalarTypeName
    where
        named value = NamedType (QualifiedName [Identifier value]) []

typeToScalarType :: Type -> Maybe ScalarType
typeToScalarType (NamedType (QualifiedName [Identifier name]) []) = lookup name table
    where
        table = [(scalarTypeName scalar, scalar) | scalar <- scalarTypes]
typeToScalarType _ = Nothing

isIntegerType :: Type -> Bool
isIntegerType valueType = maybe False integerScalar (typeToScalarType valueType)
    where
        integerScalar scalar = scalarTypeFamily scalar `elem` [SignedIntegerFamily, UnsignedIntegerFamily]

isSignedIntegerType :: Type -> Bool
isSignedIntegerType valueType = maybe False ((== SignedIntegerFamily) . scalarTypeFamily) (typeToScalarType valueType)

isUnsignedIntegerType :: Type -> Bool
isUnsignedIntegerType valueType = maybe False ((== UnsignedIntegerFamily) . scalarTypeFamily) (typeToScalarType valueType)

isFloatingType :: Type -> Bool
isFloatingType valueType = maybe False ((== FloatingFamily) . scalarTypeFamily) (typeToScalarType valueType)

isNumericType :: Type -> Bool
isNumericType valueType = isIntegerType valueType || isFloatingType valueType

integerMinimum :: ScalarType -> Maybe Integer
integerMinimum scalar = case scalarTypeFamily scalar of
    SignedIntegerFamily -> Just (negate (2 ^ (scalarTypeWidth scalar - 1)))
    UnsignedIntegerFamily -> Just 0
    _ -> Nothing

integerMaximum :: ScalarType -> Maybe Integer
integerMaximum scalar = case scalarTypeFamily scalar of
    SignedIntegerFamily -> Just (2 ^ (scalarTypeWidth scalar - 1) - 1)
    UnsignedIntegerFamily -> Just (2 ^ scalarTypeWidth scalar - 1)
    _ -> Nothing

integerFits :: ScalarType -> Integer -> Bool
integerFits scalar value = case (integerMinimum scalar, integerMaximum scalar) of
    (Just minimumValue, Just maximumValue) -> value >= minimumValue && value <= maximumValue
    _ -> False

defaultIntegerScalar :: ScalarType
defaultIntegerScalar = IntScalar

defaultFloatingScalar :: ScalarType
defaultFloatingScalar = FloatScalar
