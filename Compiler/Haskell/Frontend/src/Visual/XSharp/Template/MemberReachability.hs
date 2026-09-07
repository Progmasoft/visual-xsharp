-- SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
-- SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.1

{- |
Semantic reachability for members of one template declaration.

Template specialization is lazy at member granularity.  A request for one
member must nevertheless retain every same-declaration member reached by a
resolved call from its body.  The call graph is keyed by 'SymbolId', never by
spelling: overloads can share a source name while remaining distinct semantic
definitions.

Calls whose target is not a direct member of the declaration are deliberately
classified as external.  This module does not guess inheritance, extension,
dynamic dispatch, or another declaration's ownership from a name.  Those
decisions belong to name and call resolution before specialization.
-}
module Visual.XSharp.Template.MemberReachability
    ( TemplateMemberCall (..)
    , TemplateMemberNode (..)
    , TemplateMemberGraph (..)
    , TemplateMemberReachabilityStatistics (..)
    , TemplateMemberReachability (..)
    , TemplateMemberReachabilityError (..)
    , TemplateMemberReachabilityIssue (..)
    , buildTemplateMemberGraph
    , selectReachableMembersByName
    , selectReachableMembersBySymbol
    , verifyTemplateMemberReachability
    , renderTemplateMemberReachabilityError
    , renderTemplateMemberReachabilityIssue
    ) where

import Data.List (intercalate)
import Data.Map.Strict (Map)
import Data.Map.Strict qualified as Map
import Data.Set (Set)
import Data.Set qualified as Set
import Visual.XSharp.AST

{- | One direct call expression.  Repeated calls remain distinct evidence even
when they later collapse to one graph edge.
-}
data TemplateMemberCall = TemplateMemberCall
    { templateCallOwner :: SymbolId
    , templateCallTarget :: SymbolId
    , templateCallSpan :: SourceSpan
    }
    deriving (Eq, Ord, Read, Show)

{- | A node retains source order and declaration identity.  Source order is
used only to make output deterministic; it never chooses an overload.
-}
data TemplateMemberNode = TemplateMemberNode
    { templateMemberIndex :: Int
    , templateMemberSymbol :: SymbolId
    , templateMemberName :: Identifier
    , templateMemberCallable :: Bool
    , templateMemberCalls :: [TemplateMemberCall]
    }
    deriving (Eq, Ord, Read, Show)

data TemplateMemberGraph = TemplateMemberGraph
    { templateGraphNodes :: [TemplateMemberNode]
    , templateGraphInternalCalls :: [TemplateMemberCall]
    , templateGraphExternalCalls :: [TemplateMemberCall]
    }
    deriving (Eq, Ord, Read, Show)

data TemplateMemberReachabilityStatistics = TemplateMemberReachabilityStatistics
    { totalTemplateMembers :: Int
    , callableTemplateMembers :: Int
    , requestedTemplateMemberRoots :: Int
    , reachableTemplateMembers :: Int
    , observedTemplateMemberCalls :: Int
    , internalTemplateMemberCalls :: Int
    , externalTemplateMemberCalls :: Int
    , uniqueTemplateMemberEdges :: Int
    }
    deriving (Eq, Ord, Read, Show)

data TemplateMemberReachability = TemplateMemberReachability
    { memberReachabilityRoots :: [SymbolId]
    , memberReachabilitySymbols :: [SymbolId]
    , memberReachabilityMembers :: [Declaration ResolvedName Type]
    , memberReachabilityEdges :: [(SymbolId, SymbolId)]
    , memberReachabilityStatistics :: TemplateMemberReachabilityStatistics
    }
    deriving (Eq, Ord, Read, Show)

data TemplateMemberReachabilityError
    = InvalidTemplateMemberSymbol Identifier SymbolId
    | DuplicateTemplateMemberSymbol SymbolId [Identifier]
    | MissingTemplateMemberName Identifier
    | MissingTemplateMemberSymbol SymbolId
    deriving (Eq, Ord, Read, Show)

data TemplateMemberReachabilityIssue
    = ReachabilityRootIsNotMember SymbolId
    | ReachabilitySelectionIsNotMember SymbolId
    | DuplicateReachabilityRoot SymbolId
    | DuplicateReachabilitySelection SymbolId
    | ReachabilityRootNotSelected SymbolId
    | ReachabilityDependencyNotSelected SymbolId SymbolId
    | UnexpectedReachabilityEdge SymbolId SymbolId
    | MissingReachabilityEdge SymbolId SymbolId
    | ReachabilityMembersDiffer [SymbolId] [SymbolId]
    | IncorrectReachabilityStatistics TemplateMemberReachabilityStatistics TemplateMemberReachabilityStatistics
    deriving (Eq, Ord, Read, Show)

{- | Construct a declaration-local graph after semantic resolution.  Invalid
or duplicated symbols are rejected here even though the general template
verifier normally diagnoses them earlier; this keeps the API safe for
compiler tests and independently constructed TypedAST values.
-}
buildTemplateMemberGraph ::
    [Declaration ResolvedName Type] ->
    Either [TemplateMemberReachabilityError] TemplateMemberGraph
buildTemplateMemberGraph members = case validationProblems of
    [] -> Right graph
    problems -> Left problems
    where
        nodes = zipWith memberNode [0 ..] members
        memberSymbols = map templateMemberSymbol nodes
        memberSet = Set.fromList memberSymbols
        calls = concatMap templateMemberCalls nodes
        graph =
            TemplateMemberGraph
                nodes
                [call | call <- calls, templateCallTarget call `Set.member` memberSet]
                [call | call <- calls, templateCallTarget call `Set.notMember` memberSet]
        invalid =
            [ InvalidTemplateMemberSymbol (templateMemberName node) (templateMemberSymbol node)
            | node <- nodes
            , symbolIdValue (templateMemberSymbol node) <= 0
            ]
        duplicated =
            [ DuplicateTemplateMemberSymbol symbol [templateMemberName node | node <- nodes, templateMemberSymbol node == symbol]
            | symbol <- duplicates memberSymbols
            ]
        validationProblems = invalid ++ duplicated

selectReachableMembersByName ::
    [Identifier] ->
    [Declaration ResolvedName Type] ->
    Either [TemplateMemberReachabilityError] TemplateMemberReachability
selectReachableMembersByName requested members = do
    graph <- buildTemplateMemberGraph members
    let names = unique requested
        nodes = templateGraphNodes graph
        missing = [name | name <- names, name `notElem` map templateMemberName nodes]
        roots =
            [ templateMemberSymbol node
            | node <- nodes
            , templateMemberName node `elem` names
            ]
    if null missing
        then Right (selectFromGraph roots members graph)
        else Left (map MissingTemplateMemberName missing)

selectReachableMembersBySymbol ::
    [SymbolId] ->
    [Declaration ResolvedName Type] ->
    Either [TemplateMemberReachabilityError] TemplateMemberReachability
selectReachableMembersBySymbol requested members = do
    graph <- buildTemplateMemberGraph members
    let roots = unique requested
        available = map templateMemberSymbol (templateGraphNodes graph)
        missing = [symbol | symbol <- roots, symbol `notElem` available]
    if null missing
        then Right (selectFromGraph roots members graph)
        else Left (map MissingTemplateMemberSymbol missing)

selectFromGraph ::
    [SymbolId] ->
    [Declaration ResolvedName Type] ->
    TemplateMemberGraph ->
    TemplateMemberReachability
selectFromGraph roots members graph =
    TemplateMemberReachability
        orderedRoots
        selectedSymbols
        [member | member <- members, declarationSymbol member `Set.member` selectedSet]
        selectedEdges
        statistics
    where
        nodes = templateGraphNodes graph
        sourceOrder = map templateMemberSymbol nodes
        orderedRoots = [symbol | symbol <- sourceOrder, symbol `elem` roots]
        adjacency = adjacencyTable (templateGraphInternalCalls graph)
        selectedSet = closeReachability adjacency Set.empty orderedRoots
        selectedSymbols = [symbol | symbol <- sourceOrder, symbol `Set.member` selectedSet]
        selectedEdges =
            unique
                [ (templateCallOwner call, templateCallTarget call)
                | call <- templateGraphInternalCalls graph
                , templateCallOwner call `Set.member` selectedSet
                ]
        calls = templateGraphInternalCalls graph ++ templateGraphExternalCalls graph
        statistics =
            TemplateMemberReachabilityStatistics
                { totalTemplateMembers = length nodes
                , callableTemplateMembers = length [() | node <- nodes, templateMemberCallable node]
                , requestedTemplateMemberRoots = length orderedRoots
                , reachableTemplateMembers = length selectedSymbols
                , observedTemplateMemberCalls = length calls
                , internalTemplateMemberCalls = length (templateGraphInternalCalls graph)
                , externalTemplateMemberCalls = length (templateGraphExternalCalls graph)
                , uniqueTemplateMemberEdges = length (unique (map callEdge (templateGraphInternalCalls graph)))
                }

adjacencyTable :: [TemplateMemberCall] -> Map SymbolId [SymbolId]
adjacencyTable = foldl' insert Map.empty
    where
        insert table call =
            Map.insertWith
                (\new old -> unique (old ++ new))
                (templateCallOwner call)
                [templateCallTarget call]
                table

closeReachability :: Map SymbolId [SymbolId] -> Set SymbolId -> [SymbolId] -> Set SymbolId
closeReachability _ visited [] = visited
closeReachability adjacency visited (symbol : pending)
    | symbol `Set.member` visited = closeReachability adjacency visited pending
    | otherwise =
        let dependencies = Map.findWithDefault [] symbol adjacency
         in closeReachability adjacency (Set.insert symbol visited) (pending ++ dependencies)

{- | Recompute graph facts from declarations and compare them with a retained
result.  The specialization-plan verifier uses the selected declaration for
broader invariants; this verifier protects the reachability object itself.
-}
verifyTemplateMemberReachability ::
    [Declaration ResolvedName Type] ->
    TemplateMemberReachability ->
    Either [TemplateMemberReachabilityError] [TemplateMemberReachabilityIssue]
verifyTemplateMemberReachability members reachability = do
    graph <- buildTemplateMemberGraph members
    let available = map templateMemberSymbol (templateGraphNodes graph)
        roots = memberReachabilityRoots reachability
        selected = memberReachabilitySymbols reachability
        actualMembers = map declarationSymbol (memberReachabilityMembers reachability)
        expected = selectFromGraph roots members graph
        expectedEdges = memberReachabilityEdges expected
        actualEdges = memberReachabilityEdges reachability
        rootProblems =
            [ReachabilityRootIsNotMember symbol | symbol <- roots, symbol `notElem` available]
                ++ [DuplicateReachabilityRoot symbol | symbol <- duplicates roots]
                ++ [ReachabilityRootNotSelected symbol | symbol <- roots, symbol `notElem` selected]
        selectionProblems =
            [ReachabilitySelectionIsNotMember symbol | symbol <- selected, symbol `notElem` available]
                ++ [DuplicateReachabilitySelection symbol | symbol <- duplicates selected]
        closureProblems =
            [ ReachabilityDependencyNotSelected owner target
            | (owner, target) <- expectedEdges
            , owner `elem` selected
            , target `notElem` selected
            ]
        edgeProblems =
            [UnexpectedReachabilityEdge owner target | (owner, target) <- actualEdges, (owner, target) `notElem` expectedEdges]
                ++ [MissingReachabilityEdge owner target | (owner, target) <- expectedEdges, (owner, target) `notElem` actualEdges]
        memberProblems =
            [ReachabilityMembersDiffer selected actualMembers | selected /= actualMembers]
        statisticProblems =
            [ IncorrectReachabilityStatistics
                (memberReachabilityStatistics expected)
                (memberReachabilityStatistics reachability)
            | memberReachabilityStatistics expected /= memberReachabilityStatistics reachability
            ]
    pure (rootProblems ++ selectionProblems ++ closureProblems ++ edgeProblems ++ memberProblems ++ statisticProblems)

memberNode :: Int -> Declaration ResolvedName Type -> TemplateMemberNode
memberNode index member =
    TemplateMemberNode
        index
        (declarationSymbol member)
        (resolvedSpelling (declarationName member))
        (isFunction member)
        (memberCalls (declarationSymbol member) member)

isFunction :: Declaration name annotation -> Bool
isFunction FunctionDeclaration {} = True
isFunction _ = False

memberCalls :: SymbolId -> Declaration ResolvedName Type -> [TemplateMemberCall]
memberCalls owner declaration = case declaration of
    FunctionDeclaration {declarationBody = body} -> blockCalls owner body
    -- Nested types have their own member ownership domain.  Their bodies must
    -- not accidentally pull siblings into the enclosing template closure.
    TypeDeclaration {} -> []
    TemplateTypeDeclaration {} -> []

blockCalls :: SymbolId -> Block ResolvedName Type -> [TemplateMemberCall]
blockCalls owner (Block statements) = concatMap (statementCalls owner) statements

statementCalls :: SymbolId -> Statement ResolvedName Type -> [TemplateMemberCall]
statementCalls owner statement = case statement of
    BindingStatement _ _ _ _ _ value -> expressionCalls owner value
    AssignmentStatement _ _ _ value -> expressionCalls owner value
    ReturnStatement _ value -> maybe [] (expressionCalls owner) value
    IfStatement _ condition trueBlock falseBlock ->
        expressionCalls owner condition
            ++ blockCalls owner trueBlock
            ++ maybe [] (blockCalls owner) falseBlock
    ExpressionStatement _ value _ -> expressionCalls owner value

expressionCalls :: SymbolId -> Expression ResolvedName Type -> [TemplateMemberCall]
expressionCalls owner expression = case expression of
    NameExpression {} -> []
    LiteralExpression {} -> []
    CallExpression spanValue callee arguments _ ->
        directCall spanValue callee
            ++ expressionCalls owner callee
            ++ concatMap (expressionCalls owner) arguments
    UnaryExpression _ _ value _ -> expressionCalls owner value
    BinaryExpression _ _ left right _ -> expressionCalls owner left ++ expressionCalls owner right
    CallableExpression _ _ captures _ body _ ->
        concatMap (maybe [] (expressionCalls owner) . captureInitializer) captures
            ++ callableBodyCalls owner body
    where
        directCall spanValue callee = case callee of
            NameExpression _ target _ -> [TemplateMemberCall owner (resolvedSymbol target) spanValue]
            _ -> []

callableBodyCalls :: SymbolId -> CallableBody ResolvedName Type -> [TemplateMemberCall]
callableBodyCalls owner body = case body of
    CallableExpressionBody value -> expressionCalls owner value
    CallableBlockBody block -> blockCalls owner block

callEdge :: TemplateMemberCall -> (SymbolId, SymbolId)
callEdge call = (templateCallOwner call, templateCallTarget call)

declarationSymbol :: Declaration ResolvedName annotation -> SymbolId
declarationSymbol = resolvedSymbol . declarationName

renderTemplateMemberReachabilityError :: TemplateMemberReachabilityError -> String
renderTemplateMemberReachabilityError problem = case problem of
    InvalidTemplateMemberSymbol name symbol ->
        "template member " ++ identifierText name ++ " has invalid " ++ renderSymbol symbol
    DuplicateTemplateMemberSymbol symbol names ->
        renderSymbol symbol
            ++ " is shared by template members "
            ++ intercalate ", " (map identifierText names)
    MissingTemplateMemberName name -> "template declaration has no member named " ++ identifierText name
    MissingTemplateMemberSymbol symbol -> "template declaration has no member with " ++ renderSymbol symbol

renderTemplateMemberReachabilityIssue :: TemplateMemberReachabilityIssue -> String
renderTemplateMemberReachabilityIssue problem = case problem of
    ReachabilityRootIsNotMember symbol -> "reachability root is not a declaration member: " ++ renderSymbol symbol
    ReachabilitySelectionIsNotMember symbol -> "selected reachability symbol is not a declaration member: " ++ renderSymbol symbol
    DuplicateReachabilityRoot symbol -> "reachability root occurs more than once: " ++ renderSymbol symbol
    DuplicateReachabilitySelection symbol -> "selected reachability symbol occurs more than once: " ++ renderSymbol symbol
    ReachabilityRootNotSelected symbol -> "reachability root is absent from its selection: " ++ renderSymbol symbol
    ReachabilityDependencyNotSelected owner target ->
        "selected " ++ renderSymbol owner ++ " calls unselected " ++ renderSymbol target
    UnexpectedReachabilityEdge owner target ->
        "reachability contains an unobserved edge " ++ renderEdge owner target
    MissingReachabilityEdge owner target ->
        "reachability omits observed edge " ++ renderEdge owner target
    ReachabilityMembersDiffer expected actual ->
        "reachability member declarations differ: expected "
            ++ renderSymbols expected
            ++ ", found "
            ++ renderSymbols actual
    IncorrectReachabilityStatistics expected actual ->
        "reachability statistics differ: expected " ++ show expected ++ ", found " ++ show actual

renderSymbol :: SymbolId -> String
renderSymbol (SymbolId value) = "SymbolId " ++ show value

renderEdge :: SymbolId -> SymbolId -> String
renderEdge owner target = renderSymbol owner ++ " -> " ++ renderSymbol target

renderSymbols :: [SymbolId] -> String
renderSymbols values = "[" ++ intercalate ", " (map renderSymbol values) ++ "]"

duplicates :: (Ord value) => [value] -> [value]
duplicates values = Set.toList (go Set.empty Set.empty values)
    where
        go _ repeated [] = repeated
        go seen repeated (value : remaining)
            | value `Set.member` seen = go seen (Set.insert value repeated) remaining
            | otherwise = go (Set.insert value seen) repeated remaining

unique :: (Ord value) => [value] -> [value]
unique = go Set.empty
    where
        go _ [] = []
        go seen (value : remaining)
            | value `Set.member` seen = go seen remaining
            | otherwise = value : go (Set.insert value seen) remaining
