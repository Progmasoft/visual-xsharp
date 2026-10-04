-- SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
-- SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.1

{- | The loops around the place the type checker is at.

A @break@ or a @continue@ is checked against the innermost loop, and what a
@break@ may carry depends on how that loop is used. Blocks used as values and
loop headers are not loops, but a transfer has to cross them to reach one, so
they are entries of the same stack.
-}
module Visual.XSharp.TypeChecker.Loops
    ( LoopKind (..)
    , LoopContext (..)
    , outsideLoops
    , enterLoop
    , loopHeader
    , transferTarget
    ) where

import Visual.XSharp.AST (Type)

-- | How a loop is used, which decides what its @break@ statements carry.
data LoopKind
    = -- | A loop statement: @break@ carries no value.
      StatementLoop
    | {- | A loop used as an expression: every @break@ carries the loop's
      value, typed in the context that receives it.
      -}
      ExpressionLoop (Maybe Type)
    | {- | Not a loop: the edge of a block used as a value. A @break@ or
      @continue@ inside it leaves the block without a value and targets the
      loop around the expression the block belongs to.
      -}
      ValueBlockEdge
    | {- | Not a loop: the edge of the condition or the update clause of a
      loop. An expression there is evaluated as part of the loop's own
      control, so leaving an enclosing loop from it is not lowered yet.
      -}
      LoopHeaderEdge

{- | The loops around a statement, innermost first, and the kind of the loop
statement that is about to be checked. A loop expression checks its loop
statement with the pending kind set; every loop moves the pending kind onto
the stack for its own body.
-}
data LoopContext = LoopContext
    { pendingLoop :: LoopKind
    , enclosingLoops :: [LoopKind]
    }

-- | The context of a statement that no loop encloses.
outsideLoops :: LoopContext
outsideLoops = LoopContext StatementLoop []

-- | The context of the body of the loop statement being checked.
enterLoop :: LoopContext -> LoopContext
enterLoop loops = LoopContext StatementLoop (pendingLoop loops : enclosingLoops loops)

-- | The context of the condition or the update clause of a loop.
loopHeader :: LoopContext
loopHeader = LoopContext StatementLoop [LoopHeaderEdge]

{- | The loop a @break@ or @continue@ targets, and whether it leaves a block
used as a value on the way.
-}
transferTarget :: LoopContext -> (Bool, Maybe LoopKind)
transferTarget loops = go False (enclosingLoops loops)
    where
        go crossed kinds = case kinds of
            ValueBlockEdge : outer -> go True outer
            kind : _ -> (crossed, Just kind)
            [] -> (crossed, Nothing)
