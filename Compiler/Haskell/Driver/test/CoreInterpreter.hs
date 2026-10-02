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
bindings, conditional expressions, direct calls of module functions and all
statement forms. Integers are unbounded; a test that depends on overflow
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
    deriving (Eq, Ord, Show)

-- Local values keyed by symbol identity. Core symbols are unique within a
-- function, so one flat table is a faithful model of its locals.
type Locals = [(Int, Value)]

-- Remaining statement and call budget. It bounds every run, so a lowering
-- error that turns a loop into an endless one fails the test instead of
-- hanging the suite.
type Budget = Int

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
runFunctionWithBudget :: Budget -> CoreModule -> String -> [Value] -> Maybe Value
runFunctionWithBudget budget moduleValue name arguments = do
    function <- firstJust [function | function <- coreModuleFunctions moduleValue, functionSpelling function == name]
    fst <$> call moduleValue budget function arguments

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
    | budget <= 0 = Nothing
    | length arguments /= length (coreFunctionParameters function) = Nothing
    | otherwise = do
        let locals = zip (map (symbolOf . fst) (coreFunctionParameters function)) arguments
        (flow, _, remaining) <- executeAll moduleValue (budget - 1) locals (coreFunctionBody function)
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
    | budget <= 0 = Nothing
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
        spent = budget - 1

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
    | budget <= 0 = Nothing
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
            (flow, afterBody, bodyBudget) <- executeAll moduleValue (currentBudget - 1) currentLocals body
            case flow of
                BreakLoop -> Just (Proceed, afterBody, bodyBudget)
                ReturnValue _ -> Just (flow, afterBody, bodyBudget)
                _ -> do
                    (updateFlow, afterUpdate, updateBudget) <- executeAll moduleValue bodyBudget afterBody update
                    case updateFlow of
                        Proceed -> loop moduleValue updateBudget afterUpdate condition body update True
                        -- Leaving the loop from its update clause has no
                        -- defined meaning in this evaluator.
                        _ -> Nothing

store :: Int -> Value -> Locals -> Locals
store symbol value locals = (symbol, value) : filter ((/= symbol) . fst) locals

truth :: Value -> Maybe Bool
truth value = case value of
    BooleanValue flag -> Just flag
    IntegerValue number -> Just (number /= 0)
    UnitValue -> Nothing

evaluate :: CoreModule -> Budget -> Locals -> CoreExpression -> Maybe (Value, Locals, Budget)
evaluate moduleValue budget locals expression
    | budget <= 0 = Nothing
    | otherwise = case expression of
        CoreVariable name _ -> do
            value <- lookup (symbolOf name) locals
            Just (value, locals, budget)
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
        CorePrimitive primitive operands valueType -> do
            (values, afterLocals, afterBudget) <- evaluateMany moduleValue budget locals operands
            value <- applyPrimitive primitive valueType values
            Just (value, afterLocals, afterBudget)
        CoreApply (CoreVariable callee _) arguments _ -> do
            function <-
                firstJust
                    [ candidate
                    | candidate <- coreModuleFunctions moduleValue
                    , symbolOf (coreFunctionName candidate) == symbolOf callee
                    ]
            (values, afterLocals, afterBudget) <- evaluateMany moduleValue budget locals arguments
            (value, callBudget) <- call moduleValue afterBudget function values
            Just (value, afterLocals, callBudget)
        CoreApply {} -> Nothing
        CoreClosure {} -> Nothing
    where
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
    _ -> Nothing
    where
        -- A primitive whose result type is bool yields a Boolean even when
        -- its operands are numeric, as the logical forms of numbers do.
        integer number
            | valueType == boolType = Just (BooleanValue (number /= 0))
            | otherwise = Just (IntegerValue number)
        inShiftRange amount = amount >= 0 && amount < 64
