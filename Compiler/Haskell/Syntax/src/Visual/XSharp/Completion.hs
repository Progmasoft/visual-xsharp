-- SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
-- SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.1

{- | Whether control can reach the end of a statement or an expression.

This is a fact about control flow and is kept apart from types. A block that
ends with @return@, an @if@ expression whose two blocks both leave, and a
call one of whose arguments is such an expression never produce a value, but
no type says so: the question is answered from the typed tree by the
functions of this module. The type checker uses them to decide what must
have a value, and the desugarer uses the same functions to lower what never
completes as statements alone, so that nothing is stored for a value that
does not exist.

The answers err on the side of completing: a call is assumed to return, and
a condition is assumed to be able to take either value unless it is the
literal @true@. Whether a @break@ or @continue@ has a loop to leave is a
separate rule of the type checker.
-}
module Visual.XSharp.Completion
    ( blockCannotComplete
    , statementCannotComplete
    , doesNotComplete
    , neverCompletingOperand
    , matchArmsAlwaysAccept
    , acceptsEveryValue
    , isUnguarded
    ) where

import Visual.XSharp.AST

{- | Whether control can never reach the end of a block.

A statement cannot complete normally when it is a @return@, a @break@ or a
@continue@; an @if@ with an @else@ whose two blocks both cannot; a nested
block that cannot; a loop whose condition is the constant true, or absent in
a @for@, and that no @break@ leaves; a statement @match@ some arm of which
always accepts and all of whose arms cannot; or any statement an expression
of which is always evaluated and never completes. A block cannot complete
when one of its statements cannot, because the statements after that one are
never reached.
-}
blockCannotComplete :: Block name Type -> Bool
blockCannotComplete (Block statements) = any statementCannotComplete statements

-- | Whether control can never reach the point after a statement.
statementCannotComplete :: Statement name Type -> Bool
statementCannotComplete statement = case statement of
    ReturnStatement {} -> True
    BreakStatement {} -> True
    ContinueStatement {} -> True
    BindingStatement _ _ _ _ _ value -> doesNotComplete value
    AssignmentStatement _ _ _ value -> doesNotComplete value
    CompoundAssignmentStatement _ _ _ _ value -> doesNotComplete value
    DiscardStatement _ value -> doesNotComplete value
    IfStatement _ condition whenTrue whenFalse ->
        doesNotComplete condition
            || maybe False (\block -> blockCannotComplete whenTrue && blockCannotComplete block) whenFalse
    GuardStatement _ condition _ -> doesNotComplete condition
    BlockStatement _ nested -> blockCannotComplete nested
    WhileStatement _ condition body -> doesNotComplete condition || (isConstantTrue condition && not (blockBreaks body))
    DoWhileStatement _ body condition -> isConstantTrue condition && not (blockBreaks body)
    ForStatement _ initializer condition _ body ->
        maybe False statementCannotComplete initializer
            || maybe False doesNotComplete condition
            || (maybe True isConstantTrue condition && not (blockBreaks body))
    ExpressionStatement _ value _ -> doesNotComplete value
    _ -> False
    where
        isConstantTrue expression = case expression of
            LiteralExpression _ (BooleanLiteral True) _ -> True
            _ -> False

{- | Whether evaluating an expression never yields a value.

That is so when a part of it that is always evaluated never does, or when it
is a choice every branch of which leaves: an @if@ expression whose two
blocks do not complete, a block used as a value that cannot complete or
whose final expression does not, or a @match@ one arm of which always
accepts and no arm of which completes. An operand that is evaluated only
sometimes, such as the right operand of @&&@, decides nothing.
-}
doesNotComplete :: Expression name Type -> Bool
doesNotComplete expression = case expression of
    -- The final expression of a block is its last statement, so one walk
    -- of the statements covers it; a second look at it here would double
    -- the work at every level of nested blocks.
    BlockExpression _ block _ -> blockCannotComplete block
    ConditionalExpression _ condition first second _ ->
        doesNotComplete condition || (doesNotComplete first && doesNotComplete second)
    MatchExpression _ subjects arms _ ->
        any doesNotComplete subjects
            || ( not (null arms)
                    && matchArmsAlwaysAccept (subjectTypesOf arms) arms
                    && all (doesNotComplete . matchArmBody) arms
               )
    _ -> case neverCompletingOperand expression of
        Just _ -> True
        Nothing -> False
    where
        -- Every typed pattern carries the type of the value it accepts.
        subjectTypesOf arms = case arms of
            first : _ -> map matchPatternAnnotation (matchArmPatterns first)
            [] -> []

{- | The operands of an expression that are evaluated before its first
operand that never completes, and that operand; nothing when every operand
that is always evaluated completes. Choices between branches are not
operands in this sense and are answered by 'doesNotComplete' itself.
-}
neverCompletingOperand :: Expression name Type -> Maybe ([Expression name Type], Expression name Type)
neverCompletingOperand expression = case break doesNotComplete (alwaysEvaluated expression) of
    (before, operand : _) -> Just (before, operand)
    (_, []) -> Nothing
    where
        alwaysEvaluated value = case value of
            MemberAccessExpression _ receiver _ _ -> [receiver]
            CallExpression _ callee arguments _ -> callee : arguments
            UnaryExpression _ _ operand _ -> [operand]
            BinaryExpression _ operator left right _
                | operator `elem` [LogicalAnd, LogicalOr] -> [left]
                | otherwise -> [left, right]
            IsPatternExpression _ subject _ _ -> [subject]
            CoalesceExpression _ left _ _ -> [left]
            AssignmentExpression _ _ _ operand _ -> [operand]
            MatchExpression _ subjects _ _ -> subjects
            _ -> []

-- | Whether a pattern accepts every value of its subject.
acceptsEveryValue :: MatchPattern name annotation -> Bool
acceptsEveryValue patternValue = case patternValue of
    MatchWildcardPattern {} -> True
    MatchTypePattern {} -> True
    _ -> False

{- | Whether some arm is certain to accept, whatever the subjects are.

That is the case when an arm without a guard has only patterns that accept
every value. It is also the case when every subject is a @bool@ and the arms
without guards accept each combination of @true@ and @false@ between them;
the combinations are enumerated, which is bounded by 'maximumBoolSubjects'.
Nothing else is recognized: a guard may be false, and the literals of a wider
type are never listed in full.
-}
matchArmsAlwaysAccept :: [Type] -> [MatchArm name annotation] -> Bool
matchArmsAlwaysAccept subjectTypes arms = any catchAll unguarded || coversBooleans
    where
        unguarded = filter isUnguarded arms
        catchAll arm = all acceptsEveryValue (matchArmPatterns arm)
        coversBooleans =
            not (null subjectTypes)
                && length subjectTypes <= maximumBoolSubjects
                && all (== boolType) subjectTypes
                && all (\values -> any (`acceptsBooleans` values) unguarded) (combinations (length subjectTypes))
        combinations :: Int -> [[Bool]]
        combinations count = sequence (replicate count [True, False])

{- | The most @bool@ subjects whose combinations are enumerated to decide
whether a match accepts every value. A match with more is complete only
through a catch-all arm; the bound keeps the check linear in practice.
-}
maximumBoolSubjects :: Int
maximumBoolSubjects = 8

-- | Whether an arm's patterns accept the given values of @bool@ subjects.
acceptsBooleans :: MatchArm name annotation -> [Bool] -> Bool
acceptsBooleans arm values =
    length (matchArmPatterns arm) == length values && and (zipWith accepts (matchArmPatterns arm) values)
    where
        accepts patternValue value = case patternValue of
            MatchLiteralPattern _ (BooleanLiteral literal) _ -> literal == value
            _ -> acceptsEveryValue patternValue

-- | Whether an arm has no guard.
isUnguarded :: MatchArm name annotation -> Bool
isUnguarded arm = case matchArmGuard arm of
    Nothing -> True
    Just _ -> False

{- | Whether a @break@ in the block leaves the loop whose body the block is.

Loops nested in the block keep their own breaks. A break in a block used as
a value inside an expression leaves the same loop as one in a statement, so
expressions are searched as well; closures and loops used as expressions
are not, because a break in them cannot leave this loop.
-}
blockBreaks :: Block name annotation -> Bool
blockBreaks (Block statements) = any statementBreaks statements

statementBreaks :: Statement name annotation -> Bool
statementBreaks statement = case statement of
    BreakStatement {} -> True
    BindingStatement _ _ _ _ _ value -> expressionBreaks value
    AssignmentStatement _ _ _ value -> expressionBreaks value
    ReturnStatement _ value -> maybe False expressionBreaks value
    IfStatement _ condition whenTrue whenFalse ->
        expressionBreaks condition || blockBreaks whenTrue || maybe False blockBreaks whenFalse
    WhileStatement {} -> False
    DoWhileStatement {} -> False
    ForStatement _ initializer _ _ _ -> maybe False statementBreaks initializer
    ForEachStatement _ _ _ _ _ source _ -> expressionBreaks source
    IncrementStatement {} -> False
    CompoundAssignmentStatement _ _ _ _ value -> expressionBreaks value
    DiscardStatement _ value -> expressionBreaks value
    ContinueStatement {} -> False
    GuardStatement _ condition block -> expressionBreaks condition || blockBreaks block
    BlockStatement _ block -> blockBreaks block
    ExpressionStatement _ value _ -> expressionBreaks value

expressionBreaks :: Expression name annotation -> Bool
expressionBreaks expression = case expression of
    NameExpression {} -> False
    LiteralExpression {} -> False
    MemberAccessExpression _ receiver _ _ -> expressionBreaks receiver
    CallExpression _ callee arguments _ -> any expressionBreaks (callee : arguments)
    UnaryExpression _ _ value _ -> expressionBreaks value
    BinaryExpression _ _ left right _ -> expressionBreaks left || expressionBreaks right
    IsPatternExpression _ subject _ _ -> expressionBreaks subject
    ConditionalExpression _ condition first second _ -> any expressionBreaks [condition, first, second]
    CoalesceExpression _ left fallback _ -> expressionBreaks left || expressionBreaks fallback
    AssignmentExpression _ _ _ value _ -> expressionBreaks value
    IncrementExpression {} -> False
    LoopExpression {} -> False
    BlockExpression _ block _ -> blockBreaks block
    MatchExpression _ subjects arms _ ->
        any expressionBreaks subjects
            || any (\arm -> maybe False expressionBreaks (matchArmGuard arm) || expressionBreaks (matchArmBody arm)) arms
    CallableExpression {} -> False
