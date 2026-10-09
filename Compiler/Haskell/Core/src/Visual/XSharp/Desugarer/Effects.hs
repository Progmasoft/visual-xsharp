-- SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
-- SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.1

{- | Which calls do something that can be observed.

Visual X# evaluates by need, and effects are not lazy: an effect happens
where it is written, whether or not the value of the expression that
contains it is ever needed. The source has no notation for an effect, so the
compiler has to know which expressions have one. A store is an effect that
shows in the expression itself. Output does not: @Log(value)@ writes to the
console only because of what the body of @Log@ does.

This module finds the methods a call of which may write to the console. An
expression that contains such a call is evaluated where it stands, like an
expression that stores: its binding is not deferred, and it is not handed to
another function as a suspended computation.

The answer errs on the side of an effect. A method acts when a call anywhere
in its body acts, in a callable it creates as well as in its own statements:
creating a callable that writes is taken for writing. A call through a
callable value acts when any method that creates a callable acts, because
which callable a value holds is not followed. A program without console
output is unaffected: nothing in it acts, and everything is deferred that
was deferred before.
-}
module Visual.XSharp.Desugarer.Effects
    ( Effects
    , noEffects
    , programEffects
    , callActs
    , expressionActs
    ) where

import Data.Map.Strict (Map)
import Data.Map.Strict qualified as Map
import Visual.XSharp.AST
import Visual.XSharp.Desugarer.Handing (blockExpressions, methodBody, methodDeclarations, within)
import Visual.XSharp.RuntimeCall

-- | What is known about the calls of a program.
data Effects = Effects
    { effectsMethods :: Map SymbolId Bool
    -- ^ For each method of the program, whether a call of it may act.
    , effectsCallables :: Bool
    -- ^ Whether a call through a callable value may act.
    }
    deriving (Eq, Show)

-- | A program in which no call acts.
noEffects :: Effects
noEffects = Effects Map.empty False

{- | The effects of a program given as the declarations of every tree that is
lowered for it. All of them are read together because a method of one tree
may call a method of another: the methods of ordinary types and the
specializations of templates are lowered apart.

A method starts out as not acting and is found to act when its body holds a
call that acts; the answer is computed again from itself until it no longer
changes, which it must, because a method that acts never stops acting.
-}
programEffects :: [[Declaration ResolvedName Type]] -> Effects
programEffects trees = settle noEffects {effectsMethods = Map.map (const False) bodies}
    where
        bodies = Map.map (blockExpressions . methodBody) (Map.unions (map methodDeclarations trees))
        createsCallable = Map.map (any isCallable) bodies
        settle current =
            let methods = Map.map (any (callActsIn current)) bodies
                next =
                    Effects
                        methods
                        (or (Map.elems (Map.intersectionWith (&&) methods createsCallable)))
             in if next == current then current else settle next
        isCallable expression = case expression of
            CallableExpression {} -> True
            _ -> False

-- | Whether a call of the given name may act.
callActs :: Effects -> ResolvedName -> Bool
callActs effects name = case runtimeFunctionOfName name of
    Just function -> runtimeObservable function
    -- A name that is not a method is a callable value.
    Nothing -> Map.findWithDefault (effectsCallables effects) (resolvedSymbol name) (effectsMethods effects)

{- | Whether evaluating an expression may act: whether any call in it does.
The statements of a block, a loop or a callable inside the expression are
not searched; an expression that holds one is evaluated where it stands for
that reason alone.
-}
expressionActs :: Effects -> Expression ResolvedName annotation -> Bool
expressionActs effects = any (callActsIn effects) . within

callActsIn :: Effects -> Expression ResolvedName annotation -> Bool
callActsIn effects expression = case expression of
    CallExpression _ (NameExpression _ callee _) _ _ -> callActs effects callee
    -- The callee is itself computed: a callable value.
    CallExpression {} -> effectsCallables effects
    _ -> False
