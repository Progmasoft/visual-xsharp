-- SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
-- SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.0

{- | Data carried by the Core specialization-demand planner.

The planner intentionally records semantic locations instead of source spans.
Core can be loaded from an artifact, so a source file is not guaranteed to be
available.  A later diagnostic layer may enrich these locations with provenance
kept by the frontend without changing the deterministic planning contract.
-}
module Visual.XSharp.Core.Monomorphization.Types
    ( DemandId (..)
    , DemandLocation (..)
    , DemandOrigin (..)
    , DemandPathStep (..)
    , DemandState (..)
    , MonomorphizationDemand (..)
    , MonomorphizationError (..)
    , MonomorphizationLimits (..)
    , MonomorphizationPlan (..)
    , MonomorphizationStatistics (..)
    , defaultMonomorphizationLimits
    , emptyMonomorphizationStatistics
    , renderDemandLocation
    , renderDemandOrigin
    , renderMonomorphizationError
    ) where

import Data.List (intercalate)
import Visual.XSharp.AST
import Visual.XSharp.Core.Specialization
import Visual.XSharp.Core.Template (TemplateIssue (..))

newtype DemandId = DemandId {demandIdValue :: Int}
    deriving (Eq, Ord, Read, Show)

-- | The structural route from a function body to a type-bearing Core node.
data DemandPathStep
    = FunctionParameterPath Int
    | FunctionReturnPath
    | StatementPath Int
    | TrueBranchPath
    | FalseBranchPath
    | BindingTypePath
    | BindingValuePath
    | AssignmentValuePath
    | ReturnValuePath
    | ConditionPath
    | EvaluatedValuePath
    | CalleePath
    | ArgumentPath Int
    | PrimitiveOperandPath Int
    | ExpressionResultPath
    | ClosureCapturePath Int
    | ClosureCaptureTypePath
    | ClosureCaptureValuePath
    | ClosureParameterPath Int
    | ClosureReturnPath
    | ClosureBodyPath
    deriving (Eq, Ord, Read, Show)

data DemandLocation = DemandLocation
    { demandLocationFunction :: ResolvedName
    , demandLocationPath :: [DemandPathStep]
    }
    deriving (Eq, Ord, Read, Show)

-- | Why a concrete specialization entered the fixed-point queue.
data DemandOrigin
    = RootDemand DemandLocation
    | DependencyDemand DemandId Int
    deriving (Eq, Ord, Read, Show)

data DemandState = DemandQueued | DemandComplete
    deriving (Eq, Ord, Read, Show)

data MonomorphizationDemand = MonomorphizationDemand
    { monomorphizationDemandId :: DemandId
    , monomorphizationSpecialization :: Specialization
    , monomorphizationOrigins :: [DemandOrigin]
    , monomorphizationDependencies :: [DemandId]
    , monomorphizationDepth :: Int
    , monomorphizationState :: DemandState
    }
    deriving (Eq, Ord, Read, Show)

data MonomorphizationLimits = MonomorphizationLimits
    { maximumSpecializationDemands :: Int
    , maximumDemandDepth :: Int
    , maximumDemandOrigins :: Int
    }
    deriving (Eq, Ord, Read, Show)

defaultMonomorphizationLimits :: MonomorphizationLimits
defaultMonomorphizationLimits = MonomorphizationLimits 4096 128 16384

data MonomorphizationStatistics = MonomorphizationStatistics
    { discoveredRootOccurrences :: Int
    , uniqueSpecializationDemands :: Int
    , specializationDependencyEdges :: Int
    , specializationCacheHits :: Int
    , deepestSpecializationDemand :: Int
    }
    deriving (Eq, Ord, Read, Show)

emptyMonomorphizationStatistics :: MonomorphizationStatistics
emptyMonomorphizationStatistics = MonomorphizationStatistics 0 0 0 0 0

data MonomorphizationPlan = MonomorphizationPlan
    { monomorphizationDemands :: [MonomorphizationDemand]
    , monomorphizationStatistics :: MonomorphizationStatistics
    }
    deriving (Eq, Ord, Read, Show)

data MonomorphizationError
    = InvalidMonomorphizationLimits String
    | InvalidDemandType DemandOrigin SpecializationError
    | SpecializationLimitExceeded Int DemandOrigin Type
    | DemandDepthExceeded Int DemandOrigin Type
    | DemandOriginLimitExceeded Int DemandOrigin Type
    | InvalidCompletedDemandGraph [String]
    | InternalMissingDemand String
    deriving (Eq, Ord, Read, Show)

renderDemandLocation :: DemandLocation -> String
renderDemandLocation location =
    renderResolvedName (demandLocationFunction location)
        ++ concatMap renderPathStep (demandLocationPath location)

renderMonomorphizationError :: MonomorphizationError -> String
renderMonomorphizationError failure = case failure of
    InvalidMonomorphizationLimits message -> "invalid monomorphization limits: " ++ message
    InvalidDemandType origin reason ->
        "invalid specialization demand at "
            ++ renderDemandOrigin origin
            ++ ": "
            ++ renderSpecializationError reason
    SpecializationLimitExceeded maximumCount origin valueType ->
        "specialization demand limit "
            ++ show maximumCount
            ++ " exceeded at "
            ++ renderDemandOrigin origin
            ++ " while requesting "
            ++ show valueType
    DemandDepthExceeded maximumDepth origin valueType ->
        "specialization dependency depth "
            ++ show maximumDepth
            ++ " exceeded from "
            ++ renderDemandOrigin origin
            ++ " while requesting "
            ++ show valueType
    DemandOriginLimitExceeded maximumOrigins origin valueType ->
        "specialization origin limit "
            ++ show maximumOrigins
            ++ " exceeded at "
            ++ renderDemandOrigin origin
            ++ " while requesting "
            ++ show valueType
    InvalidCompletedDemandGraph issues ->
        "completed specialization demand graph failed validation: "
            ++ intercalate "; " issues
    InternalMissingDemand identity -> "internal specialization demand is missing from the completed plan: " ++ identity

renderResolvedName :: ResolvedName -> String
renderResolvedName name =
    identifierText (resolvedSpelling name)
        ++ "#"
        ++ show (symbolIdValue (resolvedSymbol name))

renderPathStep :: DemandPathStep -> String
renderPathStep step = case step of
    FunctionParameterPath index -> ".parameter[" ++ show index ++ "]"
    FunctionReturnPath -> ".return"
    StatementPath index -> ".statement[" ++ show index ++ "]"
    TrueBranchPath -> ".true"
    FalseBranchPath -> ".false"
    BindingTypePath -> ".binding-type"
    BindingValuePath -> ".binding-value"
    AssignmentValuePath -> ".assignment-value"
    ReturnValuePath -> ".return-value"
    ConditionPath -> ".condition"
    EvaluatedValuePath -> ".evaluated-value"
    CalleePath -> ".callee"
    ArgumentPath index -> ".argument[" ++ show index ++ "]"
    PrimitiveOperandPath index -> ".operand[" ++ show index ++ "]"
    ExpressionResultPath -> ".result"
    ClosureCapturePath index -> ".capture[" ++ show index ++ "]"
    ClosureCaptureTypePath -> ".capture-type"
    ClosureCaptureValuePath -> ".capture-value"
    ClosureParameterPath index -> ".closure-parameter[" ++ show index ++ "]"
    ClosureReturnPath -> ".closure-return"
    ClosureBodyPath -> ".closure-body"

renderDemandOrigin :: DemandOrigin -> String
renderDemandOrigin origin = case origin of
    RootDemand location -> renderDemandLocation location
    DependencyDemand (DemandId parent) argumentIndex ->
        "demand " ++ show parent ++ " argument[" ++ show argumentIndex ++ "]"

renderSpecializationError :: SpecializationError -> String
renderSpecializationError reason = case reason of
    InvalidSpecialization issues ->
        "structural validation failed ("
            ++ intercalate "; " (map templateIssueMessage issues)
            ++ ")"
    OpenSpecialization symbols -> "unbound template parameters " ++ show (map symbolIdValue symbols)
    DuplicateTypeBinding symbols -> "duplicate type bindings " ++ show (map symbolIdValue symbols)
    DuplicateValueBinding symbols -> "duplicate value bindings " ++ show (map symbolIdValue symbols)
    ConflictingBindingKinds symbols -> "conflicting type/value bindings " ++ show (map symbolIdValue symbols)
