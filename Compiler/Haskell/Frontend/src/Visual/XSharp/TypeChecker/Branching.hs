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
    {- ^ The loop context inside a block used as a value: @break@ and
    @continue@ cannot leave such a block.
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
expression is typed in the context that receives the value. A @return@ would
leave the function from the middle of an expression; that is not supported,
and neither is leaving the block with @break@ or @continue@.
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
            [ problem spanValue "VXT0047" "return inside a block used as a value is not supported"
            | not (null returns)
            ]
     in case final of
            Nothing ->
                ( BlockExpression spanValue (Block typedLeading) ErrorType
                , ErrorType
                , leadingProblems
                    ++ returnProblems
                    ++ [problem spanValue "VXT0046" "a block used as a value must end with an expression that has no semicolon"]
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
        (resultType, resultProblems) = case use of
            MatchStatement _ -> (voidType, [])
            MatchValue -> matchValueType spanValue armTypes
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
        armEnvironment =
            [ (resolvedSymbol name, (matchPatternAnnotation patternValue, False))
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
            -- The block of a statement arm is an ordinary statement block.
            (MatchStatement loops, BlockExpression blockSpan (Block statements) _) ->
                let (typedStatements, _, blockReturns, problems) =
                        branchStatements checker armEnvironment loops statements
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
complete normally: its last statement returns, leaves a loop, continues one,
or is an @if@ whose two branches both do, or a nested block that does.
-}
guardBlockProblems :: SourceSpan -> Block name annotation -> [Diagnostic]
guardBlockProblems spanValue block =
    [ problem
        spanValue
        "VXT0061"
        "the else block of a guard must end by leaving the enclosing scope with return, break, or continue"
    | not (blockLeaves block)
    ]
    where
        blockLeaves (Block statements) = case reverse statements of
            final : _ -> statementLeaves final
            [] -> False
        statementLeaves statement = case statement of
            ReturnStatement {} -> True
            BreakStatement {} -> True
            ContinueStatement {} -> True
            IfStatement _ _ whenTrue (Just whenFalse) -> blockLeaves whenTrue && blockLeaves whenFalse
            BlockStatement _ nested -> blockLeaves nested
            _ -> False

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
