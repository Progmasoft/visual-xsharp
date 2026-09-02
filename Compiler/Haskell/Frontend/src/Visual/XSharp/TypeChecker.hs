-- SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
-- SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.0
module Visual.XSharp.TypeChecker (TypeChecker (..), defaultTypeChecker, runTypeChecker) where

import Visual.XSharp.AST
import Visual.XSharp.BuiltinTypes
import Visual.XSharp.ConstantEvaluation
import Visual.XSharp.Diagnostic
import Visual.XSharp.NumericSemantics

newtype TypeChecker = TypeChecker {checkResolvedAST :: ResolvedAST -> Either [Diagnostic] TypedAST}
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
    FunctionDeclaration _ _ _ returnSyntax parameters _ _ _ ->
        FunctionType
            (map (syntaxType . parameterTypeSyntax) parameters)
            (syntaxType returnSyntax)
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
    "String" -> stringType
    "unit" -> unitType
    "void" -> voidType
    _ -> maybe (NamedType (QualifiedName [Identifier name]) []) scalarTypeToType (lookupScalar name)
    where
        lookupScalar spelling = lookup spelling [(scalarTypeName scalar, scalar) | scalar <- scalarTypes]

checkDeclaration :: TypeEnvironment -> Declaration ResolvedName () -> (Declaration ResolvedName Type, [Diagnostic])
checkDeclaration globals declaration@FunctionDeclaration {} =
    let parameters =
            [ (resolvedSymbol (parameterName parameter), (syntaxType (parameterTypeSyntax parameter), False))
            | parameter <- declarationParameters declaration
            ]
        expected = syntaxType (declarationReturnSyntax declaration)
        (body, _, explicitReturns, problems) = checkBlock (parameters ++ globals) expected (declarationBody declaration)
        finalReturn = finalExpressionType body
        returns = explicitReturns ++ maybe [] (: []) finalReturn
        inferred = inferReturn expected returns
        returnProblems =
            if expected /= ErrorType && any (not . compatible expected) returns
                then
                    [ Diagnostic
                        TypeCheckerStage
                        Error
                        "VXT0001"
                        (Just (declarationSpan declaration))
                        "return expression does not match the declared function type"
                    ]
                else []
        typedParameters = map typeParameter (declarationParameters declaration)
        functionType = FunctionType (map parameterAnnotation typedParameters) inferred
     in ( FunctionDeclaration
            (declarationSpan declaration)
            (declarationName declaration)
            functionType
            (declarationReturnSyntax declaration)
            typedParameters
            body
            (declarationIsStatic declaration)
            (declarationAccess declaration)
        , problems ++ returnProblems
        )
checkDeclaration _ declaration@TypeDeclaration {} = checkTopDeclaration declaration

typeParameter :: Parameter ResolvedName () -> Parameter ResolvedName Type
typeParameter parameter =
    Parameter
        (parameterSpan parameter)
        (parameterName parameter)
        (syntaxType (parameterTypeSyntax parameter))
        (parameterTypeSyntax parameter)

inferReturn :: Type -> [Type] -> Type
inferReturn declared _ | declared /= ErrorType = declared
inferReturn _ [] = voidType
inferReturn _ values = case filter (/= ErrorType) values of [] -> ErrorType; first : _ -> first

compatible :: Type -> Type -> Bool
compatible ErrorType _ = True
compatible _ ErrorType = True
compatible left right = left == right

checkBlock ::
    TypeEnvironment -> Type -> Block ResolvedName () -> (Block ResolvedName Type, TypeEnvironment, [Type], [Diagnostic])
checkBlock environment expected (Block statements) =
    let (checked, final, returns, problems) = go environment statements in (Block checked, final, returns, problems)
    where
        go env [] = ([], env, [], [])
        go env (statement : rest) =
            let (typed, next, returned, firstProblems) = checkStatement env expected statement
                (remaining, final, laterReturns, laterProblems) = go next rest
             in (typed : remaining, final, returned ++ laterReturns, firstProblems ++ laterProblems)

checkStatement ::
    TypeEnvironment ->
    Type ->
    Statement ResolvedName () ->
    (Statement ResolvedName Type, TypeEnvironment, [Type], [Diagnostic])
checkStatement environment expected statement = case statement of
    BindingStatement spanValue kind syntax name _ value ->
        let declared = syntaxType syntax
            target = if declared == ErrorType then Nothing else Just declared
            (typedValue, valueType, problems) = checkExpressionExpected environment target value
            bindingType = if declared == ErrorType then valueType else declared
            mismatch =
                if compatible bindingType valueType then [] else [problem spanValue "VXT0002" "binding initializer has the wrong type"]
            constantProblems = constantRangeProblems spanValue bindingType typedValue
            mutable = kind == MutableBinding
         in ( BindingStatement spanValue kind syntax name bindingType typedValue
            , (resolvedSymbol name, (bindingType, mutable)) : environment
            , []
            , problems ++ mismatch ++ constantProblems
            )
    AssignmentStatement spanValue name _ value ->
        let (typedValue, valueType, problems) = checkExpression environment value
            target = lookup (resolvedSymbol name) environment
            targetType = maybe ErrorType fst target
            immutable = case target of Just (_, False) -> [problem spanValue "VXT0003" "cannot assign to an immutable binding"]; _ -> []
            mismatch = if compatible targetType valueType then [] else [problem spanValue "VXT0004" "assignment value has the wrong type"]
         in (AssignmentStatement spanValue name targetType typedValue, environment, [], problems ++ immutable ++ mismatch)
    ReturnStatement spanValue value ->
        let (typedValue, valueType, problems) = checkOptionalExpected environment (Just expected) value
            mismatch = if compatible expected valueType then [] else [problem spanValue "VXT0005" "return value has the wrong type"]
         in (ReturnStatement spanValue typedValue, environment, [valueType], problems ++ mismatch)
    IfStatement spanValue condition trueBlock falseBlock ->
        let (typedCondition, conditionType, conditionProblems) = checkExpression environment condition
            conditionMismatch =
                if booleanContextType conditionType
                    then []
                    else [problem spanValue "VXT0006" "if condition must be bool or numeric"]
            (typedTrue, _, trueReturns, trueProblems) = checkBlock environment expected trueBlock
            (typedFalse, falseReturns, falseProblems) = case falseBlock of
                Nothing -> (Nothing, [], [])
                Just value -> let (block, _, returns, problems) = checkBlock environment expected value in (Just block, returns, problems)
         in ( IfStatement spanValue typedCondition typedTrue typedFalse
            , environment
            , trueReturns ++ falseReturns
            , conditionProblems ++ conditionMismatch ++ trueProblems ++ falseProblems
            )
    ExpressionStatement spanValue value terminated ->
        let (typedValue, _, problems) = checkExpression environment value
            effectProblems =
                if terminated && not (effectCapable value)
                    then [problem spanValue "VXT0013" "pure value expression cannot be used as a statement"]
                    else []
         in (ExpressionStatement spanValue typedValue terminated, environment, [], problems ++ effectProblems)

finalExpressionType :: Block ResolvedName Type -> Maybe Type
finalExpressionType (Block statements) = case reverse statements of
    ExpressionStatement _ expression False : _ -> Just (typedExpressionType expression)
    _ -> Nothing

typedExpressionType :: Expression name Type -> Type
typedExpressionType expression = case expression of
    NameExpression _ _ valueType -> valueType
    LiteralExpression _ _ valueType -> valueType
    CallExpression _ _ _ valueType -> valueType
    UnaryExpression _ _ _ valueType -> valueType
    BinaryExpression _ _ _ _ valueType -> valueType
    CallableExpression _ _ _ _ _ valueType -> valueType

effectCapable :: Expression name annotation -> Bool
effectCapable CallExpression {} = True
effectCapable CallableExpression {} = False
effectCapable _ = False

checkOptional ::
    TypeEnvironment -> Maybe (Expression ResolvedName ()) -> (Maybe (Expression ResolvedName Type), Type, [Diagnostic])
checkOptional _ Nothing = (Nothing, voidType, [])
checkOptional environment (Just value) = let (typed, valueType, problems) = checkExpression environment value in (Just typed, valueType, problems)

checkOptionalExpected ::
    TypeEnvironment ->
    Maybe Type ->
    Maybe (Expression ResolvedName ()) ->
    (Maybe (Expression ResolvedName Type), Type, [Diagnostic])
checkOptionalExpected _ _ Nothing = (Nothing, voidType, [])
checkOptionalExpected environment expected (Just value) =
    let (typed, valueType, problems) = checkExpressionExpected environment expected value
     in (Just typed, valueType, problems)

checkExpression :: TypeEnvironment -> Expression ResolvedName () -> (Expression ResolvedName Type, Type, [Diagnostic])
checkExpression environment = checkExpressionExpected environment Nothing

-- Expected types are semantic context, not conversions.  They choose the
-- representation of an otherwise untyped numeric literal and allow the
-- boolean numeric rule, but never silently convert a computed value.
checkExpressionExpected ::
    TypeEnvironment -> Maybe Type -> Expression ResolvedName () -> (Expression ResolvedName Type, Type, [Diagnostic])
checkExpressionExpected environment expected expression = case expression of
    NameExpression spanValue name _ ->
        let valueType = maybe ErrorType fst (lookup (resolvedSymbol name) environment)
            problems = if valueType == ErrorType then [problem spanValue "VXT0007" "name has no known type"] else []
         in (NameExpression spanValue name valueType, valueType, problems)
    LiteralExpression spanValue literal _ ->
        let (valueType, problems) = literalTypeInContext spanValue expected literal
         in (LiteralExpression spanValue literal valueType, valueType, problems)
    CallExpression spanValue callee arguments _ ->
        let (typedCallee, calleeType, calleeProblems) = checkExpression environment callee
            parameterTypes = case calleeType of FunctionType parameters _ -> parameters; _ -> []
            checkedArguments =
                zipWith
                    (\index argument -> checkExpressionExpected environment (safeIndex parameterTypes index) argument)
                    [0 ..]
                    arguments
            argumentTypes = map (\(_, valueType, _) -> valueType) checkedArguments
            (resultType, callProblems) = case calleeType of
                FunctionType parameters result
                    | length parameters /= length argumentTypes ->
                        (result, [problem spanValue "VXT0008" "call argument count does not match"])
                    | and (zipWith compatible parameters argumentTypes) -> (result, [])
                    | otherwise -> (result, [problem spanValue "VXT0009" "call argument type does not match"])
                ErrorType -> (ErrorType, [])
                _ -> (ErrorType, [problem spanValue "VXT0010" "expression is not callable"])
         in ( CallExpression spanValue typedCallee (map (\(value, _, _) -> value) checkedArguments) resultType
            , resultType
            , calleeProblems ++ concatMap (\(_, _, ps) -> ps) checkedArguments ++ callProblems
            )
    UnaryExpression spanValue operator value _ ->
        let (typedValue, valueType, problems) = checkExpressionExpected environment expected value
            rule = unaryNumericRule operator valueType
            mismatch = ruleProblems spanValue "VXT0011" rule
         in (UnaryExpression spanValue operator typedValue (numericRuleType rule), numericRuleType rule, problems ++ mismatch)
    BinaryExpression spanValue operator left right _ ->
        let (typedLeft, leftType, leftProblems) = checkExpressionExpected environment expected left
            (typedRight, rightType, rightProblems) = checkExpressionExpected environment (Just leftType) right
            rule = binaryNumericRule operator leftType rightType
            resultType = numericRuleType rule
            mismatch = ruleProblems spanValue "VXT0012" rule
         in ( BinaryExpression spanValue operator typedLeft typedRight resultType
            , resultType
            , leftProblems ++ rightProblems ++ mismatch
            )
    CallableExpression spanValue explicit captures parameters body _ ->
        let checkedCaptures = checkCaptures environment captures
            captureEnvironment =
                [ (resolvedSymbol (captureName capture), (captureAnnotation capture, True))
                | capture <- map firstCapture checkedCaptures
                ]
            typedParameters = map typeCallableParameter parameters
            parameterEnvironment =
                [ (resolvedSymbol (parameterName parameter), (parameterAnnotation parameter, False))
                | parameter <- typedParameters
                ]
            callableEnvironment = parameterEnvironment ++ captureEnvironment ++ environment
            (typedBody, resultType, bodyProblems) = checkCallableBody callableEnvironment body
            callableType = FunctionType (map parameterAnnotation typedParameters) resultType
            captureProblems = concatMap captureDiagnostics checkedCaptures
         in ( CallableExpression
                spanValue
                explicit
                (map firstCapture checkedCaptures)
                typedParameters
                typedBody
                callableType
            , callableType
            , captureProblems ++ bodyProblems
            )

type CheckedCapture = (Capture ResolvedName Type, [Diagnostic])

firstCapture :: CheckedCapture -> Capture ResolvedName Type
firstCapture = fst

captureDiagnostics :: CheckedCapture -> [Diagnostic]
captureDiagnostics = snd

checkCaptures :: TypeEnvironment -> [Capture ResolvedName ()] -> [CheckedCapture]
checkCaptures environment = map checkCapture
    where
        checkCapture (Capture spanValue mode name _ initializer) =
            let (typedInitializer, valueType, problems) = checkOptional environment initializer
                ownershipProblems = case mode of
                    StrongCapture -> []
                    _ | isReferenceType valueType -> []
                    WeakCapture -> [problem spanValue "VXT0014" "weak capture requires an AARC reference value"]
                    UnownedCapture -> [problem spanValue "VXT0015" "unowned capture requires an AARC reference value"]
             in (Capture spanValue mode name valueType typedInitializer, problems ++ ownershipProblems)

-- String and callable values are AARC references. Named user and library
-- types are conservatively treated as references until declaration metadata
-- lets the ownership pass distinguish AARC declarations from CoW values.
isReferenceType :: Type -> Bool
isReferenceType valueType = case valueType of
    FunctionType _ _ -> True
    NamedType name _ -> name `notElem` primitiveNames
    _ -> False
    where
        primitiveNames =
            [ QualifiedName [Identifier "bool"]
            , QualifiedName [Identifier "int"]
            , QualifiedName [Identifier "long"]
            , QualifiedName [Identifier "unit"]
            , QualifiedName [Identifier "void"]
            ]

typeCallableParameter :: Parameter ResolvedName () -> Parameter ResolvedName Type
typeCallableParameter parameter =
    let valueType = case parameterTypeSyntax parameter of
            AutoType -> TypeVariable (parameterName parameter)
            syntax -> syntaxType syntax
     in Parameter
            (parameterSpan parameter)
            (parameterName parameter)
            valueType
            (parameterTypeSyntax parameter)

checkCallableBody ::
    TypeEnvironment ->
    CallableBody ResolvedName () ->
    (CallableBody ResolvedName Type, Type, [Diagnostic])
checkCallableBody environment body = case body of
    CallableExpressionBody expression ->
        let (typed, valueType, problems) = checkExpression environment expression
         in (CallableExpressionBody typed, valueType, problems)
    CallableBlockBody block ->
        let (typed, _, returns, problems) = checkBlock environment ErrorType block
            finalType = maybe (inferReturn ErrorType returns) id (finalExpressionType typed)
         in (CallableBlockBody typed, finalType, problems)

booleanContextType :: Type -> Bool
booleanContextType = acceptsBooleanContext

literalTypeInContext :: SourceSpan -> Maybe Type -> Literal -> (Type, [Diagnostic])
literalTypeInContext spanValue expected literal = case literal of
    IntegerLiteral value -> integerLiteralType spanValue expected value
    FloatingLiteral _ -> floatingLiteralType expected
    CharacterLiteral _ -> (scalarTypeToType CharacterScalar, [])
    BooleanLiteral _ -> (boolType, [])
    StringLiteral _ -> (stringType, [])
    UnitLiteral -> (unitType, [])

integerLiteralType :: SourceSpan -> Maybe Type -> Integer -> (Type, [Diagnostic])
integerLiteralType spanValue expected value =
    let context = maybe NoNumericContext targetContext expected
        rule = integerLiteralRule context value
        code = case numericRuleError rule of Just (UntargetedIntegerOutsideInt _) -> "VXT0017"; _ -> "VXT0016"
     in (numericRuleType rule, ruleProblems spanValue code rule)
    where
        targetContext target | target == boolType = BooleanNumericContext
        targetContext target = TargetNumericType target

floatingLiteralType :: Maybe Type -> (Type, [Diagnostic])
floatingLiteralType expected =
    let context = maybe NoNumericContext TargetNumericType expected
        rule = floatingLiteralRule context
     in (numericRuleType rule, [])

ruleProblems :: SourceSpan -> String -> NumericRuleResult -> [Diagnostic]
ruleProblems spanValue code rule = case numericRuleError rule of
    Nothing -> []
    Just issue -> [problem spanValue code (renderNumericRuleError issue)]

constantRangeProblems :: SourceSpan -> Type -> Expression ResolvedName Type -> [Diagnostic]
constantRangeProblems spanValue target expression = case evaluateConstantInteger expression of
    Left issue -> [problem spanValue "VXT0019" (renderConstantIntegerError issue)]
    Right (Just value) -> case typeToScalarType target of
        Just scalar
            | scalarTypeFamily scalar `elem` [SignedIntegerFamily, UnsignedIntegerFamily]
            , not (integerFits scalar value) ->
                [ problem
                    spanValue
                    "VXT0018"
                    ("constant expression result " ++ show value ++ " does not fit " ++ scalarTypeName scalar)
                ]
        _ -> []
    Right Nothing -> []

safeIndex :: [a] -> Int -> Maybe a
safeIndex values index
    | index < 0 = Nothing
    | otherwise = case drop index values of value : _ -> Just value; [] -> Nothing

problem :: SourceSpan -> String -> String -> Diagnostic
problem spanValue code message = Diagnostic TypeCheckerStage Error code (Just spanValue) message
