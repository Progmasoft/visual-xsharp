-- SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
-- SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.1

{- | Typing rules of value blocks, @match@, and @guard@.

The rules need the checker for expressions and statements, which the main
type checker owns together with its template and loop context. They receive
it through 'BranchChecker', already applied to that context, so this module
states the rules of the branching forms without knowing how names, templates,
or loops are tracked.
-}
module Visual.XSharp.TypeChecker.Branching
    ( BranchChecker (..)
    , BranchEnvironment
    , MatchUse (..)
    , checkValueBlock
    , checkMatch
    , guardBlockProblems
    , matchArmsAlwaysAccept
    , blockCannotComplete
    , valueBlockLeaves
    , noValueProblem
    ) where

import Visual.XSharp.AST
import Visual.XSharp.Diagnostic
import Visual.XSharp.NumericSemantics

-- | Type and mutability of every local in scope, innermost first.
type BranchEnvironment = [(SymbolId, (Type, Bool))]

-- | The parts of the main type checker that the branching rules are built on.
data BranchChecker loops = BranchChecker
    { branchExpression ::
        BranchEnvironment ->
        Maybe Type ->
        Expression ResolvedName () ->
        (Expression ResolvedName Type, Type, [Diagnostic])
    -- ^ Check an expression, optionally in the context of an expected type.
    , branchStatements ::
        BranchEnvironment ->
        loops ->
        [Statement ResolvedName ()] ->
        ([Statement ResolvedName Type], BranchEnvironment, [Type], [Diagnostic])
    {- ^ Check statements in order. The result also has the environment after
    them and the types of the values their @return@ statements carry.
    -}
    , branchType :: TypeSyntax -> (Type, [Diagnostic])
    -- ^ Resolve a type written in source.
    , branchLiteral :: SourceSpan -> Maybe Type -> Literal -> (Type, [Diagnostic])
    -- ^ Type a literal in the context of an expected type.
    , branchHasEffect :: Expression ResolvedName () -> Bool
    -- ^ Whether evaluating an expression can do more than produce a value.
    , branchValueLoops :: loops
    {- ^ The loop context inside a block used as a value: the loops around
    the expression the block belongs to, seen across the edge of the block.
    -}
    , branchReturnType :: Type
    {- ^ The type a @return@ carries here, or the error type when the
    enclosing function does not declare one.
    -}
    }

-- | How a @match@ is used, which decides what its arms must provide.
data MatchUse loops
    = {- | An expression: every arm yields a value of one type, and some arm
      accepts whatever the subjects are.
      -}
      MatchValue
    | {- | A statement inside the given loop context: the arms are statements
      and it is not an error when no arm accepts.
      -}
      MatchStatement loops

problem :: SourceSpan -> String -> String -> Diagnostic
problem spanValue code message = Diagnostic TypeCheckerStage Error code (Just spanValue) message

{- | Check a block used as a value.

Its value is its final expression, written without a semicolon, and that
expression is typed in the context that receives the value. A block may
instead leave: with @return@ it leaves the enclosing function, with @break@
or @continue@ the enclosing loop, under the rules those statements have
anywhere else. A block that cannot complete normally produces no value and
needs no final expression; it is typed @void@, and the form that holds it
takes its type from the blocks that do complete. A block that can complete
normally must end with its value.

A @return@ in such a block is checked against the declared return type of
the enclosing function. Where that type is inferred from the returns of the
body, a return reached through an expression is not collected yet and is
rejected.
-}
checkValueBlock ::
    BranchChecker loops ->
    BranchEnvironment ->
    Maybe Type ->
    SourceSpan ->
    Block ResolvedName () ->
    (Expression ResolvedName Type, Type, [Diagnostic])
checkValueBlock checker environment expected spanValue (Block statements) =
    let (leading, final) = case reverse statements of
            ExpressionStatement finalSpan value False : before -> (reverse before, Just (finalSpan, value))
            _ -> (statements, Nothing)
        (typedLeading, inner, returns, leadingProblems) =
            branchStatements checker environment (branchValueLoops checker) leading
        returnProblems =
            [ problem
                spanValue
                "VXT0047"
                "return inside a block used as a value requires a declared return type and is not implemented inside a loop used as an expression"
            | not (null returns)
            , branchReturnType checker == ErrorType
            ]
     in case final of
            Nothing
                | blockCannotComplete (Block typedLeading) ->
                    (BlockExpression spanValue (Block typedLeading) voidType, voidType, leadingProblems ++ returnProblems)
                | otherwise ->
                    ( BlockExpression spanValue (Block typedLeading) ErrorType
                    , ErrorType
                    , leadingProblems
                        ++ returnProblems
                        ++ [ problem
                                spanValue
                                "VXT0046"
                                "a block used as a value must end with an expression that has no semicolon, or leave on every path"
                           ]
                    )
            Just (finalSpan, value) ->
                let (typedValue, valueType, valueProblems) = branchExpression checker inner expected value
                 in ( BlockExpression
                        spanValue
                        (Block (typedLeading ++ [ExpressionStatement finalSpan typedValue False]))
                        valueType
                    , valueType
                    , leadingProblems ++ returnProblems ++ valueProblems
                    )

{- | Check a @match@.

The subjects are typed first, without context. Every arm is then checked
against them: its patterns, its guard in the scope of the names the patterns
bind, and its body. The result also carries the types of the values returned
by @return@ statements in the arms of a statement match.
-}
checkMatch ::
    BranchChecker loops ->
    MatchUse loops ->
    BranchEnvironment ->
    Maybe Type ->
    SourceSpan ->
    [Expression ResolvedName ()] ->
    [MatchArm ResolvedName ()] ->
    (Expression ResolvedName Type, Type, [Type], [Diagnostic])
checkMatch checker use environment expected spanValue subjects arms =
    let checkedSubjects = [branchExpression checker environment Nothing subject | subject <- subjects]
        typedSubjects = [typed | (typed, _, _) <- checkedSubjects]
        subjectTypes = [valueType | (_, valueType, _) <- checkedSubjects]
        subjectProblems =
            concat [problems | (_, _, problems) <- checkedSubjects]
                ++ [ problem
                        (expressionSpanOf typed)
                        "VXT0058"
                        "match subjects currently support only bool and numeric values"
                   | (typed, valueType) <- zip typedSubjects subjectTypes
                   , valueType /= ErrorType
                   , not (acceptsBooleanContext valueType)
                   ]
        (typedArms, armTypes, returns, armProblems) = checkArms subjectTypes expected arms
        -- An arm whose block leaves produces no value; the arms that
        -- complete give the match its type.
        valueTypes = [armType | (arm, armType) <- zip typedArms armTypes, not (valueBlockLeaves (matchArmBody arm))]
        (resultType, resultProblems) = case use of
            MatchStatement _ -> (voidType, [])
            MatchValue
                | null valueTypes && not (null typedArms) -> (ErrorType, [noValueProblem spanValue])
                | otherwise -> matchValueType spanValue valueTypes
        coverageProblems = case use of
            MatchStatement _ -> []
            MatchValue ->
                [ problem
                    spanValue
                    "VXT0052"
                    "a match used as an expression must accept every value of its subjects; add a '_' arm"
                | not (matchArmsAlwaysAccept subjectTypes typedArms)
                ]
     in ( MatchExpression spanValue typedSubjects typedArms resultType
        , resultType
        , returns
        , subjectProblems ++ armProblems ++ unreachableArmProblems typedArms ++ resultProblems ++ coverageProblems
        )
    where
        -- An arm made only of untyped literals takes its type from the
        -- context, and without one from the first arm that has a type.
        checkArms _ _ [] = ([], [], [], [])
        checkArms subjectTypes context (arm : remaining) =
            let (typedArm, armType, armReturns, problems) = checkArm checker use environment context subjectTypes arm
                nextContext = case context of
                    Just _ -> context
                    Nothing -> if armType == ErrorType || armType == voidType then Nothing else Just armType
                (later, laterTypes, laterReturns, laterProblems) = checkArms subjectTypes nextContext remaining
             in (typedArm : later, armType : laterTypes, armReturns ++ laterReturns, problems ++ laterProblems)

checkArm ::
    BranchChecker loops ->
    MatchUse loops ->
    BranchEnvironment ->
    Maybe Type ->
    [Type] ->
    MatchArm ResolvedName () ->
    (MatchArm ResolvedName Type, Type, [Type], [Diagnostic])
checkArm checker use environment expected subjectTypes (MatchArm spanValue patterns guard body) =
    let arityProblems =
            [ problem
                spanValue
                "VXT0048"
                ( "a match arm needs one pattern for each subject: expected "
                    ++ show (length subjectTypes)
                    ++ ", found "
                    ++ show (length patterns)
                )
            | length patterns /= length subjectTypes
            ]
        -- A surplus pattern has no subject; it is checked against the error
        -- type so that it reports nothing beyond the arity problem.
        checkedPatterns = zipWith (checkMatchPattern checker) (subjectTypes ++ repeat ErrorType) patterns
        typedPatterns = map fst checkedPatterns
        patternProblems = concatMap snd checkedPatterns
        -- A pattern binding is an ordinary local: it may be assigned, like
        -- every binding that is not declared final.
        armEnvironment =
            [ (resolvedSymbol name, (matchPatternAnnotation patternValue, True))
            | patternValue <- reverse typedPatterns
            , Just name <- [matchPatternBinding patternValue]
            ]
                ++ environment
        (typedGuard, guardProblems) = case guard of
            Nothing -> (Nothing, [])
            Just condition ->
                let (typed, conditionType, problems) = branchExpression checker armEnvironment Nothing condition
                    mismatch =
                        [ problem (expressionSpanOf typed) "VXT0049" "match guard must be bool or numeric"
                        | conditionType /= ErrorType
                        , not (acceptsBooleanContext conditionType)
                        ]
                 in (Just typed, problems ++ mismatch)
        (typedBody, bodyType, returns, bodyProblems) = case (use, body) of
            -- The block of a statement arm is an ordinary statement block. A
            -- match in its last position was parsed as the value of the
            -- block; here nothing takes that value, so it is a statement.
            (MatchStatement loops, BlockExpression blockSpan (Block statements) _) ->
                let (typedStatements, _, blockReturns, problems) =
                        branchStatements checker armEnvironment loops (lastMatchAsStatement statements)
                 in (BlockExpression blockSpan (Block typedStatements) voidType, voidType, blockReturns, problems)
            (MatchStatement _, expression) ->
                let (typed, _, problems) = branchExpression checker armEnvironment Nothing expression
                    effectProblems =
                        [ problem (expressionSpanOf typed) "VXT0013" "pure value expression cannot be used as a statement"
                        | not (branchHasEffect checker expression)
                        ]
                 in (typed, voidType, [], problems ++ effectProblems)
            (MatchValue, expression) ->
                let (typed, valueType, problems) = branchExpression checker armEnvironment expected expression
                 in (typed, valueType, [], problems)
     in ( MatchArm spanValue typedPatterns typedGuard typedBody
        , bodyType
        , returns
        , arityProblems ++ patternProblems ++ guardProblems ++ bodyProblems
        )

lastMatchAsStatement :: [Statement name annotation] -> [Statement name annotation]
lastMatchAsStatement statements = case reverse statements of
    ExpressionStatement spanValue value@MatchExpression {} False : before ->
        reverse (ExpressionStatement spanValue value True : before)
    _ -> statements

{- | Check one pattern against the type of its subject.

A literal is typed as the other operand of a comparison with the subject, so
the lowering compares two values of one type. A type pattern can only name
the subject's own type: there are no class hierarchies to test against yet,
so its use is to bind the value. The annotation of a typed pattern is the
type of the value it accepts.
-}
checkMatchPattern ::
    BranchChecker loops -> Type -> MatchPattern ResolvedName () -> (MatchPattern ResolvedName Type, [Diagnostic])
checkMatchPattern checker subjectType patternValue = case patternValue of
    MatchWildcardPattern spanValue _ -> (MatchWildcardPattern spanValue subjectType, [])
    MatchLiteralPattern spanValue literal _ ->
        let (literalType, literalProblems) = branchLiteral checker spanValue (contextOf subjectType) literal
            rule = binaryNumericRule Equal subjectType literalType
            ruleProblems = case numericRuleError rule of
                Just issue | subjectType /= ErrorType -> [problem spanValue "VXT0054" (renderNumericRuleError issue)]
                _ -> []
         in (MatchLiteralPattern spanValue literal literalType, literalProblems ++ ruleProblems)
    MatchNullPattern spanValue _ ->
        ( MatchNullPattern spanValue subjectType
        , [problem spanValue "VXT0055" "a null pattern requires a reference subject, which match does not support yet"]
        )
    MatchCasePattern spanValue name _ ->
        ( MatchCasePattern spanValue name subjectType
        , [problem spanValue "VXT0056" "enum case patterns require enum declarations, which are not implemented"]
        )
    MatchTypePattern spanValue syntax name _ ->
        let (namedTypeValue, syntaxProblems) = branchType checker syntax
            relationProblems =
                [ problem
                    spanValue
                    "VXT0057"
                    "a type pattern must name the type of its subject; class hierarchies are not implemented"
                | namedTypeValue /= ErrorType
                , subjectType /= ErrorType
                , namedTypeValue /= subjectType
                ]
         in (MatchTypePattern spanValue syntax name subjectType, syntaxProblems ++ relationProblems)
    where
        contextOf valueType = if valueType == ErrorType then Nothing else Just valueType

{- | Result type of a match expression from the types of its arms.

The value is materialized in one storage slot, like the result of a
conditional expression, so every arm has the same type, and only bool and
numeric results are lowered today.
-}
matchValueType :: SourceSpan -> [Type] -> (Type, [Diagnostic])
matchValueType spanValue armTypes = case filter (/= ErrorType) armTypes of
    [] -> (ErrorType, [])
    first : remaining
        | any (/= first) remaining ->
            (first, [problem spanValue "VXT0050" "the arms of a match used as an expression must have the same type"])
        | not (acceptsBooleanContext first) ->
            (first, [problem spanValue "VXT0051" "match expressions currently support only bool and numeric results"])
        | otherwise -> (first, [])

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

{- | Arms that can never be selected because an earlier arm without a guard
accepts everything they accept: pattern by pattern, the earlier one accepts
every value or names the same literal.
-}
unreachableArmProblems :: [MatchArm name annotation] -> [Diagnostic]
unreachableArmProblems = go []
    where
        go _ [] = []
        go earlier (arm : remaining) =
            [ problem
                (matchArmSpan arm)
                "VXT0053"
                "this match arm can never be selected: an earlier arm accepts everything it accepts"
            | any (`shadows` arm) earlier
            ]
                ++ go (earlier ++ [arm]) remaining
        shadows first second =
            isUnguarded first
                && length (matchArmPatterns first) == length (matchArmPatterns second)
                && and (zipWith covers (matchArmPatterns first) (matchArmPatterns second))
        covers first second = case (first, second) of
            (MatchLiteralPattern _ left _, MatchLiteralPattern _ right _) -> left == right
            _ -> acceptsEveryValue first

isUnguarded :: MatchArm name annotation -> Bool
isUnguarded arm = case matchArmGuard arm of
    Nothing -> True
    Just _ -> False

{- | Problems of the else block of a @guard@.

The statements after a guard rely on its condition, so the block must not
complete normally on any path. That is decided from its control flow by
'blockCannotComplete', not from the spelling of its last statement.
-}
guardBlockProblems :: SourceSpan -> Block name Type -> [Diagnostic]
guardBlockProblems spanValue block =
    [ problem
        spanValue
        "VXT0061"
        "the else block of a guard can complete normally; every path through it must leave the enclosing scope"
    | not (blockCannotComplete block)
    ]

-- | The diagnostic for an @if@ or a @match@ expression none of whose branches yields a value.
noValueProblem :: SourceSpan -> Diagnostic
noValueProblem spanValue =
    problem
        spanValue
        "VXT0062"
        "every branch of this expression leaves it, so it has no value; write it as a statement"

{- | Whether a block used as a value leaves instead of yielding a value: it
has no final expression and cannot complete normally.
-}
valueBlockLeaves :: Expression name Type -> Bool
valueBlockLeaves expression = case expression of
    BlockExpression _ block@(Block statements) _ -> not (endsWithValue statements) && blockCannotComplete block
    _ -> False
    where
        endsWithValue statements = case reverse statements of
            ExpressionStatement _ _ False : _ -> True
            _ -> False

{- | Whether control can never reach the end of a block.

A statement cannot complete normally when it is a @return@, a @break@ or a
@continue@; an @if@ with an @else@ whose two blocks both cannot; a nested
block that cannot; a loop whose condition is the constant true, or absent in
a @for@, and that no @break@ leaves; or a statement @match@ some arm of which
always accepts and all of whose arms are blocks that cannot. A block cannot
complete when one of its statements cannot, because the statements after
that one are never reached.

The answer errs on the side of completing: a call is assumed to return, and
a condition is assumed to be able to take either value unless it is the
literal @true@. Whether a @break@ or @continue@ has a loop to leave is a
separate rule, reported where the statement stands.
-}
blockCannotComplete :: Block name Type -> Bool
blockCannotComplete (Block statements) = any statementCannotComplete statements

statementCannotComplete :: Statement name Type -> Bool
statementCannotComplete statement = case statement of
    ReturnStatement {} -> True
    BreakStatement {} -> True
    ContinueStatement {} -> True
    IfStatement _ _ whenTrue (Just whenFalse) -> blockCannotComplete whenTrue && blockCannotComplete whenFalse
    BlockStatement _ nested -> blockCannotComplete nested
    WhileStatement _ condition body -> isConstantTrue condition && not (blockBreaks body)
    DoWhileStatement _ body condition -> isConstantTrue condition && not (blockBreaks body)
    ForStatement _ _ condition _ body -> maybe True isConstantTrue condition && not (blockBreaks body)
    ExpressionStatement _ (MatchExpression _ _ arms annotation) _
        | annotation == voidType ->
            matchArmsAlwaysAccept (subjectTypesOf arms) arms && all (armCannotComplete . matchArmBody) arms
    _ -> False
    where
        isConstantTrue expression = case expression of
            LiteralExpression _ (BooleanLiteral True) _ -> True
            _ -> False
        armCannotComplete body = case body of
            BlockExpression _ block _ -> blockCannotComplete block
            _ -> False
        -- Every typed pattern carries the type of the value it accepts.
        subjectTypesOf arms = case arms of
            first : _ -> map matchPatternAnnotation (matchArmPatterns first)
            [] -> []

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

expressionSpanOf :: Expression name annotation -> SourceSpan
expressionSpanOf expression = case expression of
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
