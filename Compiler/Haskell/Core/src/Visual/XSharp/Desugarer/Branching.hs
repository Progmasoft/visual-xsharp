-- SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
-- SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.1

{- | Lowering of value blocks and of @match@ to Core statements.

Core has no block expression and no match. Both lower to the pair that every
storing expression lowers to: statements that run first, and a store-free
expression for the value. A match becomes a chain of 'CoreIf' statements that
test the arms in order, so exactly one body runs and no Core expression, wire
format, or later stage changes.

The lowering of the expressions and blocks inside these forms belongs to the
main desugarer, which passes it in through 'BranchLowering'.
-}
module Visual.XSharp.Desugarer.Branching
    ( BranchLowering (..)
    , lowerValueBlock
    , lowerMatch
    , maximumNestedArms
    ) where

import Visual.XSharp.AST
import Visual.XSharp.Core
import Visual.XSharp.Desugarer.Sequencing

-- | The parts of the main desugarer that the branching forms are built on.
data BranchLowering lower = BranchLowering
    { branchExpression :: Expression ResolvedName Type -> lower Lowered
    -- ^ Lower an expression to its statements and its value.
    , branchDiscarded :: Expression ResolvedName Type -> lower [CoreStatement]
    -- ^ Lower an expression whose value is dropped.
    , branchStatements :: [Statement ResolvedName Type] -> lower [CoreStatement]
    {- ^ Lower statements in statement position. The caller decides where a
    @break value;@ among them stores its value.
    -}
    , branchOperands :: [Lowered] -> lower ([CoreStatement], [CoreExpression])
    -- ^ Combine operands that are all evaluated, in order.
    , branchFresh :: String -> lower ResolvedName
    -- ^ A fresh compiler-generated local with the given name prefix.
    , branchType :: Type -> Type
    -- ^ The Core type of a source type.
    , branchLiteral :: Type -> Literal -> CoreLiteral
    -- ^ The Core literal of a source literal of the given Core type.
    }

{- | Lower a block used as a value.

The leading statements run in order; the value is the final expression. The
type checker has established that the block ends with one and that nothing
in it leaves the block early.
-}
lowerValueBlock :: (Monad lower) => BranchLowering lower -> Block ResolvedName Type -> lower Lowered
lowerValueBlock lowering (Block statements) = case reverse statements of
    ExpressionStatement _ value False : before -> do
        leading <- branchStatements lowering (reverse before)
        (prefix, lowered) <- branchExpression lowering value
        pure (leading ++ prefix, lowered)
    _ -> do
        leading <- branchStatements lowering statements
        pure (leading, CoreLiteral CoreUnit unitType)

{- | Lower a @match@.

The subjects are evaluated once, left to right, and each is bound to a local
so that every arm tests the same values. The arms then nest: an arm that does
not accept continues with the arms after it, in its else branch.

A match whose type is @void@ is the statement form: its arm bodies run for
their effects and its value is the unit literal. Otherwise every arm stores
its value into a result slot, which the type checker guarantees is assigned
on every path, because some arm always accepts.

The nesting is bounded. A match with more than 'maximumNestedArms' arms is
lowered in groups of that size: the groups follow each other in one statement
sequence, a Boolean slot records that an arm was taken, and every group after
the first runs only while the slot is still false.
-}
lowerMatch ::
    (Monad lower) =>
    BranchLowering lower ->
    Type ->
    [Expression ResolvedName Type] ->
    [MatchArm ResolvedName Type] ->
    lower Lowered
lowerMatch lowering annotation subjects arms = do
    loweredSubjects <- mapM (branchExpression lowering) subjects
    (subjectPrefix, subjectValues) <- branchOperands lowering loweredSubjects
    subjectNames <- mapM (const (branchFresh lowering "$subject")) subjectValues
    let subjectTypes = map expressionType subjectValues
        subjectBindings =
            [ CoreBind (CoreBinding name valueType False value)
            | (name, valueType, value) <- zip3 subjectNames subjectTypes subjectValues
            ]
        subjectReads = zipWith CoreVariable subjectNames subjectTypes
    if annotation == voidType
        then do
            chain <- lowerArmGroups lowering Nothing subjectReads arms
            pure (subjectPrefix ++ subjectBindings ++ chain, CoreLiteral CoreUnit unitType)
        else do
            result <- branchFresh lowering "$matched"
            let resultType = branchType lowering annotation
            chain <- lowerArmGroups lowering (Just result) subjectReads arms
            pure
                ( subjectPrefix
                    ++ subjectBindings
                    ++ CoreBind (CoreBinding result resultType True (neutralValue resultType))
                    : chain
                , CoreVariable result resultType
                )

{- | The most arms that are lowered as one chain of nested statements.

Every arm of a chain adds one level of nesting to the Core it lowers to, and
the stages after Core walk nested statements recursively. A source with a
few hundred arms must not turn into a few hundred levels, so longer matches
are split into groups of this size.
-}
maximumNestedArms :: Int
maximumNestedArms = 16

{- | Lower all arms, in groups of at most 'maximumNestedArms'.

A match that fits in one group is a single chain and needs no slot. A longer
one binds a mutable @$taken@ slot; each body sets it before it runs, and
each group after the first is the else branch of a test of the slot.
-}
lowerArmGroups ::
    (Monad lower) =>
    BranchLowering lower ->
    Maybe ResolvedName ->
    [CoreExpression] ->
    [MatchArm ResolvedName Type] ->
    lower [CoreStatement]
lowerArmGroups lowering result subjects arms
    | length arms <= maximumNestedArms = lowerArms lowering result Nothing subjects arms
    | otherwise = do
        taken <- branchFresh lowering "$taken"
        groups <- mapM (lowerArms lowering result (Just taken) subjects) (groupsOf maximumNestedArms arms)
        let takenRead = CoreVariable taken boolType
            sequenced = case groups of
                [] -> []
                first : later -> first ++ [CoreIf takenRead [] group | group <- later]
        pure (CoreBind (CoreBinding taken boolType True (CoreLiteral (CoreBoolean False) boolType)) : sequenced)

groupsOf :: Int -> [value] -> [[value]]
groupsOf size values = case splitAt size values of
    ([], _) -> []
    (group, remaining) -> group : groupsOf size remaining

{- | Lower the arms from the given one on.

The names an arm binds are bound before its test: binding a scalar that was
already evaluated has no effect of its own, and the guard may read them. An
arm that always accepts ends the chain; the type checker has rejected any arm
after it.
-}
lowerArms ::
    (Monad lower) =>
    BranchLowering lower ->
    Maybe ResolvedName ->
    Maybe ResolvedName ->
    [CoreExpression] ->
    [MatchArm ResolvedName Type] ->
    lower [CoreStatement]
lowerArms _ _ _ _ [] = pure []
lowerArms lowering result taken subjects (arm : remaining) = do
    let paired = zip (matchArmPatterns arm) subjects
        bindings =
            [ CoreBind (CoreBinding name (expressionType subject) False subject)
            | (MatchTypePattern _ _ (Just name) _, subject) <- paired
            ]
        tests = concatMap (uncurry (patternTests lowering)) paired
    armBody <- lowerArmBody lowering result (matchArmBody arm)
    -- The slot is set before the body runs, because the body may leave the
    -- function or the enclosing loop.
    let body = [CoreAssign slot (CoreLiteral (CoreBoolean True) boolType) | Just slot <- [taken]] ++ armBody
    later <- lowerArms lowering result taken subjects remaining
    case (tests, matchArmGuard arm) of
        ([], Nothing) -> pure (bindings ++ body)
        (_, Nothing) -> pure (bindings ++ [CoreIf (conjunction tests) body later])
        ([], Just guard) -> do
            (guardPrefix, guardValue) <- branchExpression lowering guard
            pure (bindings ++ guardPrefix ++ [CoreIf guardValue body later])
        -- The guard runs only when the patterns accept, and its statements
        -- with it. The decision is a Boolean slot, so a numeric guard is
        -- tested in Boolean context and never stored.
        (_, Just guard) -> do
            loweredGuard <- branchExpression lowering guard
            accepted <- branchFresh lowering "$accepted"
            let (statements, decision) = decideLogical True accepted (conjunction tests) loweredGuard
            pure (bindings ++ statements ++ [CoreIf decision body later])

{- | The comparisons one pattern needs against its subject.

A pattern that accepts every value needs none. The @null@ and enum case
patterns are rejected by the type checker; they lower to a test that never
holds so that a tree which reached here by mistake cannot select their arm.
-}
patternTests ::
    BranchLowering lower -> MatchPattern ResolvedName Type -> CoreExpression -> [CoreExpression]
patternTests lowering patternValue subject = case patternValue of
    MatchWildcardPattern {} -> []
    MatchTypePattern {} -> []
    MatchLiteralPattern _ literal literalType ->
        let loweredType = branchType lowering literalType
         in [CorePrimitive CoreEqual [subject, CoreLiteral (branchLiteral lowering loweredType literal) loweredType] boolType]
    MatchNullPattern {} -> [CoreLiteral (CoreBoolean False) boolType]
    MatchCasePattern {} -> [CoreLiteral (CoreBoolean False) boolType]

-- Every test is a Boolean without effects, so the short-circuit primitive
-- only decides how many comparisons run.
conjunction :: [CoreExpression] -> CoreExpression
conjunction tests = case tests of
    [] -> CoreLiteral (CoreBoolean True) boolType
    [test] -> test
    test : remaining -> CorePrimitive CoreLogicalAnd [test, conjunction remaining] boolType

{- | Lower the body of an arm.

With a result slot, the body is an expression whose value is stored. Without
one, a block body is a statement block and any other body is evaluated for
its effects.
-}
lowerArmBody ::
    (Monad lower) => BranchLowering lower -> Maybe ResolvedName -> Expression ResolvedName Type -> lower [CoreStatement]
lowerArmBody lowering result body = case (result, body) of
    (Just slot, _) -> do
        (prefix, value) <- branchExpression lowering body
        pure (prefix ++ [CoreAssign slot value])
    (Nothing, BlockExpression _ (Block statements) _) -> branchStatements lowering statements
    (Nothing, _) -> branchDiscarded lowering body
