-- SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
-- SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.1

{- |
Typed-AST discovery of concrete template layout demands.

Discovery is intentionally conservative. A concrete template type appearing
in an ordinary checked declaration requests layout, but never requests method
bodies. Open template declarations are definitions, not uses, so this pass
does not walk their parameter-dependent bodies. Member/call resolution will
add narrower body demands through the same planner API in a later stage.
-}
module Visual.XSharp.Template.Discovery
    ( TemplateTypeSite (..)
    , TemplateDiscoveryOrigin (..)
    , TemplateDiscoveryStatistics (..)
    , TemplateDemandDiscovery (..)
    , discoverTemplateDemands
    , renderTemplateDiscoveryOrigin
    ) where

import Data.List (intercalate)
import Visual.XSharp.AST
import Visual.XSharp.Template.Application
import Visual.XSharp.Template.Specialization

data TemplateTypeSite
    = DeclarationTypeSite
    | FunctionSignatureSite
    | ParameterTypeSite Int
    | BindingTypeSite Int
    | AssignmentTypeSite Int
    | ReturnTypeSite Int
    | ConditionTypeSite Int
    | ExpressionTypeSite Int
    | CallableTypeSite Int
    | CaptureTypeSite Int Int
    | NestedTypeArgumentSite Int
    | FunctionParameterTypeSite Int
    | FunctionResultTypeSite
    deriving (Eq, Ord, Read, Show)

data TemplateDiscoveryOrigin = TemplateDiscoveryOrigin
    { discoveryDeclaration :: ResolvedName
    , discoveryMember :: Maybe ResolvedName
    , discoverySites :: [TemplateTypeSite]
    , discoverySpan :: SourceSpan
    }
    deriving (Eq, Ord, Read, Show)

data TemplateDiscoveryStatistics = TemplateDiscoveryStatistics
    { visitedTemplateTypeNodes :: Int
    , discoveredTemplateApplications :: Int
    , ignoredOrdinaryNamedTypes :: Int
    , skippedOpenTemplateBodies :: Int
    }
    deriving (Eq, Ord, Read, Show)

data TemplateDemandDiscovery = TemplateDemandDiscovery
    { discoveredTemplateDemands :: [TemplateSpecializationDemand]
    , discoveredTemplateOrigins :: [TemplateDiscoveryOrigin]
    , templateDiscoveryStatistics :: TemplateDiscoveryStatistics
    }
    deriving (Eq, Ord, Read, Show)

data DiscoveryState = DiscoveryState
    { stateDemands :: [TemplateSpecializationDemand]
    , stateOrigins :: [TemplateDiscoveryOrigin]
    , stateVisitedTypes :: Int
    , stateIgnoredTypes :: Int
    , stateSkippedTemplates :: Int
    }

emptyState :: DiscoveryState
emptyState = DiscoveryState [] [] 0 0 0

discoverTemplateDemands :: TypedAST -> TemplateDemandDiscovery
discoverTemplateDemands typed@(TypedAST tree) =
    let catalog = buildTemplateCatalog typed
        final = foldl' (discoverTop catalog (syntaxNamespace tree)) emptyState (syntaxDeclarations tree)
        demands = reverse (stateDemands final)
        origins = reverse (stateOrigins final)
     in TemplateDemandDiscovery
            demands
            origins
            ( TemplateDiscoveryStatistics
                (stateVisitedTypes final)
                (length demands)
                (stateIgnoredTypes final)
                (stateSkippedTemplates final)
            )

discoverTop ::
    TemplateCatalog ->
    Maybe QualifiedName ->
    DiscoveryState ->
    Declaration ResolvedName Type ->
    DiscoveryState
discoverTop catalog namespace state declaration = case declaration of
    TemplateTypeDeclaration {} -> state {stateSkippedTemplates = stateSkippedTemplates state + 1}
    TypeDeclaration spanValue name annotation members ->
        let origin = TemplateDiscoveryOrigin name Nothing [DeclarationTypeSite] spanValue
            afterType = discoverType catalog namespace origin annotation state
         in foldl' (discoverMember catalog namespace name) afterType members
    FunctionDeclaration {} ->
        -- Top-level runtime functions are not part of the renewed source
        -- grammar, but keeping this traversal total makes hand-built TypedAST
        -- fixtures and future declaration categories deterministic.
        discoverFunction catalog namespace (declarationName declaration) Nothing declaration state

discoverMember ::
    TemplateCatalog ->
    Maybe QualifiedName ->
    ResolvedName ->
    DiscoveryState ->
    Declaration ResolvedName Type ->
    DiscoveryState
discoverMember catalog namespace owner state member = case member of
    FunctionDeclaration {} -> discoverFunction catalog namespace owner (Just (declarationName member)) member state
    TypeDeclaration spanValue name annotation members ->
        let origin = TemplateDiscoveryOrigin owner (Just name) [DeclarationTypeSite] spanValue
            afterType = discoverType catalog namespace origin annotation state
         in foldl' (discoverMember catalog namespace owner) afterType members
    TemplateTypeDeclaration {} -> state {stateSkippedTemplates = stateSkippedTemplates state + 1}

discoverFunction ::
    TemplateCatalog ->
    Maybe QualifiedName ->
    ResolvedName ->
    Maybe ResolvedName ->
    Declaration ResolvedName Type ->
    DiscoveryState ->
    DiscoveryState
discoverFunction catalog namespace owner member declaration state = case declaration of
    FunctionDeclaration spanValue _ annotation _ parameters body _ _ ->
        let root = TemplateDiscoveryOrigin owner member [FunctionSignatureSite] spanValue
            -- Function annotations repeat parameter types in a callable shape.
            -- Walk the result here and the declared parameters below so each
            -- source type use produces exactly one demand and one origin.
            resultType = case annotation of
                FunctionType _ result -> result
                other -> other
            afterSignature = discoverType catalog namespace root resultType state
            afterParameters =
                foldl'
                    ( \current (index, parameter) ->
                        discoverType
                            catalog
                            namespace
                            (root {discoverySites = [ParameterTypeSite index]})
                            (parameterAnnotation parameter)
                            current
                    )
                    afterSignature
                    (zip [0 ..] parameters)
         in discoverBlock catalog namespace root body afterParameters
    _ -> state

discoverBlock ::
    TemplateCatalog ->
    Maybe QualifiedName ->
    TemplateDiscoveryOrigin ->
    Block ResolvedName Type ->
    DiscoveryState ->
    DiscoveryState
discoverBlock catalog namespace root (Block statements) state =
    foldl'
        (\current (index, statement) -> discoverStatement catalog namespace root index statement current)
        state
        (zip [0 ..] statements)

discoverStatement ::
    TemplateCatalog ->
    Maybe QualifiedName ->
    TemplateDiscoveryOrigin ->
    Int ->
    Statement ResolvedName Type ->
    DiscoveryState ->
    DiscoveryState
discoverStatement catalog namespace root index statement state = case statement of
    BindingStatement spanValue _ _ _ annotation value ->
        let origin = root {discoverySites = [BindingTypeSite index], discoverySpan = spanValue}
         in discoverExpression catalog namespace origin value (discoverType catalog namespace origin annotation state)
    AssignmentStatement spanValue _ annotation value ->
        let origin = root {discoverySites = [AssignmentTypeSite index], discoverySpan = spanValue}
         in discoverExpression catalog namespace origin value (discoverType catalog namespace origin annotation state)
    ReturnStatement spanValue value ->
        let origin = root {discoverySites = [ReturnTypeSite index], discoverySpan = spanValue}
         in maybe state (\expression -> discoverExpression catalog namespace origin expression state) value
    IfStatement spanValue condition trueBlock falseBlock ->
        let origin = root {discoverySites = [ConditionTypeSite index], discoverySpan = spanValue}
            afterCondition = discoverExpression catalog namespace origin condition state
            afterTrue = discoverBlock catalog namespace origin trueBlock afterCondition
         in maybe afterTrue (\block -> discoverBlock catalog namespace origin block afterTrue) falseBlock
    ExpressionStatement spanValue expression _ ->
        discoverExpression
            catalog
            namespace
            (root {discoverySites = [ExpressionTypeSite index], discoverySpan = spanValue})
            expression
            state

discoverExpression ::
    TemplateCatalog ->
    Maybe QualifiedName ->
    TemplateDiscoveryOrigin ->
    Expression ResolvedName Type ->
    DiscoveryState ->
    DiscoveryState
discoverExpression catalog namespace origin expression state = case expression of
    NameExpression _ _ annotation -> discoverType catalog namespace origin annotation state
    LiteralExpression _ _ annotation -> discoverType catalog namespace origin annotation state
    CallExpression _ callee arguments annotation ->
        let afterType = discoverType catalog namespace origin annotation state
            afterCallee = discoverExpression catalog namespace origin callee afterType
         in foldl' (\current value -> discoverExpression catalog namespace origin value current) afterCallee arguments
    UnaryExpression _ _ value annotation ->
        discoverExpression catalog namespace origin value (discoverType catalog namespace origin annotation state)
    BinaryExpression _ _ left right annotation ->
        let afterType = discoverType catalog namespace origin annotation state
            afterLeft = discoverExpression catalog namespace origin left afterType
         in discoverExpression catalog namespace origin right afterLeft
    CallableExpression spanValue _ captures parameters body annotation ->
        let callableOrigin =
                origin
                    { discoverySites = discoverySites origin ++ [CallableTypeSite (sourceColumn (sourceStart spanValue))]
                    , discoverySpan = spanValue
                    }
            afterType = discoverType catalog namespace callableOrigin annotation state
            afterCaptures =
                foldl'
                    (discoverCapture catalog namespace callableOrigin)
                    afterType
                    (zip [0 ..] captures)
            afterParameters =
                foldl'
                    ( \current (index, parameter) ->
                        discoverType
                            catalog
                            namespace
                            (callableOrigin {discoverySites = discoverySites callableOrigin ++ [ParameterTypeSite index]})
                            (parameterAnnotation parameter)
                            current
                    )
                    afterCaptures
                    (zip [0 ..] parameters)
         in discoverCallableBody catalog namespace callableOrigin body afterParameters

discoverCapture ::
    TemplateCatalog ->
    Maybe QualifiedName ->
    TemplateDiscoveryOrigin ->
    DiscoveryState ->
    (Int, Capture ResolvedName Type) ->
    DiscoveryState
discoverCapture catalog namespace root state (index, capture) =
    let origin = root {discoverySites = discoverySites root ++ [CaptureTypeSite index 0], discoverySpan = captureSpan capture}
        afterType = discoverType catalog namespace origin (captureAnnotation capture) state
     in maybe afterType (\value -> discoverExpression catalog namespace origin value afterType) (captureInitializer capture)

discoverCallableBody ::
    TemplateCatalog ->
    Maybe QualifiedName ->
    TemplateDiscoveryOrigin ->
    CallableBody ResolvedName Type ->
    DiscoveryState ->
    DiscoveryState
discoverCallableBody catalog namespace origin body state = case body of
    CallableExpressionBody expression -> discoverExpression catalog namespace origin expression state
    CallableBlockBody block -> discoverBlock catalog namespace origin block state

discoverType ::
    TemplateCatalog ->
    Maybe QualifiedName ->
    TemplateDiscoveryOrigin ->
    Type ->
    DiscoveryState ->
    DiscoveryState
discoverType catalog namespace origin valueType state =
    let visited = state {stateVisitedTypes = stateVisitedTypes state + 1}
     in case valueType of
            NamedType name arguments ->
                let afterRoot = case resolveTemplateTarget namespace name catalog of
                        Just target
                            | isClosedType valueType ->
                                visited
                                    { stateDemands =
                                        TemplateSpecializationDemand
                                            (TemplateApplication target arguments)
                                            TemplateLayoutDemand
                                            (renderTemplateDiscoveryOrigin origin)
                                            : stateDemands visited
                                    , stateOrigins = origin : stateOrigins visited
                                    }
                        _ -> visited {stateIgnoredTypes = stateIgnoredTypes visited + 1}
                 in foldl'
                        (\current (index, argument) -> discoverArgument catalog namespace origin index argument current)
                        afterRoot
                        (zip [0 ..] arguments)
            FunctionType parameters result ->
                let afterParameters =
                        foldl'
                            ( \current (index, parameter) ->
                                discoverType
                                    catalog
                                    namespace
                                    (origin {discoverySites = discoverySites origin ++ [FunctionParameterTypeSite index]})
                                    parameter
                                    current
                            )
                            visited
                            (zip [0 ..] parameters)
                 in discoverType
                        catalog
                        namespace
                        (origin {discoverySites = discoverySites origin ++ [FunctionResultTypeSite]})
                        result
                        afterParameters
            TypeVariable _ -> visited
            ErrorType -> visited

discoverArgument ::
    TemplateCatalog ->
    Maybe QualifiedName ->
    TemplateDiscoveryOrigin ->
    Int ->
    TemplateArgument ->
    DiscoveryState ->
    DiscoveryState
discoverArgument catalog namespace origin index argument state = case argument of
    TypeTemplateArgument nested ->
        discoverType
            catalog
            namespace
            (origin {discoverySites = discoverySites origin ++ [NestedTypeArgumentSite index]})
            nested
            state
    ValueTemplateArgument _ -> state

resolveTemplateTarget :: Maybe QualifiedName -> QualifiedName -> TemplateCatalog -> Maybe QualifiedName
resolveTemplateTarget namespace name (TemplateCatalog declarations)
    | any ((== name) . templateDeclarationName) declarations = Just name
    | otherwise = case (namespace, qualifiedNameParts name) of
        (Just (QualifiedName owner), [_]) ->
            let qualified = QualifiedName (owner ++ qualifiedNameParts name)
             in if any ((== qualified) . templateDeclarationName) declarations then Just qualified else Nothing
        _ -> Nothing

isClosedType :: Type -> Bool
isClosedType valueType = case valueType of
    NamedType _ arguments -> all closedArgument arguments
    FunctionType parameters result -> all isClosedType parameters && isClosedType result
    TypeVariable _ -> False
    ErrorType -> False
    where
        closedArgument (TypeTemplateArgument nested) = isClosedType nested
        closedArgument (ValueTemplateArgument (TemplateValueParameter _)) = False
        closedArgument (ValueTemplateArgument _) = True

renderTemplateDiscoveryOrigin :: TemplateDiscoveryOrigin -> String
renderTemplateDiscoveryOrigin origin =
    sourceFile (discoverySpan origin)
        ++ ":"
        ++ show (sourceLine (sourceStart (discoverySpan origin)))
        ++ ":"
        ++ show (sourceColumn (sourceStart (discoverySpan origin)))
        ++ " in "
        ++ renderResolved (discoveryDeclaration origin)
        ++ maybe "" (("." ++) . renderResolved) (discoveryMember origin)
        ++ " ["
        ++ intercalate "/" (map renderSite (discoverySites origin))
        ++ "]"

renderResolved :: ResolvedName -> String
renderResolved = identifierText . resolvedSpelling

renderSite :: TemplateTypeSite -> String
renderSite site = case site of
    DeclarationTypeSite -> "declaration"
    FunctionSignatureSite -> "signature"
    ParameterTypeSite index -> "parameter:" ++ show index
    BindingTypeSite index -> "binding:" ++ show index
    AssignmentTypeSite index -> "assignment:" ++ show index
    ReturnTypeSite index -> "return:" ++ show index
    ConditionTypeSite index -> "condition:" ++ show index
    ExpressionTypeSite index -> "expression:" ++ show index
    CallableTypeSite index -> "callable:" ++ show index
    CaptureTypeSite index initializer -> "capture:" ++ show index ++ ":" ++ show initializer
    NestedTypeArgumentSite index -> "argument:" ++ show index
    FunctionParameterTypeSite index -> "function-parameter:" ++ show index
    FunctionResultTypeSite -> "function-result"
