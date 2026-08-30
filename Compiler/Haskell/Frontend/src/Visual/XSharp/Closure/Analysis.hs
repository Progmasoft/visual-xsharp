-- SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
-- SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.0

{- | Structural closure facts shared by diagnostics, tooling, and lowering.

This module deliberately does not choose a heap layout.  It describes the
semantic facts which survive into CorePrep: source order, ownership mode,
mutation, nesting, and whether a captured value is read by a nested closure.
-}
module Visual.XSharp.Closure.Analysis
    ( ClosureId (..)
    , CaptureUse (..)
    , ClosureSummary (..)
    , ClosureCatalog (..)
    , analyzeClosures
    , closureById
    , closuresCapturing
    , stronglyCapturedSymbols
    , nonOwningCapturedSymbols
    ) where

import Data.List (nubBy)
import Visual.XSharp.AST

newtype ClosureId = ClosureId {closureIdValue :: Int}
    deriving (Eq, Ord, Read, Show)

data CaptureUse = CaptureUse
    { captureUseName :: ResolvedName
    , captureUseType :: Type
    , captureUseMode :: CaptureMode
    , captureUseOrder :: Int
    , captureUseExplicit :: Bool
    , captureUseAlias :: Bool
    , captureUseRead :: Bool
    , captureUseWritten :: Bool
    , captureUseReadByNestedClosure :: Bool
    }
    deriving (Eq, Ord, Read, Show)

data ClosureSummary = ClosureSummary
    { closureSummaryId :: ClosureId
    , closureSummaryParent :: Maybe ClosureId
    , closureSummarySpan :: SourceSpan
    , closureSummaryType :: Type
    , closureSummaryExplicitCaptureMode :: Bool
    , closureSummaryParameters :: [(ResolvedName, Type)]
    , closureSummaryCaptures :: [CaptureUse]
    , closureSummaryChildren :: [ClosureId]
    , closureSummaryContainsReturn :: Bool
    , closureSummaryContainsCall :: Bool
    }
    deriving (Eq, Ord, Read, Show)

newtype ClosureCatalog = ClosureCatalog {closureSummaries :: [ClosureSummary]}
    deriving (Eq, Ord, Read, Show)

data WalkState = WalkState
    { walkNextId :: Int
    , walkSummaries :: [ClosureSummary]
    }

analyzeClosures :: TypedAST -> ClosureCatalog
analyzeClosures (TypedAST tree) =
    let final = walkTree (WalkState 1 []) tree
     in ClosureCatalog (walkSummaries final)

closureById :: ClosureId -> ClosureCatalog -> Maybe ClosureSummary
closureById identifier (ClosureCatalog summaries) =
    findFirst ((== identifier) . closureSummaryId) summaries

closuresCapturing :: SymbolId -> ClosureCatalog -> [ClosureSummary]
closuresCapturing symbol (ClosureCatalog summaries) =
    [ summary
    | summary <- summaries
    , any ((== symbol) . resolvedSymbol . captureUseName) (closureSummaryCaptures summary)
    ]

stronglyCapturedSymbols :: ClosureCatalog -> [SymbolId]
stronglyCapturedSymbols = capturedSymbolsByMode (== StrongCapture)

nonOwningCapturedSymbols :: ClosureCatalog -> [SymbolId]
nonOwningCapturedSymbols = capturedSymbolsByMode (/= StrongCapture)

capturedSymbolsByMode :: (CaptureMode -> Bool) -> ClosureCatalog -> [SymbolId]
capturedSymbolsByMode predicate (ClosureCatalog summaries) =
    unique
        [ resolvedSymbol (captureUseName capture)
        | summary <- summaries
        , capture <- closureSummaryCaptures summary
        , predicate (captureUseMode capture)
        ]

walkTree :: WalkState -> SyntaxTree ResolvedName Type -> WalkState
walkTree state tree = foldDeclarations (syntaxDeclarations tree) state

foldDeclarations :: [Declaration ResolvedName Type] -> WalkState -> WalkState
foldDeclarations declarations initial = foldl walkDeclaration initial declarations

walkDeclaration :: WalkState -> Declaration ResolvedName Type -> WalkState
walkDeclaration state declaration = case declaration of
    TypeDeclaration {typeMembers = members} -> foldl walkDeclaration state members
    FunctionDeclaration {declarationBody = body} -> walkBlock Nothing state body

walkBlock :: Maybe ClosureId -> WalkState -> Block ResolvedName Type -> WalkState
walkBlock parent state block = foldStatements parent (blockStatements block) state

foldStatements :: Maybe ClosureId -> [Statement ResolvedName Type] -> WalkState -> WalkState
foldStatements parent statements initial = foldl (walkStatement parent) initial statements

walkStatement :: Maybe ClosureId -> WalkState -> Statement ResolvedName Type -> WalkState
walkStatement parent state statement = case statement of
    BindingStatement _ _ _ _ _ value -> walkExpression parent state value
    AssignmentStatement _ _ _ value -> walkExpression parent state value
    ReturnStatement _ value -> maybe state (walkExpression parent state) value
    IfStatement _ condition trueBlock falseBlock ->
        let afterCondition = walkExpression parent state condition
            afterTrue = walkBlock parent afterCondition trueBlock
         in maybe afterTrue (walkBlock parent afterTrue) falseBlock
    ExpressionStatement _ value _ -> walkExpression parent state value

walkExpression :: Maybe ClosureId -> WalkState -> Expression ResolvedName Type -> WalkState
walkExpression parent state expression = case expression of
    NameExpression {} -> state
    LiteralExpression {} -> state
    CallExpression _ callee arguments _ ->
        foldl (walkExpression parent) (walkExpression parent state callee) arguments
    UnaryExpression _ _ value _ -> walkExpression parent state value
    BinaryExpression _ _ left right _ ->
        walkExpression parent (walkExpression parent state left) right
    callable@CallableExpression {} -> walkCallable parent state callable

walkCallable :: Maybe ClosureId -> WalkState -> Expression ResolvedName Type -> WalkState
walkCallable parent state (CallableExpression spanValue explicit captures parameters body valueType) =
    let identifier = ClosureId (walkNextId state)
        bodyFacts = inspectBody body
        explicitUses = zipWith (captureUse bodyFacts explicit) [0 ..] captures
        implicitUses =
            if explicit
                then []
                else inferImplicitUses parameters bodyFacts
        beforeChildren = state {walkNextId = walkNextId state + 1}
        afterChildren = walkCallableBody (Just identifier) beforeChildren body
        children =
            [ closureSummaryId candidateSummary
            | candidateSummary <- walkSummaries afterChildren
            , closureSummaryParent candidateSummary == Just identifier
            ]
        nestedReads =
            concatMap
                (map (resolvedSymbol . captureUseName) . closureSummaryCaptures)
                [ nestedSummary
                | nestedSummary <- walkSummaries afterChildren
                , closureSummaryId nestedSummary `elem` descendantsOf children (ClosureCatalog (walkSummaries afterChildren))
                ]
        markNested use =
            use {captureUseReadByNestedClosure = resolvedSymbol (captureUseName use) `elem` nestedReads}
        summary =
            ClosureSummary
                { closureSummaryId = identifier
                , closureSummaryParent = parent
                , closureSummarySpan = spanValue
                , closureSummaryType = valueType
                , closureSummaryExplicitCaptureMode = explicit
                , closureSummaryParameters =
                    [(parameterName parameter, parameterAnnotation parameter) | parameter <- parameters]
                , closureSummaryCaptures = map markNested (explicitUses ++ implicitUses)
                , closureSummaryChildren = children
                , closureSummaryContainsReturn = bodyContainsReturn body
                , closureSummaryContainsCall = bodyContainsCall body
                }
     in afterChildren {walkSummaries = walkSummaries afterChildren ++ [summary]}
walkCallable _ state _ = state

walkCallableBody :: Maybe ClosureId -> WalkState -> CallableBody ResolvedName Type -> WalkState
walkCallableBody parent state body = case body of
    CallableExpressionBody expression -> walkExpression parent state expression
    CallableBlockBody block -> walkBlock parent state block

data BodyFacts = BodyFacts
    { factReads :: [(ResolvedName, Type)]
    , factWrites :: [ResolvedName]
    , factLocals :: [ResolvedName]
    }

inspectBody :: CallableBody ResolvedName Type -> BodyFacts
inspectBody body = case body of
    CallableExpressionBody expression -> expressionFacts expression
    CallableBlockBody block -> blockFacts block

emptyFacts :: BodyFacts
emptyFacts = BodyFacts [] [] []

appendFacts :: BodyFacts -> BodyFacts -> BodyFacts
appendFacts left right =
    BodyFacts
        (factReads left ++ factReads right)
        (factWrites left ++ factWrites right)
        (factLocals left ++ factLocals right)

blockFacts :: Block ResolvedName Type -> BodyFacts
blockFacts = foldl appendFacts emptyFacts . map statementFacts . blockStatements

statementFacts :: Statement ResolvedName Type -> BodyFacts
statementFacts statement = case statement of
    BindingStatement _ _ _ name _ value ->
        (expressionFacts value) {factLocals = name : factLocals (expressionFacts value)}
    AssignmentStatement _ name _ value ->
        (expressionFacts value) {factWrites = name : factWrites (expressionFacts value)}
    ReturnStatement _ value -> maybe emptyFacts expressionFacts value
    IfStatement _ condition trueBlock falseBlock ->
        expressionFacts condition
            `appendFacts` blockFacts trueBlock
            `appendFacts` maybe emptyFacts blockFacts falseBlock
    ExpressionStatement _ value _ -> expressionFacts value

expressionFacts :: Expression ResolvedName Type -> BodyFacts
expressionFacts expression = case expression of
    NameExpression _ name valueType -> BodyFacts [(name, valueType)] [] []
    LiteralExpression {} -> emptyFacts
    CallExpression _ callee arguments _ ->
        foldl appendFacts (expressionFacts callee) (map expressionFacts arguments)
    UnaryExpression _ _ value _ -> expressionFacts value
    BinaryExpression _ _ left right _ -> expressionFacts left `appendFacts` expressionFacts right
    CallableExpression {} -> emptyFacts

captureUse :: BodyFacts -> Bool -> Int -> Capture ResolvedName Type -> CaptureUse
captureUse facts explicit order capture =
    let symbol = resolvedSymbol (captureName capture)
        isRead = any ((== symbol) . resolvedSymbol . fst) (factReads facts)
        writes = any ((== symbol) . resolvedSymbol) (factWrites facts)
     in CaptureUse
            (captureName capture)
            (captureAnnotation capture)
            (captureMode capture)
            order
            explicit
            (captureAlias capture)
            isRead
            writes
            False

captureAlias :: Capture ResolvedName Type -> Bool
captureAlias capture = case captureInitializer capture of
    Just (NameExpression _ source _) -> resolvedSymbol source /= resolvedSymbol (captureName capture)
    Just _ -> True
    Nothing -> False

inferImplicitUses :: [Parameter ResolvedName Type] -> BodyFacts -> [CaptureUse]
inferImplicitUses parameters facts =
    let localSymbols = map (resolvedSymbol . parameterName) parameters ++ map resolvedSymbol (factLocals facts)
        freeReads =
            uniqueBy
                (resolvedSymbol . fst)
                [(name, valueType) | (name, valueType) <- factReads facts, resolvedSymbol name `notElem` localSymbols]
     in [ CaptureUse
            name
            valueType
            StrongCapture
            order
            False
            False
            True
            (any ((== resolvedSymbol name) . resolvedSymbol) (factWrites facts))
            False
        | (order, (name, valueType)) <- zip [0 ..] freeReads
        ]

bodyContainsReturn :: CallableBody name annotation -> Bool
bodyContainsReturn (CallableExpressionBody _) = True
bodyContainsReturn (CallableBlockBody block) = any statementContainsReturn (blockStatements block)

statementContainsReturn :: Statement name annotation -> Bool
statementContainsReturn statement = case statement of
    ReturnStatement {} -> True
    IfStatement _ _ yes no ->
        any statementContainsReturn (blockStatements yes)
            || maybe False (any statementContainsReturn . blockStatements) no
    _ -> False

bodyContainsCall :: CallableBody name annotation -> Bool
bodyContainsCall (CallableExpressionBody expression) = expressionContainsCall expression
bodyContainsCall (CallableBlockBody block) = any statementContainsCall (blockStatements block)

statementContainsCall :: Statement name annotation -> Bool
statementContainsCall statement = case statement of
    BindingStatement _ _ _ _ _ value -> expressionContainsCall value
    AssignmentStatement _ _ _ value -> expressionContainsCall value
    ReturnStatement _ value -> maybe False expressionContainsCall value
    IfStatement _ condition yes no ->
        expressionContainsCall condition
            || any statementContainsCall (blockStatements yes)
            || maybe False (any statementContainsCall . blockStatements) no
    ExpressionStatement _ value _ -> expressionContainsCall value

expressionContainsCall :: Expression name annotation -> Bool
expressionContainsCall expression = case expression of
    CallExpression {} -> True
    UnaryExpression _ _ value _ -> expressionContainsCall value
    BinaryExpression _ _ left right _ -> expressionContainsCall left || expressionContainsCall right
    CallableExpression {} -> False
    _ -> False

descendantsOf :: [ClosureId] -> ClosureCatalog -> [ClosureId]
descendantsOf roots catalog = roots ++ concatMap children roots
    where
        children identifier = case closureById identifier catalog of
            Nothing -> []
            Just summary -> descendantsOf (closureSummaryChildren summary) catalog

findFirst :: (value -> Bool) -> [value] -> Maybe value
findFirst _ [] = Nothing
findFirst predicate (value : remaining)
    | predicate value = Just value
    | otherwise = findFirst predicate remaining

unique :: (Eq value) => [value] -> [value]
unique = nubBy (==)

uniqueBy :: (Eq key) => (value -> key) -> [value] -> [value]
uniqueBy select = nubBy (\left right -> select left == select right)
