-- SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
-- SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.0
module Visual.XSharp.Resolver.NameResolution (NameResolution (..), defaultNameResolution, runNameResolution) where

import Visual.XSharp.AST
import Visual.XSharp.Diagnostic

newtype NameResolution = NameResolution {resolveRenamedAST :: RenamedAST -> Either [Diagnostic] ResolvedAST}
runNameResolution :: NameResolution -> RenamedAST -> Either [Diagnostic] ResolvedAST
runNameResolution = resolveRenamedAST
defaultNameResolution :: NameResolution
defaultNameResolution = NameResolution resolveTree

resolveTree :: RenamedAST -> Either [Diagnostic] ResolvedAST
resolveTree (RenamedAST tree) = case traverseTree tree of
    (resolved, []) -> Right (ResolvedAST resolved)
    (_, problems) -> Left problems

traverseTree :: SyntaxTree RenamedName () -> (SyntaxTree ResolvedName (), [Diagnostic])
traverseTree (SyntaxTree namespace declarations) =
    let values = map resolveDeclaration declarations in (SyntaxTree namespace (map fst values), concatMap snd values)

resolveDeclaration :: Declaration RenamedName () -> (Declaration ResolvedName (), [Diagnostic])
resolveDeclaration declaration = case declaration of
    TypeDeclaration spanValue sourceName _ members ->
        let name = resolveName spanValue sourceName; resolvedMembers = map resolveDeclaration members
         in (TypeDeclaration spanValue (fst name) () (map fst resolvedMembers), snd name ++ concatMap snd resolvedMembers)
    FunctionDeclaration spanValue sourceName _ returnSyntax sourceParameters sourceBody isStatic access ->
        let name = resolveName spanValue sourceName
            parameters = map resolveParameter sourceParameters
            (body, bodyProblems) = resolveBlock sourceBody
         in ( FunctionDeclaration spanValue (fst name) () returnSyntax (map fst parameters) body isStatic access
            , snd name ++ concatMap snd parameters ++ bodyProblems
            )

resolveParameter :: Parameter RenamedName () -> (Parameter ResolvedName (), [Diagnostic])
resolveParameter (Parameter spanValue name _ syntax) = let (resolved, problems) = resolveName spanValue name in (Parameter spanValue resolved () syntax, problems)

resolveBlock :: Block RenamedName () -> (Block ResolvedName (), [Diagnostic])
resolveBlock (Block statements) = let values = map resolveStatement statements in (Block (map fst values), concatMap snd values)

resolveStatement :: Statement RenamedName () -> (Statement ResolvedName (), [Diagnostic])
resolveStatement statement = case statement of
    BindingStatement spanValue kind syntax name _ value ->
        let (resolvedName, nameProblems) = resolveName spanValue name; (resolvedValue, valueProblems) = resolveExpression value
         in (BindingStatement spanValue kind syntax resolvedName () resolvedValue, nameProblems ++ valueProblems)
    AssignmentStatement spanValue name _ value ->
        let (resolvedName, nameProblems) = resolveName spanValue name; (resolvedValue, valueProblems) = resolveExpression value
         in (AssignmentStatement spanValue resolvedName () resolvedValue, nameProblems ++ valueProblems)
    ReturnStatement spanValue value -> let (resolved, problems) = resolveOptional value in (ReturnStatement spanValue resolved, problems)
    IfStatement spanValue condition trueBlock falseBlock ->
        let (resolvedCondition, conditionProblems) = resolveExpression condition
            (resolvedTrue, trueProblems) = resolveBlock trueBlock
            (resolvedFalse, falseProblems) = case falseBlock of
                Nothing -> (Nothing, [])
                Just value -> let (block, problems) = resolveBlock value in (Just block, problems)
         in (IfStatement spanValue resolvedCondition resolvedTrue resolvedFalse, conditionProblems ++ trueProblems ++ falseProblems)
    ExpressionStatement spanValue value terminated -> let (resolved, problems) = resolveExpression value in (ExpressionStatement spanValue resolved terminated, problems)

resolveOptional :: Maybe (Expression RenamedName ()) -> (Maybe (Expression ResolvedName ()), [Diagnostic])
resolveOptional Nothing = (Nothing, [])
resolveOptional (Just value) = let (resolved, problems) = resolveExpression value in (Just resolved, problems)

resolveExpression :: Expression RenamedName () -> (Expression ResolvedName (), [Diagnostic])
resolveExpression expression = case expression of
    NameExpression spanValue name _ -> let (resolved, problems) = resolveName spanValue name in (NameExpression spanValue resolved (), problems)
    LiteralExpression spanValue literal _ -> (LiteralExpression spanValue literal (), [])
    CallExpression spanValue callee arguments _ ->
        let (resolvedCallee, firstProblems) = resolveExpression callee; values = map resolveExpression arguments
         in (CallExpression spanValue resolvedCallee (map fst values) (), firstProblems ++ concatMap snd values)
    UnaryExpression spanValue operator value _ -> let (resolved, problems) = resolveExpression value in (UnaryExpression spanValue operator resolved (), problems)
    BinaryExpression spanValue operator left right _ ->
        let (resolvedLeft, leftProblems) = resolveExpression left; (resolvedRight, rightProblems) = resolveExpression right
         in (BinaryExpression spanValue operator resolvedLeft resolvedRight (), leftProblems ++ rightProblems)
    CallableExpression spanValue explicit captures parameters body _ ->
        let resolvedCaptures = map resolveCapture captures
            resolvedParameters = map resolveParameter parameters
            (resolvedBody, bodyProblems) = resolveCallableBody body
         in ( CallableExpression
                spanValue
                explicit
                (map fst resolvedCaptures)
                (map fst resolvedParameters)
                resolvedBody
                ()
            , concatMap snd resolvedCaptures ++ concatMap snd resolvedParameters ++ bodyProblems
            )

resolveCapture :: Capture RenamedName () -> (Capture ResolvedName (), [Diagnostic])
resolveCapture (Capture spanValue mode name _ initializer) =
    let (resolvedName, nameProblems) = resolveName spanValue name
        (resolvedInitializer, initializerProblems) = resolveOptional initializer
     in ( Capture spanValue mode resolvedName () resolvedInitializer
        , nameProblems ++ initializerProblems
        )

resolveCallableBody ::
    CallableBody RenamedName () ->
    (CallableBody ResolvedName (), [Diagnostic])
resolveCallableBody body = case body of
    CallableExpressionBody expression ->
        let (resolved, problems) = resolveExpression expression
         in (CallableExpressionBody resolved, problems)
    CallableBlockBody block ->
        let (resolved, problems) = resolveBlock block
         in (CallableBlockBody resolved, problems)

resolveName :: SourceSpan -> RenamedName -> (ResolvedName, [Diagnostic])
resolveName spanValue name
    | renamedUnique name > 0 = (ResolvedName (SymbolId (renamedUnique name)) (renamedSpelling name), [])
    | renamedUnique name == 0 =
        ( ResolvedName (SymbolId 0) (renamedSpelling name)
        ,
            [ Diagnostic
                NameResolutionStage
                Error
                "VXN0002"
                (Just spanValue)
                "reserved symbol id zero reached name resolution"
            ]
        )
    | otherwise =
        ( ResolvedName (SymbolId (-1)) (renamedSpelling name)
        ,
            [ Diagnostic
                NameResolutionStage
                Error
                "VXN0001"
                (Just spanValue)
                ("unknown name " ++ identifierText (renamedSpelling name))
            ]
        )
