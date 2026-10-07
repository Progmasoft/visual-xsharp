-- SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
-- SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.1

{- | A small reference evaluator for scalar Core, used only by tests.

It executes Core statements directly, without CorePrep, the optimizer or the
native backend, so a test can state what a source program must compute and
check that against the Core the Desugarer produced and, separately, against
the Core the optimizer left. It is deliberately independent of both: it
shares no lowering or rewriting code with the compiler.

The subset is the one the tests need: integer and Boolean values, the
arithmetic, comparison, bitwise and logical primitives, expression-local
bindings, conditional expressions, direct calls of module functions,
closures, callables that remember their result, and all statement forms. Integers are unbounded; a test that depends on overflow
belongs to the native execution tests instead. Anything outside the subset,
and any run that exceeds its step budget, yields 'Nothing' rather than a
guess.
-}
module CoreInterpreter
    ( Value (..)
    , runFunction
    , runFunctionWithBudget
    ) where

import Data.Bits (complement, shiftL, shiftR, xor, (.&.), (.|.))
import Visual.XSharp.AST
import Visual.XSharp.Core

-- | A runtime value of the supported subset.
data Value
    = IntegerValue Integer
    | BooleanValue Bool
    | UnitValue
    | {- | A closure: the values its captures had when it was created, the
      symbols of its parameters, and its body. A capture initializer is
      evaluated once, where the closure is created.
      -}
      ClosureValue [(Int, Value)] [Int] [CoreStatement]
    | {- | A callable that remembers its result: the cell that holds the
      result once there is one, and the callable that computes it. Copies of
      the value name the same cell, which is how they share the result.
      -}
      MemoValue Int Value
    deriving (Eq, Show)

-- Local values keyed by symbol identity. Core symbols are unique within a
-- function, so one flat table is a faithful model of its locals.
type Locals = [(Int, Value)]

{- | What a run carries from step to step besides its locals.

The steps left bound every run, so a lowering error that turns a loop into
an endless one fails the test instead of hanging the suite. The cells hold
the results that callables have remembered; they belong to the run and not
to a function, because a callable is passed between functions and must find
its result wherever it is called.
-}
data Budget = Budget
    { stepsLeft :: Int
    , cells :: [(Int, Value)]
    , nextCell :: Int
    }

-- | Whether no step is left.
exhausted :: Budget -> Bool
exhausted budget = stepsLeft budget <= 0

-- | The budget after one step.
step :: Budget -> Budget
step budget = budget {stepsLeft = stepsLeft budget - 1}

data Flow
    = Proceed
    | BreakLoop
    | ContinueLoop
    | ReturnValue Value

-- | Run a function by name with the default step budget.
runFunction :: CoreModule -> String -> [Value] -> Maybe Value
runFunction = runFunctionWithBudget 200000

{- | Run a function by name; 'Nothing' when it is missing, leaves the
supported subset, or exhausts the budget.
-}
runFunctionWithBudget :: Int -> CoreModule -> String -> [Value] -> Maybe Value
runFunctionWithBudget steps moduleValue name arguments = do
    function <- firstJust [function | function <- coreModuleFunctions moduleValue, functionSpelling function == name]
    fst <$> call moduleValue (Budget steps [] 0) function arguments

functionSpelling :: CoreFunction -> String
functionSpelling = identifierText . resolvedSpelling . coreFunctionName

firstJust :: [value] -> Maybe value
firstJust values = case values of
    value : _ -> Just value
    [] -> Nothing

symbolOf :: ResolvedName -> Int
symbolOf = symbolIdValue . resolvedSymbol

call :: CoreModule -> Budget -> CoreFunction -> [Value] -> Maybe (Value, Budget)
call moduleValue budget function arguments
    | exhausted budget = Nothing
    | length arguments /= length (coreFunctionParameters function) = Nothing
    | otherwise = do
        let locals = zip (map (symbolOf . fst) (coreFunctionParameters function)) arguments
        (flow, _, remaining) <- executeAll moduleValue (step budget) locals (coreFunctionBody function)
        case flow of
            ReturnValue value -> Just (value, remaining)
            Proceed -> Just (UnitValue, remaining)
            _ -> Nothing

executeAll :: CoreModule -> Budget -> Locals -> [CoreStatement] -> Maybe (Flow, Locals, Budget)
executeAll _ budget locals [] = Just (Proceed, locals, budget)
executeAll moduleValue budget locals (statement : remaining) = do
    (flow, nextLocals, nextBudget) <- execute moduleValue budget locals statement
    case flow of
        Proceed -> executeAll moduleValue nextBudget nextLocals remaining
        _ -> Just (flow, nextLocals, nextBudget)

execute :: CoreModule -> Budget -> Locals -> CoreStatement -> Maybe (Flow, Locals, Budget)
execute moduleValue budget locals statement
    | exhausted budget = Nothing
    | otherwise = case statement of
        CoreBind binding -> do
            (value, afterLocals, afterBudget) <- evaluate moduleValue spent locals (coreBindingValue binding)
            Just (Proceed, store (symbolOf (coreBindingName binding)) value afterLocals, afterBudget)
        CoreAssign name expression -> do
            (value, afterLocals, afterBudget) <- evaluate moduleValue spent locals expression
            -- An assignment never introduces a local: a missing target is a
            -- lowering error, not something to paper over.
            _ <- lookup (symbolOf name) afterLocals
            Just (Proceed, store (symbolOf name) value afterLocals, afterBudget)
        CoreReturn expression -> do
            (value, afterLocals, afterBudget) <- evaluate moduleValue spent locals expression
            Just (ReturnValue value, afterLocals, afterBudget)
        CoreEvaluate expression -> do
            (_, afterLocals, afterBudget) <- evaluate moduleValue spent locals expression
            Just (Proceed, afterLocals, afterBudget)
        CoreIf condition whenTrue whenFalse -> do
            (value, afterLocals, afterBudget) <- evaluate moduleValue spent locals condition
            selected <- truth value
            executeAll moduleValue afterBudget afterLocals (if selected then whenTrue else whenFalse)
        CoreWhile condition body -> loop moduleValue spent locals (Just condition) body [] True
        CoreDoWhile body condition -> loop moduleValue spent locals (Just condition) body [] False
        CoreFor condition body update -> loop moduleValue spent locals (Just condition) body update True
        CoreBreak -> Just (BreakLoop, locals, spent)
        CoreContinue -> Just (ContinueLoop, locals, spent)
    where
        spent = step budget

{- | Run one structured loop.

The condition is tested before the body when @testFirst@ holds and after it
otherwise. @continue@ ends the body, runs the update clause and then tests
the condition, in every loop form.
-}
loop ::
    CoreModule ->
    Budget ->
    Locals ->
    Maybe CoreExpression ->
    [CoreStatement] ->
    [CoreStatement] ->
    Bool ->
    Maybe (Flow, Locals, Budget)
loop moduleValue budget locals condition body update testFirst
    | exhausted budget = Nothing
    | testFirst = do
        (enter, afterLocals, afterBudget) <- test budget locals
        if enter then iteration afterBudget afterLocals else Just (Proceed, afterLocals, afterBudget)
    | otherwise = iteration budget locals
    where
        test currentBudget currentLocals = case condition of
            Nothing -> Just (True, currentLocals, currentBudget)
            Just expression -> do
                (value, afterLocals, afterBudget) <- evaluate moduleValue currentBudget currentLocals expression
                selected <- truth value
                Just (selected, afterLocals, afterBudget)
        iteration currentBudget currentLocals = do
            (flow, afterBody, bodyBudget) <- executeAll moduleValue (step currentBudget) currentLocals body
            case flow of
                BreakLoop -> Just (Proceed, afterBody, bodyBudget)
                ReturnValue _ -> Just (flow, afterBody, bodyBudget)
                _ -> do
                    (updateFlow, afterUpdate, updateBudget) <- executeAll moduleValue bodyBudget afterBody update
                    case updateFlow of
                        Proceed -> loop moduleValue updateBudget afterUpdate condition body update True
                        -- A break in the update clause leaves the loop,
                        -- and a return leaves the function.
                        BreakLoop -> Just (Proceed, afterUpdate, updateBudget)
                        ReturnValue _ -> Just (updateFlow, afterUpdate, updateBudget)
                        -- The Core verifier rejects a continue placed
                        -- directly in an update clause.
                        ContinueLoop -> Nothing

store :: Int -> Value -> Locals -> Locals
store symbol value locals = (symbol, value) : filter ((/= symbol) . fst) locals

truth :: Value -> Maybe Bool
truth value = case value of
    BooleanValue flag -> Just flag
    IntegerValue number -> Just (number /= 0)
    UnitValue -> Nothing
    ClosureValue {} -> Nothing
    MemoValue {} -> Nothing

evaluate :: CoreModule -> Budget -> Locals -> CoreExpression -> Maybe (Value, Locals, Budget)
evaluate moduleValue budget locals expression
    | exhausted budget = Nothing
    | otherwise = case expression of
        CoreVariable name _ -> case lookup (symbolOf name) locals of
            Just value -> Just (value, locals, budget)
            -- A method named where a value is expected is a closure
            -- without captures.
            Nothing -> do
                method <-
                    firstJust
                        [ candidate
                        | candidate <- coreModuleFunctions moduleValue
                        , symbolOf (coreFunctionName candidate) == symbolOf name
                        ]
                Just
                    ( ClosureValue [] (map (symbolOf . fst) (coreFunctionParameters method)) (coreFunctionBody method)
                    , locals
                    , budget
                    )
        CoreLiteral literal valueType -> do
            value <- literalValue literal valueType
            Just (value, locals, budget)
        CoreLet name _ bound body _ -> do
            (value, afterLocals, afterBudget) <- evaluate moduleValue budget locals bound
            evaluate moduleValue afterBudget (store (symbolOf name) value afterLocals) body
        CoreConditional condition whenTrue whenFalse _ -> do
            (value, afterLocals, afterBudget) <- evaluate moduleValue budget locals condition
            selected <- truth value
            evaluate moduleValue afterBudget afterLocals (if selected then whenTrue else whenFalse)
        CorePrimitive CoreLogicalAnd [left, right] _ -> shortCircuit False left right
        CorePrimitive CoreLogicalOr [left, right] _ -> shortCircuit True left right
        -- Remembering takes a cell that no other callable has. Nothing is
        -- computed here: the callable is not called.
        CorePrimitive CoreMemoize [operand] _ -> do
            (target, afterLocals, afterBudget) <- evaluate moduleValue budget locals operand
            case target of
                ClosureValue _ [] _ ->
                    Just
                        ( MemoValue (nextCell afterBudget) target
                        , afterLocals
                        , afterBudget {nextCell = nextCell afterBudget + 1}
                        )
                _ -> Nothing
        CorePrimitive primitive operands valueType -> do
            (values, afterLocals, afterBudget) <- evaluateMany moduleValue budget locals operands
            value <- applyPrimitive primitive valueType values
            Just (value, afterLocals, afterBudget)
        CoreApply (CoreVariable callee _) arguments _
            | function : _ <-
                [ candidate
                | candidate <- coreModuleFunctions moduleValue
                , symbolOf (coreFunctionName candidate) == symbolOf callee
                ] -> do
                (values, afterLocals, afterBudget) <- evaluateMany moduleValue budget locals arguments
                (value, callBudget) <- call moduleValue afterBudget function values
                Just (value, afterLocals, callBudget)
        -- The callee is evaluated before the arguments, like any operand.
        CoreApply callee arguments _ -> do
            (target, calleeLocals, calleeBudget) <- evaluate moduleValue budget locals callee
            (values, afterLocals, afterBudget) <- evaluateMany moduleValue calleeBudget calleeLocals arguments
            (value, callBudget) <- callClosure target values afterBudget
            Just (value, afterLocals, callBudget)
        CoreClosure captures parameters _ body _ -> do
            (values, afterLocals, afterBudget) <- evaluateMany moduleValue budget locals (map coreCaptureValue captures)
            Just
                ( ClosureValue (zip (map (symbolOf . coreCaptureName) captures) values) (map (symbolOf . fst) parameters) body
                , afterLocals
                , afterBudget
                )
    where
        -- A closure runs on its captures and its arguments alone: it sees
        -- no local of the function that calls it.
        callClosure target values currentBudget = case target of
            ClosureValue captured parameters body
                | not (exhausted currentBudget) && length parameters == length values -> do
                    (flow, _, remaining) <-
                        executeAll moduleValue (step currentBudget) (zip parameters values ++ captured) body
                    case flow of
                        ReturnValue value -> Just (value, remaining)
                        Proceed -> Just (UnitValue, remaining)
                        _ -> Nothing
            -- The first call computes the result and keeps it in the cell;
            -- every later call, of any copy, reads the cell. A call that
            -- does not finish leaves the cell empty.
            MemoValue cell computation
                | not (exhausted currentBudget) && null values -> case lookup cell (cells currentBudget) of
                    Just remembered -> Just (remembered, step currentBudget)
                    Nothing -> do
                        (value, remaining) <- callClosure computation [] (step currentBudget)
                        Just (value, remaining {cells = (cell, value) : cells remaining})
            _ -> Nothing
        -- The right operand runs only when the left one does not decide.
        shortCircuit decidingValue left right = do
            (leftValue, afterLeft, leftBudget) <- evaluate moduleValue budget locals left
            leftTruth <- truth leftValue
            if leftTruth == decidingValue
                then Just (BooleanValue decidingValue, afterLeft, leftBudget)
                else do
                    (rightValue, afterRight, rightBudget) <- evaluate moduleValue leftBudget afterLeft right
                    rightTruth <- truth rightValue
                    Just (BooleanValue rightTruth, afterRight, rightBudget)

evaluateMany :: CoreModule -> Budget -> Locals -> [CoreExpression] -> Maybe ([Value], Locals, Budget)
evaluateMany _ budget locals [] = Just ([], locals, budget)
evaluateMany moduleValue budget locals (expression : remaining) = do
    (value, afterLocals, afterBudget) <- evaluate moduleValue budget locals expression
    (values, finalLocals, finalBudget) <- evaluateMany moduleValue afterBudget afterLocals remaining
    Just (value : values, finalLocals, finalBudget)

literalValue :: CoreLiteral -> Type -> Maybe Value
literalValue literal valueType = case literal of
    CoreInteger number
        | valueType == boolType -> Just (BooleanValue (number /= 0))
        | otherwise -> Just (IntegerValue number)
    CoreBoolean flag -> Just (BooleanValue flag)
    CoreUnit -> Just UnitValue
    CoreFloating _ -> Nothing
    CoreString _ -> Nothing
    CoreNull -> Nothing

applyPrimitive :: CorePrimitive -> Type -> [Value] -> Maybe Value
applyPrimitive primitive valueType values = case (primitive, values) of
    (CoreAdd, [IntegerValue left, IntegerValue right]) -> integer (left + right)
    (CoreSubtract, [IntegerValue left, IntegerValue right]) -> integer (left - right)
    (CoreMultiply, [IntegerValue left, IntegerValue right]) -> integer (left * right)
    (CoreDivide, [IntegerValue left, IntegerValue right]) | right /= 0 -> integer (left `quot` right)
    (CoreRemainder, [IntegerValue left, IntegerValue right]) | right /= 0 -> integer (left `rem` right)
    (CoreNegate, [IntegerValue operand]) -> integer (negate operand)
    (CoreShiftLeft, [IntegerValue left, IntegerValue right]) | inShiftRange right -> integer (left `shiftL` fromInteger right)
    (CoreShiftRight, [IntegerValue left, IntegerValue right]) | inShiftRange right -> integer (left `shiftR` fromInteger right)
    (CoreBitwiseAnd, [IntegerValue left, IntegerValue right]) -> integer (left .&. right)
    (CoreBitwiseXor, [IntegerValue left, IntegerValue right]) -> integer (left `xor` right)
    (CoreBitwiseOr, [IntegerValue left, IntegerValue right]) -> integer (left .|. right)
    (CoreBitwiseNot, [IntegerValue operand]) -> integer (complement operand)
    (CoreLessThan, [IntegerValue left, IntegerValue right]) -> Just (BooleanValue (left < right))
    (CoreLessEqual, [IntegerValue left, IntegerValue right]) -> Just (BooleanValue (left <= right))
    (CoreGreaterThan, [IntegerValue left, IntegerValue right]) -> Just (BooleanValue (left > right))
    (CoreGreaterEqual, [IntegerValue left, IntegerValue right]) -> Just (BooleanValue (left >= right))
    (CoreEqual, [left, right]) -> Just (BooleanValue (left == right))
    (CoreNotEqual, [left, right]) -> Just (BooleanValue (left /= right))
    (CoreLogicalNot, [operand]) -> BooleanValue . not <$> truth operand
    -- Remembering needs the cells of the run; 'evaluate' handles it.
    (CoreMemoize, _) -> Nothing
    _ -> Nothing
    where
        -- A primitive whose result type is bool yields a Boolean even when
        -- its operands are numeric, as the logical forms of numbers do.
        integer number
            | valueType == boolType = Just (BooleanValue (number /= 0))
            | otherwise = Just (IntegerValue number)
        inShiftRange amount = amount >= 0 && amount < 64
