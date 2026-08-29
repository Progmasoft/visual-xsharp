-- SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
-- SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.0
module Visual.XSharp.TypeChecker
    ( TypeChecker (..), defaultTypeChecker, runTypeChecker ) where

import Visual.XSharp.AST
import Visual.XSharp.Diagnostic

newtype TypeChecker = TypeChecker { checkResolvedAST :: ResolvedAST -> Either [Diagnostic] TypedAST }
runTypeChecker :: TypeChecker -> ResolvedAST -> Either [Diagnostic] TypedAST
runTypeChecker = checkResolvedAST
defaultTypeChecker :: TypeChecker
defaultTypeChecker = TypeChecker checkTree

type TypeEnvironment = [(SymbolId, (Type, Bool))]

checkTree :: ResolvedAST -> Either [Diagnostic] TypedAST
checkTree (ResolvedAST (SyntaxTree namespace declarations)) =
    let checked = map checkTopDeclaration declarations
        problems = concatMap snd checked
    in if null problems then Right (TypedAST (SyntaxTree namespace (map fst checked))) else Left problems

signature :: Declaration ResolvedName () -> Type
signature declaration = case declaration of
    FunctionDeclaration _ _ _ returnSyntax parameters _ _ _ -> FunctionType
        (map (syntaxType . parameterTypeSyntax) parameters) (syntaxType returnSyntax)
    TypeDeclaration _ name _ _ -> NamedType (QualifiedName [resolvedSpelling name]) []

checkTopDeclaration :: Declaration ResolvedName () -> (Declaration ResolvedName Type, [Diagnostic])
checkTopDeclaration declaration = case declaration of
    TypeDeclaration spanValue name _ members ->
        let signatures = [(resolvedSymbol (declarationName member), (signature member, False)) | member <- members]
            checked = map (checkDeclaration signatures) members
            valueType = NamedType (QualifiedName [resolvedSpelling name]) []
        in (TypeDeclaration spanValue name valueType (map fst checked), concatMap snd checked)
    FunctionDeclaration {} -> checkDeclaration [] declaration

syntaxType :: TypeSyntax -> Type
syntaxType AutoType = ErrorType
syntaxType (ExplicitType (Identifier name)) = case name of
    "bool" -> boolType; "int" -> intType; "long" -> named "long"; "string" -> stringType; "unit" -> unitType
    _ -> NamedType (QualifiedName [Identifier name]) []
  where named value = NamedType (QualifiedName [Identifier value]) []

checkDeclaration :: TypeEnvironment -> Declaration ResolvedName () -> (Declaration ResolvedName Type, [Diagnostic])
checkDeclaration globals declaration@FunctionDeclaration{} =
    let parameters = [(resolvedSymbol (parameterName parameter), (syntaxType (parameterTypeSyntax parameter), False)) | parameter <- declarationParameters declaration]
        expected = syntaxType (declarationReturnSyntax declaration)
        (body, _, explicitReturns, problems) = checkBlock (parameters ++ globals) expected (declarationBody declaration)
        finalReturn = finalExpressionType body
        returns = explicitReturns ++ maybe [] (:[]) finalReturn
        inferred = inferReturn expected returns
        returnProblems = if expected /= ErrorType && any (not . compatible expected) returns
            then [Diagnostic TypeCheckerStage Error "VXT0001" (Just (declarationSpan declaration)) "return expression does not match the declared function type"] else []
        typedParameters = map typeParameter (declarationParameters declaration)
        functionType = FunctionType (map parameterAnnotation typedParameters) inferred
    in (FunctionDeclaration (declarationSpan declaration) (declarationName declaration) functionType
            (declarationReturnSyntax declaration) typedParameters body (declarationIsStatic declaration) (declarationAccess declaration), problems ++ returnProblems)
checkDeclaration _ declaration@TypeDeclaration{} = checkTopDeclaration declaration

typeParameter :: Parameter ResolvedName () -> Parameter ResolvedName Type
typeParameter parameter = Parameter (parameterSpan parameter) (parameterName parameter)
    (syntaxType (parameterTypeSyntax parameter)) (parameterTypeSyntax parameter)

inferReturn :: Type -> [Type] -> Type
inferReturn declared _ | declared /= ErrorType = declared
inferReturn _ [] = unitType
inferReturn _ values = case filter (/= ErrorType) values of [] -> ErrorType; first:_ -> first

compatible :: Type -> Type -> Bool
compatible ErrorType _ = True
compatible _ ErrorType = True
compatible left right = left == right

checkBlock :: TypeEnvironment -> Type -> Block ResolvedName () -> (Block ResolvedName Type, TypeEnvironment, [Type], [Diagnostic])
checkBlock environment expected (Block statements) =
    let (checked, final, returns, problems) = go environment statements in (Block checked, final, returns, problems)
  where
    go env [] = ([], env, [], [])
    go env (statement:rest) =
        let (typed, next, returned, firstProblems) = checkStatement env expected statement
            (remaining, final, laterReturns, laterProblems) = go next rest
        in (typed:remaining, final, returned ++ laterReturns, firstProblems ++ laterProblems)

checkStatement :: TypeEnvironment -> Type -> Statement ResolvedName () -> (Statement ResolvedName Type, TypeEnvironment, [Type], [Diagnostic])
checkStatement environment expected statement = case statement of
    BindingStatement spanValue kind syntax name _ value ->
        let (typedValue, valueType, problems) = checkExpression environment value
            declared = syntaxType syntax
            bindingType = if declared == ErrorType then valueType else declared
            mismatch = if compatible bindingType valueType then [] else [problem spanValue "VXT0002" "binding initializer has the wrong type"]
            mutable = kind == MutableBinding
        in (BindingStatement spanValue kind syntax name bindingType typedValue,
            (resolvedSymbol name, (bindingType, mutable)):environment, [], problems ++ mismatch)
    AssignmentStatement spanValue name _ value ->
        let (typedValue, valueType, problems) = checkExpression environment value
            target = lookup (resolvedSymbol name) environment
            targetType = maybe ErrorType fst target
            immutable = case target of Just (_, False) -> [problem spanValue "VXT0003" "cannot assign to an immutable binding"]; _ -> []
            mismatch = if compatible targetType valueType then [] else [problem spanValue "VXT0004" "assignment value has the wrong type"]
        in (AssignmentStatement spanValue name targetType typedValue, environment, [], problems ++ immutable ++ mismatch)
    ReturnStatement spanValue value ->
        let (typedValue, valueType, problems) = checkOptional environment value
            mismatch = if compatible expected valueType then [] else [problem spanValue "VXT0005" "return value has the wrong type"]
        in (ReturnStatement spanValue typedValue, environment, [valueType], problems ++ mismatch)
    IfStatement spanValue condition trueBlock falseBlock ->
        let (typedCondition, conditionType, conditionProblems) = checkExpression environment condition
            conditionMismatch = if compatible boolType conditionType then [] else [problem spanValue "VXT0006" "if condition must be bool"]
            (typedTrue, _, trueReturns, trueProblems) = checkBlock environment expected trueBlock
            (typedFalse, falseReturns, falseProblems) = case falseBlock of
                Nothing -> (Nothing, [], [])
                Just value -> let (block, _, returns, problems) = checkBlock environment expected value in (Just block, returns, problems)
        in (IfStatement spanValue typedCondition typedTrue typedFalse, environment, trueReturns ++ falseReturns,
            conditionProblems ++ conditionMismatch ++ trueProblems ++ falseProblems)
    ExpressionStatement spanValue value terminated ->
        let (typedValue, _, problems) = checkExpression environment value
            effectProblems = if terminated && not (effectCapable value)
                then [problem spanValue "VXT0013" "pure value expression cannot be used as a statement"] else []
        in (ExpressionStatement spanValue typedValue terminated, environment, [], problems ++ effectProblems)

finalExpressionType :: Block ResolvedName Type -> Maybe Type
finalExpressionType (Block statements) = case reverse statements of
    ExpressionStatement _ expression False:_ -> Just (typedExpressionType expression)
    _ -> Nothing

typedExpressionType :: Expression name Type -> Type
typedExpressionType expression = case expression of
    NameExpression _ _ valueType -> valueType; LiteralExpression _ _ valueType -> valueType
    CallExpression _ _ _ valueType -> valueType; UnaryExpression _ _ _ valueType -> valueType
    BinaryExpression _ _ _ _ valueType -> valueType

effectCapable :: Expression name annotation -> Bool
effectCapable CallExpression{} = True
effectCapable _ = False

checkOptional :: TypeEnvironment -> Maybe (Expression ResolvedName ()) -> (Maybe (Expression ResolvedName Type), Type, [Diagnostic])
checkOptional _ Nothing = (Nothing, unitType, [])
checkOptional environment (Just value) = let (typed, valueType, problems) = checkExpression environment value in (Just typed, valueType, problems)

checkExpression :: TypeEnvironment -> Expression ResolvedName () -> (Expression ResolvedName Type, Type, [Diagnostic])
checkExpression environment expression = case expression of
    NameExpression spanValue name _ ->
        let valueType = maybe ErrorType fst (lookup (resolvedSymbol name) environment)
            problems = if valueType == ErrorType then [problem spanValue "VXT0007" "name has no known type"] else []
        in (NameExpression spanValue name valueType, valueType, problems)
    LiteralExpression spanValue literal _ -> let valueType = literalType literal in (LiteralExpression spanValue literal valueType, valueType, [])
    CallExpression spanValue callee arguments _ ->
        let (typedCallee, calleeType, calleeProblems) = checkExpression environment callee
            checkedArguments = map (checkExpression environment) arguments
            argumentTypes = map (\(_, valueType, _) -> valueType) checkedArguments
            (resultType, callProblems) = case calleeType of
                FunctionType parameters result | length parameters /= length argumentTypes -> (result, [problem spanValue "VXT0008" "call argument count does not match"])
                                               | and (zipWith compatible parameters argumentTypes) -> (result, [])
                                               | otherwise -> (result, [problem spanValue "VXT0009" "call argument type does not match"])
                ErrorType -> (ErrorType, [])
                _ -> (ErrorType, [problem spanValue "VXT0010" "expression is not callable"])
        in (CallExpression spanValue typedCallee (map (\(value, _, _) -> value) checkedArguments) resultType, resultType,
            calleeProblems ++ concatMap (\(_, _, ps) -> ps) checkedArguments ++ callProblems)
    UnaryExpression spanValue operator value _ ->
        let (typedValue, valueType, problems) = checkExpression environment value
            expectedType = if operator == LogicalNot then boolType else intType
            mismatch = if compatible expectedType valueType then [] else [problem spanValue "VXT0011" "unary operator operand has the wrong type"]
        in (UnaryExpression spanValue operator typedValue expectedType, expectedType, problems ++ mismatch)
    BinaryExpression spanValue operator left right _ ->
        let (typedLeft, leftType, leftProblems) = checkExpression environment left
            (typedRight, rightType, rightProblems) = checkExpression environment right
            (operandType, resultType) = operatorTypes operator
            mismatch = if compatible operandType leftType && compatible operandType rightType then [] else [problem spanValue "VXT0012" "binary operator operands have the wrong type"]
        in (BinaryExpression spanValue operator typedLeft typedRight resultType, resultType, leftProblems ++ rightProblems ++ mismatch)

operatorTypes :: BinaryOperator -> (Type, Type)
operatorTypes operator | operator `elem` [LogicalAnd, LogicalOr] = (boolType, boolType)
operatorTypes operator | operator `elem` [LessThan, LessEqual, GreaterThan, GreaterEqual, Equal, NotEqual] = (intType, boolType)
operatorTypes _ = (intType, intType)

literalType :: Literal -> Type
literalType literal = case literal of IntegerLiteral _ -> intType; BooleanLiteral _ -> boolType; StringLiteral _ -> stringType; UnitLiteral -> unitType

problem :: SourceSpan -> String -> String -> Diagnostic
problem spanValue code message = Diagnostic TypeCheckerStage Error code (Just spanValue) message
