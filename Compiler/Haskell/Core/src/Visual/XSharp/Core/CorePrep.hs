-- SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
-- SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.1
module Visual.XSharp.Core.CorePrep
    ( CorePrepAtom (..)
    , CorePrepCapture (..)
    , CorePrepOperation (..)
    , CorePrepInstruction (..)
    , CorePrepTerminator (..)
    , CorePrepBlock (..)
    , CorePrepFunction (..)
    , CorePrepModule (..)
    , prepareCore
    ) where

import Visual.XSharp.AST
import Visual.XSharp.Core
import Visual.XSharp.Diagnostic

data CorePrepAtom = CorePrepVariable ResolvedName Type | CorePrepLiteral CoreLiteral Type
    deriving (Eq, Ord, Read, Show)
data CorePrepOperation
    = CorePrepCopy CorePrepAtom
    | CorePrepCall CorePrepAtom [CorePrepAtom]
    | CorePrepPrimitive CorePrimitive [CorePrepAtom]
    | CorePrepMakeClosure ResolvedName [CorePrepCapture]
    deriving (Eq, Ord, Read, Show)
data CorePrepCapture = CorePrepCapture CaptureMode ResolvedName Type CorePrepAtom
    deriving (Eq, Ord, Read, Show)
data CorePrepInstruction
    = CorePrepBind ResolvedName Type Bool CorePrepOperation
    | CorePrepAssign ResolvedName CorePrepAtom
    | CorePrepEvaluate CorePrepOperation
    deriving (Eq, Ord, Read, Show)
data CorePrepTerminator
    = CorePrepReturn CorePrepAtom
    | CorePrepBranch CorePrepAtom Int Int
    | CorePrepJump Int
    | CorePrepUnreachable
    deriving (Eq, Ord, Read, Show)
data CorePrepBlock = CorePrepBlock
    { corePrepBlockId :: Int
    , corePrepBlockInstructions :: [CorePrepInstruction]
    , corePrepBlockTerminator :: CorePrepTerminator
    }
    deriving (Eq, Ord, Read, Show)
data CorePrepFunction = CorePrepFunction
    { corePrepFunctionName :: ResolvedName
    , corePrepFunctionParameters :: [(ResolvedName, Type)]
    , corePrepFunctionReturnType :: Type
    , corePrepFunctionEntry :: Int
    , corePrepFunctionBlocks :: [CorePrepBlock]
    }
    deriving (Eq, Ord, Read, Show)
data CorePrepModule = CorePrepModule
    {corePrepModuleName :: QualifiedName, corePrepModuleFunctions :: [CorePrepFunction]}
    deriving (Eq, Ord, Read, Show)

data PrepState = PrepState
    { nextTemporary :: Int
    , nextBlock :: Int
    , pendingFunctions :: [CoreFunction]
    }

-- An OpenBlock is the current continuation while expressions are being
-- atomized. Most expressions only append instructions, but short-circuit
-- expressions close the current block, emit a conditional region, and return
-- a fresh join block. Keeping that distinction explicit prevents a nested
-- logical expression from being flattened back into eager instruction order.
data OpenBlock = OpenBlock
    { openBlockId :: Int
    , openBlockInstructions :: [CorePrepInstruction]
    }

prepareCore :: CoreModule -> Either [Diagnostic] CorePrepModule
prepareCore moduleValue =
    let seed = 1 + maximum (0 : concatMap symbolIds (coreModuleFunctions moduleValue))
        initial = PrepState seed 1 []
        (functions, _) = prepareFunctionQueue initial (coreModuleFunctions moduleValue)
     in Right (CorePrepModule (coreModuleName moduleValue) functions)

-- Closure conversion appends lifted functions to this work queue.  Processing
-- the queue to exhaustion also supports nested closures without a separate
-- whole-module mutation pass.
prepareFunctionQueue :: PrepState -> [CoreFunction] -> ([CorePrepFunction], PrepState)
prepareFunctionQueue state [] = case pendingFunctions state of
    [] -> ([], state)
    pending -> prepareFunctionQueue (state {pendingFunctions = []}) pending
prepareFunctionQueue state (function : remaining) =
    let (prepared, afterFunction) = prepareFunction (state {nextBlock = 1}) function
        pending = pendingFunctions afterFunction
        nextState = afterFunction {pendingFunctions = []}
        (later, final) = prepareFunctionQueue nextState (remaining ++ pending)
     in (prepared : later, final)

prepareFunction :: PrepState -> CoreFunction -> (CorePrepFunction, PrepState)
prepareFunction state function =
    let (blocks, after) = prepareStatements state (OpenBlock 0 []) (coreFunctionBody function)
     in ( CorePrepFunction
            (coreFunctionName function)
            (coreFunctionParameters function)
            (coreFunctionReturnType function)
            0
            blocks
        , after
        )

symbolIds :: CoreFunction -> [Int]
symbolIds function =
    symbolIdValue (resolvedSymbol (coreFunctionName function))
        : map (symbolIdValue . resolvedSymbol . fst) (coreFunctionParameters function)
        ++ concatMap statementSymbolIds (coreFunctionBody function)

statementSymbolIds :: CoreStatement -> [Int]
statementSymbolIds statement = case statement of
    CoreBind binding -> symbol (coreBindingName binding) : expressionSymbolIds (coreBindingValue binding)
    CoreAssign name expression -> symbol name : expressionSymbolIds expression
    CoreReturn expression -> expressionSymbolIds expression
    CoreIf condition trueBranch falseBranch ->
        expressionSymbolIds condition ++ concatMap statementSymbolIds trueBranch ++ concatMap statementSymbolIds falseBranch
    CoreEvaluate expression -> expressionSymbolIds expression
    where
        symbol = symbolIdValue . resolvedSymbol

expressionSymbolIds :: CoreExpression -> [Int]
expressionSymbolIds expression = case expression of
    CoreVariable name _ -> [symbol name]
    CoreLiteral _ _ -> []
    CoreApply callee arguments _ -> expressionSymbolIds callee ++ concatMap expressionSymbolIds arguments
    CorePrimitive _ arguments _ -> concatMap expressionSymbolIds arguments
    CoreClosure captures parameters _ body _ ->
        map (symbol . coreCaptureName) captures
            ++ concatMap (expressionSymbolIds . coreCaptureValue) captures
            ++ map (symbol . fst) parameters
            ++ concatMap statementSymbolIds body
    where
        symbol = symbolIdValue . resolvedSymbol

prepareStatements :: PrepState -> OpenBlock -> [CoreStatement] -> ([CorePrepBlock], PrepState)
prepareStatements state open [] = ([closeBlock open CorePrepUnreachable], state)
prepareStatements state open (statement : remaining) = case statement of
    CoreBind binding ->
        let (closed, continued, operation, after) = atomizeOperation state open (coreBindingValue binding)
            instruction = CorePrepBind (coreBindingName binding) (coreBindingType binding) (coreBindingMutable binding) operation
            (later, final) = prepareStatements after (appendInstruction continued instruction) remaining
         in (closed ++ later, final)
    CoreAssign name value ->
        let (closed, continued, atom, after) = atomize state open value
            (later, final) = prepareStatements after (appendInstruction continued (CorePrepAssign name atom)) remaining
         in (closed ++ later, final)
    CoreEvaluate value ->
        let (closed, continued, operation, after) = atomizeOperation state open value
            (later, final) = prepareStatements after (appendInstruction continued (CorePrepEvaluate operation)) remaining
         in (closed ++ later, final)
    CoreReturn value ->
        let (closed, continued, atom, after) = atomize state open value
         in (closed ++ [closeBlock continued (CorePrepReturn atom)], after)
    CoreIf condition trueBranch falseBranch ->
        let (conditionBlocks, conditionOpen, conditionAtom, afterCondition) = atomize state open condition
            (booleanOpen, booleanAtom, afterBoolean) = booleanizeAtom afterCondition conditionOpen conditionAtom
            trueId = nextBlock afterBoolean
            falseId = trueId + 1
            joinId = falseId + 1
            branchState = afterBoolean {nextBlock = joinId + 1}
            (trueBlocks, afterTrue) = prepareBranch branchState trueId joinId trueBranch
            (falseBlocks, afterFalse) = prepareBranch afterTrue falseId joinId falseBranch
            header = closeBlock booleanOpen (CorePrepBranch booleanAtom trueId falseId)
            (tailBlocks, final) = prepareStatements afterFalse (OpenBlock joinId []) remaining
         in (conditionBlocks ++ [header] ++ trueBlocks ++ falseBlocks ++ tailBlocks, final)

appendInstruction :: OpenBlock -> CorePrepInstruction -> OpenBlock
appendInstruction open instruction =
    open {openBlockInstructions = openBlockInstructions open ++ [instruction]}

closeBlock :: OpenBlock -> CorePrepTerminator -> CorePrepBlock
closeBlock open terminator =
    CorePrepBlock (openBlockId open) (openBlockInstructions open) terminator

-- Numeric conditions are a source-language convenience. Core retains their
-- numeric type for optimization, while CorePrep makes the zero comparison
-- explicit so every native branch still consumes a canonical bool atom.
booleanizeAtom :: PrepState -> OpenBlock -> CorePrepAtom -> (OpenBlock, CorePrepAtom, PrepState)
booleanizeAtom state open atom
    | corePrepAtomType atom == boolType = (open, atom, state)
    | otherwise =
        let identifier = nextTemporary state
            temporary = ResolvedName (SymbolId identifier) (Identifier ("$condition" ++ show identifier))
            zero = CorePrepLiteral (CoreInteger 0) (corePrepAtomType atom)
            instruction = CorePrepBind temporary boolType False (CorePrepPrimitive CoreNotEqual [atom, zero])
         in (appendInstruction open instruction, CorePrepVariable temporary boolType, state {nextTemporary = identifier + 1})

booleanizeMany :: PrepState -> OpenBlock -> [CorePrepAtom] -> (OpenBlock, [CorePrepAtom], PrepState)
booleanizeMany state open [] = (open, [], state)
booleanizeMany state open (atom : remaining) =
    let (continued, boolean, after) = booleanizeAtom state open atom
        (finalOpen, later, final) = booleanizeMany after continued remaining
     in (finalOpen, boolean : later, final)

corePrepAtomType :: CorePrepAtom -> Type
corePrepAtomType atom = case atom of
    CorePrepVariable _ valueType -> valueType
    CorePrepLiteral _ valueType -> valueType

prepareBranch :: PrepState -> Int -> Int -> [CoreStatement] -> ([CorePrepBlock], PrepState)
prepareBranch state blockId joinId statements =
    let (blocks, after) = prepareStatements state (OpenBlock blockId []) statements
     in (map addJump blocks, after)
    where
        addJump block | corePrepBlockTerminator block == CorePrepUnreachable = block {corePrepBlockTerminator = CorePrepJump joinId}
        addJump block = block

atomize :: PrepState -> OpenBlock -> CoreExpression -> ([CorePrepBlock], OpenBlock, CorePrepAtom, PrepState)
atomize state open expression = case expression of
    CoreVariable name valueType -> ([], open, CorePrepVariable name valueType, state)
    CoreLiteral literal valueType -> ([], open, CorePrepLiteral literal valueType, state)
    CorePrimitive primitive [left, right] _
        | primitive == CoreLogicalAnd || primitive == CoreLogicalOr ->
            atomizeShortCircuit state open primitive left right
    _ ->
        let (closed, continued, operation, afterOperation) = atomizeOperation state open expression
            temporary =
                ResolvedName (SymbolId (nextTemporary afterOperation)) (Identifier ("$coreprep" ++ show (nextTemporary afterOperation)))
            valueType = expressionType expression
            instruction = CorePrepBind temporary valueType False operation
         in ( closed
            , appendInstruction continued instruction
            , CorePrepVariable temporary valueType
            , afterOperation {nextTemporary = nextTemporary afterOperation + 1}
            )

atomizeOperation ::
    PrepState -> OpenBlock -> CoreExpression -> ([CorePrepBlock], OpenBlock, CorePrepOperation, PrepState)
atomizeOperation state open expression = case expression of
    CoreVariable name valueType -> ([], open, CorePrepCopy (CorePrepVariable name valueType), state)
    CoreLiteral literal valueType -> ([], open, CorePrepCopy (CorePrepLiteral literal valueType), state)
    CorePrimitive primitive [left, right] _
        | primitive == CoreLogicalAnd || primitive == CoreLogicalOr ->
            let (closed, continued, atom, after) = atomizeShortCircuit state open primitive left right
             in (closed, continued, CorePrepCopy atom, after)
    CoreApply callee arguments _ ->
        let (calleeBlocks, calleeOpen, calleeAtom, afterCallee) = atomize state open callee
            (argumentBlocks, argumentOpen, argumentAtoms, afterArguments) = atomizeMany afterCallee calleeOpen arguments
         in (calleeBlocks ++ argumentBlocks, argumentOpen, CorePrepCall calleeAtom argumentAtoms, afterArguments)
    CorePrimitive primitive arguments _ ->
        let (closed, continued, atoms, after) = atomizeMany state open arguments
            logical = primitive `elem` [CoreLogicalAnd, CoreLogicalOr, CoreLogicalNot]
            (finalOpen, preparedAtoms, final) =
                if logical then booleanizeMany after continued atoms else (continued, atoms, after)
         in (closed, finalOpen, CorePrepPrimitive primitive preparedAtoms, final)
    CoreClosure captures parameters returnType body _ ->
        let closureId = nextTemporary state
            closureName =
                ResolvedName (SymbolId closureId) (Identifier ("$closure" ++ show closureId))
            (captureBlocks, captureOpen, preparedCaptures, afterCaptures) =
                atomizeCaptures
                    (state {nextTemporary = closureId + 1})
                    open
                    captures
            hiddenParameters = [(coreCaptureName capture, coreCaptureType capture) | capture <- captures]
            lifted = CoreFunction closureName (hiddenParameters ++ parameters) returnType body
            finalState =
                afterCaptures
                    { pendingFunctions = pendingFunctions afterCaptures ++ [lifted]
                    }
         in (captureBlocks, captureOpen, CorePrepMakeClosure closureName preparedCaptures, finalState)

-- Logical conjunction and disjunction are control flow, not ordinary eager
-- primitive instructions. The result slot is initialized before branching and
-- overwritten only on the path that evaluates the right operand. Consequently
-- every join predecessor carries an initialized Bool without requiring a phi
-- node in CorePrep's storage-oriented IR.
atomizeShortCircuit ::
    PrepState ->
    OpenBlock ->
    CorePrimitive ->
    CoreExpression ->
    CoreExpression ->
    ([CorePrepBlock], OpenBlock, CorePrepAtom, PrepState)
atomizeShortCircuit state open primitive left right =
    let (leftBlocks, leftOpen, leftAtom, afterLeft) = atomize state open left
        (conditionOpen, conditionAtom, afterCondition) = booleanizeAtom afterLeft leftOpen leftAtom
        resultId = nextTemporary afterCondition
        resultName = ResolvedName (SymbolId resultId) (Identifier ("$shortcircuit" ++ show resultId))
        resultAtom = CorePrepVariable resultName boolType
        defaultValue = CorePrepLiteral (CoreBoolean (primitive == CoreLogicalOr)) boolType
        initializedOpen =
            appendInstruction conditionOpen (CorePrepBind resultName boolType True (CorePrepCopy defaultValue))
        rightId = nextBlock afterCondition
        joinId = rightId + 1
        afterReservation =
            afterCondition
                { nextTemporary = resultId + 1
                , nextBlock = joinId + 1
                }
        branch =
            if primitive == CoreLogicalAnd
                then CorePrepBranch conditionAtom rightId joinId
                else CorePrepBranch conditionAtom joinId rightId
        header = closeBlock initializedOpen branch
        (rightBlocks, rightOpen, rightAtom, afterRight) =
            atomize afterReservation (OpenBlock rightId []) right
        (booleanRightOpen, booleanRight, final) = booleanizeAtom afterRight rightOpen rightAtom
        assignedRight = appendInstruction booleanRightOpen (CorePrepAssign resultName booleanRight)
        rightExit = closeBlock assignedRight (CorePrepJump joinId)
     in ( leftBlocks ++ [header] ++ rightBlocks ++ [rightExit]
        , OpenBlock joinId []
        , resultAtom
        , final
        )

atomizeMany :: PrepState -> OpenBlock -> [CoreExpression] -> ([CorePrepBlock], OpenBlock, [CorePrepAtom], PrepState)
atomizeMany state open [] = ([], open, [], state)
atomizeMany state open (value : remaining) =
    let (closed, continued, atom, after) = atomize state open value
        (laterBlocks, finalOpen, atoms, final) = atomizeMany after continued remaining
     in (closed ++ laterBlocks, finalOpen, atom : atoms, final)

atomizeCaptures ::
    PrepState ->
    OpenBlock ->
    [CoreCapture] ->
    ([CorePrepBlock], OpenBlock, [CorePrepCapture], PrepState)
atomizeCaptures state open [] = ([], open, [], state)
atomizeCaptures state open (capture : remaining) =
    let (closed, continued, atom, afterValue) = atomize state open (coreCaptureValue capture)
        prepared =
            CorePrepCapture
                (coreCaptureMode capture)
                (coreCaptureName capture)
                (coreCaptureType capture)
                atom
        (laterBlocks, finalOpen, laterCaptures, final) = atomizeCaptures afterValue continued remaining
     in (closed ++ laterBlocks, finalOpen, prepared : laterCaptures, final)
