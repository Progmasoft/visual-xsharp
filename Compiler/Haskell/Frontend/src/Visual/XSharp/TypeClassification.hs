-- SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
-- SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.1

{- |
Recursive value/reference classification for resolved Visual X# types.

The declaration catalog is explicit on purpose. A spelling alone cannot tell
the frontend whether a user-defined type is a value, reference, or CoW family,
and silently treating every unknown name as a reference would make ownership
lowering unsound. The resolver will populate this catalog when all renewed
declaration families are represented in the typed AST.
-}
module Visual.XSharp.TypeClassification
    ( StorageClass (..)
    , NominalKind (..)
    , NominalCatalog
    , CatalogError (..)
    , emptyNominalCatalog
    , registerNominal
    , lookupNominal
    , nominalCatalogSize
    , classifyNominal
    , classifyType
    , usesAarc
    , usesCopyOnWrite
    ) where

import Data.Map.Strict (Map)
import qualified Data.Map.Strict as Map
import Visual.XSharp.AST
import Visual.XSharp.BuiltinTypes (typeToScalarType)

-- | Storage behavior needed by Core ownership lowering.
data StorageClass
    = TrivialValueStorage
    | CopyOnWriteValueStorage
    | AarcReferenceStorage
    | UnresolvedStorage
    deriving (Eq, Ord, Read, Show)

-- | Semantic declaration families, deliberately independent of parser tokens.
data NominalKind
    = DataNominal
    | LiteralTypeNominal
    | ClassicEnumNominal
    | ClassNominal
    | DataClassNominal
    | EnumClassNominal
    | ObjectNominal
    | InterfaceNominal
    deriving (Bounded, Enum, Eq, Ord, Read, Show)

newtype NominalCatalog = NominalCatalog (Map QualifiedName NominalKind)
    deriving (Eq, Read, Show)

data CatalogError
    = EmptyNominalName
    | EmptyNominalNameComponent QualifiedName
    | DuplicateNominalName QualifiedName
    deriving (Eq, Ord, Read, Show)

emptyNominalCatalog :: NominalCatalog
emptyNominalCatalog = NominalCatalog Map.empty

-- | Insert exactly one case-sensitive, fully qualified declaration name.
registerNominal :: QualifiedName -> NominalKind -> NominalCatalog -> Either CatalogError NominalCatalog
registerNominal name kind (NominalCatalog declarations)
    | null parts = Left EmptyNominalName
    | any (null . identifierText) parts = Left (EmptyNominalNameComponent name)
    | Map.member name declarations = Left (DuplicateNominalName name)
    | otherwise = Right (NominalCatalog (Map.insert name kind declarations))
  where
    parts = qualifiedNameParts name

lookupNominal :: QualifiedName -> NominalCatalog -> Maybe NominalKind
lookupNominal name (NominalCatalog declarations) = Map.lookup name declarations

nominalCatalogSize :: NominalCatalog -> Int
nominalCatalogSize (NominalCatalog declarations) = Map.size declarations

classifyNominal :: NominalKind -> StorageClass
classifyNominal kind = case kind of
    DataNominal -> CopyOnWriteValueStorage
    LiteralTypeNominal -> CopyOnWriteValueStorage
    ClassicEnumNominal -> CopyOnWriteValueStorage
    ClassNominal -> AarcReferenceStorage
    DataClassNominal -> AarcReferenceStorage
    EnumClassNominal -> AarcReferenceStorage
    ObjectNominal -> AarcReferenceStorage
    InterfaceNominal -> AarcReferenceStorage

-- | Classify the complete constructed type, including nested type arguments.
classifyType :: NominalCatalog -> Type -> StorageClass
classifyType catalog valueType = case intrinsicStorage valueType of
    Just storage -> storage
    Nothing -> case valueType of
        NamedType name arguments -> case lookupNominal name catalog of
            Nothing -> UnresolvedStorage
            Just kind -> refineConstructedValue catalog (classifyNominal kind) arguments
        TypeVariable _ -> UnresolvedStorage
        ErrorType -> UnresolvedStorage
        FunctionType _ _ -> AarcReferenceStorage

usesAarc :: NominalCatalog -> Type -> Bool
usesAarc catalog valueType = classifyType catalog valueType == AarcReferenceStorage

usesCopyOnWrite :: NominalCatalog -> Type -> Bool
usesCopyOnWrite catalog valueType = classifyType catalog valueType == CopyOnWriteValueStorage

intrinsicStorage :: Type -> Maybe StorageClass
intrinsicStorage valueType
    | typeToScalarType valueType /= Nothing = Just TrivialValueStorage
    | valueType == voidType = Just TrivialValueStorage
    | valueType == unitType = Just TrivialValueStorage
    | valueType == stringType = Just AarcReferenceStorage
intrinsicStorage (FunctionType _ _) = Just AarcReferenceStorage
intrinsicStorage _ = Nothing

refineConstructedValue :: NominalCatalog -> StorageClass -> [TemplateArgument] -> StorageClass
refineConstructedValue _ AarcReferenceStorage _ = AarcReferenceStorage
refineConstructedValue _ UnresolvedStorage _ = UnresolvedStorage
refineConstructedValue _ TrivialValueStorage _ = TrivialValueStorage
refineConstructedValue catalog CopyOnWriteValueStorage arguments =
    finish (foldl' inspectArgument (False, False) arguments)
  where
    -- Value arguments affect specialization identity, but never ownership.
    inspectArgument state (ValueTemplateArgument _) = state
    inspectArgument (hasReference, hasUnresolved) (TypeTemplateArgument argumentType) =
        case classifyType catalog argumentType of
            AarcReferenceStorage -> (True, hasUnresolved)
            UnresolvedStorage -> (hasReference, True)
            _ -> (hasReference, hasUnresolved)

    -- A known reference dominates unknown siblings. This keeps A<T, String>
    -- and A<String, T> equivalent even before T has been substituted.
    finish (True, _) = AarcReferenceStorage
    finish (False, True) = UnresolvedStorage
    finish (False, False) = CopyOnWriteValueStorage
