-- SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
-- SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.1

-- | Lower the typed source AST into target-independent, source-attributed Core.
module Visual.XSharp.Desugarer (Desugarer (..), defaultDesugarer, runDesugarer) where

import Control.Monad.State.Strict
import Data.Bits (xor)
import Data.Char (ord)
import Data.List (nub)
import Data.Word (Word64)
import Visual.XSharp.AST
import Visual.XSharp.Core
import Visual.XSharp.Desugarer.Sequencing
import Visual.XSharp.Diagnostic (Diagnostic (..), DiagnosticSeverity (Error), DiagnosticStage (DesugarerStage))

-- | Pluggable desugaring pass from typed source semantics into Core IR.
newtype Desugarer = Desugarer
    { desugarTypedAST :: TypedAST -> Either [Diagnostic] CoreModule
    -- ^ Lower a typed tree or return source-level lowering diagnostics.
    }

-- | Run a supplied desugaring implementation.
runDesugarer :: Desugarer -> TypedAST -> Either [Diagnostic] CoreModule
runDesugarer = desugarTypedAST

-- | Default Core lowerer used by the compiler pipeline.
defaultDesugarer :: Desugarer
defaultDesugarer = Desugarer lowerTree

lowerTree :: TypedAST -> Either [Diagnostic] CoreModule
lowerTree (TypedAST tree@(SyntaxTree namespace declarations)) =
    evalStateT lowerModule (1 + maximum (0 : syntaxSymbolIds tree))
    where
        defaultName = QualifiedName [Identifier "Main"]
        sourceFiles = nub (map portableSourcePath (concatMap declarationSourceFiles declarations))
        lowerModule = do
            functions <- concat <$> mapM lowerTop declarations
            pure
                ( CoreModuleWithSources
                    (maybe defaultName id namespace)
                    functions
                    sourceFiles
                    (functionSources declarations)
                )

-- Keep the physical file catalog even for a declaration that does not yet
-- lower to executable code.  The project driver uses it to produce stable,
-- one-source-per-artifact output names after optimization has removed dead
-- functions.
declarationSourceFiles :: Declaration name annotation -> [FilePath]
declarationSourceFiles declaration = case declaration of
    FunctionDeclaration {} -> [portableSpanSource declaration]
    TypeDeclaration {typeMembers = members} ->
        portableSpanSource declaration : concatMap declarationSourceFiles members
    TemplateTypeDeclaration {typeMembers = members} ->
        portableSpanSource declaration : concatMap declarationSourceFiles members

-- Artifact paths always use portable separators, even when discovery ran on
-- Windows; these names become stable wire and output identities.
portableSourcePath :: FilePath -> FilePath
portableSourcePath = map (\character -> if character == '\\' then '/' else character)

portableSpanSource :: Declaration name annotation -> FilePath
portableSpanSource = portableSourcePath . sourceFile . declarationSpan

functionSources :: [Declaration ResolvedName Type] -> [(Int, FilePath)]
functionSources = concatMap declarationFunctionSources
    where
        declarationFunctionSources declaration = case declaration of
            FunctionDeclaration {} -> owner declaration
            TypeDeclaration {typeMembers = members} -> concatMap declarationFunctionSources members
            TemplateTypeDeclaration {typeMembers = members} -> concatMap declarationFunctionSources members
        owner declaration =
            [
                ( symbolIdValue (resolvedSymbol (declarationName declaration))
                , portableSpanSource declaration
                )
            ]

type Lower = StateT Int (Either [Diagnostic])

freshPatternSubject :: Lower ResolvedName
freshPatternSubject = freshGenerated "$pattern"

freshCoalesceSubject :: Lower ResolvedName
freshCoalesceSubject = freshGenerated "$coalesce"

freshGenerated :: String -> Lower ResolvedName
freshGenerated prefix = do
    identifier <- get
    put (identifier + 1)
    pure (ResolvedName (SymbolId identifier) (Identifier (prefix ++ show identifier)))

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

{- | Where a @break value;@ stores its value: the result slot of the loop
expression it leaves. A loop statement has no slot, and its body is lowered
without one, so a value-carrying break can only reach its own loop's slot.
-}
type BreakTarget = Maybe (ResolvedName, Type)

lowerBlock :: Block ResolvedName Type -> Lower [CoreStatement]
lowerBlock = lowerBlockInto Nothing

lowerBlockInto :: BreakTarget -> Block ResolvedName Type -> Lower [CoreStatement]
lowerBlockInto target (Block statements) = concat <$> mapM (lowerStatementInto target) statements

lowerFunctionBlock :: Type -> Block ResolvedName Type -> Lower [CoreStatement]
lowerFunctionBlock returnType (Block statements) = case reverse statements of
    ExpressionStatement _ expression False : remaining
        | returnType /= unitType -> do
            prefix <- concat <$> mapM lowerStatement (reverse remaining)
            (valuePrefix, value) <- lowerExpression expression
            pure (prefix ++ valuePrefix ++ [CoreReturn value])
    _ -> concat <$> mapM lowerStatement statements

lowerStatement :: Statement ResolvedName Type -> Lower [CoreStatement]
lowerStatement = lowerStatementInto Nothing

lowerStatementInto :: BreakTarget -> Statement ResolvedName Type -> Lower [CoreStatement]
lowerStatementInto target statement = case statement of
    BindingStatement _ kind _ name valueType value -> do
        (prefix, lowered) <- lowerExpression value
        pure (prefix ++ [CoreBind (CoreBinding name (lowerBoundaryType valueType) (kind == MutableBinding) lowered)])
    AssignmentStatement _ name _ value -> do
        (prefix, lowered) <- lowerExpression value
        pure (prefix ++ [CoreAssign name lowered])
    ReturnStatement _ Nothing -> pure [CoreReturn (CoreLiteral CoreUnit unitType)]
    ReturnStatement _ (Just value) -> do
        (prefix, lowered) <- lowerExpression value
        pure (prefix ++ [CoreReturn lowered])
    IfStatement _ condition trueBlock falseBlock -> do
        (prefix, loweredCondition) <- lowerExpression condition
        whenTrue <- lowerBlockInto target trueBlock
        whenFalse <- maybe (pure []) (lowerBlockInto target) falseBlock
        pure (prefix ++ [CoreIf loweredCondition whenTrue whenFalse])
    WhileStatement {} -> lowerLoop Nothing statement
    DoWhileStatement _ body condition -> do
        loweredBody <- lowerBlock body
        loweredCondition <- lowerExpression condition
        if null (fst loweredCondition)
            then pure [CoreDoWhile loweredBody (snd loweredCondition)]
            else do
                first <- freshGenerated "$first"
                pure (doWhileLoop first loweredBody loweredCondition)
    ForStatement {} -> lowerLoop Nothing statement
    ForEachStatement spanValue _ _ _ _ _ _ ->
        lift
            ( Left
                [ Diagnostic
                    DesugarerStage
                    Error
                    "VXD0001"
                    (Just spanValue)
                    "enumerable for loops cannot be lowered without the generator and Enumerable ABI"
                ]
            )
    IncrementStatement _ name valueType ->
        let loweredType = lowerBoundaryType valueType
         in pure [stepStatement name loweredType (CoreVariable name loweredType)]
    CompoundAssignmentStatement _ operator name valueType value -> lowerCompound operator name valueType value
    DiscardStatement _ value -> lowerDiscarded value
    BreakStatement _ Nothing -> pure [CoreBreak]
    BreakStatement spanValue (Just value) -> case target of
        Just (slot, _) -> do
            (prefix, lowered) <- lowerExpression value
            pure (prefix ++ [CoreAssign slot lowered, CoreBreak])
        Nothing ->
            lift
                ( Left
                    [ Diagnostic
                        DesugarerStage
                        Error
                        "VXD0002"
                        (Just spanValue)
                        "a value-carrying break reached Core lowering outside a loop expression"
                    ]
                )
    ContinueStatement _ -> pure [CoreContinue]
    ExpressionStatement _ value _ -> lowerDiscarded value

{- | Lower a @while@ or @for@ loop whose body stores break values into the
given target. Loops nested in the body are statements of their own and are
lowered without a target.
-}
lowerLoop :: BreakTarget -> Statement ResolvedName Type -> Lower [CoreStatement]
lowerLoop target loop = case loop of
    WhileStatement _ condition body -> do
        loweredCondition <- lowerExpression condition
        loweredBody <- lowerBlockInto target body
        pure [whileLoop loweredCondition loweredBody]
    ForStatement _ initializer condition updates body -> do
        loweredInitializer <- maybe (pure []) lowerStatement initializer
        loweredCondition <- maybe (pure ([], CoreLiteral (CoreBoolean True) boolType)) lowerExpression condition
        loweredBody <- lowerBlockInto target body
        loweredUpdates <- concat <$> mapM lowerStatement updates
        pure (loweredInitializer ++ [forLoop loweredCondition loweredBody loweredUpdates])
    _ -> lowerStatement loop

{- | Lower an expression whose value is dropped.

The expression is still evaluated. When it stored into a local, the stores
are its statements and the remaining value is a plain read with nothing left
to evaluate, so no evaluation statement is emitted for it.
-}
lowerDiscarded :: Expression ResolvedName Type -> Lower [CoreStatement]
lowerDiscarded value = do
    (prefix, lowered) <- lowerExpression value
    pure (prefix ++ [CoreEvaluate lowered | null prefix || not (survives [] lowered)])

-- | @target = operand + 1@, reading the given operand.
stepStatement :: ResolvedName -> Type -> CoreExpression -> CoreStatement
stepStatement name loweredType from =
    CoreAssign name (CorePrimitive CoreAdd [from, CoreLiteral (CoreInteger 1) loweredType] loweredType)

{- | Lower @target op= value@ to the statements that perform it.

The type checker has established that the operator result has the target
type, so the stored primitive needs no conversion. The target is read before
the right operand is evaluated; when the right operand itself assigns the
target, that earlier value is kept in a temporary.
-}
lowerCompound :: BinaryOperator -> ResolvedName -> Type -> Expression ResolvedName Type -> Lower [CoreStatement]
lowerCompound operator name valueType value = do
    (prefix, lowered) <- lowerExpression value
    let loweredType = lowerBoundaryType valueType
        current = CoreVariable name loweredType
        store left = CoreAssign name (CorePrimitive (lowerBinary operator) [left, lowered] loweredType)
    if survives (assignedSymbols prefix) current
        then pure (prefix ++ [store current])
        else do
            previous <- freshGenerated "$target"
            pure
                ( CoreBind (CoreBinding previous loweredType False current)
                    : prefix
                    ++ [store (CoreVariable previous loweredType)]
                )

{- | Make a lowered operand usable after the statements of later operands.

Operands are evaluated left to right. When a later operand has statements,
an earlier operand that would not survive them is bound to a temporary at
the point where the source evaluates it.
-}
holdAcross :: [CoreStatement] -> Lowered -> Lower Lowered
holdAcross later lowered@(prefix, value)
    | null later || survives (assignedSymbols later) value = pure lowered
    | otherwise = do
        name <- freshGenerated "$operand"
        let valueType = expressionType value
        pure (prefix ++ [CoreBind (CoreBinding name valueType False value)], CoreVariable name valueType)

-- | Combine operands that are all evaluated, in order.
sequenceOperands :: [Lowered] -> Lower ([CoreStatement], [CoreExpression])
sequenceOperands [] = pure ([], [])
sequenceOperands (operand : later) = do
    (laterPrefix, laterValues) <- sequenceOperands later
    (prefix, value) <- holdAcross laterPrefix operand
    pure (prefix ++ laterPrefix, value : laterValues)

-- | Combine one operand with the operands evaluated after it.
sequenceAfter :: Lowered -> [Lowered] -> Lower ([CoreStatement], CoreExpression, [CoreExpression])
sequenceAfter first later = do
    (laterPrefix, laterValues) <- sequenceOperands later
    (prefix, value) <- holdAcross laterPrefix first
    pure (prefix ++ laterPrefix, value, laterValues)

{- | Lower an expression to the statements that perform its stores and a
store-free Core expression for its value.

An expression without assignment or increment operands has no statements,
and its Core expression is the direct translation of the source.
-}
lowerExpression :: Expression ResolvedName Type -> Lower Lowered
lowerExpression expression = case expression of
    NameExpression _ name valueType -> pure ([], CoreVariable name (lowerBoundaryType valueType))
    LiteralExpression _ literal valueType ->
        let loweredType = lowerBoundaryType valueType
         in pure ([], CoreLiteral (lowerLiteral loweredType literal) loweredType)
    MemberAccessExpression spanValue _ _ _ ->
        lift
            ( Left
                [ Diagnostic
                    DesugarerStage
                    Error
                    "VXD0003"
                    (Just spanValue)
                    "an unresolved member selector reached Core lowering"
                ]
            )
    CallExpression _ callee arguments valueType -> do
        loweredCallee <- lowerExpression callee
        loweredArguments <- mapM lowerExpression arguments
        (prefix, calleeValue, argumentValues) <- sequenceAfter loweredCallee loweredArguments
        pure (prefix, CoreApply calleeValue argumentValues (lowerBoundaryType valueType))
    UnaryExpression _ UnaryPlus value _ -> lowerExpression value
    UnaryExpression _ operator value valueType -> do
        (prefix, lowered) <- lowerExpression value
        pure (prefix, CorePrimitive (lowerUnary operator) [lowered] (lowerBoundaryType valueType))
    BinaryExpression _ operator left right valueType
        | operator `elem` [LogicalAnd, LogicalOr] -> do
            (leftPrefix, loweredLeft) <- lowerExpression left
            loweredRight <- lowerExpression right
            if null (fst loweredRight)
                then
                    pure
                        ( leftPrefix
                        , CorePrimitive (lowerBinary operator) [loweredLeft, snd loweredRight] (lowerBoundaryType valueType)
                        )
                else do
                    result <- freshGenerated "$logical"
                    let (statements, value) = decideLogical (operator == LogicalAnd) result loweredLeft loweredRight
                    pure (leftPrefix ++ statements, value)
        | otherwise -> do
            loweredLeft <- lowerExpression left
            loweredRight <- lowerExpression right
            (prefix, leftValue, rightValues) <- sequenceAfter loweredLeft [loweredRight]
            pure (prefix, CorePrimitive (lowerBinary operator) (leftValue : rightValues) (lowerBoundaryType valueType))
    IsPatternExpression _ subject patternValue _ -> do
        (prefix, loweredSubject) <- lowerExpression subject
        subjectName <- freshPatternSubject
        let subjectType = lowerBoundaryType (expressionAnnotation subject)
            subjectRead = CoreVariable subjectName subjectType
            predicate = lowerPattern subjectRead subjectType patternValue
        pure (prefix, CoreLet subjectName subjectType loweredSubject predicate boolType)
    ConditionalExpression _ condition first second valueType -> do
        (conditionPrefix, loweredCondition) <- lowerExpression condition
        loweredFirst <- lowerExpression first
        loweredSecond <- lowerExpression second
        let loweredType = lowerBoundaryType valueType
        if null (fst loweredFirst) && null (fst loweredSecond)
            then
                pure
                    ( conditionPrefix
                    , CoreConditional loweredCondition (snd loweredFirst) (snd loweredSecond) loweredType
                    )
            else do
                result <- freshGenerated "$selected"
                let (statements, value) = selectInto result loweredType loweredCondition loweredFirst loweredSecond
                pure (conditionPrefix ++ statements, value)
    -- The left operand is both the test and the first result. Binding it once
    -- keeps its effects single even though it is read twice.
    CoalesceExpression _ left fallback valueType -> do
        (leftPrefix, loweredLeft) <- lowerExpression left
        loweredFallback <- lowerExpression fallback
        subjectName <- freshCoalesceSubject
        let loweredType = lowerBoundaryType valueType
            subjectRead = CoreVariable subjectName loweredType
        if null (fst loweredFallback)
            then
                pure
                    ( leftPrefix
                    , CoreLet
                        subjectName
                        loweredType
                        loweredLeft
                        (CoreConditional subjectRead subjectRead (snd loweredFallback) loweredType)
                        loweredType
                    )
            else do
                -- The fallback stores into a local, so it runs as statements
                -- and only when the left value is false in Boolean context.
                result <- freshGenerated "$selected"
                let (statements, value) = selectInto result loweredType subjectRead ([], subjectRead) loweredFallback
                pure
                    ( leftPrefix ++ CoreBind (CoreBinding subjectName loweredType False loweredLeft) : statements
                    , value
                    )
    -- A simple assignment stores its right operand and yields the stored
    -- value, which is read back from the target.
    AssignmentExpression _ Nothing name value valueType -> do
        (prefix, lowered) <- lowerExpression value
        pure (prefix ++ [CoreAssign name lowered], CoreVariable name (lowerBoundaryType valueType))
    AssignmentExpression _ (Just operator) name value valueType -> do
        statements <- lowerCompound operator name valueType value
        pure (statements, CoreVariable name (lowerBoundaryType valueType))
    -- A prefix form yields the new value, so the target is read after the
    -- store. A postfix form yields the previous value, kept in a temporary.
    IncrementExpression _ isPrefix name valueType ->
        let loweredType = lowerBoundaryType valueType
            current = CoreVariable name loweredType
         in if isPrefix
                then pure ([stepStatement name loweredType current], current)
                else do
                    previous <- freshGenerated "$previous"
                    let previousRead = CoreVariable previous loweredType
                    pure
                        (
                            [ CoreBind (CoreBinding previous loweredType False current)
                            , stepStatement name loweredType previousRead
                            ]
                        , previousRead
                        )
    -- A loop expression runs its loop as statements. Each break that leaves
    -- it stores its value into the result slot first; the type checker has
    -- established that the loop cannot end any other way.
    LoopExpression _ loop valueType -> do
        result <- freshGenerated "$loop"
        let loweredType = lowerBoundaryType valueType
        statements <- lowerLoop (Just (result, loweredType)) loop
        pure
            ( CoreBind (CoreBinding result loweredType True (neutralValue loweredType)) : statements
            , CoreVariable result loweredType
            )
    CallableExpression _ explicit captures parameters body valueType -> do
        let loweredParameters =
                [(parameterName parameter, lowerBoundaryType (parameterAnnotation parameter)) | parameter <- parameters]
        loweredBody <- lowerCallableBody body
        (prefix, loweredCaptures) <- lowerCaptures captures
        let returnType = case valueType of
                FunctionType _ result -> lowerBoundaryType result
                _ -> ErrorType
            closure sourceCaptures =
                CoreClosure sourceCaptures loweredParameters returnType loweredBody (lowerBoundaryType valueType)
        pure $
            if explicit
                then (prefix, closure loweredCaptures)
                else ([], closure (discoverImplicitCaptures loweredParameters loweredBody))

-- Capture initializers are evaluated in order when the closure is created,
-- like the operands of any other expression.
lowerCaptures :: [Capture ResolvedName Type] -> Lower ([CoreStatement], [CoreCapture])
lowerCaptures captures = do
    initializers <- mapM lowerInitializer captures
    (prefix, values) <- sequenceOperands initializers
    pure (prefix, zipWith capture captures values)
    where
        lowerInitializer source =
            maybe
                (pure ([], CoreVariable (captureName source) (lowerBoundaryType (captureAnnotation source))))
                lowerExpression
                (captureInitializer source)
        capture source =
            CoreCapture (captureMode source) (captureName source) (lowerBoundaryType (captureAnnotation source))

lowerCallableBody :: CallableBody ResolvedName Type -> Lower [CoreStatement]
lowerCallableBody body = case body of
    CallableExpressionBody expression -> do
        (prefix, value) <- lowerExpression expression
        pure (prefix ++ [CoreReturn value])
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
            [subject, CoreLiteral (CoreInteger (toInteger (typeIdentity targetType))) (namedType "uint")]
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
            CoreWhile _ body -> localSymbols body
            CoreDoWhile body _ -> localSymbols body
            CoreFor _ body update -> localSymbols body ++ localSymbols update
            _ -> []

statementReads :: [CoreStatement] -> [(ResolvedName, Type)]
statementReads = concatMap collect
    where
        collect statement = case statement of
            CoreBind binding -> expressionReads (coreBindingValue binding)
            CoreAssign _ value -> expressionReads value
            CoreReturn value -> expressionReads value
            CoreIf condition yes no -> expressionReads condition ++ statementReads yes ++ statementReads no
            CoreWhile condition body -> expressionReads condition ++ statementReads body
            CoreDoWhile body condition -> statementReads body ++ expressionReads condition
            CoreFor condition body update ->
                expressionReads condition ++ statementReads body ++ statementReads update
            CoreEvaluate value -> expressionReads value
            CoreBreak -> []
            CoreContinue -> []

expressionReads :: CoreExpression -> [(ResolvedName, Type)]
expressionReads expression = case expression of
    CoreVariable name valueType -> [(name, valueType)]
    CoreLiteral _ _ -> []
    CoreApply callee arguments _ -> expressionReads callee ++ concatMap expressionReads arguments
    CorePrimitive _ arguments _ -> concatMap expressionReads arguments
    CoreLet name _ value body _ ->
        expressionReads value ++ filter ((/= resolvedSymbol name) . resolvedSymbol . fst) (expressionReads body)
    CoreConditional condition whenTrue whenFalse _ ->
        expressionReads condition ++ expressionReads whenTrue ++ expressionReads whenFalse
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
    WhileStatement _ condition body -> expressionIds condition ++ blockSymbolIds body
    DoWhileStatement _ body condition -> blockSymbolIds body ++ expressionIds condition
    ForStatement _ initializer condition updates body ->
        maybe [] statementIds initializer
            ++ maybe [] expressionIds condition
            ++ concatMap statementIds updates
            ++ blockSymbolIds body
    ForEachStatement _ _ _ name _ source body ->
        symbolValue name : expressionIds source ++ blockSymbolIds body
    IncrementStatement _ name _ -> [symbolValue name]
    CompoundAssignmentStatement _ _ name _ value -> symbolValue name : expressionIds value
    DiscardStatement _ value -> expressionIds value
    BreakStatement _ value -> maybe [] expressionIds value
    ContinueStatement {} -> []
    ExpressionStatement _ value _ -> expressionIds value

expressionIds :: Expression ResolvedName Type -> [Int]
expressionIds expression = case expression of
    NameExpression _ name _ -> [symbolValue name]
    LiteralExpression {} -> []
    MemberAccessExpression _ receiver _ _ -> expressionIds receiver
    CallExpression _ callee arguments _ -> expressionIds callee ++ concatMap expressionIds arguments
    UnaryExpression _ _ value _ -> expressionIds value
    BinaryExpression _ _ left right _ -> expressionIds left ++ expressionIds right
    IsPatternExpression _ subject _ _ -> expressionIds subject
    ConditionalExpression _ condition first second _ -> concatMap expressionIds [condition, first, second]
    CoalesceExpression _ left fallback _ -> expressionIds left ++ expressionIds fallback
    AssignmentExpression _ _ name value _ -> symbolValue name : expressionIds value
    IncrementExpression _ _ name _ -> [symbolValue name]
    LoopExpression _ loop _ -> statementIds loop
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
