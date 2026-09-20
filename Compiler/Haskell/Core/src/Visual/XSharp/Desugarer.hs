-- SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
-- SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.1
module Visual.XSharp.Desugarer (Desugarer (..), defaultDesugarer, runDesugarer) where

import Control.Monad.State.Strict
import Data.Bits (xor)
import Data.Char (ord)
import Data.Word (Word64)
import Visual.XSharp.AST
import Visual.XSharp.Core
import Visual.XSharp.Diagnostic (Diagnostic)

newtype Desugarer = Desugarer {desugarTypedAST :: TypedAST -> Either [Diagnostic] CoreModule}
runDesugarer :: Desugarer -> TypedAST -> Either [Diagnostic] CoreModule
runDesugarer = desugarTypedAST
defaultDesugarer :: Desugarer
defaultDesugarer = Desugarer (Right . lowerTree)

lowerTree :: TypedAST -> CoreModule
lowerTree (TypedAST tree@(SyntaxTree namespace declarations)) =
    evalState
        (CoreModule (maybe defaultName id namespace) . concat <$> mapM lowerTop declarations)
        (1 + maximum (0 : syntaxSymbolIds tree))
    where
        defaultName = QualifiedName [Identifier "Main"]

type Lower = State Int

freshPatternSubject :: Lower ResolvedName
freshPatternSubject = do
    identifier <- get
    put (identifier + 1)
    pure (ResolvedName (SymbolId identifier) (Identifier ("$pattern" ++ show identifier)))

lowerTop :: Declaration ResolvedName Type -> Lower [CoreFunction]
lowerTop TypeDeclaration {typeMembers = members} = mapM lowerDeclaration members
-- Open template bodies are retained in TypedAST until specialization chooses
-- concrete arguments. Lowering them here would leak unresolved type variables
-- into Core and create one fake unspecialized native function.
lowerTop TemplateTypeDeclaration {} = pure []
lowerTop function@FunctionDeclaration {} = (: []) <$> lowerDeclaration function

lowerDeclaration :: Declaration ResolvedName Type -> Lower CoreFunction
lowerDeclaration declaration@FunctionDeclaration {} = do
    body <- lowerFunctionBlock returnType (declarationBody declaration)
    pure
        ( CoreFunction
            (declarationName declaration)
            [ (parameterName parameter, lowerBoundaryType (parameterAnnotation parameter))
            | parameter <- declarationParameters declaration
            ]
            returnType
            body
        )
    where
        returnType = lowerBoundaryType $ case declarationAnnotation declaration of FunctionType _ result -> result; value -> value
lowerDeclaration TypeDeclaration {} = error "type declarations are lowered through lowerTop"
lowerDeclaration TemplateTypeDeclaration {} = error "template declarations require specialization before Core lowering"

lowerBlock :: Block ResolvedName Type -> Lower [CoreStatement]
lowerBlock (Block statements) = mapM lowerStatement statements

lowerFunctionBlock :: Type -> Block ResolvedName Type -> Lower [CoreStatement]
lowerFunctionBlock returnType (Block statements) = case reverse statements of
    ExpressionStatement _ expression False : remaining
        | returnType /= unitType -> do
            prefix <- mapM lowerStatement (reverse remaining)
            value <- lowerExpression expression
            pure (prefix ++ [CoreReturn value])
    _ -> mapM lowerStatement statements

lowerStatement :: Statement ResolvedName Type -> Lower CoreStatement
lowerStatement statement = case statement of
    BindingStatement _ kind _ name valueType value -> do
        lowered <- lowerExpression value
        pure (CoreBind (CoreBinding name (lowerBoundaryType valueType) (kind == MutableBinding) lowered))
    AssignmentStatement _ name _ value -> CoreAssign name <$> lowerExpression value
    ReturnStatement _ value -> CoreReturn <$> maybe (pure (CoreLiteral CoreUnit unitType)) lowerExpression value
    IfStatement _ condition trueBlock falseBlock ->
        CoreIf <$> lowerExpression condition <*> lowerBlock trueBlock <*> maybe (pure []) lowerBlock falseBlock
    ExpressionStatement _ value _ -> CoreEvaluate <$> lowerExpression value

lowerExpression :: Expression ResolvedName Type -> Lower CoreExpression
lowerExpression expression = case expression of
    NameExpression _ name valueType -> pure (CoreVariable name (lowerBoundaryType valueType))
    LiteralExpression _ literal valueType ->
        let loweredType = lowerBoundaryType valueType
         in pure (CoreLiteral (lowerLiteral loweredType literal) loweredType)
    CallExpression _ callee arguments valueType ->
        CoreApply <$> lowerExpression callee <*> mapM lowerExpression arguments <*> pure (lowerBoundaryType valueType)
    UnaryExpression _ UnaryPlus value _ -> lowerExpression value
    UnaryExpression _ operator value valueType -> do
        lowered <- lowerExpression value
        pure (CorePrimitive (lowerUnary operator) [lowered] (lowerBoundaryType valueType))
    BinaryExpression _ operator left right valueType -> do
        loweredLeft <- lowerExpression left
        loweredRight <- lowerExpression right
        pure (CorePrimitive (lowerBinary operator) [loweredLeft, loweredRight] (lowerBoundaryType valueType))
    IsPatternExpression _ subject patternValue _ -> do
        loweredSubject <- lowerExpression subject
        subjectName <- freshPatternSubject
        let subjectType = lowerBoundaryType (expressionAnnotation subject)
            subjectRead = CoreVariable subjectName subjectType
            predicate = lowerPattern subjectRead subjectType patternValue
        pure (CoreLet subjectName subjectType loweredSubject predicate boolType)
    CallableExpression _ explicit captures parameters body valueType -> do
        let loweredParameters =
                [(parameterName parameter, lowerBoundaryType (parameterAnnotation parameter)) | parameter <- parameters]
        loweredBody <- lowerCallableBody body
        loweredCaptures <- mapM lowerCapture captures
        let sourceCaptures = if explicit then loweredCaptures else discoverImplicitCaptures loweredParameters loweredBody
            returnType = case valueType of
                FunctionType _ result -> lowerBoundaryType result
                _ -> ErrorType
        pure (CoreClosure sourceCaptures loweredParameters returnType loweredBody (lowerBoundaryType valueType))

lowerCapture :: Capture ResolvedName Type -> Lower CoreCapture
lowerCapture capture = do
    value <-
        maybe
            (pure (CoreVariable (captureName capture) (lowerBoundaryType (captureAnnotation capture))))
            lowerExpression
            (captureInitializer capture)
    pure (CoreCapture (captureMode capture) (captureName capture) (lowerBoundaryType (captureAnnotation capture)) value)

lowerCallableBody :: CallableBody ResolvedName Type -> Lower [CoreStatement]
lowerCallableBody body = case body of
    CallableExpressionBody expression -> (: []) . CoreReturn <$> lowerExpression expression
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
    IsPatternExpression _ _ _ valueType -> valueType
    CallableExpression _ _ _ _ _ valueType -> valueType

lowerPattern :: CoreExpression -> Type -> Pattern ResolvedName Type -> CoreExpression
lowerPattern subject subjectType patternValue = case patternValue of
    WildcardPattern {} -> CoreLiteral (CoreBoolean True) boolType
    NullPattern {} -> CorePrimitive CoreEqual [subject, CoreLiteral CoreNull subjectType] boolType
    LiteralPattern _ literal literalType ->
        CorePrimitive CoreEqual [subject, CoreLiteral (lowerLiteral literalType literal) literalType] boolType
    TypePattern _ _ targetType ->
        CorePrimitive
            CoreTypeIs
            [subject, CoreLiteral (CoreInteger (toInteger (typeIdentity targetType))) (namedType "ulong")]
            boolType
    RelationalPattern _ operator literal literalType ->
        CorePrimitive
            (lowerRelationalPattern operator)
            [subject, CoreLiteral (lowerLiteral literalType literal) literalType]
            boolType
    NotPattern _ nested _ -> CorePrimitive CoreLogicalNot [lowerPattern subject subjectType nested] boolType
    AndPattern _ left right _ ->
        CorePrimitive CoreLogicalAnd [lowerPattern subject subjectType left, lowerPattern subject subjectType right] boolType
    OrPattern _ left right _ ->
        CorePrimitive CoreLogicalOr [lowerPattern subject subjectType left, lowerPattern subject subjectType right] boolType

lowerRelationalPattern :: RelationalPatternOperator -> CorePrimitive
lowerRelationalPattern operator = case operator of
    PatternLessThan -> CoreLessThan
    PatternLessEqual -> CoreLessEqual
    PatternGreaterThan -> CoreGreaterThan
    PatternGreaterEqual -> CoreGreaterEqual
    PatternEqual -> CoreEqual
    PatternNotEqual -> CoreNotEqual

-- FNV-1a is specified rather than delegated to a host hash library. The same
-- canonical identity is embedded into AARC type metadata by native code, so
-- a pattern test remains stable across processes and target platforms.
typeIdentity :: Type -> Word64
typeIdentity = foldl step 14695981039346656037 . map (fromIntegral . ord) . canonicalTypeName
    where
        step hashValue byte = (hashValue `xor` byte) * 1099511628211

canonicalTypeName :: Type -> String
canonicalTypeName valueType = case valueType of
    NamedType (QualifiedName parts) arguments ->
        joinWith "." (map identifierText parts)
            ++ if null arguments then "" else "<" ++ joinWith "," (map canonicalArgument arguments) ++ ">"
    FunctionType parameters result ->
        "(" ++ joinWith "," (map canonicalTypeName parameters) ++ ")->" ++ canonicalTypeName result
    TypeVariable name -> "$" ++ show (symbolIdValue (resolvedSymbol name))
    ErrorType -> "<error>"
    where
        canonicalArgument argument = case argument of
            TypeTemplateArgument nested -> canonicalTypeName nested
            ValueTemplateArgument value -> show value

joinWith :: String -> [String] -> String
joinWith _ [] = ""
joinWith _ [value] = value
joinWith separator (value : rest) = value ++ separator ++ joinWith separator rest

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
    CoreLet name _ value body _ ->
        expressionReads value ++ filter ((/= resolvedSymbol name) . resolvedSymbol . fst) (expressionReads body)
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
    | NamedType name arguments <- valueType = NamedType name (map lowerTemplateArgument arguments)
    | otherwise = valueType

lowerTemplateArgument :: TemplateArgument -> TemplateArgument
lowerTemplateArgument argument = case argument of
    TypeTemplateArgument valueType -> TypeTemplateArgument (lowerBoundaryType valueType)
    ValueTemplateArgument value -> ValueTemplateArgument value

-- Generated Core bindings must never collide with source symbols. Gathering
-- the complete typed tree once is cheaper and more robust than reserving a
-- magic numeric range or deriving identities from source positions.
syntaxSymbolIds :: SyntaxTree ResolvedName Type -> [Int]
syntaxSymbolIds (SyntaxTree _ declarations) = concatMap declarationSymbolIds declarations

declarationSymbolIds :: Declaration ResolvedName Type -> [Int]
declarationSymbolIds declaration =
    symbolValue (declarationName declaration)
        : case declaration of
            TypeDeclaration {typeMembers = members} -> concatMap declarationSymbolIds members
            TemplateTypeDeclaration {declarationTemplateParameters = parameters, typeMembers = members} ->
                map (symbolValue . templateParameterName) parameters ++ concatMap declarationSymbolIds members
            FunctionDeclaration {declarationParameters = parameters, declarationBody = body} ->
                map (symbolValue . parameterName) parameters ++ blockSymbolIds body

blockSymbolIds :: Block ResolvedName Type -> [Int]
blockSymbolIds (Block statements) = concatMap statementIds statements

statementIds :: Statement ResolvedName Type -> [Int]
statementIds statement = case statement of
    BindingStatement _ _ _ name _ value -> symbolValue name : expressionIds value
    AssignmentStatement _ name _ value -> symbolValue name : expressionIds value
    ReturnStatement _ value -> maybe [] expressionIds value
    IfStatement _ condition yes no -> expressionIds condition ++ blockSymbolIds yes ++ maybe [] blockSymbolIds no
    ExpressionStatement _ value _ -> expressionIds value

expressionIds :: Expression ResolvedName Type -> [Int]
expressionIds expression = case expression of
    NameExpression _ name _ -> [symbolValue name]
    LiteralExpression {} -> []
    CallExpression _ callee arguments _ -> expressionIds callee ++ concatMap expressionIds arguments
    UnaryExpression _ _ value _ -> expressionIds value
    BinaryExpression _ _ left right _ -> expressionIds left ++ expressionIds right
    IsPatternExpression _ subject _ _ -> expressionIds subject
    CallableExpression _ _ captures parameters body _ ->
        map (symbolValue . captureName) captures
            ++ concatMap (maybe [] expressionIds . captureInitializer) captures
            ++ map (symbolValue . parameterName) parameters
            ++ callableBodyIds body

callableBodyIds :: CallableBody ResolvedName Type -> [Int]
callableBodyIds body = case body of
    CallableExpressionBody expression -> expressionIds expression
    CallableBlockBody block -> blockSymbolIds block

symbolValue :: ResolvedName -> Int
symbolValue = symbolIdValue . resolvedSymbol
lowerUnary :: UnaryOperator -> CorePrimitive
lowerUnary UnaryNegate = CoreNegate
lowerUnary LogicalNot = CoreLogicalNot
lowerUnary BitwiseNot = CoreBitwiseNot
lowerUnary UnaryPlus = CoreAdd
lowerBinary :: BinaryOperator -> CorePrimitive
lowerBinary operator = case operator of
    Add -> CoreAdd
    Subtract -> CoreSubtract
    Multiply -> CoreMultiply
    Divide -> CoreDivide
    FloorDivide -> CoreFloorDivide
    Remainder -> CoreRemainder
    Power -> CorePower
    ShiftLeft -> CoreShiftLeft
    ShiftRight -> CoreShiftRight
    BitwiseAnd -> CoreBitwiseAnd
    BitwiseXor -> CoreBitwiseXor
    BitwiseOr -> CoreBitwiseOr
    LessThan -> CoreLessThan
    LessEqual -> CoreLessEqual
    GreaterThan -> CoreGreaterThan
    GreaterEqual -> CoreGreaterEqual
    Equal -> CoreEqual
    NotEqual -> CoreNotEqual
    LogicalAnd -> CoreLogicalAnd
    LogicalOr -> CoreLogicalOr
