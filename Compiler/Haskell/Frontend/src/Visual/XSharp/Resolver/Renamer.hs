-- SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
-- SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.1

{- | Assign stable symbol identities to declarations and references, detecting
duplicate declarations while preserving overload-family semantics.
-}
module Visual.XSharp.Resolver.Renamer (Renamer (..), defaultRenamer, runRenamer) where

import Visual.XSharp.AST
import Visual.XSharp.Diagnostic

-- | Pluggable pass that assigns declaration and local-binding identities.
newtype Renamer = Renamer
    { renameParsedAST :: ParsedAST -> Either [Diagnostic] RenamedAST
    -- ^ Rename a parsed tree or report declaration and binding errors.
    }

-- | Execute a supplied renaming implementation.
runRenamer :: Renamer -> ParsedAST -> Either [Diagnostic] RenamedAST
runRenamer = renameParsedAST

-- | Default renamer with deterministic positive symbol IDs.
defaultRenamer :: Renamer
defaultRenamer = Renamer renameTree

type Environment = [(Identifier, RenamedName)]

renameTree :: ParsedAST -> Either [Diagnostic] RenamedAST
renameTree (ParsedAST (SyntaxTree namespace declarations)) =
    let (globals, next, duplicateProblems) =
            declareMany
                RenamerStage
                "VXR0001"
                1
                []
                [(declarationName declaration, declarationSpan declaration) | declaration <- declarations]
        (renamed, _, problems) = renameDeclarations globals next declarations
        allProblems = duplicateProblems ++ problems
     in if null allProblems then Right (RenamedAST (SyntaxTree namespace renamed)) else Left allProblems

declareMany ::
    DiagnosticStage -> String -> Int -> Environment -> [(Identifier, SourceSpan)] -> (Environment, Int, [Diagnostic])
declareMany _ _ next environment [] = (environment, next, [])
declareMany stage code next environment ((name, spanValue) : remaining) =
    let duplicate = case lookup name environment of
            Just _ -> [Diagnostic stage Error code (Just spanValue) ("duplicate declaration " ++ identifierText name)]
            Nothing -> []
        renamed = RenamedName name next
        (final, after, problems) = declareMany stage code (next + 1) ((name, renamed) : environment) remaining
     in (final, after, duplicate ++ problems)

-- Methods with one spelling are a single overload family. Other declarations
-- still occupy that shared member namespace, so a method cannot coexist with
-- a field-like or nested declaration of the same name. Each overload receives
-- its own SymbolId; the type checker later validates signature uniqueness.
declareMembers :: Int -> [Declaration Identifier ()] -> (Environment, [RenamedName], Int, [Diagnostic])
declareMembers next declarations = go [] [] next declarations
    where
        go environment _ current [] = (environment, [], current, [])
        go environment seen current (declaration : remaining) =
            let name = declarationName declaration
                isMethod = case declaration of FunctionDeclaration {} -> True; _ -> False
                collision = case lookup name seen of
                    Nothing -> []
                    Just previousWasMethod
                        | previousWasMethod && isMethod -> []
                        | otherwise ->
                            [ Diagnostic
                                RenamerStage
                                Error
                                "VXR0004"
                                (Just (declarationSpan declaration))
                                ("duplicate declaration " ++ identifierText name ++ " in one type")
                            ]
                renamed = RenamedName name current
                (finalEnvironment, laterNames, finalNext, laterProblems) =
                    go ((name, renamed) : environment) ((name, isMethod) : seen) (current + 1) remaining
             in (finalEnvironment, renamed : laterNames, finalNext, collision ++ laterProblems)

renameDeclarations ::
    Environment -> Int -> [Declaration Identifier ()] -> ([Declaration RenamedName ()], Int, [Diagnostic])
renameDeclarations globals next declarations =
    renameDeclarationsWithBindings
        globals
        next
        declarations
        (map (\declaration -> valueOrMissing (declarationName declaration) globals) declarations)

-- Declaration nodes need their own binding, not a name-only lookup. In an
-- overload family every source spelling is equal, while each declared method
-- must retain the SymbolId allocated for its exact source position.
renameDeclarationsWithBindings ::
    Environment ->
    Int ->
    [Declaration Identifier ()] ->
    [RenamedName] ->
    ([Declaration RenamedName ()], Int, [Diagnostic])
renameDeclarationsWithBindings _ next [] [] = ([], next, [])
renameDeclarationsWithBindings globals next (declaration : remaining) (assignedName : assignedRemaining) =
    case declaration of
        TypeDeclaration spanValue _ _ members ->
            let name = assignedName
                (declaredMembers, memberNames, afterMembers, duplicateProblems) = declareMembers next members
                (renamedMembers, afterBody, memberProblems) =
                    renameDeclarationsWithBindings (declaredMembers ++ globals) afterMembers members memberNames
                renamed = TypeDeclaration spanValue name () renamedMembers
                (rest, final, restProblems) = renameDeclarationsWithBindings globals afterBody remaining assignedRemaining
             in (renamed : rest, final, duplicateProblems ++ memberProblems ++ restProblems)
        TemplateTypeDeclaration spanValue _ _ sourceTemplateParameters members ->
            let name = assignedName
                (templateParameters, templateEnvironment, afterTemplateParameters, templateProblems) =
                    renameTemplateParameters globals next sourceTemplateParameters
                (declaredMembers, memberNames, afterMembers, duplicateProblems) = declareMembers afterTemplateParameters members
                memberEnvironment = declaredMembers ++ templateEnvironment
                (renamedMembers, afterBody, memberProblems) =
                    renameDeclarationsWithBindings memberEnvironment afterMembers members memberNames
                renamed = TemplateTypeDeclaration spanValue name () templateParameters renamedMembers
                (rest, final, restProblems) = renameDeclarationsWithBindings globals afterBody remaining assignedRemaining
             in ( renamed : rest
                , final
                , templateProblems ++ duplicateProblems ++ memberProblems ++ restProblems
                )
        -- The members of an enum are named by spelling under their enum
        -- and take no symbols of their own.
        EnumDeclaration spanValue _ _ underlying cases ->
            let (rest, final, restProblems) = renameDeclarationsWithBindings globals next remaining assignedRemaining
             in (EnumDeclaration spanValue assignedName () underlying cases : rest, final, restProblems)
        FunctionDeclaration spanValue _ _ returnSyntax sourceParameters sourceBody isStatic access ->
            let name = assignedName
                (parameters, parameterEnvironment, afterParameters, parameterProblems) = renameParameters globals next sourceParameters
                (body, afterBody, bodyProblems) = renameBlock parameterEnvironment afterParameters sourceBody
                renamed = FunctionDeclaration spanValue name () returnSyntax parameters body isStatic access
                (rest, final, restProblems) = renameDeclarationsWithBindings globals afterBody remaining assignedRemaining
             in (renamed : rest, final, parameterProblems ++ bodyProblems ++ restProblems)
renameDeclarationsWithBindings _ next declarations _ =
    ( []
    , next
    ,
        [ Diagnostic
            RenamerStage
            Error
            "VXR0007"
            (case declarations of declaration : _ -> Just (declarationSpan declaration); [] -> Nothing)
            "declaration binding sequence does not match its syntax sequence"
        ]
    )

-- All template parameters enter scope together.  This permits the documented
-- default `T = U, U = int` while keeping source-order SymbolIds deterministic.
-- Defaults remain syntax trees; their spelling is resolved by the type checker
-- against the semantic parameter catalog created here.
renameTemplateParameters ::
    Environment ->
    Int ->
    [TemplateParameter Identifier ()] ->
    ([TemplateParameter RenamedName ()], Environment, Int, [Diagnostic])
renameTemplateParameters outer next parameters =
    let declarations = [(templateParameterName parameter, templateParameterSpan parameter) | parameter <- parameters]
        (environment, after, problems) = declareMany RenamerStage "VXR0006" next outer declarations
        renamed =
            [ TemplateParameter
                (templateParameterSpan parameter)
                (valueOrMissing (templateParameterName parameter) environment)
                ()
                (templateParameterKind parameter)
                (templateParameterIsPack parameter)
                (templateParameterDefault parameter)
            | parameter <- parameters
            ]
     in (renamed, environment, after, problems)

renameParameters ::
    Environment -> Int -> [Parameter Identifier ()] -> ([Parameter RenamedName ()], Environment, Int, [Diagnostic])
renameParameters environment next parameters = go environment next parameters [] []
    where
        go env current [] output problems = (reverse output, env, current, reverse problems)
        go env current (Parameter spanValue name _ syntax : rest) output problems =
            let duplicate =
                    if any ((== name) . fst) (take (length output) env)
                        then Diagnostic RenamerStage Error "VXR0002" (Just spanValue) ("duplicate parameter " ++ identifierText name) : problems
                        else problems
                renamed = RenamedName name current
             in go ((name, renamed) : env) (current + 1) rest (Parameter spanValue renamed () syntax : output) duplicate

renameBlock :: Environment -> Int -> Block Identifier () -> (Block RenamedName (), Int, [Diagnostic])
renameBlock environment next (Block statements) = let (values, _, final, problems) = go environment next statements in (Block values, final, problems)
    where
        go env current [] = ([], env, current, [])
        go env current (statement : rest) =
            let (renamed, nextEnv, after, firstProblems) = renameStatement env current statement
                (remaining, finalEnv, final, restProblems) = go nextEnv after rest
             in (renamed : remaining, finalEnv, final, firstProblems ++ restProblems)

renameStatement ::
    Environment -> Int -> Statement Identifier () -> (Statement RenamedName (), Environment, Int, [Diagnostic])
renameStatement environment next statement = case statement of
    BindingStatement spanValue kind syntax name _ value ->
        let (renamedValue, afterValue, problems) = renameExpression environment next value
            duplicate = any ((== name) . fst) environment
            renamed = RenamedName name afterValue
            duplicateProblems =
                if duplicate
                    then [Diagnostic RenamerStage Error "VXR0003" (Just spanValue) ("duplicate local " ++ identifierText name)]
                    else []
         in ( BindingStatement spanValue kind syntax renamed () renamedValue
            , (name, renamed) : environment
            , afterValue + 1
            , problems ++ duplicateProblems
            )
    AssignmentStatement spanValue name _ value ->
        let (renamedValue, after, problems) = renameExpression environment next value
         in (AssignmentStatement spanValue (valueOrMissing name environment) () renamedValue, environment, after, problems)
    ReturnStatement spanValue value ->
        let (renamedValue, after, problems) = renameOptional environment next value
         in (ReturnStatement spanValue renamedValue, environment, after, problems)
    IfStatement spanValue condition trueBlock falseBlock ->
        let (renamedCondition, afterCondition, conditionProblems) = renameExpression environment next condition
            (renamedTrue, afterTrue, trueProblems) = renameBlock environment afterCondition trueBlock
            (renamedFalse, afterFalse, falseProblems) = case falseBlock of
                Nothing -> (Nothing, afterTrue, [])
                Just value -> let (block, after, problems) = renameBlock environment afterTrue value in (Just block, after, problems)
         in ( IfStatement spanValue renamedCondition renamedTrue renamedFalse
            , environment
            , afterFalse
            , conditionProblems ++ trueProblems ++ falseProblems
            )
    WhileStatement spanValue condition body ->
        let (renamedCondition, afterCondition, conditionProblems) = renameExpression environment next condition
            (renamedBody, afterBody, bodyProblems) = renameBlock environment afterCondition body
         in (WhileStatement spanValue renamedCondition renamedBody, environment, afterBody, conditionProblems ++ bodyProblems)
    DoWhileStatement spanValue body condition ->
        let (renamedBody, afterBody, bodyProblems) = renameBlock environment next body
            (renamedCondition, afterCondition, conditionProblems) = renameExpression environment afterBody condition
         in ( DoWhileStatement spanValue renamedBody renamedCondition
            , environment
            , afterCondition
            , bodyProblems ++ conditionProblems
            )
    ForStatement spanValue initializer condition updates body ->
        let (renamedInitializer, loopEnvironment, afterInitializer, initializerProblems) = case initializer of
                Nothing -> (Nothing, environment, next, [])
                Just value ->
                    let (renamed, nested, after, problems) = renameStatement environment next value
                     in (Just renamed, nested, after, problems)
            (renamedCondition, afterCondition, conditionProblems) = case condition of
                Nothing -> (Nothing, afterInitializer, [])
                Just value ->
                    let (renamed, after, problems) = renameExpression loopEnvironment afterInitializer value
                     in (Just renamed, after, problems)
            (renamedUpdates, afterUpdates, updateProblems) = renameStatementList loopEnvironment afterCondition updates
            (renamedBody, afterBody, bodyProblems) = renameBlock loopEnvironment afterUpdates body
         in ( ForStatement spanValue renamedInitializer renamedCondition renamedUpdates renamedBody
            , environment
            , afterBody
            , initializerProblems ++ conditionProblems ++ updateProblems ++ bodyProblems
            )
    ForEachStatement spanValue kind syntax sourceName _ source body ->
        let (renamedSource, afterSource, sourceProblems) = renameExpression environment next source
            duplicate = any ((== sourceName) . fst) environment
            renamedName = RenamedName sourceName afterSource
            duplicateProblems =
                if duplicate
                    then [Diagnostic RenamerStage Error "VXR0003" (Just spanValue) ("duplicate loop binding " ++ identifierText sourceName)]
                    else []
            (renamedBody, afterBody, bodyProblems) =
                renameBlock ((sourceName, renamedName) : environment) (afterSource + 1) body
         in ( ForEachStatement spanValue kind syntax renamedName () renamedSource renamedBody
            , environment
            , afterBody
            , sourceProblems ++ duplicateProblems ++ bodyProblems
            )
    IncrementStatement spanValue name _ ->
        (IncrementStatement spanValue (valueOrMissing name environment) (), environment, next, [])
    CompoundAssignmentStatement spanValue operator name _ value ->
        let (renamedValue, after, problems) = renameExpression environment next value
         in ( CompoundAssignmentStatement spanValue operator (valueOrMissing name environment) () renamedValue
            , environment
            , after
            , problems
            )
    DiscardStatement spanValue value ->
        let (renamedValue, after, problems) = renameExpression environment next value
         in (DiscardStatement spanValue renamedValue, environment, after, problems)
    BreakStatement spanValue value ->
        let (renamed, after, problems) = renameOptional environment next value
         in (BreakStatement spanValue renamed, environment, after, problems)
    ContinueStatement spanValue -> (ContinueStatement spanValue, environment, next, [])
    GuardStatement spanValue condition block ->
        let (renamedCondition, afterCondition, conditionProblems) = renameExpression environment next condition
            (renamedBlock, afterBlock, blockProblems) = renameBlock environment afterCondition block
         in (GuardStatement spanValue renamedCondition renamedBlock, environment, afterBlock, conditionProblems ++ blockProblems)
    -- The names a nested block declares end with the block.
    BlockStatement spanValue block ->
        let (renamedBlock, afterBlock, problems) = renameBlock environment next block
         in (BlockStatement spanValue renamedBlock, environment, afterBlock, problems)
    ExpressionStatement spanValue value terminated ->
        let (renamedValue, after, problems) = renameExpression environment next value
         in (ExpressionStatement spanValue renamedValue terminated, environment, after, problems)

renameStatementList ::
    Environment -> Int -> [Statement Identifier ()] -> ([Statement RenamedName ()], Int, [Diagnostic])
renameStatementList _ next [] = ([], next, [])
renameStatementList environment next (statement : remaining) =
    let (renamed, nested, after, firstProblems) = renameStatement environment next statement
        (later, final, laterProblems) = renameStatementList nested after remaining
     in (renamed : later, final, firstProblems ++ laterProblems)

renameOptional ::
    Environment -> Int -> Maybe (Expression Identifier ()) -> (Maybe (Expression RenamedName ()), Int, [Diagnostic])
renameOptional _ next Nothing = (Nothing, next, [])
renameOptional environment next (Just value) = let (renamed, after, problems) = renameExpression environment next value in (Just renamed, after, problems)

renameExpression :: Environment -> Int -> Expression Identifier () -> (Expression RenamedName (), Int, [Diagnostic])
renameExpression environment next expression = case expression of
    NameExpression spanValue name _ -> (NameExpression spanValue (valueOrMissing name environment) (), next, [])
    LiteralExpression spanValue literal _ -> (LiteralExpression spanValue literal (), next, [])
    MemberAccessExpression spanValue receiver member _ ->
        let (renamedReceiver, afterReceiver, problems) = renameExpression environment next receiver
         in (MemberAccessExpression spanValue renamedReceiver member (), afterReceiver, problems)
    CallExpression spanValue callee arguments _ ->
        let (renamedCallee, afterCallee, firstProblems) = renameExpression environment next callee
            (renamedArguments, after, problems) = renameExpressions environment afterCallee arguments
         in (CallExpression spanValue renamedCallee renamedArguments (), after, firstProblems ++ problems)
    UnaryExpression spanValue operator value _ ->
        let (renamed, after, problems) = renameExpression environment next value
         in (UnaryExpression spanValue operator renamed (), after, problems)
    BinaryExpression spanValue operator left right _ ->
        let (renamedLeft, afterLeft, leftProblems) = renameExpression environment next left
            (renamedRight, afterRight, rightProblems) = renameExpression environment afterLeft right
         in (BinaryExpression spanValue operator renamedLeft renamedRight (), afterRight, leftProblems ++ rightProblems)
    IsPatternExpression spanValue subject patternValue _ ->
        let (renamedSubject, afterSubject, subjectProblems) = renameExpression environment next subject
            (renamedPattern, afterPattern, patternProblems) = renamePattern environment afterSubject patternValue
         in ( IsPatternExpression spanValue renamedSubject renamedPattern ()
            , afterPattern
            , subjectProblems ++ patternProblems
            )
    ConditionalExpression spanValue condition first second _ ->
        let (renamedCondition, afterCondition, conditionProblems) = renameExpression environment next condition
            (renamedFirst, afterFirst, firstProblems) = renameExpression environment afterCondition first
            (renamedSecond, afterSecond, secondProblems) = renameExpression environment afterFirst second
         in ( ConditionalExpression spanValue renamedCondition renamedFirst renamedSecond ()
            , afterSecond
            , conditionProblems ++ firstProblems ++ secondProblems
            )
    CoalesceExpression spanValue left fallback _ ->
        let (renamedLeft, afterLeft, leftProblems) = renameExpression environment next left
            (renamedFallback, afterFallback, fallbackProblems) = renameExpression environment afterLeft fallback
         in (CoalesceExpression spanValue renamedLeft renamedFallback (), afterFallback, leftProblems ++ fallbackProblems)
    AssignmentExpression spanValue operator name value _ ->
        let (renamedValue, after, problems) = renameExpression environment next value
         in ( AssignmentExpression spanValue operator (valueOrMissing name environment) renamedValue ()
            , after
            , problems
            )
    IncrementExpression spanValue isPrefix name _ ->
        (IncrementExpression spanValue isPrefix (valueOrMissing name environment) (), next, [])
    -- Names a loop introduces are scoped to the loop, as for the statement.
    LoopExpression spanValue loop _ ->
        let (renamedLoop, _, after, problems) = renameStatement environment next loop
         in (LoopExpression spanValue renamedLoop (), after, problems)
    -- Names a value block introduces are scoped to the block.
    BlockExpression spanValue block _ ->
        let (renamedBlock, after, problems) = renameBlock environment next block
         in (BlockExpression spanValue renamedBlock (), after, problems)
    MatchExpression spanValue subjects arms _ ->
        let (renamedSubjects, afterSubjects, subjectProblems) = renameExpressions environment next subjects
            (renamedArms, afterArms, armProblems) = renameMatchArms environment afterSubjects arms
         in (MatchExpression spanValue renamedSubjects renamedArms (), afterArms, subjectProblems ++ armProblems)
    CallableExpression spanValue explicit sourceCaptures sourceParameters sourceBody _ ->
        let (captures, captureEnvironment, afterCaptures, captureProblems) =
                renameCaptures environment next sourceCaptures
            bodyOuterEnvironment =
                if explicit
                    then captureEnvironment
                    else captureEnvironment ++ environment
            (parameters, parameterEnvironment, afterParameters, parameterProblems) =
                renameParameters bodyOuterEnvironment afterCaptures sourceParameters
            (body, afterBody, bodyProblems) =
                renameCallableBody parameterEnvironment afterParameters sourceBody
         in ( CallableExpression spanValue explicit captures parameters body ()
            , afterBody
            , captureProblems ++ parameterProblems ++ bodyProblems
            )

renameMatchArms ::
    Environment -> Int -> [MatchArm Identifier ()] -> ([MatchArm RenamedName ()], Int, [Diagnostic])
renameMatchArms _ next [] = ([], next, [])
renameMatchArms environment next (arm : remaining) =
    let (renamed, after, problems) = renameMatchArm environment next arm
        (later, final, laterProblems) = renameMatchArms environment after remaining
     in (renamed : later, final, problems ++ laterProblems)

{- | Rename one arm. The names its patterns bind are in scope in its guard
and its body and nowhere else, so every arm starts from the environment of
the match itself.
-}
renameMatchArm :: Environment -> Int -> MatchArm Identifier () -> (MatchArm RenamedName (), Int, [Diagnostic])
renameMatchArm environment next (MatchArm spanValue patterns guard body) =
    let (renamedPatterns, armEnvironment, afterPatterns, patternProblems) = bindPatterns environment next patterns
        (renamedGuard, afterGuard, guardProblems) = renameOptional armEnvironment afterPatterns guard
        (renamedBody, afterBody, bodyProblems) = renameExpression armEnvironment afterGuard body
     in ( MatchArm spanValue renamedPatterns renamedGuard renamedBody
        , afterBody
        , patternProblems ++ guardProblems ++ bodyProblems
        )
    where
        bindPatterns env current [] = ([], env, current, [])
        bindPatterns env current (patternValue : rest) =
            let (renamed, nextEnv, after, problems) = renameMatchPattern env current patternValue
                (later, finalEnv, final, laterProblems) = bindPatterns nextEnv after rest
             in (renamed : later, finalEnv, final, problems ++ laterProblems)

-- A binding pattern declares a local like any other: it receives a fresh
-- identity and may not reuse a name that is already in scope, which includes
-- a name bound by an earlier pattern of the same arm.
renameMatchPattern ::
    Environment ->
    Int ->
    MatchPattern Identifier () ->
    (MatchPattern RenamedName (), Environment, Int, [Diagnostic])
renameMatchPattern environment next patternValue = case patternValue of
    MatchWildcardPattern spanValue _ -> (MatchWildcardPattern spanValue (), environment, next, [])
    MatchLiteralPattern spanValue literal _ -> (MatchLiteralPattern spanValue literal (), environment, next, [])
    MatchNullPattern spanValue _ -> (MatchNullPattern spanValue (), environment, next, [])
    MatchCasePattern spanValue name _ -> (MatchCasePattern spanValue name (), environment, next, [])
    MatchTypePattern spanValue syntax Nothing _ -> (MatchTypePattern spanValue syntax Nothing (), environment, next, [])
    MatchTypePattern spanValue syntax (Just name) _ ->
        let renamed = RenamedName name next
            duplicateProblems =
                [ Diagnostic RenamerStage Error "VXR0008" (Just spanValue) ("duplicate match binding " ++ identifierText name)
                | any ((== name) . fst) environment
                ]
         in ( MatchTypePattern spanValue syntax (Just renamed) ()
            , (name, renamed) : environment
            , next + 1
            , duplicateProblems
            )

renamePattern :: Environment -> Int -> Pattern Identifier () -> (Pattern RenamedName (), Int, [Diagnostic])
renamePattern environment next patternValue = case patternValue of
    WildcardPattern spanValue _ -> (WildcardPattern spanValue (), next, [])
    NullPattern spanValue _ -> (NullPattern spanValue (), next, [])
    LiteralPattern spanValue literal _ -> (LiteralPattern spanValue literal (), next, [])
    TypePattern spanValue syntax _ -> (TypePattern spanValue syntax (), next, [])
    RelationalPattern spanValue operator literal _ ->
        (RelationalPattern spanValue operator literal (), next, [])
    NotPattern spanValue nested _ ->
        let (renamed, after, problems) = renamePattern environment next nested
         in (NotPattern spanValue renamed (), after, problems)
    AndPattern spanValue left right _ -> renamePair AndPattern spanValue left right
    OrPattern spanValue left right _ -> renamePair OrPattern spanValue left right
    where
        renamePair constructor spanValue left right =
            let (renamedLeft, afterLeft, leftProblems) = renamePattern environment next left
                (renamedRight, afterRight, rightProblems) = renamePattern environment afterLeft right
             in (constructor spanValue renamedLeft renamedRight (), afterRight, leftProblems ++ rightProblems)

-- Capture initializers are renamed in the surrounding scope and in source
-- order.  Each captured binding receives a fresh identity, which is how the
-- AST records that assigning inside a closure does not alias the outer binding.
renameCaptures ::
    Environment ->
    Int ->
    [Capture Identifier ()] ->
    ([Capture RenamedName ()], Environment, Int, [Diagnostic])
renameCaptures environment next captures = go next captures [] [] []
    where
        go current [] output localEnvironment problems =
            (reverse output, localEnvironment, current, reverse problems)
        go current (Capture spanValue mode name _ initializer : remaining) output localEnvironment problems =
            let sourceExpression = case initializer of
                    Just value -> value
                    Nothing -> NameExpression spanValue name ()
                (renamedInitializer, afterInitializer, initializerProblems) =
                    renameExpression environment current sourceExpression
                duplicate = any ((== name) . fst) localEnvironment
                duplicateProblems =
                    if duplicate
                        then
                            Diagnostic
                                RenamerStage
                                Error
                                "VXR0005"
                                (Just spanValue)
                                ("duplicate capture " ++ identifierText name)
                                : problems
                        else problems
                renamedName = RenamedName name afterInitializer
                renamedCapture = Capture spanValue mode renamedName () (Just renamedInitializer)
             in go
                    (afterInitializer + 1)
                    remaining
                    (renamedCapture : output)
                    ((name, renamedName) : localEnvironment)
                    (reverse initializerProblems ++ duplicateProblems)

renameCallableBody ::
    Environment ->
    Int ->
    CallableBody Identifier () ->
    (CallableBody RenamedName (), Int, [Diagnostic])
renameCallableBody environment next body = case body of
    CallableExpressionBody expression ->
        let (renamed, after, problems) = renameExpression environment next expression
         in (CallableExpressionBody renamed, after, problems)
    CallableBlockBody block ->
        let (renamed, after, problems) = renameBlock environment next block
         in (CallableBlockBody renamed, after, problems)

renameExpressions ::
    Environment -> Int -> [Expression Identifier ()] -> ([Expression RenamedName ()], Int, [Diagnostic])
renameExpressions _ next [] = ([], next, [])
renameExpressions environment next (value : rest) =
    let (renamed, after, problems) = renameExpression environment next value
        (remaining, final, restProblems) = renameExpressions environment after rest
     in (renamed : remaining, final, problems ++ restProblems)

valueOrMissing :: Identifier -> Environment -> RenamedName
valueOrMissing name environment = maybe (RenamedName name (-1)) id (lookup name environment)
