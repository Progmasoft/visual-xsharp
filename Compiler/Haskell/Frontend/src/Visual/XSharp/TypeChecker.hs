-- SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
-- SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.1
module Visual.XSharp.TypeChecker (TypeChecker (..), defaultTypeChecker, runTypeChecker) where

import Visual.XSharp.AST
import Visual.XSharp.BuiltinTypes
import Visual.XSharp.ConstantEvaluation
import Visual.XSharp.Diagnostic
import Visual.XSharp.NumericSemantics
import Visual.XSharp.TemplateValue

newtype TypeChecker = TypeChecker {checkResolvedAST :: ResolvedAST -> Either [Diagnostic] TypedAST}
runTypeChecker :: TypeChecker -> ResolvedAST -> Either [Diagnostic] TypedAST
runTypeChecker = checkResolvedAST
defaultTypeChecker :: TypeChecker
defaultTypeChecker = TypeChecker checkTree

type TypeEnvironment = [(SymbolId, (Type, Bool))]

-- Type syntax deliberately keeps source spellings.  This side environment is
-- the bridge from those spellings to the SymbolIds assigned by the renamer.
-- Type and value parameters are separate because `T` in a type position and
-- `N` in `[T; N]` have different semantic representations.
data TemplateContext = TemplateContext
    { templateTypeNames :: [(Identifier, ResolvedName)]
    , templateValueNames :: [(Identifier, ResolvedName)]
    }

emptyTemplateContext :: TemplateContext
emptyTemplateContext = TemplateContext [] []

checkTree :: ResolvedAST -> Either [Diagnostic] TypedAST
checkTree (ResolvedAST (SyntaxTree namespace declarations)) =
    let checked = map checkTopDeclaration declarations
        problems = concatMap snd checked
     in if null problems then Right (TypedAST (SyntaxTree namespace (map fst checked))) else Left problems

signature :: TemplateContext -> Declaration ResolvedName () -> Type
signature context declaration = case declaration of
    FunctionDeclaration _ _ _ returnSyntax parameters _ _ _ ->
        FunctionType
            (map (syntaxTypeIn context . parameterTypeSyntax) parameters)
            (syntaxTypeIn context returnSyntax)
    TypeDeclaration _ name _ _ -> NamedType (QualifiedName [resolvedSpelling name]) []
    TemplateTypeDeclaration _ name _ parameters _ ->
        NamedType
            (QualifiedName [resolvedSpelling name])
            (map templateParameterAsArgument parameters)

checkTopDeclaration :: Declaration ResolvedName () -> (Declaration ResolvedName Type, [Diagnostic])
checkTopDeclaration declaration = case declaration of
    TypeDeclaration spanValue name _ members ->
        let signatures = [(resolvedSymbol (declarationName member), (signature emptyTemplateContext member, False)) | member <- members]
            checked = map (checkDeclarationWith emptyTemplateContext signatures) members
            valueType = NamedType (QualifiedName [resolvedSpelling name]) []
         in (TypeDeclaration spanValue name valueType (map fst checked), concatMap snd checked)
    TemplateTypeDeclaration spanValue name _ parameters members ->
        let context = templateContext parameters
            typedTemplateParameters = map (typeTemplateParameter context) parameters
            templateValues =
                [ (resolvedSymbol (templateParameterName parameter), (templateParameterAnnotation parameter, False))
                | parameter <- typedTemplateParameters
                , case templateParameterKind parameter of TemplateValueParameterKind _ -> True; _ -> False
                ]
            signatures = [(resolvedSymbol (declarationName member), (signature context member, False)) | member <- members]
            checked = map (checkDeclarationWith context (templateValues ++ signatures)) members
            parameterProblems = validateTemplateParameters context parameters
            valueType =
                NamedType
                    (QualifiedName [resolvedSpelling name])
                    (map templateParameterAsArgument typedTemplateParameters)
         in ( TemplateTypeDeclaration spanValue name valueType typedTemplateParameters (map fst checked)
            , parameterProblems ++ concatMap snd checked
            )
    FunctionDeclaration {} -> checkDeclarationWith emptyTemplateContext [] declaration

templateContext :: [TemplateParameter ResolvedName annotation] -> TemplateContext
templateContext parameters =
    TemplateContext
        [ (resolvedSpelling name, name)
        | parameter <- parameters
        , case templateParameterKind parameter of
            TemplateTypeParameter -> True
            TemplateTemplateParameter _ -> True
            _ -> False
        , let name = templateParameterName parameter
        ]
        [ (resolvedSpelling name, name)
        | parameter <- parameters
        , case templateParameterKind parameter of TemplateValueParameterKind _ -> True; _ -> False
        , let name = templateParameterName parameter
        ]

templateParameterAsArgument :: TemplateParameter ResolvedName annotation -> TemplateArgument
templateParameterAsArgument parameter = case templateParameterKind parameter of
    TemplateValueParameterKind _ -> ValueTemplateArgument (TemplateValueParameter (templateParameterName parameter))
    _ -> TypeTemplateArgument (TypeVariable (templateParameterName parameter))

syntaxTypeIn :: TemplateContext -> TypeSyntax -> Type
syntaxTypeIn _ AutoType = ErrorType
syntaxTypeIn context (QualifiedTypeSyntax name arguments) =
    case name of
        QualifiedName [identifier] | Just resolved <- lookup identifier (templateTypeNames context) -> TypeVariable resolved
        _ -> NamedType name (map (syntaxTemplateArgumentIn context) arguments)
syntaxTypeIn context (BuiltinArrayTypeSyntax element) =
    -- `[]T` is a language type, not a public class invented by the compiler.
    -- Its structural spelling keeps that distinction visible through Core
    -- until ownership-aware lowering assigns the final runtime layout.
    NamedType (QualifiedName [Identifier "[]"]) [TypeTemplateArgument (syntaxTypeIn context element)]
syntaxTypeIn context (ArrayTypeSyntax element) =
    NamedType
        (QualifiedName [Identifier "System", Identifier "Array"])
        [TypeTemplateArgument (syntaxTypeIn context element)]
syntaxTypeIn context (FixedArrayTypeSyntax element size) =
    NamedType
        (QualifiedName [Identifier "System", Identifier "Array"])
        [TypeTemplateArgument (syntaxTypeIn context element), ValueTemplateArgument (syntaxTemplateValueIn context size)]
syntaxTypeIn context (DictionaryTypeSyntax key value) =
    NamedType
        (QualifiedName [Identifier "System", Identifier "Dictionary"])
        [TypeTemplateArgument (syntaxTypeIn context key), TypeTemplateArgument (syntaxTypeIn context value)]
syntaxTypeIn context (CallableTypeSyntax parameters result) = FunctionType (map (syntaxTypeIn context) parameters) (syntaxTypeIn context result)
syntaxTypeIn context (ExplicitType identifier@(Identifier name)) = case lookup identifier (templateTypeNames context) of
    Just resolved -> TypeVariable resolved
    Nothing -> case name of
        "String" -> stringType
        "unit" -> unitType
        "void" -> voidType
        _ -> maybe (NamedType (QualifiedName [Identifier name]) []) scalarTypeToType (lookupScalar name)
    where
        lookupScalar spelling = lookup spelling [(scalarTypeName scalar, scalar) | scalar <- scalarTypes]

syntaxTemplateArgumentIn :: TemplateContext -> TemplateArgumentSyntax -> TemplateArgument
syntaxTemplateArgumentIn context argument = case argument of
    TemplateTypeSyntax valueType -> TypeTemplateArgument (syntaxTypeIn context valueType)
    TemplateValueArgumentSyntax value -> ValueTemplateArgument (syntaxTemplateValueIn context value)

-- Parser construction guarantees the expression tree is side-effect free.
-- Exact evaluation here gives every concrete specialization one canonical
-- identity. Invalid arithmetic becomes a sentinel and is diagnosed by the
-- type-syntax validation pass before Core can be emitted.
syntaxTemplateValueIn :: TemplateContext -> TemplateValueSyntax -> TemplateValue
syntaxTemplateValueIn context value = case value of
    TemplateNameSyntax _ (QualifiedName [identifier])
        | Just resolved <- lookup identifier (templateValueNames context) -> TemplateValueParameter resolved
    _ -> case evaluateTemplateValue value of
        Right result -> result
        _ -> IntegerTemplateValue 0

typeTemplateParameter :: TemplateContext -> TemplateParameter ResolvedName () -> TemplateParameter ResolvedName Type
typeTemplateParameter context parameter =
    TemplateParameter
        (templateParameterSpan parameter)
        (templateParameterName parameter)
        annotation
        (templateParameterKind parameter)
        (templateParameterIsPack parameter)
        (templateParameterDefault parameter)
    where
        annotation = case templateParameterKind parameter of
            TemplateValueParameterKind valueType -> syntaxTypeIn context valueType
            _ -> TypeVariable (templateParameterName parameter)

validateTemplateParameters :: TemplateContext -> [TemplateParameter ResolvedName ()] -> [Diagnostic]
validateTemplateParameters context = concatMap validate
    where
        validate parameter =
            kindProblems parameter
                ++ defaultProblems parameter
                ++ packDefaultProblems parameter
        kindProblems parameter = case templateParameterKind parameter of
            TemplateTypeParameter -> []
            TemplateValueParameterKind valueType -> typeSyntaxProblemsIn context valueType
            TemplateTemplateParameter shapes -> concatMap shapeProblems shapes
        shapeProblems shape = case templateParameterShapeKind shape of
            TemplateTypeParameterShape -> []
            TemplateValueParameterShape valueType -> typeSyntaxProblemsIn context valueType
            TemplateTemplateParameterShape shapes -> concatMap shapeProblems shapes
        defaultProblems parameter = case templateParameterDefault parameter of
            Nothing -> []
            Just (TemplateTypeDefault valueType) -> typeSyntaxProblemsIn context valueType
            Just (TemplateValueDefault value) -> templateValueProblemsIn context "VXT0020" value
        packDefaultProblems parameter
            | templateParameterIsPack parameter
            , Just _ <- templateParameterDefault parameter =
                [problem (templateParameterSpan parameter) "VXT0019" "a template parameter pack cannot have a default"]
            | otherwise = []

typeSyntaxProblemsIn :: TemplateContext -> TypeSyntax -> [Diagnostic]
typeSyntaxProblemsIn context syntax = case syntax of
    ExplicitType _ -> []
    AutoType -> []
    BuiltinArrayTypeSyntax element -> typeSyntaxProblemsIn context element
    ArrayTypeSyntax element -> typeSyntaxProblemsIn context element
    DictionaryTypeSyntax key value -> typeSyntaxProblemsIn context key ++ typeSyntaxProblemsIn context value
    CallableTypeSyntax parameters result -> concatMap (typeSyntaxProblemsIn context) parameters ++ typeSyntaxProblemsIn context result
    QualifiedTypeSyntax _ arguments -> concatMap (templateArgumentProblemsIn context) arguments
    FixedArrayTypeSyntax element size ->
        typeSyntaxProblemsIn context element
            ++ case syntaxTemplateValueIn context size of
                TemplateValueParameter _ -> []
                _ -> case evaluateFixedArraySize size of
                    Left issue -> [problem (templateValueSyntaxSpan size) "VXT0016" (renderTemplateValueError issue)]
                    Right _ -> []

templateArgumentProblemsIn :: TemplateContext -> TemplateArgumentSyntax -> [Diagnostic]
templateArgumentProblemsIn context argument = case argument of
    TemplateTypeSyntax valueType -> typeSyntaxProblemsIn context valueType
    TemplateValueArgumentSyntax value -> templateValueProblemsIn context "VXT0017" value

templateValueProblemsIn :: TemplateContext -> String -> TemplateValueSyntax -> [Diagnostic]
templateValueProblemsIn context code value = case syntaxTemplateValueIn context value of
    TemplateValueParameter _ -> []
    _ -> case evaluateTemplateValue value of
        Left issue -> [problem (templateValueSyntaxSpan value) code (renderTemplateValueError issue)]
        Right _ -> []

checkDeclarationWith ::
    TemplateContext -> TypeEnvironment -> Declaration ResolvedName () -> (Declaration ResolvedName Type, [Diagnostic])
checkDeclarationWith context globals declaration@FunctionDeclaration {} =
    let parameters =
            [ (resolvedSymbol (parameterName parameter), (syntaxTypeIn context (parameterTypeSyntax parameter), False))
            | parameter <- declarationParameters declaration
            ]
        expected = syntaxTypeIn context (declarationReturnSyntax declaration)
        (body, _, explicitReturns, problems) = checkBlockWith context (parameters ++ globals) expected (declarationBody declaration)
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
        typedParameters = map (typeParameterWith context) (declarationParameters declaration)
        signatureProblems =
            typeSyntaxProblemsIn context (declarationReturnSyntax declaration)
                ++ concatMap (typeSyntaxProblemsIn context . parameterTypeSyntax) (declarationParameters declaration)
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
        , signatureProblems ++ problems ++ returnProblems
        )
checkDeclarationWith _ _ declaration@TypeDeclaration {} = checkTopDeclaration declaration
checkDeclarationWith _ _ declaration@TemplateTypeDeclaration {} = checkTopDeclaration declaration

typeParameterWith :: TemplateContext -> Parameter ResolvedName () -> Parameter ResolvedName Type
typeParameterWith context parameter =
    Parameter
        (parameterSpan parameter)
        (parameterName parameter)
        (syntaxTypeIn context (parameterTypeSyntax parameter))
        (parameterTypeSyntax parameter)

inferReturn :: Type -> [Type] -> Type
inferReturn declared _ | declared /= ErrorType = declared
inferReturn _ [] = voidType
inferReturn _ values = case filter (/= ErrorType) values of [] -> ErrorType; first : _ -> first

compatible :: Type -> Type -> Bool
compatible ErrorType _ = True
compatible _ ErrorType = True
compatible left right = left == right

checkBlockWith ::
    TemplateContext ->
    TypeEnvironment ->
    Type ->
    Block ResolvedName () ->
    (Block ResolvedName Type, TypeEnvironment, [Type], [Diagnostic])
checkBlockWith context environment expected (Block statements) =
    let (checked, final, returns, problems) = go environment statements in (Block checked, final, returns, problems)
    where
        go env [] = ([], env, [], [])
        go env (statement : rest) =
            let (typed, next, returned, firstProblems) = checkStatementWith context env expected statement
                (remaining, final, laterReturns, laterProblems) = go next rest
             in (typed : remaining, final, returned ++ laterReturns, firstProblems ++ laterProblems)

checkStatementWith ::
    TemplateContext ->
    TypeEnvironment ->
    Type ->
    Statement ResolvedName () ->
    (Statement ResolvedName Type, TypeEnvironment, [Type], [Diagnostic])
checkStatementWith context environment expected statement = case statement of
    BindingStatement spanValue kind syntax name _ value ->
        let declared = syntaxTypeIn context syntax
            target = if declared == ErrorType then Nothing else Just declared
            (typedValue, valueType, problems) = checkExpressionExpectedWith context environment target value
            bindingType = if declared == ErrorType then valueType else declared
            mismatch =
                if compatible bindingType valueType then [] else [problem spanValue "VXT0002" "binding initializer has the wrong type"]
            constantProblems = constantRangeProblems spanValue bindingType typedValue
            mutable = kind == MutableBinding
         in ( BindingStatement spanValue kind syntax name bindingType typedValue
            , (resolvedSymbol name, (bindingType, mutable)) : environment
            , []
            , typeSyntaxProblemsIn context syntax ++ problems ++ mismatch ++ constantProblems
            )
    AssignmentStatement spanValue name _ value ->
        let (typedValue, valueType, problems) = checkExpressionWith context environment value
            target = lookup (resolvedSymbol name) environment
            targetType = maybe ErrorType fst target
            immutable = case target of Just (_, False) -> [problem spanValue "VXT0003" "cannot assign to an immutable binding"]; _ -> []
            mismatch = if compatible targetType valueType then [] else [problem spanValue "VXT0004" "assignment value has the wrong type"]
         in (AssignmentStatement spanValue name targetType typedValue, environment, [], problems ++ immutable ++ mismatch)
    ReturnStatement spanValue value ->
        let (typedValue, valueType, problems) = checkOptionalExpectedWith context environment (Just expected) value
            mismatch = if compatible expected valueType then [] else [problem spanValue "VXT0005" "return value has the wrong type"]
         in (ReturnStatement spanValue typedValue, environment, [valueType], problems ++ mismatch)
    IfStatement spanValue condition trueBlock falseBlock ->
        let (typedCondition, conditionType, conditionProblems) = checkExpressionWith context environment condition
            conditionMismatch =
                if booleanContextType conditionType
                    then []
                    else [problem spanValue "VXT0006" "if condition must be bool or numeric"]
            (typedTrue, _, trueReturns, trueProblems) = checkBlockWith context environment expected trueBlock
            (typedFalse, falseReturns, falseProblems) = case falseBlock of
                Nothing -> (Nothing, [], [])
                Just value ->
                    let (block, _, returns, problems) = checkBlockWith context environment expected value in (Just block, returns, problems)
         in ( IfStatement spanValue typedCondition typedTrue typedFalse
            , environment
            , trueReturns ++ falseReturns
            , conditionProblems ++ conditionMismatch ++ trueProblems ++ falseProblems
            )
    ExpressionStatement spanValue value terminated ->
        let (typedValue, _, problems) = checkExpressionWith context environment value
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

checkOptionalWith ::
    TemplateContext ->
    TypeEnvironment ->
    Maybe (Expression ResolvedName ()) ->
    (Maybe (Expression ResolvedName Type), Type, [Diagnostic])
checkOptionalWith _ _ Nothing = (Nothing, voidType, [])
checkOptionalWith context environment (Just value) =
    let (typed, valueType, problems) = checkExpressionWith context environment value
     in (Just typed, valueType, problems)

checkOptionalExpectedWith ::
    TemplateContext ->
    TypeEnvironment ->
    Maybe Type ->
    Maybe (Expression ResolvedName ()) ->
    (Maybe (Expression ResolvedName Type), Type, [Diagnostic])
checkOptionalExpectedWith _ _ _ Nothing = (Nothing, voidType, [])
checkOptionalExpectedWith context environment expected (Just value) =
    let (typed, valueType, problems) = checkExpressionExpectedWith context environment expected value
     in (Just typed, valueType, problems)

checkExpressionWith ::
    TemplateContext ->
    TypeEnvironment ->
    Expression ResolvedName () ->
    (Expression ResolvedName Type, Type, [Diagnostic])
checkExpressionWith context environment = checkExpressionExpectedWith context environment Nothing

-- Expected types are semantic context, not conversions.  They choose the
-- representation of an otherwise untyped numeric literal and allow the
-- boolean numeric rule, but never silently convert a computed value.
checkExpressionExpectedWith ::
    TemplateContext ->
    TypeEnvironment ->
    Maybe Type ->
    Expression ResolvedName () ->
    (Expression ResolvedName Type, Type, [Diagnostic])
checkExpressionExpectedWith context environment expected expression = case expression of
    NameExpression spanValue name _ ->
        let valueType = maybe ErrorType fst (lookup (resolvedSymbol name) environment)
            problems = if valueType == ErrorType then [problem spanValue "VXT0007" "name has no known type"] else []
         in (NameExpression spanValue name valueType, valueType, problems)
    LiteralExpression spanValue literal _ ->
        let (valueType, problems) = literalTypeInContext spanValue expected literal
         in (LiteralExpression spanValue literal valueType, valueType, problems)
    CallExpression spanValue callee arguments _ ->
        let (typedCallee, calleeType, calleeProblems) = checkExpressionWith context environment callee
            parameterTypes = case calleeType of FunctionType parameters _ -> parameters; _ -> []
            checkedArguments =
                zipWith
                    (\index argument -> checkExpressionExpectedWith context environment (safeIndex parameterTypes index) argument)
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
        let operandExpected = if operator == LogicalNot then Nothing else expected
            (typedValue, valueType, problems) = checkExpressionExpectedWith context environment operandExpected value
            rule = unaryNumericRule operator valueType
            mismatch = ruleProblems spanValue "VXT0011" rule
         in (UnaryExpression spanValue operator typedValue (numericRuleType rule), numericRuleType rule, problems ++ mismatch)
    BinaryExpression spanValue operator left right _ ->
        -- A Boolean result does not imply Boolean operands: pushing the return
        -- context into 1 == 2 would convert both literals to true. Comparisons
        -- infer their operand domain; logical operands may use distinct numeric
        -- types and therefore do not borrow each other's expected type.
        let operandExpected = if booleanResult operator then Nothing else expected
            (typedLeft, leftType, leftProblems) = checkExpressionExpectedWith context environment operandExpected left
            rightExpected = if operator `elem` [LogicalAnd, LogicalOr] then Nothing else Just leftType
            (typedRight, rightType, rightProblems) = checkExpressionExpectedWith context environment rightExpected right
            rule = binaryNumericRule operator leftType rightType
            resultType = numericRuleType rule
            mismatch = ruleProblems spanValue "VXT0012" rule
         in ( BinaryExpression spanValue operator typedLeft typedRight resultType
            , resultType
            , leftProblems ++ rightProblems ++ mismatch
            )
    CallableExpression spanValue explicit captures parameters body _ ->
        let checkedCaptures = checkCapturesWith context environment captures
            captureEnvironment =
                [ (resolvedSymbol (captureName capture), (captureAnnotation capture, True))
                | capture <- map firstCapture checkedCaptures
                ]
            typedParameters = map (typeCallableParameterWith context) parameters
            parameterEnvironment =
                [ (resolvedSymbol (parameterName parameter), (parameterAnnotation parameter, False))
                | parameter <- typedParameters
                ]
            callableEnvironment = parameterEnvironment ++ captureEnvironment ++ environment
            (typedBody, resultType, bodyProblems) = checkCallableBodyWith context callableEnvironment body
            callableType = FunctionType (map parameterAnnotation typedParameters) resultType
            captureProblems = concatMap captureDiagnostics checkedCaptures
            parameterProblems = concatMap (typeSyntaxProblemsIn context . parameterTypeSyntax) parameters
         in ( CallableExpression
                spanValue
                explicit
                (map firstCapture checkedCaptures)
                typedParameters
                typedBody
                callableType
            , callableType
            , captureProblems ++ parameterProblems ++ bodyProblems
            )

type CheckedCapture = (Capture ResolvedName Type, [Diagnostic])

booleanResult :: BinaryOperator -> Bool
booleanResult operator =
    operator `elem` [LogicalAnd, LogicalOr, Equal, NotEqual, LessThan, LessEqual, GreaterThan, GreaterEqual]

firstCapture :: CheckedCapture -> Capture ResolvedName Type
firstCapture = fst

captureDiagnostics :: CheckedCapture -> [Diagnostic]
captureDiagnostics = snd

checkCapturesWith :: TemplateContext -> TypeEnvironment -> [Capture ResolvedName ()] -> [CheckedCapture]
checkCapturesWith context environment = map checkCapture
    where
        checkCapture (Capture spanValue mode name _ initializer) =
            let (typedInitializer, valueType, problems) = checkOptionalWith context environment initializer
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

typeCallableParameterWith :: TemplateContext -> Parameter ResolvedName () -> Parameter ResolvedName Type
typeCallableParameterWith context parameter =
    let valueType = case parameterTypeSyntax parameter of
            AutoType -> TypeVariable (parameterName parameter)
            syntax -> syntaxTypeIn context syntax
     in Parameter
            (parameterSpan parameter)
            (parameterName parameter)
            valueType
            (parameterTypeSyntax parameter)

checkCallableBodyWith ::
    TemplateContext ->
    TypeEnvironment ->
    CallableBody ResolvedName () ->
    (CallableBody ResolvedName Type, Type, [Diagnostic])
checkCallableBodyWith context environment body = case body of
    CallableExpressionBody expression ->
        let (typed, valueType, problems) = checkExpressionWith context environment expression
         in (CallableExpressionBody typed, valueType, problems)
    CallableBlockBody block ->
        let (typed, _, returns, problems) = checkBlockWith context environment ErrorType block
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
