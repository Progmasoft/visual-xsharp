-- SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
-- SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.1

{- | What the lowering needs to know to evaluate a value by need.

Visual X# is a lazy language: a value is computed when it is first needed and
at most once, and a value that is never needed is never computed. Its effects
are not lazy: a store into a variable and a transfer of control happen where
they are written. The source has no notation for either fact; the compiler
tells the two kinds of expression apart.

This module holds that distinction for the initializer of a local binding.
An initializer may be deferred when evaluating it later gives what evaluating
it now would give: it stores nothing, transfers nowhere, and reads the
variables it names as they are where the binding stands. Deferring it is
observable only when it might fail or never return, so only then is it worth
a flag and a test at every use.
-}
module Visual.XSharp.Desugarer.Laziness
    ( deferrableExpression
    , worthDeferring
    , expressionNames
    , renameNames
    , capturedSymbols
    , alwaysReads
    , neededNext
    ) where

import Visual.XSharp.AST
import Visual.XSharp.Core

{- | Whether evaluating an expression at a later point of the same function
gives the value that evaluating it in place would give, provided the
variables it reads still have the values they had.

That holds for expressions built from names, literals, operators, tests and
calls: a call cannot store into a local of its caller. It does not hold for
an expression that stores, that leaves, or that contains statements.
-}
deferrableExpression :: Expression name annotation -> Bool
deferrableExpression expression = case expression of
    NameExpression {} -> True
    LiteralExpression {} -> True
    MemberAccessExpression _ receiver _ _ -> deferrableExpression receiver
    CallExpression _ callee arguments _ -> all deferrableExpression (callee : arguments)
    UnaryExpression _ _ value _ -> deferrableExpression value
    BinaryExpression _ _ left right _ -> deferrableExpression left && deferrableExpression right
    IsPatternExpression _ subject _ _ -> deferrableExpression subject
    ConditionalExpression _ condition first second _ -> all deferrableExpression [condition, first, second]
    CoalesceExpression {} -> False
    AssignmentExpression {} -> False
    IncrementExpression {} -> False
    LoopExpression {} -> False
    BlockExpression {} -> False
    MatchExpression {} -> False
    CallableExpression {} -> False

{- | Whether an expression can fail or never return, which is when it matters
whether it is evaluated at all: it calls something, or it divides.
-}
worthDeferring :: Expression name annotation -> Bool
worthDeferring expression = case expression of
    CallExpression {} -> True
    BinaryExpression _ operator left right _ ->
        operator `elem` [Divide, FloorDivide, Remainder] || worthDeferring left || worthDeferring right
    MemberAccessExpression _ receiver _ _ -> worthDeferring receiver
    UnaryExpression _ _ value _ -> worthDeferring value
    IsPatternExpression _ subject _ _ -> worthDeferring subject
    ConditionalExpression _ condition first second _ -> any worthDeferring [condition, first, second]
    _ -> False

-- | The names an expression that may be deferred reads, with their types.
expressionNames :: Expression name annotation -> [(name, annotation)]
expressionNames expression = case expression of
    NameExpression _ name annotation -> [(name, annotation)]
    MemberAccessExpression _ receiver _ _ -> expressionNames receiver
    CallExpression _ callee arguments _ -> concatMap expressionNames (callee : arguments)
    UnaryExpression _ _ value _ -> expressionNames value
    BinaryExpression _ _ left right _ -> expressionNames left ++ expressionNames right
    IsPatternExpression _ subject _ _ -> expressionNames subject
    ConditionalExpression _ condition first second _ -> concatMap expressionNames [condition, first, second]
    _ -> []

{- | Replace the names an expression that may be deferred reads. A name that
has no replacement is kept.
-}
renameNames :: (name -> name) -> Expression name annotation -> Expression name annotation
renameNames rename expression = case expression of
    NameExpression spanValue name annotation -> NameExpression spanValue (rename name) annotation
    MemberAccessExpression spanValue receiver member annotation ->
        MemberAccessExpression spanValue (renameNames rename receiver) member annotation
    CallExpression spanValue callee arguments annotation ->
        CallExpression spanValue (renameNames rename callee) (map (renameNames rename) arguments) annotation
    UnaryExpression spanValue operator value annotation ->
        UnaryExpression spanValue operator (renameNames rename value) annotation
    BinaryExpression spanValue operator left right annotation ->
        BinaryExpression spanValue operator (renameNames rename left) (renameNames rename right) annotation
    IsPatternExpression spanValue subject patternValue annotation ->
        IsPatternExpression spanValue (renameNames rename subject) patternValue annotation
    ConditionalExpression spanValue condition first second annotation ->
        ConditionalExpression
            spanValue
            (renameNames rename condition)
            (renameNames rename first)
            (renameNames rename second)
            annotation
    _ -> expression

{- | Whether evaluating an expression always reads the given name, whatever
values it meets: the name stands where neither a short-circuit operator nor
a conditional can skip it. Blocks, matches, loops and callables used as
values are not searched; the answer for them is that it is not known.
-}
alwaysReads :: (Eq name) => name -> Expression name annotation -> Bool
alwaysReads name expression = case expression of
    NameExpression _ found _ -> found == name
    MemberAccessExpression _ receiver _ _ -> alwaysReads name receiver
    CallExpression _ callee arguments _ -> any (alwaysReads name) (callee : arguments)
    UnaryExpression _ _ value _ -> alwaysReads name value
    BinaryExpression _ operator left right _
        | operator `elem` [LogicalAnd, LogicalOr] -> alwaysReads name left
        | otherwise -> alwaysReads name left || alwaysReads name right
    IsPatternExpression _ subject _ _ -> alwaysReads name subject
    ConditionalExpression _ condition _ _ _ -> alwaysReads name condition
    AssignmentExpression _ _ _ value _ -> alwaysReads name value
    _ -> False

{- | Whether the statement that follows a binding is certain to read it.

A value that is certain to be needed by the very next statement may be
computed where its binding stands: nothing can be observed between the two
places except which of two computations fails first when both fail. That
saves the flag and the test for the common case of a value that is used at
once.

The next statement reads the name for certain when the expression it
evaluates first always reads it. A binding evaluates its initializer first
only when it is itself evaluated in place, which the second argument
decides for it from the statements after it.
-}
neededNext ::
    (Eq name) =>
    -- | Whether a later binding is evaluated in place, given what follows it.
    (name -> annotation -> Expression name annotation -> [Statement name annotation] -> Bool) ->
    name ->
    [Statement name annotation] ->
    Bool
neededNext inPlaceBinding name following = case following of
    next : later -> case next of
        BindingStatement _ _ _ bound annotation value ->
            alwaysReads name value && inPlaceBinding bound annotation value later
        AssignmentStatement _ _ _ value -> alwaysReads name value
        CompoundAssignmentStatement _ _ _ _ value -> alwaysReads name value
        ReturnStatement _ (Just value) -> alwaysReads name value
        IfStatement _ condition _ _ -> alwaysReads name condition
        GuardStatement _ condition _ -> alwaysReads name condition
        WhileStatement _ condition _ -> alwaysReads name condition
        DiscardStatement _ value -> alwaysReads name value
        ExpressionStatement _ value _ -> alwaysReads name value
        _ -> False
    [] -> False

{- | Every local that a closure created by the statements captures, at any
depth. A closure takes the value of a capture when it is created, so a
binding that a closure captures has to have its value by then.
-}
capturedSymbols :: [CoreStatement] -> [SymbolId]
capturedSymbols = concatMap statement
    where
        statement value = case value of
            CoreBind binding -> expression (coreBindingValue binding)
            CoreAssign _ assigned -> expression assigned
            CoreReturn returned -> expression returned
            CoreEvaluate evaluated -> expression evaluated
            CoreIf condition whenTrue whenFalse -> expression condition ++ capturedSymbols whenTrue ++ capturedSymbols whenFalse
            CoreWhile condition body -> expression condition ++ capturedSymbols body
            CoreDoWhile body condition -> capturedSymbols body ++ expression condition
            CoreFor condition body update -> expression condition ++ capturedSymbols body ++ capturedSymbols update
            CoreBreak -> []
            CoreContinue -> []
        expression value = case value of
            CoreVariable {} -> []
            CoreLiteral {} -> []
            CoreApply callee arguments _ -> concatMap expression (callee : arguments)
            CorePrimitive _ arguments _ -> concatMap expression arguments
            CoreLet _ _ bound body _ -> expression bound ++ expression body
            CoreConditional condition whenTrue whenFalse _ -> concatMap expression [condition, whenTrue, whenFalse]
            CoreClosure captures _ _ body _ ->
                map (resolvedSymbol . coreCaptureName) captures
                    ++ concatMap (expression . coreCaptureValue) captures
                    ++ capturedSymbols body
