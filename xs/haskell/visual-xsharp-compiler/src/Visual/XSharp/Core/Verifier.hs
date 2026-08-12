-- SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
-- SPDX-License-Identifier: MPL-2.0

module Visual.XSharp.Core.Verifier (verifyCore) where

import Data.List (group, sort)
import Data.Map.Strict qualified as Map
import Visual.XSharp.AST
import Visual.XSharp.Core
import Visual.XSharp.Diagnostic

type Environment = Map.Map SymbolId (Type, Bool)

verifyCore :: CoreModule -> Either [Diagnostic] CoreModule
verifyCore moduleValue =
    case moduleProblems moduleValue of
        [] -> Right moduleValue
        problems -> Left problems

moduleProblems :: CoreModule -> [Diagnostic]
moduleProblems moduleValue =
    emptyName (coreModuleName moduleValue)
        ++ duplicates "VXC1002" "duplicate Core function symbol" functionSymbols
        ++ concatMap (verifyFunction functionEnvironment) (coreModuleFunctions moduleValue)
    where
        functions = coreModuleFunctions moduleValue
        functionSymbols = map (resolvedSymbol . coreFunctionName) functions
        functionEnvironment =
            Map.fromList
                [ ( resolvedSymbol (coreFunctionName function)
                  , (FunctionType (map snd (coreFunctionParameters function)) (coreFunctionReturnType function), False)
                  )
                | function <- functions
                ]

verifyFunction :: Environment -> CoreFunction -> [Diagnostic]
verifyFunction functionEnvironment function =
    unresolvedType "VXC1003" "Core function has an unresolved return type" (coreFunctionReturnType function)
        ++ duplicates "VXC1004" "duplicate Core parameter symbol" parameterSymbols
        ++ concatMap (uncurry verifyParameter) (coreFunctionParameters function)
        ++ fst (verifyStatements initialEnvironment (coreFunctionReturnType function) (coreFunctionBody function))
        ++ missingReturn
    where
        parameterSymbols = map (resolvedSymbol . fst) (coreFunctionParameters function)
        initialEnvironment =
            Map.union
                (Map.fromList [(resolvedSymbol name, (valueType, False)) | (name, valueType) <- coreFunctionParameters function])
                functionEnvironment
        missingReturn =
            [ problem "VXC1005" "non-void Core function may complete without returning a value"
            | coreFunctionReturnType function /= unitType && not (statementsAlwaysReturn (coreFunctionBody function))
            ]

verifyParameter :: ResolvedName -> Type -> [Diagnostic]
verifyParameter name valueType =
    invalidSymbol "VXC1006" "Core parameter symbol must be positive" name
        ++ unresolvedType "VXC1007" "Core parameter has an unresolved type" valueType

verifyStatements :: Environment -> Type -> [CoreStatement] -> ([Diagnostic], Environment)
verifyStatements environment _ [] = ([], environment)
verifyStatements environment returnType (statement : remaining) =
    let (currentProblems, nextEnvironment) = verifyStatement environment returnType statement
        (remainingProblems, finalEnvironment) = verifyStatements nextEnvironment returnType remaining
     in (currentProblems ++ remainingProblems, finalEnvironment)

verifyStatement :: Environment -> Type -> CoreStatement -> ([Diagnostic], Environment)
verifyStatement environment returnType statement = case statement of
    CoreBind binding ->
        let name = coreBindingName binding
            symbol = resolvedSymbol name
            declaredType = coreBindingType binding
            expressionProblems = verifyExpression environment (coreBindingValue binding)
            bindingProblems =
                invalidSymbol "VXC1008" "Core binding symbol must be positive" name
                    ++ unresolvedType "VXC1009" "Core binding has an unresolved type" declaredType
                    ++ [problem "VXC1010" "Core binding symbol is already defined" | Map.member symbol environment]
                    ++ typeMismatch
                        "VXC1011"
                        "Core binding value type does not match its declaration"
                        declaredType
                        (expressionType (coreBindingValue binding))
            next = Map.insert symbol (declaredType, coreBindingMutable binding) environment
         in (bindingProblems ++ expressionProblems, next)
    CoreAssign name value ->
        let symbol = resolvedSymbol name
            target = Map.lookup symbol environment
            targetProblems = case target of
                Nothing -> [problem "VXC1012" "Core assignment targets an undefined symbol"]
                Just (_, False) -> [problem "VXC1013" "Core assignment targets an immutable symbol"]
                Just (targetType, True) ->
                    typeMismatch
                        "VXC1014"
                        "Core assignment value has the wrong type"
                        targetType
                        (expressionType value)
         in ( invalidSymbol "VXC1015" "Core assignment symbol must be positive" name
                ++ verifyExpression environment value
                ++ targetProblems
            , environment
            )
    CoreReturn value ->
        ( verifyExpression environment value
            ++ typeMismatch "VXC1016" "Core return value has the wrong type" returnType (expressionType value)
        , environment
        )
    CoreIf condition trueBranch falseBranch ->
        let conditionProblems =
                verifyExpression environment condition
                    ++ typeMismatch "VXC1017" "Core condition must be bool" boolType (expressionType condition)
            (trueProblems, _) = verifyStatements environment returnType trueBranch
            (falseProblems, _) = verifyStatements environment returnType falseBranch
         in (conditionProblems ++ trueProblems ++ falseProblems, environment)
    CoreEvaluate value -> (verifyExpression environment value, environment)

verifyExpression :: Environment -> CoreExpression -> [Diagnostic]
verifyExpression environment expression =
    unresolvedType "VXC1018" "Core expression has an unresolved type" (expressionType expression)
        ++ case expression of
            CoreVariable name valueType ->
                invalidSymbol "VXC1019" "Core variable symbol must be positive" name
                    ++ case Map.lookup (resolvedSymbol name) environment of
                        Nothing -> [problem "VXC1020" "Core expression references an undefined symbol"]
                        Just (declaredType, _) ->
                            typeMismatch
                                "VXC1021"
                                "Core variable type disagrees with its definition"
                                declaredType
                                valueType
            CoreLiteral literal valueType -> literalProblems literal valueType
            CoreApply callee arguments valueType ->
                verifyExpression environment callee
                    ++ concatMap (verifyExpression environment) arguments
                    ++ callProblems callee arguments valueType
            CorePrimitive primitive arguments valueType ->
                concatMap (verifyExpression environment) arguments ++ primitiveProblems primitive arguments valueType

callProblems :: CoreExpression -> [CoreExpression] -> Type -> [Diagnostic]
callProblems callee arguments resultType = case expressionType callee of
    FunctionType parameterTypes declaredResult ->
        [problem "VXC1022" "Core call has the wrong argument count" | length parameterTypes /= length arguments]
            ++ concat
                (zipWith (typeMismatch "VXC1023" "Core call argument has the wrong type") parameterTypes (map expressionType arguments))
            ++ typeMismatch "VXC1024" "Core call result type disagrees with the callee" declaredResult resultType
    _ -> [problem "VXC1025" "Core call target is not a function"]

primitiveProblems :: CorePrimitive -> [CoreExpression] -> Type -> [Diagnostic]
primitiveProblems primitive arguments resultType =
    [problem "VXC1026" "Core primitive has the wrong operand count" | length arguments /= arity]
        ++ concatMap (typeMismatch "VXC1027" "Core primitive operand has the wrong type" operandType . expressionType) arguments
        ++ typeMismatch "VXC1028" "Core primitive result has the wrong type" expectedResult resultType
    where
        unary = primitive `elem` [CoreNegate, CoreLogicalNot]
        logical = primitive `elem` [CoreLogicalAnd, CoreLogicalOr, CoreLogicalNot]
        comparison = primitive `elem` [CoreLessThan, CoreLessEqual, CoreGreaterThan, CoreGreaterEqual, CoreEqual, CoreNotEqual]
        arity = if unary then 1 else 2
        operandType = if logical then boolType else intType
        expectedResult = if logical || comparison then boolType else intType

literalProblems :: CoreLiteral -> Type -> [Diagnostic]
literalProblems literal valueType = typeMismatch "VXC1029" "Core literal payload does not match its type" expected valueType
    where
        expected = case literal of
            CoreInteger _ -> intType
            CoreString _ -> stringType
            CoreBoolean _ -> boolType
            CoreUnit -> unitType

statementsAlwaysReturn :: [CoreStatement] -> Bool
statementsAlwaysReturn [] = False
statementsAlwaysReturn (statement : remaining) = case statement of
    CoreReturn _ -> True
    CoreIf _ trueBranch falseBranch ->
        (not (null falseBranch) && statementsAlwaysReturn trueBranch && statementsAlwaysReturn falseBranch)
            || statementsAlwaysReturn remaining
    _ -> statementsAlwaysReturn remaining

emptyName :: QualifiedName -> [Diagnostic]
emptyName (QualifiedName parts) =
    [ problem "VXC1001" "Core module name must contain at least one non-empty part"
    | null parts || any (null . identifierText) parts
    ]

invalidSymbol :: String -> String -> ResolvedName -> [Diagnostic]
invalidSymbol code message name = [problem code message | symbolIdValue (resolvedSymbol name) <= 0]

unresolvedType :: String -> String -> Type -> [Diagnostic]
unresolvedType code message valueType = [problem code message | containsError valueType]

containsError :: Type -> Bool
containsError valueType = case valueType of
    ErrorType -> True
    NamedType _ arguments -> any containsError arguments
    FunctionType parameters result -> any containsError parameters || containsError result
    TypeVariable _ -> False

typeMismatch :: String -> String -> Type -> Type -> [Diagnostic]
typeMismatch code message expected actual = [problem code message | expected /= actual]

duplicates :: (Ord a) => String -> String -> [a] -> [Diagnostic]
duplicates code message values = [problem code message | groupValue <- group (sort values), length groupValue > 1]

problem :: String -> String -> Diagnostic
problem code message = Diagnostic CoreStage Error code Nothing message
