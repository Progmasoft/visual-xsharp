-- SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
-- SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.1

{- | Semantic checker for trees whose references already have stable identities.

The checker attaches types and returns diagnostics without rewriting unresolved
names. It runs after Renamer and Name Resolution so lexical spelling is never
used as a substitute for symbol identity.
-}
module Visual.XSharp.TypeChecker (TypeChecker (..), defaultTypeChecker, runTypeChecker) where

import Visual.XSharp.AST
import Visual.XSharp.BuiltinTypes
import Visual.XSharp.ConstantEvaluation
import Visual.XSharp.Diagnostic
import Visual.XSharp.NumericSemantics
import Visual.XSharp.TemplateValue
import Visual.XSharp.TypeChecker.Branching

-- | A resolved-tree checker that produces a typed tree only when checking succeeds.
newtype TypeChecker = TypeChecker {checkResolvedAST :: ResolvedAST -> Either [Diagnostic] TypedAST}

-- | Apply a checker to the output of Name Resolution.
runTypeChecker :: TypeChecker -> ResolvedAST -> Either [Diagnostic] TypedAST
runTypeChecker = checkResolvedAST

-- | The production checker for the currently implemented Visual X# subset.
defaultTypeChecker :: TypeChecker
defaultTypeChecker = TypeChecker checkTree

type TypeEnvironment = [(SymbolId, (Type, Bool))]

-- The type checker owns a whole source-set catalog before it checks any body.
-- That permits calls to later-declared classes without making parsing depend
-- on declaration order or mutating the Renamer's lexical environment.
data MethodCandidate = MethodCandidate
    { candidateOwner :: SymbolId
    , candidateDeclaration :: Declaration ResolvedName ()
    }

data TypeCatalog = TypeCatalog
    { catalogTypes :: [(SymbolId, ResolvedName)]
    , catalogMethods :: [MethodCandidate]
    }

-- Type syntax deliberately keeps source spellings.  This side environment is
-- the bridge from those spellings to the SymbolIds assigned by the renamer.
-- Type and value parameters are separate because `T` in a type position and
-- `N` in `[T; N]` have different semantic representations.
data TemplateContext = TemplateContext
    { templateTypeNames :: [(Identifier, ResolvedName)]
    , templateValueNames :: [(Identifier, ResolvedName)]
    , templateCatalog :: TypeCatalog
    , templateCurrentType :: Maybe SymbolId
    }

emptyTemplateContext :: TypeCatalog -> Maybe SymbolId -> TemplateContext
emptyTemplateContext catalog owner = TemplateContext [] [] catalog owner

{- | Type-check every top-level declaration and collect independent diagnostics.
No partially typed AST escapes when any declaration has an error.
-}
checkTree :: ResolvedAST -> Either [Diagnostic] TypedAST
checkTree (ResolvedAST (SyntaxTree namespace declarations)) =
    let catalog = catalogDeclarations declarations
        checked = map (checkTopDeclaration catalog) declarations
        problems = concatMap snd checked
     in if null problems then Right (TypedAST (SyntaxTree namespace (map fst checked))) else Left problems

catalogDeclarations :: [Declaration ResolvedName ()] -> TypeCatalog
catalogDeclarations declarations =
    TypeCatalog
        [ (resolvedSymbol (declarationName declaration), declarationName declaration)
        | declaration <- declarations
        , isTypeDeclaration declaration
        ]
        [ MethodCandidate (resolvedSymbol (declarationName owner)) member
        | owner <- declarations
        , isTypeDeclaration owner
        , member <- typeMembersOf owner
        , case member of FunctionDeclaration {} -> True; _ -> False
        ]
    where
        isTypeDeclaration TypeDeclaration {} = True
        isTypeDeclaration TemplateTypeDeclaration {} = True
        isTypeDeclaration _ = False
        typeMembersOf TypeDeclaration {typeMembers = members} = members
        typeMembersOf TemplateTypeDeclaration {typeMembers = members} = members
        typeMembersOf _ = []

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

checkTopDeclaration :: TypeCatalog -> Declaration ResolvedName () -> (Declaration ResolvedName Type, [Diagnostic])
checkTopDeclaration catalog declaration = case declaration of
    TypeDeclaration spanValue name _ members ->
        let owner = resolvedSymbol name
            context = emptyTemplateContext catalog (Just owner)
            signatures = [(resolvedSymbol (declarationName member), (signature context member, False)) | member <- members]
            checked = map (checkDeclarationWith context signatures) members
            overloadProblems = duplicateOverloadProblems context members
            valueType = NamedType (QualifiedName [resolvedSpelling name]) []
         in (TypeDeclaration spanValue name valueType (map fst checked), overloadProblems ++ concatMap snd checked)
    TemplateTypeDeclaration spanValue name _ parameters members ->
        let owner = resolvedSymbol name
            context = (templateContext catalog (Just owner) parameters)
            typedTemplateParameters = map (typeTemplateParameter context) parameters
            templateValues =
                [ (resolvedSymbol (templateParameterName parameter), (templateParameterAnnotation parameter, False))
                | parameter <- typedTemplateParameters
                , case templateParameterKind parameter of TemplateValueParameterKind _ -> True; _ -> False
                ]
            signatures = [(resolvedSymbol (declarationName member), (signature context member, False)) | member <- members]
            checked = map (checkDeclarationWith context (templateValues ++ signatures)) members
            parameterProblems = validateTemplateParameters context parameters
            overloadProblems = duplicateOverloadProblems context members
            valueType =
                NamedType
                    (QualifiedName [resolvedSpelling name])
                    (map templateParameterAsArgument typedTemplateParameters)
         in ( TemplateTypeDeclaration spanValue name valueType typedTemplateParameters (map fst checked)
            , parameterProblems ++ overloadProblems ++ concatMap snd checked
            )
    FunctionDeclaration {} -> checkDeclarationWith (emptyTemplateContext catalog Nothing) [] declaration

{- | Keep type and value parameters in separate lookup tables.
Identical source spelling in the two categories must not collapse their roles.
-}
templateContext :: TypeCatalog -> Maybe SymbolId -> [TemplateParameter ResolvedName annotation] -> TemplateContext
templateContext catalog owner parameters =
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
        catalog
        owner

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

{- | Canonicalize a template argument before it contributes to specialization identity.
The fallback value is only a recovery sentinel; validation reports the original
invalid expression before a specialization plan is emitted.
-}
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
        (body, _, explicitReturns, problems) = checkBlockWith context (parameters ++ globals) expected outsideLoops (declarationBody declaration)
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
checkDeclarationWith context _ declaration@TypeDeclaration {} = checkTopDeclaration (templateCatalog context) declaration
checkDeclarationWith context _ declaration@TemplateTypeDeclaration {} = checkTopDeclaration (templateCatalog context) declaration

-- A method overload is distinguished only by its ordered parameter types.
-- Access, return type, and static-ness intentionally do not rescue duplicate
-- signatures; that matches the declaration rules in Spec/Language/Decls.vxs.
duplicateOverloadProblems :: TemplateContext -> [Declaration ResolvedName ()] -> [Diagnostic]
duplicateOverloadProblems context members = reverse problems
    where
        (_, problems) = foldl inspect ([], []) members
        inspect (seen, diagnostics) declaration@FunctionDeclaration {} =
            let duplicate = any (sameSignature declaration) seen
                currentDiagnostics =
                    if duplicate
                        then
                            [ problem
                                (declarationSpan declaration)
                                "VXT0028"
                                ( "method overload has a duplicate parameter signature: "
                                    ++ identifierText (resolvedSpelling (declarationName declaration))
                                )
                            ]
                        else []
             in (declaration : seen, reverse currentDiagnostics ++ diagnostics)
        inspect state _ = state
        sameSignature current previous =
            resolvedSpelling (declarationName previous) == resolvedSpelling (declarationName current)
                && methodParameterTypes context previous == methodParameterTypes context current
        methodParameterTypes valueContext FunctionDeclaration {declarationParameters = parameters} =
            map (syntaxTypeIn valueContext . parameterTypeSyntax) parameters
        methodParameterTypes _ _ = []

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

-- | How a loop is used, which decides what its @break@ statements carry.
data LoopKind
    = -- | A loop statement: @break@ carries no value.
      StatementLoop
    | {- | A loop used as an expression: every @break@ carries the loop's
      value, typed in the context that receives it.
      -}
      ExpressionLoop (Maybe Type)
    | {- | Not a loop: the edge of a block used as a value. A @break@ or
      @continue@ inside it would have to leave the block before its value
      exists, so neither may cross this edge.
      -}
      ValueBlockEdge

{- | The loops around a statement, innermost first, and the kind of the loop
statement that is about to be checked. A loop expression checks its loop
statement with the pending kind set; every loop moves the pending kind onto
the stack for its own body.
-}
data LoopContext = LoopContext
    { pendingLoop :: LoopKind
    , enclosingLoops :: [LoopKind]
    }

outsideLoops :: LoopContext
outsideLoops = LoopContext StatementLoop []

enterLoop :: LoopContext -> LoopContext
enterLoop loops = LoopContext StatementLoop (pendingLoop loops : enclosingLoops loops)

-- | The context inside a block used as a value, whatever surrounds it.
insideValueBlock :: LoopContext
insideValueBlock = LoopContext StatementLoop [ValueBlockEdge]

{- | The checker for expressions and statements as the branching rules of
"Visual.XSharp.TypeChecker.Branching" receive it: applied to the template
context and to the type that a @return@ must have.
-}
branchChecker :: TemplateContext -> Type -> BranchChecker LoopContext
branchChecker context expected =
    BranchChecker
        { branchExpression = checkExpressionExpectedWith context
        , branchStatements = \environment loops statements ->
            let (Block typed, final, returns, problems) = checkBlockWith context environment expected loops (Block statements)
             in (typed, final, returns, problems)
        , branchType = \syntax -> (syntaxTypeIn context syntax, typeSyntaxProblemsIn context syntax)
        , branchLiteral = literalTypeInContext
        , branchHasEffect = effectCapable
        , branchValueLoops = insideValueBlock
        }

-- | The context of a statement that belongs to a loop header, not its body.
settledLoops :: LoopContext -> LoopContext
settledLoops loops = loops {pendingLoop = StatementLoop}

checkBlockWith ::
    TemplateContext ->
    TypeEnvironment ->
    Type ->
    LoopContext ->
    Block ResolvedName () ->
    (Block ResolvedName Type, TypeEnvironment, [Type], [Diagnostic])
checkBlockWith context environment expected loops (Block statements) =
    let (checked, final, returns, problems) = go environment statements in (Block checked, final, returns, problems)
    where
        go env [] = ([], env, [], [])
        go env (statement : rest) =
            let (typed, next, returned, firstProblems) = checkStatementWith context env expected loops statement
                (remaining, final, laterReturns, laterProblems) = go next rest
             in (typed : remaining, final, returned ++ laterReturns, firstProblems ++ laterProblems)

checkStatementWith ::
    TemplateContext ->
    TypeEnvironment ->
    Type ->
    LoopContext ->
    Statement ResolvedName () ->
    (Statement ResolvedName Type, TypeEnvironment, [Type], [Diagnostic])
checkStatementWith context environment expected loops statement = case statement of
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
        -- The target type is context for the value, as a declared type is
        -- for a binding initializer: it types an otherwise untyped literal.
        let target = lookup (resolvedSymbol name) environment
            targetType = maybe ErrorType fst target
            valueExpected = if targetType == ErrorType then Nothing else Just targetType
            (typedValue, valueType, problems) = checkExpressionExpectedWith context environment valueExpected value
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
            (typedTrue, _, trueReturns, trueProblems) = checkBlockWith context environment expected loops trueBlock
            (typedFalse, falseReturns, falseProblems) = case falseBlock of
                Nothing -> (Nothing, [], [])
                Just value ->
                    let (block, _, returns, problems) = checkBlockWith context environment expected loops value
                     in (Just block, returns, problems)
         in ( IfStatement spanValue typedCondition typedTrue typedFalse
            , environment
            , trueReturns ++ falseReturns
            , conditionProblems ++ conditionMismatch ++ trueProblems ++ falseProblems
            )
    WhileStatement spanValue condition body ->
        let (typedCondition, conditionType, conditionProblems) = checkExpressionWith context environment condition
            conditionProblems' =
                if booleanContextType conditionType
                    then []
                    else [problem spanValue "VXT0020" "while condition must be bool or numeric"]
            (typedBody, _, returns, bodyProblems) = checkBlockWith context environment expected (enterLoop loops) body
         in ( WhileStatement spanValue typedCondition typedBody
            , environment
            , returns
            , conditionProblems ++ conditionProblems' ++ bodyProblems
            )
    DoWhileStatement spanValue body condition ->
        let (typedBody, _, returns, bodyProblems) = checkBlockWith context environment expected (enterLoop loops) body
            (typedCondition, conditionType, conditionProblems) = checkExpressionWith context environment condition
            conditionProblems' =
                if booleanContextType conditionType
                    then []
                    else [problem spanValue "VXT0020" "do/while condition must be bool or numeric"]
         in ( DoWhileStatement spanValue typedBody typedCondition
            , environment
            , returns
            , bodyProblems ++ conditionProblems ++ conditionProblems'
            )
    ForStatement spanValue initializer condition updates body ->
        let (typedInitializer, loopEnvironment, initializerProblems) = case initializer of
                Nothing -> (Nothing, environment, [])
                Just value ->
                    let (typed, nested, _, problems) = checkStatementWith context environment expected (settledLoops loops) value
                     in (Just typed, nested, problems)
            (typedCondition, conditionType, conditionProblems) = case condition of
                Nothing -> (Nothing, boolType, [])
                Just value ->
                    let (typed, valueType, problems) = checkExpressionWith context loopEnvironment value
                     in (Just typed, valueType, problems)
            conditionProblems' =
                if booleanContextType conditionType
                    then []
                    else [problem spanValue "VXT0020" "for condition must be bool or numeric"]
            (typedBody, _, returns, bodyProblems) = checkBlockWith context loopEnvironment expected (enterLoop loops) body
            (typedUpdates, updateProblems) = checkStatementsWith context loopEnvironment expected (enterLoop loops) updates
         in ( ForStatement spanValue typedInitializer typedCondition typedUpdates typedBody
            , environment
            , returns
            , initializerProblems ++ conditionProblems ++ conditionProblems' ++ bodyProblems ++ updateProblems
            )
    ForEachStatement spanValue kind syntax name _ source body ->
        let valueType = syntaxTypeIn context syntax
            (typedSource, _, sourceProblems) = checkExpressionWith context environment source
            (typedBody, _, returns, bodyProblems) =
                checkBlockWith
                    context
                    ((resolvedSymbol name, (valueType, kind == MutableBinding)) : environment)
                    expected
                    (enterLoop loops)
                    body
            unsupported = problem spanValue "VXT0021" "enumerable for loops require the generator and Enumerable ABI, which is not implemented"
         in ( ForEachStatement spanValue kind syntax name valueType typedSource typedBody
            , environment
            , returns
            , unsupported : typeSyntaxProblemsIn context syntax ++ sourceProblems ++ bodyProblems
            )
    IncrementStatement spanValue name _ ->
        let target = lookup (resolvedSymbol name) environment
            targetType = maybe ErrorType fst target
            writableProblems = case target of
                Just (_, False) -> [problem spanValue "VXT0022" "increment cannot modify an immutable binding"]
                Nothing -> [problem spanValue "VXT0023" "increment target is not defined"]
                _ -> []
            numericProblems =
                if isNumericType targetType && targetType /= boolType
                    then []
                    else [problem spanValue "VXT0024" "increment target must have a numeric type"]
         in (IncrementStatement spanValue name targetType, environment, [], writableProblems ++ numericProblems)
    CompoundAssignmentStatement spanValue operator name _ value ->
        -- `target op= value` has the typing of `target = target op value`:
        -- the operator rule is applied to the target type and the result
        -- must be storable without a conversion.
        let target = lookup (resolvedSymbol name) environment
            targetType = maybe ErrorType fst target
            operandExpected = if targetType == ErrorType then Nothing else Just targetType
            (typedValue, valueType, problems) = checkExpressionExpectedWith context environment operandExpected value
            immutable = case target of
                Just (_, False) -> [problem spanValue "VXT0003" "cannot assign to an immutable binding"]
                _ -> []
            rule = binaryNumericRule operator targetType valueType
            known = targetType /= ErrorType && valueType /= ErrorType
            operatorProblems = if known then ruleProblems spanValue "VXT0012" rule else []
            resultProblems =
                if known && null operatorProblems && numericRuleType rule /= targetType
                    then [problem spanValue "VXT0035" "compound assignment result does not have the target type"]
                    else []
         in ( CompoundAssignmentStatement spanValue operator name targetType typedValue
            , environment
            , []
            , problems ++ immutable ++ operatorProblems ++ resultProblems
            )
    DiscardStatement spanValue value ->
        let (typedValue, _, problems) = checkExpressionWith context environment value
         in (DiscardStatement spanValue typedValue, environment, [], problems)
    BreakStatement spanValue value ->
        -- A break leaves the innermost loop. Whether it may, or must, carry
        -- a value is decided by how that loop is used; the value takes its
        -- context from the place that receives the loop's value.
        let innermost = case enclosingLoops loops of
                kind : _ -> Just kind
                [] -> Nothing
            valueExpected = case innermost of
                Just (ExpressionLoop target) -> target
                _ -> Nothing
            (typedValue, _, valueProblems) = checkOptionalExpectedWith context environment valueExpected value
            placementProblems = case (innermost, value) of
                (Nothing, _) -> [problem spanValue "VXT0025" "break is only valid inside a loop"]
                (Just StatementLoop, Just _) ->
                    [problem spanValue "VXT0026" "a value-carrying break is only valid in a loop used as an expression"]
                (Just (ExpressionLoop _), Nothing) ->
                    [problem spanValue "VXT0040" "a loop used as an expression must be left by a break that carries a value"]
                (Just ValueBlockEdge, _) ->
                    [problem spanValue "VXT0059" "leaving a block that is used as a value with break is not implemented"]
                _ -> []
         in (BreakStatement spanValue typedValue, environment, [], placementProblems ++ valueProblems)
    ContinueStatement spanValue ->
        let problems = case enclosingLoops loops of
                [] -> [problem spanValue "VXT0027" "continue is only valid inside a loop"]
                ValueBlockEdge : _ ->
                    [problem spanValue "VXT0059" "leaving a block that is used as a value with continue is not implemented"]
                _ -> []
         in (ContinueStatement spanValue, environment, [], problems)
    GuardStatement spanValue condition block ->
        let (typedCondition, conditionType, conditionProblems) = checkExpressionWith context environment condition
            conditionMismatch =
                [ problem spanValue "VXT0060" "guard condition must be bool or numeric"
                | conditionType /= ErrorType
                , not (booleanContextType conditionType)
                ]
            (typedBlock, _, returns, blockProblems) = checkBlockWith context environment expected loops block
         in ( GuardStatement spanValue typedCondition typedBlock
            , environment
            , returns
            , conditionProblems ++ conditionMismatch ++ blockProblems ++ guardBlockProblems spanValue typedBlock
            )
    -- A nested block is checked in the scope it starts in; what it declares
    -- does not reach the statements after it.
    BlockStatement spanValue block ->
        let (typedBlock, _, returns, problems) = checkBlockWith context environment expected loops block
         in (BlockStatement spanValue typedBlock, environment, returns, problems)
    -- A match that is a statement of its own: its arms are statements, they
    -- may return and may leave the enclosing loop, and no arm has to accept.
    ExpressionStatement spanValue (MatchExpression matchSpan subjects arms _) terminated
        | terminated ->
            let (typedMatch, _, returns, problems) =
                    checkMatch (branchChecker context expected) (MatchStatement loops) environment Nothing matchSpan subjects arms
             in (ExpressionStatement spanValue typedMatch terminated, environment, returns, problems)
    ExpressionStatement spanValue value terminated ->
        let (typedValue, _, problems) = checkExpressionWith context environment value
            effectProblems =
                if terminated && not (effectCapable value)
                    then [problem spanValue "VXT0013" "pure value expression cannot be used as a statement"]
                    else []
         in (ExpressionStatement spanValue typedValue terminated, environment, [], problems ++ effectProblems)

checkStatementsWith ::
    TemplateContext ->
    TypeEnvironment ->
    Type ->
    LoopContext ->
    [Statement ResolvedName ()] ->
    ([Statement ResolvedName Type], [Diagnostic])
checkStatementsWith context environment expected loops = go environment
    where
        go _ [] = ([], [])
        go current (statement : remaining) =
            let (typed, next, _, problems) = checkStatementWith context current expected loops statement
                (later, laterProblems) = go next remaining
             in (typed : later, problems ++ laterProblems)

finalExpressionType :: Block ResolvedName Type -> Maybe Type
finalExpressionType (Block statements) = case reverse statements of
    ExpressionStatement _ expression False : _ -> Just (typedExpressionType expression)
    _ -> Nothing

typedExpressionType :: Expression name Type -> Type
typedExpressionType expression = case expression of
    NameExpression _ _ valueType -> valueType
    LiteralExpression _ _ valueType -> valueType
    MemberAccessExpression _ _ _ valueType -> valueType
    CallExpression _ _ _ valueType -> valueType
    UnaryExpression _ _ _ valueType -> valueType
    BinaryExpression _ _ _ _ valueType -> valueType
    IsPatternExpression _ _ _ valueType -> valueType
    ConditionalExpression _ _ _ _ valueType -> valueType
    CoalesceExpression _ _ _ valueType -> valueType
    AssignmentExpression _ _ _ _ valueType -> valueType
    IncrementExpression _ _ _ valueType -> valueType
    LoopExpression _ _ valueType -> valueType
    BlockExpression _ _ valueType -> valueType
    MatchExpression _ _ _ valueType -> valueType
    CallableExpression _ _ _ _ _ valueType -> valueType

effectCapable :: Expression name annotation -> Bool
effectCapable CallExpression {} = True
effectCapable (IsPatternExpression _ subject _ _) = effectCapable subject
effectCapable (ConditionalExpression _ condition first second _) = any effectCapable [condition, first, second]
effectCapable (CoalesceExpression _ left fallback _) = effectCapable left || effectCapable fallback
effectCapable AssignmentExpression {} = True
effectCapable IncrementExpression {} = True
effectCapable LoopExpression {} = True
effectCapable (BlockExpression _ (Block statements) _) = not (null statements)
effectCapable (MatchExpression _ subjects arms _) =
    any effectCapable (subjects ++ concatMap matchArmExpressions arms)
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
    MemberAccessExpression spanValue receiver member _ ->
        let (typedReceiver, _, receiverProblems) = checkExpressionWith context environment receiver
            memberProblems = [problem spanValue "VXT0034" "member selection is currently supported only as a type-qualified method call"]
         in (MemberAccessExpression spanValue typedReceiver member ErrorType, ErrorType, receiverProblems ++ memberProblems)
    CallExpression spanValue callee arguments _ ->
        case callee of
            MemberAccessExpression _ receiver member _ ->
                checkTypeQualifiedCall context environment expected spanValue receiver member arguments
            NameExpression calleeSpan name _
                | Just owner <- memberOwnerForSymbol context (resolvedSymbol name)
                , Just owner == templateCurrentType context ->
                    checkMemberOverloadCall
                        context
                        environment
                        expected
                        spanValue
                        calleeSpan
                        (overloadsFor context owner (resolvedSpelling name))
                        arguments
                        False
            _ -> checkOrdinaryCall context environment spanValue callee arguments
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
        let leftResult = case (operator, expected) of
                (FloorDivide, Just target)
                    | isIntegerType target ->
                        let inferred@(_, inferredType, _) = checkExpressionWith context environment left
                         in if isIntegerType inferredType
                                then checkExpressionExpectedWith context environment (Just target) left
                                else inferred
                (FloorDivide, _) -> checkExpressionWith context environment left
                _ -> checkExpressionExpectedWith context environment (if booleanResult operator then Nothing else expected) left
            (typedLeft, leftType, leftProblems) = leftResult
            rightExpected = if operator `elem` [LogicalAnd, LogicalOr] then Nothing else Just leftType
            (typedRight, rightType, rightProblems) = checkExpressionExpectedWith context environment rightExpected right
            rule = binaryNumericRule operator leftType rightType
            resultType = numericRuleType rule
            mismatch = ruleProblems spanValue "VXT0012" rule
         in ( BinaryExpression spanValue operator typedLeft typedRight resultType
            , resultType
            , leftProblems ++ rightProblems ++ mismatch
            )
    IsPatternExpression spanValue subject patternValue _ ->
        let (typedSubject, subjectType, subjectProblems) = checkExpressionWith context environment subject
            (typedPattern, patternProblems) = checkPatternWith context subjectType patternValue
         in ( IsPatternExpression spanValue typedSubject typedPattern boolType
            , boolType
            , subjectProblems ++ patternProblems
            )
    ConditionalExpression spanValue condition first second _ ->
        let (typedCondition, conditionType, conditionProblems) = checkExpressionWith context environment condition
            conditionMismatch =
                [ problem (sourceSpanOf condition) "VXT0036" "conditional test must be bool or numeric"
                | conditionType /= ErrorType
                , not (booleanContextType conditionType)
                ]
            ((typedFirst, firstType, firstProblems), (typedSecond, secondType, secondProblems)) =
                checkOperandPair context environment expected first second
            (resultType, resultProblems) =
                selectedValueType spanValue "VXT0037" "conditional results must have the same type" firstType secondType
         in ( ConditionalExpression spanValue typedCondition typedFirst typedSecond resultType
            , resultType
            , conditionProblems ++ conditionMismatch ++ firstProblems ++ secondProblems ++ resultProblems
            )
    CoalesceExpression spanValue left fallback _ ->
        let ((typedLeft, leftType, leftProblems), (typedFallback, fallbackType, fallbackProblems)) =
                checkOperandPair context environment expected left fallback
            (resultType, resultProblems) =
                selectedValueType
                    spanValue
                    "VXT0038"
                    "truthy coalescing operands must have the same type"
                    leftType
                    fallbackType
         in ( CoalesceExpression spanValue typedLeft typedFallback resultType
            , resultType
            , leftProblems ++ fallbackProblems ++ resultProblems
            )
    -- An assignment used as a value has the typing of its statement form and
    -- yields the stored value, so its type is the target type. The context
    -- that receives the value does not flow into the right operand: the
    -- target alone decides what may be stored.
    AssignmentExpression spanValue Nothing name value _ ->
        let target = lookup (resolvedSymbol name) environment
            targetType = maybe ErrorType fst target
            valueExpected = if targetType == ErrorType then Nothing else Just targetType
            (typedValue, valueType, problems) = checkExpressionExpectedWith context environment valueExpected value
            immutable = immutableTargetProblems spanValue target
            mismatch =
                [problem spanValue "VXT0004" "assignment value has the wrong type" | not (compatible targetType valueType)]
         in ( AssignmentExpression spanValue Nothing name typedValue targetType
            , targetType
            , problems ++ immutable ++ mismatch
            )
    AssignmentExpression spanValue (Just operator) name value _ ->
        let target = lookup (resolvedSymbol name) environment
            targetType = maybe ErrorType fst target
            operandExpected = if targetType == ErrorType then Nothing else Just targetType
            (typedValue, valueType, problems) = checkExpressionExpectedWith context environment operandExpected value
            immutable = immutableTargetProblems spanValue target
            rule = binaryNumericRule operator targetType valueType
            known = targetType /= ErrorType && valueType /= ErrorType
            operatorProblems = if known then ruleProblems spanValue "VXT0012" rule else []
            resultProblems =
                [ problem spanValue "VXT0035" "compound assignment result does not have the target type"
                | known
                , null operatorProblems
                , numericRuleType rule /= targetType
                ]
         in ( AssignmentExpression spanValue (Just operator) name typedValue targetType
            , targetType
            , problems ++ immutable ++ operatorProblems ++ resultProblems
            )
    IncrementExpression spanValue isPrefix name _ ->
        let target = lookup (resolvedSymbol name) environment
            targetType = maybe ErrorType fst target
            writableProblems = case target of
                Just (_, False) -> [problem spanValue "VXT0022" "increment cannot modify an immutable binding"]
                Nothing -> [problem spanValue "VXT0023" "increment target is not defined"]
                _ -> []
            numericProblems =
                [ problem spanValue "VXT0024" "increment target must have a numeric type"
                | not (isNumericType targetType && targetType /= boolType)
                ]
         in ( IncrementExpression spanValue isPrefix name targetType
            , targetType
            , writableProblems ++ numericProblems
            )
    -- A loop used as an expression yields the operand of the break that
    -- leaves it. It must not be able to end any other way, so its condition
    -- is the constant true, or absent in a `for`, and every break that
    -- leaves it carries a value.
    LoopExpression spanValue loop _ ->
        let loops = LoopContext (ExpressionLoop expected) []
            (typedLoop, _, _, loopProblems) = checkStatementWith context environment ErrorType loops loop
            (resultType, resultProblems) = loopValueType spanValue (loopBreakTypes typedLoop)
            endProblems =
                [ problem
                    spanValue
                    "VXT0041"
                    "a loop used as an expression can end without a value; its condition must be the constant true"
                | loopMayEndWithoutValue typedLoop
                ]
            returnProblems =
                [ problem spanValue "VXT0045" "return inside a loop used as an expression is not supported"
                | statementReturns typedLoop
                ]
         in ( LoopExpression spanValue typedLoop resultType
            , resultType
            , loopProblems ++ endProblems ++ returnProblems ++ resultProblems
            )
    BlockExpression spanValue block _ ->
        checkValueBlock (branchChecker context ErrorType) environment expected spanValue block
    MatchExpression spanValue subjects arms _ ->
        let (typedMatch, resultType, _, problems) =
                checkMatch (branchChecker context ErrorType) MatchValue environment expected spanValue subjects arms
         in (typedMatch, resultType, problems)
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

{- | Types of the values carried by the breaks that leave this loop itself.
Breaks of nested loops leave those loops, so nested loops are not entered.
-}
loopBreakTypes :: Statement ResolvedName Type -> [Type]
loopBreakTypes loop = case loop of
    WhileStatement _ _ body -> blockBreaks body
    ForStatement _ _ _ _ body -> blockBreaks body
    _ -> []
    where
        blockBreaks (Block statements) = concatMap statementBreaks statements
        statementBreaks statement = case statement of
            BreakStatement _ (Just value) -> [typedExpressionType value]
            IfStatement _ _ trueBlock falseBlock -> blockBreaks trueBlock ++ maybe [] blockBreaks falseBlock
            GuardStatement _ _ block -> blockBreaks block
            BlockStatement _ block -> blockBreaks block
            ExpressionStatement _ (MatchExpression _ _ arms _) _ ->
                concat [blockBreaks block | BlockExpression _ block _ <- map matchArmBody arms]
            _ -> []

{- | Result type of a loop expression from the types of its break values.

The value is materialized in one storage slot, like the result of a
conditional expression, so all break values have one type, and only bool and
numeric results are lowered today.
-}
loopValueType :: SourceSpan -> [Type] -> (Type, [Diagnostic])
loopValueType spanValue breakTypes = case filter (/= ErrorType) breakTypes of
    [] ->
        ( ErrorType
        , [problem spanValue "VXT0042" "a loop used as an expression has no break that carries a value" | null breakTypes]
        )
    first : remaining
        | any (/= first) remaining ->
            (first, [problem spanValue "VXT0043" "the break values of a loop used as an expression must have the same type"])
        | not (booleanContextType first) ->
            (first, [problem spanValue "VXT0044" "loop expressions currently support only bool and numeric results"])
        | otherwise -> (first, [])

-- | Whether the loop can finish by its condition becoming false.
loopMayEndWithoutValue :: Statement ResolvedName Type -> Bool
loopMayEndWithoutValue loop = case loop of
    WhileStatement _ condition _ -> not (isConstantTrue condition)
    ForStatement _ _ condition _ _ -> maybe False (not . isConstantTrue) condition
    _ -> True
    where
        isConstantTrue expression = case expression of
            LiteralExpression _ (BooleanLiteral True) _ -> True
            _ -> False

-- | Whether a statement contains a return outside any closure.
statementReturns :: Statement name annotation -> Bool
statementReturns statement = case statement of
    ReturnStatement {} -> True
    IfStatement _ _ trueBlock falseBlock -> blockReturns trueBlock || maybe False blockReturns falseBlock
    WhileStatement _ _ body -> blockReturns body
    DoWhileStatement _ body _ -> blockReturns body
    ForStatement _ initializer _ updates body ->
        maybe False statementReturns initializer || any statementReturns updates || blockReturns body
    ForEachStatement _ _ _ _ _ _ body -> blockReturns body
    GuardStatement _ _ block -> blockReturns block
    BlockStatement _ block -> blockReturns block
    ExpressionStatement _ (MatchExpression _ _ arms _) _ ->
        or [blockReturns block | BlockExpression _ block _ <- map matchArmBody arms]
    _ -> False
    where
        blockReturns (Block statements) = any statementReturns statements

immutableTargetProblems :: SourceSpan -> Maybe (Type, Bool) -> [Diagnostic]
immutableTargetProblems spanValue target = case target of
    Just (_, False) -> [problem spanValue "VXT0003" "cannot assign to an immutable binding"]
    _ -> []

{- | Check the two value operands of a conditional form exactly once each.

An operand made only of untyped numeric literals takes its type from the
other operand, in either direction, so @flag ? 1 : wide@ selects the type of
@wide@ just as @flag ? wide : 1@ does. The order is chosen from syntax
before either operand is checked; checking an operand twice would make the
cost exponential in the nesting depth of chained conditionals.
-}
checkOperandPair ::
    TemplateContext ->
    TypeEnvironment ->
    Maybe Type ->
    Expression ResolvedName () ->
    Expression ResolvedName () ->
    ( (Expression ResolvedName Type, Type, [Diagnostic])
    , (Expression ResolvedName Type, Type, [Diagnostic])
    )
checkOperandPair context environment expected first second
    | expected == Nothing && takesContextualType first && not (takesContextualType second) =
        let secondResult@(_, secondType, _) = checkExpressionWith context environment second
         in (checkExpressionExpectedWith context environment (contextFrom secondType) first, secondResult)
    | otherwise =
        let firstResult@(_, firstType, _) = checkExpressionExpectedWith context environment expected first
            secondExpected = maybe (contextFrom firstType) Just expected
         in (firstResult, checkExpressionExpectedWith context environment secondExpected second)
    where
        contextFrom valueType = if valueType == ErrorType then Nothing else Just valueType

-- | Whether an expression consists only of numeric literals and arithmetic.
takesContextualType :: Expression name annotation -> Bool
takesContextualType expression = case expression of
    LiteralExpression _ (IntegerLiteral _) _ -> True
    LiteralExpression _ (FloatingLiteral _) _ -> True
    UnaryExpression _ operator value _ -> operator /= LogicalNot && takesContextualType value
    BinaryExpression _ operator left right _ ->
        not (booleanResult operator) && takesContextualType left && takesContextualType right
    _ -> False

{- | Result type shared by the two value operands of a conditional form.

The result is materialized in one storage slot, so both operands must have
the same type. Only bool and numeric results are lowered today; owned values
need move and release rules for the slot that the backend does not have yet.
-}
selectedValueType :: SourceSpan -> String -> String -> Type -> Type -> (Type, [Diagnostic])
selectedValueType spanValue mismatchCode mismatchMessage firstType secondType
    | firstType == ErrorType = (secondType, [])
    | secondType == ErrorType = (firstType, [])
    | firstType /= secondType = (firstType, [problem spanValue mismatchCode mismatchMessage])
    | not (booleanContextType firstType) =
        ( firstType
        , [problem spanValue "VXT0039" "conditional expressions currently support only bool and numeric results"]
        )
    | otherwise = (firstType, [])

checkOrdinaryCall ::
    TemplateContext ->
    TypeEnvironment ->
    SourceSpan ->
    Expression ResolvedName () ->
    [Expression ResolvedName ()] ->
    (Expression ResolvedName Type, Type, [Diagnostic])
checkOrdinaryCall context environment spanValue callee arguments =
    let (typedCallee, calleeType, calleeProblems) = checkExpressionExpectedWith context environment Nothing callee
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

typeQualifiedReceiver :: Expression ResolvedName () -> Maybe ResolvedName
typeQualifiedReceiver (NameExpression _ name _) = Just name
typeQualifiedReceiver _ = Nothing

memberOwnerForSymbol :: TemplateContext -> SymbolId -> Maybe SymbolId
memberOwnerForSymbol context symbol =
    candidateOwner <$> firstMatch
    where
        firstMatch = findCandidate (catalogMethods (templateCatalog context))
        findCandidate [] = Nothing
        findCandidate (candidate : remaining)
            | resolvedSymbol (declarationName (candidateDeclaration candidate)) == symbol = Just candidate
            | otherwise = findCandidate remaining

overloadsFor :: TemplateContext -> SymbolId -> Identifier -> [MethodCandidate]
overloadsFor context owner name =
    [ candidate
    | candidate <- catalogMethods (templateCatalog context)
    , candidateOwner candidate == owner
    , resolvedSpelling (declarationName (candidateDeclaration candidate)) == name
    ]

checkTypeQualifiedCall ::
    TemplateContext ->
    TypeEnvironment ->
    Maybe Type ->
    SourceSpan ->
    Expression ResolvedName () ->
    Identifier ->
    [Expression ResolvedName ()] ->
    (Expression ResolvedName Type, Type, [Diagnostic])
checkTypeQualifiedCall context environment expected callSpan receiver member arguments =
    case typeQualifiedReceiver receiver of
        Just typeName
            | let owner = resolvedSymbol typeName
            , owner `elem` map fst (catalogTypes (templateCatalog context)) ->
                checkMemberOverloadCall
                    context
                    environment
                    expected
                    callSpan
                    (sourceSpanOf receiver)
                    (overloadsFor context owner member)
                    arguments
                    True
        _ ->
            let (typedReceiver, _, receiverProblems) = checkExpressionWith context environment receiver
                argumentsChecked = map (checkExpressionWith context environment) arguments
                typedArguments = [value | (value, _, _) <- argumentsChecked]
                diagnostics =
                    receiverProblems
                        ++ concat [problems | (_, _, problems) <- argumentsChecked]
                        ++ [problem callSpan "VXT0032" "the left side of a static member call must name a declared type"]
             in ( CallExpression
                    callSpan
                    (MemberAccessExpression callSpan typedReceiver member ErrorType)
                    typedArguments
                    ErrorType
                , ErrorType
                , diagnostics
                )

sourceSpanOf :: Expression name annotation -> SourceSpan
sourceSpanOf expression = case expression of
    NameExpression spanValue _ _ -> spanValue
    LiteralExpression spanValue _ _ -> spanValue
    MemberAccessExpression spanValue _ _ _ -> spanValue
    CallExpression spanValue _ _ _ -> spanValue
    UnaryExpression spanValue _ _ _ -> spanValue
    BinaryExpression spanValue _ _ _ _ -> spanValue
    IsPatternExpression spanValue _ _ _ -> spanValue
    ConditionalExpression spanValue _ _ _ _ -> spanValue
    CoalesceExpression spanValue _ _ _ -> spanValue
    AssignmentExpression spanValue _ _ _ _ -> spanValue
    IncrementExpression spanValue _ _ _ -> spanValue
    LoopExpression spanValue _ _ -> spanValue
    BlockExpression spanValue _ _ -> spanValue
    MatchExpression spanValue _ _ _ -> spanValue
    CallableExpression spanValue _ _ _ _ _ -> spanValue

checkMemberOverloadCall ::
    TemplateContext ->
    TypeEnvironment ->
    Maybe Type ->
    SourceSpan ->
    SourceSpan ->
    [MethodCandidate] ->
    [Expression ResolvedName ()] ->
    Bool ->
    (Expression ResolvedName Type, Type, [Diagnostic])
checkMemberOverloadCall context environment _ callSpan calleeSpan candidates arguments requireStatic =
    let callableCandidates = if requireStatic then filter candidateIsStatic candidates else candidates
        visibleCandidates = filter (candidateVisibleFrom context) callableCandidates
        candidateAttempts = map attempt visibleCandidates
        viable = [value | value@(_, _, _, True) <- candidateAttempts]
        arityMatches = filter candidateArityMatches visibleCandidates
        failureCode
            | null candidates = "VXT0029"
            | requireStatic && null callableCandidates = "VXT0031"
            | null visibleCandidates = "VXT0033"
            | null arityMatches = "VXT0008"
            | otherwise = "VXT0009"
        failureMessage
            | null candidates = "no method with this name is declared on the selected type"
            | requireStatic && null callableCandidates = "an instance method cannot be called through a type name"
            | null visibleCandidates = "the selected method is not accessible from this declaration"
            | null arityMatches = "call argument count does not match any overload"
            | otherwise = "call argument types do not match any overload"
        (selected, resultType, typedArguments, diagnostics) = case viable of
            [(candidate, valueType, checked, _)] -> (Just candidate, valueType, checked, [])
            [] ->
                ( Nothing
                , ErrorType
                , map (\argument -> fst3 (checkExpressionWith context environment argument)) arguments
                , [problem callSpan failureCode failureMessage]
                )
            _ ->
                ( Nothing
                , ErrorType
                , map (\argument -> fst3 (checkExpressionWith context environment argument)) arguments
                , [problem callSpan "VXT0030" "the call is ambiguous between multiple equally viable overloads"]
                )
        typedCallee = case selected of
            Just candidate ->
                let declaration = candidateDeclaration candidate
                 in NameExpression calleeSpan (declarationName declaration) (signature context declaration)
            Nothing -> case visibleCandidates of
                candidate : _ ->
                    let declaration = candidateDeclaration candidate
                     in NameExpression calleeSpan (declarationName declaration) (signature context declaration)
                [] -> NameExpression calleeSpan (ResolvedName (SymbolId (-1)) (Identifier "<unresolved-member>")) ErrorType
     in (CallExpression callSpan typedCallee typedArguments resultType, resultType, diagnostics)
    where
        attempt candidate =
            let declaration = candidateDeclaration candidate
                functionType = signature context declaration
                (parameters, resultType) = case functionType of
                    FunctionType types result -> (types, result)
                    _ -> ([], ErrorType)
                checkedArguments =
                    zipWith
                        (\index argument -> checkExpressionExpectedWith context environment (safeIndex parameters index) argument)
                        [0 ..]
                        arguments
                argumentTypes = [valueType | (_, valueType, _) <- checkedArguments]
                problems = concat [nested | (_, _, nested) <- checkedArguments]
                matching = length parameters == length arguments && null problems && and (zipWith compatible parameters argumentTypes)
             in (candidate, resultType, [value | (value, _, _) <- checkedArguments], matching)
        candidateIsStatic (MethodCandidate _ FunctionDeclaration {declarationIsStatic = isStatic}) = isStatic
        candidateIsStatic _ = False
        candidateArityMatches candidate = case candidateDeclaration candidate of
            FunctionDeclaration {declarationParameters = parameters} -> length parameters == length arguments
            _ -> False

candidateVisibleFrom :: TemplateContext -> MethodCandidate -> Bool
candidateVisibleFrom context (MethodCandidate owner FunctionDeclaration {declarationAccess = access}) =
    case access of
        PrivateAccess -> templateCurrentType context == Just owner
        ProtectedAccess -> templateCurrentType context == Just owner
        _ -> True
candidateVisibleFrom _ _ = False

fst3 :: (a, b, c) -> a
fst3 (first, _, _) = first

-- A pattern is checked against the already typed subject. This keeps literal
-- inference deterministic and makes the later decision-tree lowering free of
-- source-level conversion guesses.
checkPatternWith :: TemplateContext -> Type -> Pattern ResolvedName () -> (Pattern ResolvedName Type, [Diagnostic])
checkPatternWith context subjectType patternValue = case patternValue of
    WildcardPattern spanValue _ -> (WildcardPattern spanValue subjectType, [])
    NullPattern spanValue _ ->
        let problems =
                if isReferenceType subjectType
                    then []
                    else [problem spanValue "VXT0020" "null pattern requires an AARC reference subject"]
         in (NullPattern spanValue subjectType, problems)
    LiteralPattern spanValue literal _ ->
        let (literalType, literalProblems) = literalTypeInContext spanValue (Just subjectType) literal
            rule = binaryNumericRule Equal subjectType literalType
            problems = literalProblems ++ ruleProblems spanValue "VXT0021" rule
         in (LiteralPattern spanValue literal literalType, problems)
    TypePattern spanValue syntax _ ->
        let targetType = syntaxTypeIn context syntax
            syntaxProblems = typeSyntaxProblemsIn context syntax
            possible =
                targetType /= ErrorType
                    && subjectType /= ErrorType
                    && (compatible subjectType targetType || isReferenceType subjectType && isReferenceType targetType)
            relationProblems =
                if possible
                    then []
                    else [problem spanValue "VXT0022" "type pattern can never match the subject type"]
         in (TypePattern spanValue syntax targetType, syntaxProblems ++ relationProblems)
    RelationalPattern spanValue operator literal _ ->
        let (literalType, literalProblems) = literalTypeInContext spanValue (Just subjectType) literal
            binary = relationalPatternBinary operator
            rule = binaryNumericRule binary subjectType literalType
            problems = literalProblems ++ ruleProblems spanValue "VXT0023" rule
         in (RelationalPattern spanValue operator literal literalType, problems)
    NotPattern spanValue nested _ ->
        let (typed, problems) = checkPatternWith context subjectType nested
         in (NotPattern spanValue typed boolType, problems)
    AndPattern spanValue left right _ -> checkPatternPair AndPattern spanValue left right
    OrPattern spanValue left right _ -> checkPatternPair OrPattern spanValue left right
    where
        checkPatternPair constructor spanValue left right =
            let (typedLeft, leftProblems) = checkPatternWith context subjectType left
                (typedRight, rightProblems) = checkPatternWith context subjectType right
             in (constructor spanValue typedLeft typedRight boolType, leftProblems ++ rightProblems)

relationalPatternBinary :: RelationalPatternOperator -> BinaryOperator
relationalPatternBinary operator = case operator of
    PatternLessThan -> LessThan
    PatternLessEqual -> LessEqual
    PatternGreaterThan -> GreaterThan
    PatternGreaterEqual -> GreaterEqual
    PatternEqual -> Equal
    PatternNotEqual -> NotEqual

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

-- String and callable values are AARC references. Every canonical scalar is a
-- value, not merely the handful historically accepted by closure tests. Named
-- user and library types remain conservative until resolved declaration
-- metadata connects the ownership catalog to this check.
isReferenceType :: Type -> Bool
isReferenceType valueType = case valueType of
    FunctionType _ _ -> True
    _ | typeToScalarType valueType /= Nothing -> False
    NamedType _ _ -> valueType /= unitType && valueType /= voidType
    _ -> False

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
        let (typed, _, returns, problems) = checkBlockWith context environment ErrorType outsideLoops block
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
