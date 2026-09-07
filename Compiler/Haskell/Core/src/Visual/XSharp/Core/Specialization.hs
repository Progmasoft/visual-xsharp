-- SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
-- SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.1

{- | Deterministic specialization planning for Core monomorphization.

Parsing and type checking preserve ordered template arguments, while this
module owns the transition from a possibly-open type to a concrete cache
entry.  It deliberately contains no code generation policy: the same plan
can feed Core cloning, CorePrep scheduling, or incremental build metadata.
-}
module Visual.XSharp.Core.Specialization
    ( Specialization (..)
    , SpecializationCatalog
    , SpecializationError (..)
    , SpecializationId (..)
    , TypeBindings
    , ValueBindings
    , catalogSize
    , emptyCatalog
    , findSpecialization
    , findSpecializationByType
    , internSpecialization
    , internSpecializations
    , prepareSpecialization
    , specializationSnapshot
    ) where

import Data.List (group, sort)
import Data.Map.Strict (Map)
import Data.Map.Strict qualified as Map
import Visual.XSharp.AST
import Visual.XSharp.Core.Template

type TypeBindings = [(SymbolId, Type)]
type ValueBindings = [(SymbolId, TemplateValue)]

newtype SpecializationId = SpecializationId {specializationIdValue :: Int}
    deriving (Eq, Ord, Read, Show)

data Specialization = Specialization
    { specializationId :: SpecializationId
    , specializationIdentity :: String
    , specializationType :: Type
    }
    deriving (Eq, Ord, Read, Show)

data SpecializationCatalog = SpecializationCatalog
    { catalogNextId :: SpecializationId
    , catalogByIdentity :: Map String Specialization
    , catalogById :: Map SpecializationId Specialization
    }
    deriving (Eq, Read, Show)

data SpecializationError
    = InvalidSpecialization [TemplateIssue]
    | OpenSpecialization [SymbolId]
    | DuplicateTypeBinding [SymbolId]
    | DuplicateValueBinding [SymbolId]
    | ConflictingBindingKinds [SymbolId]
    deriving (Eq, Ord, Read, Show)

emptyCatalog :: SpecializationCatalog
emptyCatalog = SpecializationCatalog (SpecializationId 1) Map.empty Map.empty

catalogSize :: SpecializationCatalog -> Int
catalogSize = Map.size . catalogById

{- | Substitute bindings and prove that the resulting key is both structurally
valid and closed. Binding diagnostics are deterministic and reported before
structural issues, making build failures independent of map insertion order.
-}
prepareSpecialization :: TypeBindings -> ValueBindings -> Type -> Either SpecializationError Type
prepareSpecialization typeBindings valueBindings input = do
    validateBindings typeBindings valueBindings
    let concrete = substituteTemplateType typeBindings valueBindings input
        issues = validateTemplateType 128 concrete
        parameters = collectTemplateParameters concrete
    if not (null issues)
        then Left (InvalidSpecialization issues)
        else
            if not (null parameters)
                then Left (OpenSpecialization parameters)
                else Right concrete

{- | Intern one already-concrete type. The Bool distinguishes a newly planned
specialization from an existing cache hit without assigning a second id.
-}
internSpecialization ::
    Type ->
    SpecializationCatalog ->
    Either SpecializationError (Specialization, Bool, SpecializationCatalog)
internSpecialization input catalog = do
    concrete <- prepareSpecialization [] [] input
    let identity = renderTemplateIdentity concrete
    case Map.lookup identity (catalogByIdentity catalog) of
        Just existing -> Right (existing, False, catalog)
        Nothing ->
            let entry = Specialization (catalogNextId catalog) identity concrete
                next = successor (catalogNextId catalog)
                updated =
                    SpecializationCatalog
                        next
                        (Map.insert identity entry (catalogByIdentity catalog))
                        (Map.insert (specializationId entry) entry (catalogById catalog))
             in Right (entry, True, updated)

{- | Intern a worklist atomically. Because the catalog is immutable, returning
Left naturally discards entries prepared earlier in the same batch. Repeated
keys in one worklist share their first insertion-order id.
-}
internSpecializations ::
    [Type] ->
    SpecializationCatalog ->
    Either SpecializationError ([Specialization], SpecializationCatalog)
internSpecializations inputs initial = go [] initial inputs
    where
        go entries catalog [] = Right (reverse entries, catalog)
        go entries catalog (input : remaining) = do
            (entry, _, updated) <- internSpecialization input catalog
            go (entry : entries) updated remaining

findSpecialization :: SpecializationId -> SpecializationCatalog -> Maybe Specialization
findSpecialization key = Map.lookup key . catalogById

findSpecializationByType :: Type -> SpecializationCatalog -> Maybe Specialization
findSpecializationByType input catalog =
    Map.lookup (renderTemplateIdentity input) (catalogByIdentity catalog)

specializationSnapshot :: SpecializationCatalog -> [Specialization]
specializationSnapshot = Map.elems . catalogById

successor :: SpecializationId -> SpecializationId
successor (SpecializationId value) = SpecializationId (value + 1)

validateBindings :: TypeBindings -> ValueBindings -> Either SpecializationError ()
validateBindings typeBindings valueBindings
    | not (null duplicateTypes) = Left (DuplicateTypeBinding duplicateTypes)
    | not (null duplicateValues) = Left (DuplicateValueBinding duplicateValues)
    | not (null conflicts) = Left (ConflictingBindingKinds conflicts)
    | otherwise = Right ()
    where
        typeKeys = map fst typeBindings
        valueKeys = map fst valueBindings
        duplicateTypes = duplicates typeKeys
        duplicateValues = duplicates valueKeys
        conflicts = Map.keys (Map.intersection (keySet typeKeys) (keySet valueKeys))

        keySet = Map.fromList . map (,())
        duplicates values = [first | duplicate@(first : _) <- group (sort values), length duplicate > 1]
