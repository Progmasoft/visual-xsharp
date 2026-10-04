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
import Visual.XSharp.Completion
import Visual.XSharp.Core
import Visual.XSharp.Desugarer.Branching
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
    evalStateT lowerModule (LowerState (1 + maximum (0 : syntaxSymbolIds tree)) Nothing [CoreContinue] [CoreBreak])
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

{- | What the lowering carries besides the tree.

The break target and the lowering of @continue@ belong to the statement
being lowered. They are state rather than arguments because a statement is
also reached through an expression: a block used as a value holds statements,
and a @break@ or @continue@ in it must reach the loop around the expression.
-}
data LowerState = LowerState
    { lowerNextSymbol :: Int
    -- ^ The identity of the next compiler-generated local.
    , lowerBreakTarget :: BreakTarget
    -- ^ Where a @break value;@ of the statement being lowered stores.
    , lowerContinue :: [CoreStatement]
    {- ^ What a @continue@ of the statement being lowered becomes: the Core
    @continue@ in a loop body, and a @break@ of the enclosing one-pass loop
    in an update clause, where it ends the update.
    -}
    , lowerLeave :: [CoreStatement]
    {- ^ What a @break@ of the statement being lowered becomes, after the
    store of its value if it carries one: the Core @break@, preceded in an
    update clause that is wrapped in a one-pass loop by the store that makes
    the loop around it leave as well.
    -}
    }

type Lower = StateT LowerState (Either [Diagnostic])

-- | Lower with the given break target, and restore the previous one after.
targeting :: BreakTarget -> Lower a -> Lower a
targeting target action = do
    previous <- gets lowerBreakTarget
    modify (\current -> current {lowerBreakTarget = target})
    result <- action
    modify (\current -> current {lowerBreakTarget = previous})
    pure result

{- | Lower with the given lowerings of @continue@ and of @break@, and restore
the previous ones after.
-}
transferringWith :: [CoreStatement] -> [CoreStatement] -> Lower a -> Lower a
transferringWith continues leaves action = do
    previousContinue <- gets lowerContinue
    previousLeave <- gets lowerLeave
    modify (\current -> current {lowerContinue = continues, lowerLeave = leaves})
    result <- action
    modify (\current -> current {lowerContinue = previousContinue, lowerLeave = previousLeave})
    pure result

{- | Lower a part of a Core loop in which the transfers of the source loop
are the transfers of that Core loop: its body, and its condition.
-}
inCoreLoop :: Lower a -> Lower a
inCoreLoop = transferringWith [CoreContinue] [CoreBreak]

{- | The branching forms as they are lowered at the current place: a
@break value;@ in a block used as a value stores where one in the statement
around it would.
-}
branching :: (BranchLowering Lower -> Lower a) -> Lower a
branching use = gets lowerBreakTarget >>= use . branchLowering

freshPatternSubject :: Lower ResolvedName
freshPatternSubject = freshGenerated "$pattern"

freshCoalesceSubject :: Lower ResolvedName
freshCoalesceSubject = freshGenerated "$coalesce"

freshGenerated :: String -> Lower ResolvedName
freshGenerated prefix = do
    identifier <- gets lowerNextSymbol
    modify (\current -> current {lowerNextSymbol = identifier + 1})
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

{- | The lowering of this module as the branching forms of
"Visual.XSharp.Desugarer.Branching" receive it. The target is where a
@break value;@ among their statements stores its value: the result slot of
the loop expression around them, if there is one.
-}
branchLowering :: BreakTarget -> BranchLowering Lower
branchLowering target =
    BranchLowering
        { branchExpression = lowerExpression
        , branchDiscarded = lowerDiscarded
        , branchStatements = lowerBlockInto target . Block
        , branchOperands = sequenceOperands
        , branchFresh = freshGenerated
        , branchType = lowerBoundaryType
        , branchLiteral = lowerLiteral
        }

lowerBlock :: Block ResolvedName Type -> Lower [CoreStatement]
lowerBlock = lowerBlockInto Nothing

{- | Lower the statements of a block in order.

A statement an expression of which never completes ends the block: it is
lowered as the statements that run until control leaves, without the store,
the binding or the test that would have received the value, and the
statements after it, which are never reached, are not lowered. Nothing is
invented for a value that does not exist.
-}
lowerBlockInto :: BreakTarget -> Block ResolvedName Type -> Lower [CoreStatement]
lowerBlockInto target (Block statements) = go statements
    where
        go [] = pure []
        go (statement : remaining) = case neverCompletingExpression statement of
            Just (before, expression) -> do
                leading <- concat <$> mapM (lowerStatementInto target) before
                (leading ++) <$> lowerNeverCompleting (branchLowering target) expression
            Nothing -> (++) <$> lowerStatementInto target statement <*> go remaining

{- | The expression of a statement that is always evaluated and never
completes, with the statements that run before it as part of the same
statement.
-}
neverCompletingExpression ::
    Statement ResolvedName Type -> Maybe ([Statement ResolvedName Type], Expression ResolvedName Type)
neverCompletingExpression statement = case statement of
    BindingStatement _ _ _ _ _ value -> only value
    AssignmentStatement _ _ _ value -> only value
    CompoundAssignmentStatement _ _ _ _ value -> only value
    DiscardStatement _ value -> only value
    ReturnStatement _ (Just value) -> only value
    IfStatement _ condition _ _ -> only condition
    GuardStatement _ condition _ -> only condition
    -- A condition that leaves by a break of its own loop is lowered with
    -- the loop, which that break needs around it.
    WhileStatement _ condition _
        | not (any isBreak (expressionTransfers condition)) -> only condition
    ForStatement _ initializer (Just condition) _ _
        | doesNotComplete condition
        , not (any isBreak (expressionTransfers condition)) ->
            Just (maybe [] (: []) initializer, condition)
    ExpressionStatement _ value _ -> only value
    _ -> Nothing
    where
        only value = if doesNotComplete value then Just ([], value) else Nothing

lowerFunctionBlock :: Type -> Block ResolvedName Type -> Lower [CoreStatement]
lowerFunctionBlock returnType (Block statements) = case reverse statements of
    ExpressionStatement _ expression False : remaining
        | returnType /= unitType
        , not (blockCannotComplete (Block (reverse remaining)))
        , not (doesNotComplete expression) -> do
            prefix <- lowerBlock (Block (reverse remaining))
            (valuePrefix, value) <- lowerExpression expression
            pure (prefix ++ valuePrefix ++ [CoreReturn value])
    _ -> lowerBlock (Block statements)

lowerStatement :: Statement ResolvedName Type -> Lower [CoreStatement]
lowerStatement = lowerStatementInto Nothing

lowerStatementInto :: BreakTarget -> Statement ResolvedName Type -> Lower [CoreStatement]
lowerStatementInto target statement = targeting target (lowerStatementWith target statement)

-- The break target is already the current one.
lowerStatementWith :: BreakTarget -> Statement ResolvedName Type -> Lower [CoreStatement]
lowerStatementWith target statement = case statement of
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
    -- A loop statement has no slot for break values, and a continue in its
    -- body or condition is its own.
    DoWhileStatement _ body condition -> do
        loweredBody <- inCoreLoop (lowerBlock body)
        loweredCondition <- inCoreLoop (targeting Nothing (lowerExpression condition))
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
    BreakStatement _ Nothing -> gets lowerLeave
    BreakStatement spanValue (Just value) -> case target of
        Just (slot, _) -> do
            (prefix, lowered) <- lowerExpression value
            leave <- gets lowerLeave
            pure (prefix ++ [CoreAssign slot lowered] ++ leave)
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
    ContinueStatement _ -> gets lowerContinue
    -- The block runs when the condition is false and never completes
    -- normally, so the statements after the guard follow the empty branch.
    GuardStatement _ condition block -> do
        (prefix, loweredCondition) <- lowerExpression condition
        whenFalse <- lowerBlockInto target block
        pure (prefix ++ [CoreIf loweredCondition [] whenFalse])
    -- Core has no block statement. Every local has its own symbol, so the
    -- statements of a nested block join the enclosing sequence unchanged.
    BlockStatement _ block -> lowerBlockInto target block
    -- The arms of a statement match are statements of the enclosing body:
    -- a break in them leaves the enclosing loop and stores into its slot.
    ExpressionStatement _ (MatchExpression _ subjects arms annotation) _
        | annotation == voidType -> fst <$> lowerMatch (branchLowering target) annotation subjects arms
    ExpressionStatement _ value _ -> lowerDiscarded value

{- | Lower a @while@ or @for@ loop whose body and condition store break
values into the given target. Loops nested in the body are statements of
their own and are lowered without a target.

A @break@ in the condition leaves the loop. The statements of a condition
that has any stand at the top of the Core loop body, so the Core @break@
there leaves the right loop without further work.

A @continue@ in the condition abandons the rest of the condition and
evaluates the condition again; in a @for@ loop the update clause does not
run. In a @while@ loop the condition stands at the top of the Core loop
body, where the Core @continue@ does exactly that. In a @for@ loop the Core
@continue@ would run the update, so a condition that holds a @continue@ is
evaluated in a loop of its own, which the @continue@ repeats and which is
left once the condition has a result. A @break@ in such a condition leaves
that inner loop with the result still false, which leaves the @for@.

A @continue@ in the update clause ends the update, and the condition is
tested next. Core has no such transfer inside an update region, so an update
clause that holds one is wrapped in a loop that runs once: its statements,
then a @break@. The @continue@ becomes a @break@ of that loop.

A @break@ in the update clause leaves the loop, which the Core @break@ in an
update region does. In an update clause that is wrapped, it first sets a
flag that is bound before the loop, and the loop is left after the wrapper
when the flag is set.
-}
lowerLoop :: BreakTarget -> Statement ResolvedName Type -> Lower [CoreStatement]
lowerLoop target loop = case loop of
    WhileStatement _ condition body -> do
        loweredCondition <- inLoop (lowerExpression condition)
        loweredBody <- inCoreLoop (lowerBlockInto target body)
        pure [whileLoop loweredCondition loweredBody]
    ForStatement _ initializer condition updates body -> do
        loweredInitializer <- maybe (pure []) lowerStatement initializer
        evaluatedCondition <- maybe (pure ([], alwaysTrue)) (inLoop . lowerExpression) condition
        loweredCondition <-
            if maybe False (any isContinue . expressionTransfers) condition
                then repeatable evaluatedCondition
                else pure evaluatedCondition
        loweredBody <- inCoreLoop (lowerBlockInto target body)
        let updateTransfers = concatMap statementTransfers updates
            inUpdate = lowerBlockInto target (Block updates)
        (bindings, updateRegion) <-
            if any isContinue updateTransfers
                then
                    if any isBreak updateTransfers
                        then do
                            leave <- freshGenerated "$leave"
                            let leaving = CoreVariable leave boolType
                            loweredUpdates <-
                                transferringWith [CoreBreak] [CoreAssign leave alwaysTrue, CoreBreak] inUpdate
                            pure
                                ( [CoreBind (CoreBinding leave boolType True alwaysFalse)]
                                ,
                                    [ CoreWhile alwaysTrue (loweredUpdates ++ [CoreBreak])
                                    , CoreIf leaving [CoreBreak] []
                                    ]
                                )
                        else do
                            loweredUpdates <- transferringWith [CoreBreak] [CoreBreak] inUpdate
                            pure ([], [CoreWhile alwaysTrue (loweredUpdates ++ [CoreBreak])])
                else do
                    loweredUpdates <- inCoreLoop inUpdate
                    pure ([], loweredUpdates)
        pure (loweredInitializer ++ bindings ++ [forLoop loweredCondition loweredBody updateRegion])
    _ -> lowerStatement loop
    where
        inLoop = inCoreLoop . targeting target
        alwaysTrue = CoreLiteral (CoreBoolean True) boolType
        alwaysFalse = CoreLiteral (CoreBoolean False) boolType
        -- The condition in a loop of its own, which a continue repeats. Its
        -- result is a flag, because the condition may be numeric.
        repeatable (prefix, value) = do
            holds <- freshGenerated "$holds"
            pure
                (
                    [ CoreBind (CoreBinding holds boolType True alwaysFalse)
                    , CoreWhile alwaysTrue (prefix ++ [CoreIf value [CoreAssign holds alwaysTrue] [], CoreBreak])
                    ]
                , CoreVariable holds boolType
                )

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
        -- A right operand that never completes leaves when it is
        -- evaluated, so the operator yields a value only on the path that
        -- skips it, and that value is known: false for `&&`, true for `||`.
        | operator `elem` [LogicalAnd, LogicalOr]
        , doesNotComplete right -> do
            (leftPrefix, loweredLeft) <- lowerExpression left
            leaving <- branching (`lowerNeverCompleting` right)
            let conjunction = operator == LogicalAnd
                skip = if conjunction then CoreIf loweredLeft leaving [] else CoreIf loweredLeft [] leaving
            pure (leftPrefix ++ [skip], CoreLiteral (CoreBoolean (not conjunction)) boolType)
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
    -- A choice that never yields a value is reached here only where the
    -- place that reads it survives its statements, which is the condition
    -- of a do/while loop; a statement whose expression never completes is
    -- lowered by 'lowerBlockInto' without that place. The condition is
    -- never tested, so the constant is not a value of the program.
    ConditionalExpression _ _ _ _ valueType
        | valueType == voidType -> do
            statements <- branching (`lowerNeverCompleting` expression)
            pure (statements, CoreLiteral (CoreBoolean False) boolType)
    MatchExpression _ _ arms valueType
        | valueType == voidType && not (null arms) -> do
            statements <- branching (`lowerNeverCompleting` expression)
            pure (statements, CoreLiteral (CoreBoolean False) boolType)
    -- A branch that does not complete has no value to lower.
    ConditionalExpression _ condition first second valueType
        | any doesNotComplete [first, second] -> do
            loweredCondition <- lowerExpression condition
            branching (\lowering -> lowerSelection lowering valueType loweredCondition first second)
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
    -- A fallback that never completes leaves when the left value is false
    -- in Boolean context, so the operator yields the left value or nothing.
    CoalesceExpression _ left fallback valueType
        | doesNotComplete fallback -> do
            (leftPrefix, loweredLeft) <- lowerExpression left
            leaving <- branching (`lowerNeverCompleting` fallback)
            subjectName <- freshCoalesceSubject
            let loweredType = lowerBoundaryType valueType
                subjectRead = CoreVariable subjectName loweredType
            pure
                ( leftPrefix
                    ++ [ CoreBind (CoreBinding subjectName loweredType False loweredLeft)
                       , CoreIf subjectRead [] leaving
                       ]
                , subjectRead
                )
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
    -- A loop that no break leaves never yields a value; like a choice
    -- that never does, it is reached here only as the condition of a
    -- do/while loop.
    LoopExpression _ loop valueType
        | valueType == voidType -> do
            statements <- lowerLoop Nothing loop
            pure (statements, CoreLiteral (CoreBoolean False) boolType)
    LoopExpression _ loop valueType -> do
        result <- freshGenerated "$loop"
        let loweredType = lowerBoundaryType valueType
        statements <- lowerLoop (Just (result, loweredType)) loop
        pure
            ( CoreBind (CoreBinding result loweredType True (neutralValue loweredType)) : statements
            , CoreVariable result loweredType
            )
    BlockExpression _ block _ -> branching (`lowerValueBlock` block)
    MatchExpression _ subjects arms valueType -> branching (\lowering -> lowerMatch lowering valueType subjects arms)
    CallableExpression _ explicit captures parameters body valueType -> do
        let loweredParameters =
                [(parameterName parameter, lowerBoundaryType (parameterAnnotation parameter)) | parameter <- parameters]
        -- A callable is a function of its own: no loop of the function
        -- that creates it is around its body.
        loweredBody <- inCoreLoop (targeting Nothing (lowerCallableBody body))
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
    CallableExpressionBody expression
        | doesNotComplete expression -> branching (`lowerNeverCompleting` expression)
        | otherwise -> do
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
    BlockExpression _ _ valueType -> valueType
    MatchExpression _ _ _ valueType -> valueType
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
    -- A closure reads, from the place that creates it, its capture
    -- initializers and whatever its body reads that is not its own: its
    -- parameters, its captures and its locals belong to the closure. Without
    -- that, a closure around this one would capture them as if they were
    -- names of its surroundings.
    CoreClosure captures parameters _ body _ ->
        let own =
                map (resolvedSymbol . fst) parameters
                    ++ map (resolvedSymbol . coreCaptureName) captures
                    ++ localSymbols body
         in concatMap (expressionReads . coreCaptureValue) captures
                ++ filter ((`notElem` own) . resolvedSymbol . fst) (statementReads body)

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
    GuardStatement _ condition block -> expressionIds condition ++ blockSymbolIds block
    BlockStatement _ block -> blockSymbolIds block
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
    BlockExpression _ block _ -> blockSymbolIds block
    MatchExpression _ subjects arms _ ->
        [ symbolValue name
        | arm <- arms
        , Just name <- map matchPatternBinding (matchArmPatterns arm)
        ]
            ++ concatMap expressionIds (subjects ++ concatMap matchArmExpressions arms)
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
