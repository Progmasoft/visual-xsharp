-- SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
-- SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.0
module Visual.XSharp.Desugarer (Desugarer (..), defaultDesugarer, runDesugarer) where

import Visual.XSharp.AST
import Visual.XSharp.Core
import Visual.XSharp.Diagnostic (Diagnostic)

newtype Desugarer = Desugarer {desugarTypedAST :: TypedAST -> Either [Diagnostic] CoreModule}
runDesugarer :: Desugarer -> TypedAST -> Either [Diagnostic] CoreModule
runDesugarer = desugarTypedAST
defaultDesugarer :: Desugarer
defaultDesugarer = Desugarer (Right . lowerTree)

lowerTree :: TypedAST -> CoreModule
lowerTree (TypedAST (SyntaxTree namespace declarations)) = CoreModule (maybe defaultName id namespace) (concatMap lowerTop declarations)
    where
        defaultName = QualifiedName [Identifier "Main"]

lowerTop :: Declaration ResolvedName Type -> [CoreFunction]
lowerTop TypeDeclaration {typeMembers = members} = map lowerDeclaration members
lowerTop function@FunctionDeclaration {} = [lowerDeclaration function]

lowerDeclaration :: Declaration ResolvedName Type -> CoreFunction
lowerDeclaration declaration@FunctionDeclaration {} =
    CoreFunction
        (declarationName declaration)
        [ (parameterName parameter, lowerBoundaryType (parameterAnnotation parameter))
        | parameter <- declarationParameters declaration
        ]
        returnType
        (lowerFunctionBlock returnType (declarationBody declaration))
    where
        returnType = lowerBoundaryType $ case declarationAnnotation declaration of FunctionType _ result -> result; value -> value
lowerDeclaration TypeDeclaration {} = error "type declarations are lowered through lowerTop"

lowerBlock :: Block ResolvedName Type -> [CoreStatement]
lowerBlock (Block statements) = map lowerStatement statements

lowerFunctionBlock :: Type -> Block ResolvedName Type -> [CoreStatement]
lowerFunctionBlock returnType (Block statements) = case reverse statements of
    ExpressionStatement _ expression False : remaining
        | returnType /= unitType ->
            map lowerStatement (reverse remaining) ++ [CoreReturn (lowerExpression expression)]
    _ -> map lowerStatement statements

lowerStatement :: Statement ResolvedName Type -> CoreStatement
lowerStatement statement = case statement of
    BindingStatement _ kind _ name valueType value ->
        CoreBind (CoreBinding name (lowerBoundaryType valueType) (kind == MutableBinding) (lowerExpression value))
    AssignmentStatement _ name _ value -> CoreAssign name (lowerExpression value)
    ReturnStatement _ value -> CoreReturn (maybe (CoreLiteral CoreUnit unitType) lowerExpression value)
    IfStatement _ condition trueBlock falseBlock -> CoreIf (lowerExpression condition) (lowerBlock trueBlock) (maybe [] lowerBlock falseBlock)
    ExpressionStatement _ value _ -> CoreEvaluate (lowerExpression value)

lowerExpression :: Expression ResolvedName Type -> CoreExpression
lowerExpression expression = case expression of
    NameExpression _ name valueType -> CoreVariable name (lowerBoundaryType valueType)
    LiteralExpression _ literal valueType ->
        let loweredType = lowerBoundaryType valueType
         in CoreLiteral (lowerLiteral loweredType literal) loweredType
    CallExpression _ callee arguments valueType ->
        CoreApply (lowerExpression callee) (map lowerExpression arguments) (lowerBoundaryType valueType)
    UnaryExpression _ UnaryPlus value _ -> lowerExpression value
    UnaryExpression _ operator value valueType ->
        CorePrimitive (lowerUnary operator) [lowerExpression value] (lowerBoundaryType valueType)
    BinaryExpression _ operator left right valueType ->
        CorePrimitive (lowerBinary operator) [lowerExpression left, lowerExpression right] (lowerBoundaryType valueType)
    CallableExpression _ explicit captures parameters body valueType ->
        let loweredParameters =
                [(parameterName parameter, lowerBoundaryType (parameterAnnotation parameter)) | parameter <- parameters]
            loweredBody = lowerCallableBody body
            sourceCaptures =
                if explicit
                    then map lowerCapture captures
                    else discoverImplicitCaptures loweredParameters loweredBody
            returnType = case valueType of
                FunctionType _ result -> lowerBoundaryType result
                _ -> ErrorType
         in CoreClosure sourceCaptures loweredParameters returnType loweredBody (lowerBoundaryType valueType)

lowerCapture :: Capture ResolvedName Type -> CoreCapture
lowerCapture capture =
    CoreCapture
        (captureMode capture)
        (captureName capture)
        (lowerBoundaryType (captureAnnotation capture))
        ( maybe
            (CoreVariable (captureName capture) (lowerBoundaryType (captureAnnotation capture)))
            lowerExpression
            (captureInitializer capture)
        )

lowerCallableBody :: CallableBody ResolvedName Type -> [CoreStatement]
lowerCallableBody body = case body of
    CallableExpressionBody expression -> [CoreReturn (lowerExpression expression)]
    CallableBlockBody block ->
        let returnType = maybe unitType id (callableFinalType block)
         in lowerFunctionBlock returnType block

callableFinalType :: Block ResolvedName Type -> Maybe Type
callableFinalType (Block statements) = case reverse statements of
    ExpressionStatement _ expression False : _ -> Just (expressionAnnotation expression)
    _ -> Nothing

expressionAnnotation :: Expression name Type -> Type
expressionAnnotation expression = case expression of
    NameExpression _ _ valueType -> valueType
    LiteralExpression _ _ valueType -> valueType
    CallExpression _ _ _ valueType -> valueType
    UnaryExpression _ _ _ valueType -> valueType
    BinaryExpression _ _ _ _ valueType -> valueType
    CallableExpression _ _ _ _ _ valueType -> valueType

-- Implicit captures are the free symbols of the lowered callable body.  The
-- analysis is deliberately performed after desugaring so syntactic sugar
-- cannot hide a read.  Locals introduced by the callable and its parameters
-- are removed before stable first-use ordering is assigned.
discoverImplicitCaptures :: [(ResolvedName, Type)] -> [CoreStatement] -> [CoreCapture]
discoverImplicitCaptures parameters statements =
    let bound = map (resolvedSymbol . fst) parameters ++ localSymbols statements
        free = filter (\(name, _) -> resolvedSymbol name `notElem` bound) (statementReads statements)
     in [CoreCapture StrongCapture name valueType (CoreVariable name valueType) | (name, valueType) <- uniqueReads free]

localSymbols :: [CoreStatement] -> [SymbolId]
localSymbols = concatMap collect
    where
        collect statement = case statement of
            CoreBind binding -> [resolvedSymbol (coreBindingName binding)]
            CoreIf _ yes no -> localSymbols yes ++ localSymbols no
            _ -> []

statementReads :: [CoreStatement] -> [(ResolvedName, Type)]
statementReads = concatMap collect
    where
        collect statement = case statement of
            CoreBind binding -> expressionReads (coreBindingValue binding)
            CoreAssign _ value -> expressionReads value
            CoreReturn value -> expressionReads value
            CoreIf condition yes no -> expressionReads condition ++ statementReads yes ++ statementReads no
            CoreEvaluate value -> expressionReads value

expressionReads :: CoreExpression -> [(ResolvedName, Type)]
expressionReads expression = case expression of
    CoreVariable name valueType -> [(name, valueType)]
    CoreLiteral _ _ -> []
    CoreApply callee arguments _ -> expressionReads callee ++ concatMap expressionReads arguments
    CorePrimitive _ arguments _ -> concatMap expressionReads arguments
    CoreClosure captures _ _ body _ -> concatMap (expressionReads . coreCaptureValue) captures ++ statementReads body

uniqueReads :: [(ResolvedName, Type)] -> [(ResolvedName, Type)]
uniqueReads = foldl append []
    where
        append output value@(name, _)
            | any ((== resolvedSymbol name) . resolvedSymbol . fst) output = output
            | otherwise = output ++ [value]

lowerLiteral :: Type -> Literal -> CoreLiteral
lowerLiteral valueType literal = case literal of
    IntegerLiteral value
        | valueType == boolType -> CoreBoolean (value /= 0)
        | otherwise -> CoreInteger value
    FloatingLiteral spelling -> CoreFloating spelling
    CharacterLiteral value -> CoreInteger value
    BooleanLiteral value -> CoreBoolean value
    StringLiteral value -> CoreString value
    UnitLiteral -> CoreUnit

-- The frontend keeps source 'void' separate from value-producing 'unit'. The
-- native Core contract predates that distinction and represents no-result as
-- unit, so erasure happens once while crossing from Typed AST into Core.
lowerBoundaryType :: Type -> Type
lowerBoundaryType valueType
    | valueType == voidType = unitType
    | FunctionType parameters result <- valueType =
        FunctionType (map lowerBoundaryType parameters) (lowerBoundaryType result)
    | NamedType name arguments <- valueType = NamedType name (map lowerBoundaryType arguments)
    | otherwise = valueType
lowerUnary :: UnaryOperator -> CorePrimitive
lowerUnary UnaryNegate = CoreNegate
lowerUnary LogicalNot = CoreLogicalNot
lowerUnary UnaryPlus = CoreAdd
lowerBinary :: BinaryOperator -> CorePrimitive
lowerBinary operator = case operator of
    Add -> CoreAdd
    Subtract -> CoreSubtract
    Multiply -> CoreMultiply
    Divide -> CoreDivide
    FloorDivide -> CoreFloorDivide
    Remainder -> CoreRemainder
    LessThan -> CoreLessThan
    LessEqual -> CoreLessEqual
    GreaterThan -> CoreGreaterThan
    GreaterEqual -> CoreGreaterEqual
    Equal -> CoreEqual
    NotEqual -> CoreNotEqual
    LogicalAnd -> CoreLogicalAnd
    LogicalOr -> CoreLogicalOr
