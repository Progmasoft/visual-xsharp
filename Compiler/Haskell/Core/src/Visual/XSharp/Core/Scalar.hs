-- SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
-- SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.0

-- | Canonical scalar facts shared by Core verification and optimization.
module Visual.XSharp.Core.Scalar
    ( coreTypeSpelling
    , coreIntegerTypeNames
    , coreFloatingTypeNames
    , coreNumericTypeNames
    , isCoreIntegerType
    , isCoreFloatingType
    , isCoreNumericType
    , integerFitsCoreType
    , validCoreFloatingSpelling
    ) where

import Visual.XSharp.AST

coreTypeSpelling :: Type -> String
coreTypeSpelling (NamedType (QualifiedName [Identifier name]) []) = name
coreTypeSpelling _ = ""

coreIntegerTypeNames :: [String]
coreIntegerTypeNames =
    [ "char"
    , "byte"
    , "short"
    , "long"
    , "int"
    , "longint"
    , "ubyte"
    , "ushort"
    , "ulong"
    , "uint"
    , "ulongint"
    ]

coreFloatingTypeNames :: [String]
coreFloatingTypeNames = ["sfloat", "lfloat", "float", "double"]

coreNumericTypeNames :: [String]
coreNumericTypeNames = coreIntegerTypeNames ++ coreFloatingTypeNames

isCoreIntegerType :: Type -> Bool
isCoreIntegerType valueType = coreTypeSpelling valueType `elem` coreIntegerTypeNames

isCoreFloatingType :: Type -> Bool
isCoreFloatingType valueType = coreTypeSpelling valueType `elem` coreFloatingTypeNames

isCoreNumericType :: Type -> Bool
isCoreNumericType valueType = coreTypeSpelling valueType `elem` coreNumericTypeNames

integerFitsCoreType :: Type -> Integer -> Bool
integerFitsCoreType valueType value = case lookup (coreTypeSpelling valueType) integerRanges of
    Just (minimumValue, maximumValue) -> value >= minimumValue && value <= maximumValue
    Nothing -> False
    where
        signed :: Int -> (Integer, Integer)
        signed width = (negate (2 ^ (width - 1)), 2 ^ (width - 1) - 1)
        unsigned :: Int -> (Integer, Integer)
        unsigned width = (0, 2 ^ width - 1)
        integerRanges =
            [ ("char", unsigned 32)
            , ("byte", signed 8)
            , ("short", signed 16)
            , ("long", signed 32)
            , ("int", signed 64)
            , ("longint", signed 128)
            , ("ubyte", unsigned 8)
            , ("ushort", unsigned 16)
            , ("ulong", unsigned 32)
            , ("uint", unsigned 64)
            , ("ulongint", unsigned 128)
            ]

validCoreFloatingSpelling :: String -> Bool
validCoreFloatingSpelling spelling
    | spelling `elem` ["nan", "+nan", "-nan", "inf", "+inf", "-inf"] = True
    | otherwise = case dropSign spelling of
        [] -> False
        unsignedSpelling ->
            let (mantissa, exponentPart) = break (`elem` "eE") unsignedSpelling
             in validMantissa mantissa && validExponent exponentPart
    where
        dropSign ('+' : remaining) = remaining
        dropSign ('-' : remaining) = remaining
        dropSign value = value
        validMantissa value = case break (== '.') value of
            (whole, []) -> digits whole
            (whole, _ : fraction) ->
                (not (null whole) || not (null fraction))
                    && digitsOrEmpty whole
                    && digitsOrEmpty fraction
        validExponent [] = True
        validExponent (_ : remaining) = digits (dropExponentSign remaining)
        dropExponentSign ('+' : remaining) = remaining
        dropExponentSign ('-' : remaining) = remaining
        dropExponentSign value = value
        -- Core artifacts are a stable, language-neutral boundary. Accepting
        -- Unicode decimal categories here would make spellings depend on the
        -- host implementation of character classification even though source
        -- numeric tokens are deliberately ASCII.
        asciiDigit character = character >= '0' && character <= '9'
        digits value = not (null value) && all asciiDigit value
        digitsOrEmpty value = null value || all asciiDigit value
