-- SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
-- SPDX-License-Identifier: MPL-2.0
module Visual.XSharp.Resolver.Renamer
    ( Renamer (..), defaultRenamer, runRenamer ) where

import Visual.XSharp.AST
import Visual.XSharp.Diagnostic

newtype Renamer = Renamer { renameParsedAST :: ParsedAST -> Either [Diagnostic] RenamedAST }
runRenamer :: Renamer -> ParsedAST -> Either [Diagnostic] RenamedAST
runRenamer = renameParsedAST
defaultRenamer :: Renamer
defaultRenamer = Renamer renameTree

type Environment = [(Identifier, RenamedName)]

renameTree :: ParsedAST -> Either [Diagnostic] RenamedAST
renameTree (ParsedAST (SyntaxTree namespace declarations)) =
    let (globals, next, duplicateProblems) = declareMany RenamerStage "VXR0001" 0 [] [(declarationName declaration, declarationSpan declaration) | declaration <- declarations]
        (renamed, _, problems) = renameDeclarations globals next declarations
        allProblems = duplicateProblems ++ problems
    in if null allProblems then Right (RenamedAST (SyntaxTree namespace renamed)) else Left allProblems

declareMany :: DiagnosticStage -> String -> Int -> Environment -> [(Identifier, SourceSpan)] -> (Environment, Int, [Diagnostic])
declareMany _ _ next environment [] = (environment, next, [])
declareMany stage code next environment ((name, spanValue):remaining) =
    let duplicate = case lookup name environment of
            Just _ -> [Diagnostic stage Error code (Just spanValue) ("duplicate declaration " ++ identifierText name)]
            Nothing -> []
        renamed = RenamedName name next
        (final, after, problems) = declareMany stage code (next + 1) ((name, renamed):environment) remaining
    in (final, after, duplicate ++ problems)

renameDeclarations :: Environment -> Int -> [Declaration Identifier ()] -> ([Declaration RenamedName ()], Int, [Diagnostic])
renameDeclarations _ next [] = ([], next, [])
renameDeclarations globals next (declaration:remaining) =
    case declaration of
        TypeDeclaration spanValue sourceName _ members ->
            let name = valueOrMissing sourceName globals
                (declaredMembers, afterMembers, duplicateProblems) = declareMany RenamerStage "VXR0004" next []
                    [(declarationName member, declarationSpan member) | member <- members]
                (renamedMembers, afterBody, memberProblems) = renameDeclarations (declaredMembers ++ globals) afterMembers members
                renamed = TypeDeclaration spanValue name () renamedMembers
                (rest, final, restProblems) = renameDeclarations globals afterBody remaining
            in (renamed:rest, final, duplicateProblems ++ memberProblems ++ restProblems)
        FunctionDeclaration spanValue sourceName _ returnSyntax sourceParameters sourceBody isStatic access ->
            let name = valueOrMissing sourceName globals
                (parameters, parameterEnvironment, afterParameters, parameterProblems) = renameParameters globals next sourceParameters
                (body, afterBody, bodyProblems) = renameBlock parameterEnvironment afterParameters sourceBody
                renamed = FunctionDeclaration spanValue name () returnSyntax parameters body isStatic access
                (rest, final, restProblems) = renameDeclarations globals afterBody remaining
            in (renamed:rest, final, parameterProblems ++ bodyProblems ++ restProblems)

renameParameters :: Environment -> Int -> [Parameter Identifier ()] -> ([Parameter RenamedName ()], Environment, Int, [Diagnostic])
renameParameters environment next parameters = go environment next parameters [] []
  where
    go env current [] output problems = (reverse output, env, current, reverse problems)
    go env current (Parameter spanValue name _ syntax:rest) output problems =
        let duplicate = if any ((== name) . fst) (take (length output) env)
                then Diagnostic RenamerStage Error "VXR0002" (Just spanValue) ("duplicate parameter " ++ identifierText name) : problems
                else problems
            renamed = RenamedName name current
        in go ((name, renamed):env) (current + 1) rest (Parameter spanValue renamed () syntax:output) duplicate

renameBlock :: Environment -> Int -> Block Identifier () -> (Block RenamedName (), Int, [Diagnostic])
renameBlock environment next (Block statements) = let (values, _, final, problems) = go environment next statements in (Block values, final, problems)
  where
    go env current [] = ([], env, current, [])
    go env current (statement:rest) =
        let (renamed, nextEnv, after, firstProblems) = renameStatement env current statement
            (remaining, finalEnv, final, restProblems) = go nextEnv after rest
        in (renamed:remaining, finalEnv, final, firstProblems ++ restProblems)

renameStatement :: Environment -> Int -> Statement Identifier () -> (Statement RenamedName (), Environment, Int, [Diagnostic])
renameStatement environment next statement = case statement of
    BindingStatement spanValue kind syntax name _ value ->
        let (renamedValue, afterValue, problems) = renameExpression environment next value
            duplicate = any ((== name) . fst) environment
            renamed = RenamedName name afterValue
            duplicateProblems = if duplicate then [Diagnostic RenamerStage Error "VXR0003" (Just spanValue) ("duplicate local " ++ identifierText name)] else []
        in (BindingStatement spanValue kind syntax renamed () renamedValue, (name, renamed):environment, afterValue + 1, problems ++ duplicateProblems)
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
        in (IfStatement spanValue renamedCondition renamedTrue renamedFalse, environment, afterFalse, conditionProblems ++ trueProblems ++ falseProblems)
    ExpressionStatement spanValue value terminated ->
        let (renamedValue, after, problems) = renameExpression environment next value
        in (ExpressionStatement spanValue renamedValue terminated, environment, after, problems)

renameOptional :: Environment -> Int -> Maybe (Expression Identifier ()) -> (Maybe (Expression RenamedName ()), Int, [Diagnostic])
renameOptional _ next Nothing = (Nothing, next, [])
renameOptional environment next (Just value) = let (renamed, after, problems) = renameExpression environment next value in (Just renamed, after, problems)

renameExpression :: Environment -> Int -> Expression Identifier () -> (Expression RenamedName (), Int, [Diagnostic])
renameExpression environment next expression = case expression of
    NameExpression spanValue name _ -> (NameExpression spanValue (valueOrMissing name environment) (), next, [])
    LiteralExpression spanValue literal _ -> (LiteralExpression spanValue literal (), next, [])
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

renameExpressions :: Environment -> Int -> [Expression Identifier ()] -> ([Expression RenamedName ()], Int, [Diagnostic])
renameExpressions _ next [] = ([], next, [])
renameExpressions environment next (value:rest) =
    let (renamed, after, problems) = renameExpression environment next value
        (remaining, final, restProblems) = renameExpressions environment after rest
    in (renamed:remaining, final, problems ++ restProblems)

valueOrMissing :: Identifier -> Environment -> RenamedName
valueOrMissing name environment = maybe (RenamedName name (-1)) id (lookup name environment)
