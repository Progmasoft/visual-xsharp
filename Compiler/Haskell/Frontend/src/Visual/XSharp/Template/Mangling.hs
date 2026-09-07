-- SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
-- SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.0

{- |
Reversible-by-construction ASCII names for internal template emission.

This is not a public ABI promise. It gives Core/native handoff a deterministic,
collision-free spelling while the public ABI and linker compatibility policy
are still being designed. Every structural boundary is tagged and counted;
Unicode identifiers are encoded as fixed-width scalar values.
-}
module Visual.XSharp.Template.Mangling
    ( TemplateMangleLimits (..)
    , defaultTemplateMangleLimits
    , TemplateMangleError (..)
    , MangledTemplateType (..)
    , MangledTemplateMember (..)
    , mangleTemplateType
    , mangleTemplateMember
    , mangleTemplateMembers
    , validMangledTemplateSymbol
    , renderTemplateMangleError
    ) where

import Data.Char (isAsciiLower, isAsciiUpper, isDigit, ord)
import Numeric (showHex)
import Visual.XSharp.AST

data TemplateMangleLimits = TemplateMangleLimits
    { maximumMangledTypeDepth :: Int
    , maximumMangledSymbolLength :: Int
    , maximumMangledNameParts :: Int
    , maximumMangledArguments :: Int
    }
    deriving (Eq, Ord, Read, Show)

defaultTemplateMangleLimits :: TemplateMangleLimits
defaultTemplateMangleLimits = TemplateMangleLimits 128 65535 1024 4096

data TemplateMangleError
    = InvalidTemplateMangleLimits String
    | OpenTypeCannotBeMangled ResolvedName
    | ErrorTypeCannotBeMangled
    | EmptyQualifiedNameCannotBeMangled
    | EmptyIdentifierCannotBeMangled
    | InvalidIdentifierScalar Integer
    | InvalidCharacterTemplateValue Integer
    | MangledTypeDepthExceeded Int
    | MangledNamePartLimitExceeded Int Int
    | MangledArgumentLimitExceeded Int Int
    | MangledSymbolLengthExceeded Int Int
    | ExpectedMangleableMemberDeclaration
    deriving (Eq, Ord, Read, Show)

newtype MangledTemplateType = MangledTemplateType
    { mangledTemplateTypeText :: String
    }
    deriving (Eq, Ord, Read, Show)

data MangledTemplateMember = MangledTemplateMember
    { mangledMemberSourceSymbol :: SymbolId
    , mangledMemberSourceName :: Identifier
    , mangledMemberText :: String
    }
    deriving (Eq, Ord, Read, Show)

mangleTemplateType :: TemplateMangleLimits -> Type -> Either TemplateMangleError MangledTemplateType
mangleTemplateType limits valueType = do
    validateLimits limits
    payload <- encodeType limits 0 valueType
    let symbol = "_VXT1_T" ++ payload
    enforceLength limits symbol
    pure (MangledTemplateType symbol)

mangleTemplateMember ::
    TemplateMangleLimits ->
    Type ->
    Declaration ResolvedName Type ->
    Either TemplateMangleError MangledTemplateMember
mangleTemplateMember limits owner member = do
    MangledTemplateType ownerName <- mangleTemplateType limits owner
    case member of
        FunctionDeclaration {} -> do
            name <- encodeIdentifier (resolvedSpelling (declarationName member))
            signature <- encodeType limits 0 (declarationAnnotation member)
            let staticTag = if declarationIsStatic member then "S1" else "S0"
                accessTag = case declarationAccess member of
                    DefaultAccess -> "A0"
                    PublicAccess -> "A1"
                    PrivateAccess -> "A2"
                    InternalAccess -> "A3"
                    ProtectedAccess -> "A4"
                symbol = ownerName ++ "_M" ++ name ++ "_" ++ staticTag ++ accessTag ++ "_Y" ++ signature
            enforceLength limits symbol
            pure
                ( MangledTemplateMember
                    (resolvedSymbol (declarationName member))
                    (resolvedSpelling (declarationName member))
                    symbol
                )
        TypeDeclaration {} -> nestedTypeMember ownerName member
        TemplateTypeDeclaration {} -> Left ExpectedMangleableMemberDeclaration
    where
        nestedTypeMember ownerName declaration = do
            name <- encodeIdentifier (resolvedSpelling (declarationName declaration))
            annotation <- encodeType limits 0 (declarationAnnotation declaration)
            let symbol = ownerName ++ "_N" ++ name ++ "_Y" ++ annotation
            enforceLength limits symbol
            pure
                ( MangledTemplateMember
                    (resolvedSymbol (declarationName declaration))
                    (resolvedSpelling (declarationName declaration))
                    symbol
                )

mangleTemplateMembers ::
    TemplateMangleLimits ->
    Type ->
    Declaration ResolvedName Type ->
    Either [TemplateMangleError] [MangledTemplateMember]
mangleTemplateMembers limits owner declaration = case declaration of
    TypeDeclaration {typeMembers = members} -> collect (map (mangleTemplateMember limits owner) members)
    _ -> Left [ExpectedMangleableMemberDeclaration]

validateLimits :: TemplateMangleLimits -> Either TemplateMangleError ()
validateLimits limits
    | maximumMangledTypeDepth limits < 0 = invalid "maximum type depth cannot be negative"
    | maximumMangledSymbolLength limits <= 0 = invalid "maximum symbol length must be positive"
    | maximumMangledNameParts limits <= 0 = invalid "maximum qualified-name part count must be positive"
    | maximumMangledArguments limits < 0 = invalid "maximum argument count cannot be negative"
    | otherwise = Right ()
    where
        invalid = Left . InvalidTemplateMangleLimits

encodeType :: TemplateMangleLimits -> Int -> Type -> Either TemplateMangleError String
encodeType limits depth valueType
    | depth > maximumMangledTypeDepth limits = Left (MangledTypeDepthExceeded (maximumMangledTypeDepth limits))
    | otherwise = case valueType of
        NamedType name arguments -> do
            encodedName <- encodeQualifiedName limits name
            if length arguments > maximumMangledArguments limits
                then Left (MangledArgumentLimitExceeded (maximumMangledArguments limits) (length arguments))
                else do
                    encodedArguments <- traverse (encodeArgument limits (depth + 1)) arguments
                    pure ("N" ++ encodedName ++ "A" ++ count encodedArguments ++ concat encodedArguments)
        FunctionType parameters result -> do
            if length parameters > maximumMangledArguments limits
                then Left (MangledArgumentLimitExceeded (maximumMangledArguments limits) (length parameters))
                else do
                    encodedParameters <- traverse (encodeType limits (depth + 1)) parameters
                    encodedResult <- encodeType limits (depth + 1) result
                    pure ("F" ++ count encodedParameters ++ concatMap frame encodedParameters ++ "R" ++ frame encodedResult)
        TypeVariable name -> Left (OpenTypeCannotBeMangled name)
        ErrorType -> Left ErrorTypeCannotBeMangled

encodeArgument :: TemplateMangleLimits -> Int -> TemplateArgument -> Either TemplateMangleError String
encodeArgument limits depth argument = case argument of
    TypeTemplateArgument nested -> ("T" ++) . frame <$> encodeType limits depth nested
    ValueTemplateArgument value -> ("V" ++) . frame <$> encodeValue value

encodeValue :: TemplateValue -> Either TemplateMangleError String
encodeValue value = case value of
    IntegerTemplateValue integer ->
        pure (if integer < 0 then "IN" ++ digits (abs integer) else "IP" ++ digits integer)
    BooleanTemplateValue boolean -> pure (if boolean then "B1" else "B0")
    CharacterTemplateValue scalar
        | validScalar scalar -> pure ("C" ++ scalarHex scalar)
        | otherwise -> Left (InvalidCharacterTemplateValue scalar)
    TemplateValueParameter name -> Left (OpenTypeCannotBeMangled name)
    where
        digits integer = show integer ++ "_"

encodeQualifiedName :: TemplateMangleLimits -> QualifiedName -> Either TemplateMangleError String
encodeQualifiedName limits (QualifiedName parts)
    | null parts = Left EmptyQualifiedNameCannotBeMangled
    | length parts > maximumMangledNameParts limits =
        Left (MangledNamePartLimitExceeded (maximumMangledNameParts limits) (length parts))
    | otherwise = do
        encoded <- traverse encodeIdentifier parts
        pure (count encoded ++ concatMap frame encoded)

encodeIdentifier :: Identifier -> Either TemplateMangleError String
encodeIdentifier (Identifier value)
    | null value = Left EmptyIdentifierCannotBeMangled
    | otherwise = do
        encoded <- traverse encodeCharacter value
        pure (count value ++ concat encoded)

encodeCharacter :: Char -> Either TemplateMangleError String
encodeCharacter character
    | validScalar scalar = Right (scalarHex scalar)
    | otherwise = Left (InvalidIdentifierScalar scalar)
    where
        scalar = toInteger (ord character)

scalarHex :: Integer -> String
scalarHex scalar = replicate (6 - length encoded) '0' ++ encoded
    where
        encoded = showHex scalar ""

validScalar :: Integer -> Bool
validScalar scalar =
    scalar >= 0
        && scalar <= 0x10ffff
        && not (scalar >= 0xd800 && scalar <= 0xdfff)

frame :: String -> String
frame value = show (length value) ++ "_" ++ value

count :: [value] -> String
count values = show (length values) ++ "_"

enforceLength :: TemplateMangleLimits -> String -> Either TemplateMangleError ()
enforceLength limits symbol
    | length symbol > maximumMangledSymbolLength limits =
        Left (MangledSymbolLengthExceeded (maximumMangledSymbolLength limits) (length symbol))
    | otherwise = Right ()

validMangledTemplateSymbol :: String -> Bool
validMangledTemplateSymbol symbol =
    "_VXT1_" `prefixOf` symbol
        && all validCharacter symbol
    where
        validCharacter character =
            isAsciiLower character
                || isAsciiUpper character
                || isDigit character
                || character == '_'

renderTemplateMangleError :: TemplateMangleError -> String
renderTemplateMangleError issue = case issue of
    InvalidTemplateMangleLimits message -> "invalid template mangle limits: " ++ message
    OpenTypeCannotBeMangled name -> "open template symbol cannot be mangled: " ++ identifierText (resolvedSpelling name)
    ErrorTypeCannotBeMangled -> "ErrorType cannot be part of a mangled template symbol"
    EmptyQualifiedNameCannotBeMangled -> "an empty qualified name cannot be mangled"
    EmptyIdentifierCannotBeMangled -> "an empty identifier cannot be mangled"
    InvalidIdentifierScalar scalar -> "identifier contains invalid Unicode scalar " ++ show scalar
    InvalidCharacterTemplateValue scalar -> "character template value is invalid Unicode scalar " ++ show scalar
    MangledTypeDepthExceeded limit -> "template mangling exceeded type depth " ++ show limit
    MangledNamePartLimitExceeded limit actual -> limitMessage "qualified-name part" limit actual
    MangledArgumentLimitExceeded limit actual -> limitMessage "template argument" limit actual
    MangledSymbolLengthExceeded limit actual -> limitMessage "mangled symbol length" limit actual
    ExpectedMangleableMemberDeclaration -> "template member mangling requires a closed type or function member"
    where
        limitMessage subject limit actual =
            subject ++ " limit " ++ show limit ++ " was exceeded by " ++ show actual

prefixOf :: String -> String -> Bool
prefixOf [] _ = True
prefixOf _ [] = False
prefixOf (left : leftRest) (right : rightRest) = left == right && prefixOf leftRest rightRest

collect :: [Either problem value] -> Either [problem] [value]
collect values = case [problem | Left problem <- values] of
    [] -> Right [value | Right value <- values]
    problems -> Left problems
