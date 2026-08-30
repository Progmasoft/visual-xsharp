-- SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
-- SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.0
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
    let (blocks, after) = prepareStatements state 0 [] (coreFunctionBody function)
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

prepareStatements :: PrepState -> Int -> [CorePrepInstruction] -> [CoreStatement] -> ([CorePrepBlock], PrepState)
prepareStatements state blockId instructions [] = ([CorePrepBlock blockId instructions CorePrepUnreachable], state)
prepareStatements state blockId instructions (statement : remaining) = case statement of
    CoreBind binding ->
        let (prefix, operation, after) = atomizeOperation state (coreBindingValue binding)
            instruction = CorePrepBind (coreBindingName binding) (coreBindingType binding) (coreBindingMutable binding) operation
         in prepareStatements after blockId (instructions ++ prefix ++ [instruction]) remaining
    CoreAssign name value ->
        let (prefix, atom, after) = atomize state value
         in prepareStatements after blockId (instructions ++ prefix ++ [CorePrepAssign name atom]) remaining
    CoreEvaluate value ->
        let (prefix, operation, after) = atomizeOperation state value
         in prepareStatements after blockId (instructions ++ prefix ++ [CorePrepEvaluate operation]) remaining
    CoreReturn value ->
        let (prefix, atom, after) = atomize state value
         in ([CorePrepBlock blockId (instructions ++ prefix) (CorePrepReturn atom)], after)
    CoreIf condition trueBranch falseBranch ->
        let (conditionPrefix, conditionAtom, afterCondition) = atomize state condition
            trueId = nextBlock afterCondition
            falseId = trueId + 1
            joinId = falseId + 1
            branchState = afterCondition {nextBlock = joinId + 1}
            (trueBlocks, afterTrue) = prepareBranch branchState trueId joinId trueBranch
            (falseBlocks, afterFalse) = prepareBranch afterTrue falseId joinId falseBranch
            header = CorePrepBlock blockId (instructions ++ conditionPrefix) (CorePrepBranch conditionAtom trueId falseId)
            (tailBlocks, final) = prepareStatements afterFalse joinId [] remaining
         in (header : trueBlocks ++ falseBlocks ++ tailBlocks, final)

prepareBranch :: PrepState -> Int -> Int -> [CoreStatement] -> ([CorePrepBlock], PrepState)
prepareBranch state blockId joinId statements =
    let (blocks, after) = prepareStatements state blockId [] statements
     in (map addJump blocks, after)
    where
        addJump block | corePrepBlockTerminator block == CorePrepUnreachable = block {corePrepBlockTerminator = CorePrepJump joinId}
        addJump block = block

atomize :: PrepState -> CoreExpression -> ([CorePrepInstruction], CorePrepAtom, PrepState)
atomize state expression = case expression of
    CoreVariable name valueType -> ([], CorePrepVariable name valueType, state)
    CoreLiteral literal valueType -> ([], CorePrepLiteral literal valueType, state)
    _ ->
        let (prefix, operation, afterOperation) = atomizeOperation state expression
            temporary =
                ResolvedName (SymbolId (nextTemporary afterOperation)) (Identifier ("$coreprep" ++ show (nextTemporary afterOperation)))
            valueType = expressionType expression
            instruction = CorePrepBind temporary valueType False operation
         in ( prefix ++ [instruction]
            , CorePrepVariable temporary valueType
            , afterOperation {nextTemporary = nextTemporary afterOperation + 1}
            )

atomizeOperation :: PrepState -> CoreExpression -> ([CorePrepInstruction], CorePrepOperation, PrepState)
atomizeOperation state expression = case expression of
    CoreVariable name valueType -> ([], CorePrepCopy (CorePrepVariable name valueType), state)
    CoreLiteral literal valueType -> ([], CorePrepCopy (CorePrepLiteral literal valueType), state)
    CoreApply callee arguments _ ->
        let (calleePrefix, calleeAtom, afterCallee) = atomize state callee
            (argumentPrefix, argumentAtoms, afterArguments) = atomizeMany afterCallee arguments
         in (calleePrefix ++ argumentPrefix, CorePrepCall calleeAtom argumentAtoms, afterArguments)
    CorePrimitive primitive arguments _ ->
        let (prefix, atoms, after) = atomizeMany state arguments in (prefix, CorePrepPrimitive primitive atoms, after)
    CoreClosure captures parameters returnType body _ ->
        let closureId = nextTemporary state
            closureName =
                ResolvedName (SymbolId closureId) (Identifier ("$closure" ++ show closureId))
            (capturePrefix, preparedCaptures, afterCaptures) =
                atomizeCaptures
                    (state {nextTemporary = closureId + 1})
                    captures
            hiddenParameters = [(coreCaptureName capture, coreCaptureType capture) | capture <- captures]
            lifted = CoreFunction closureName (hiddenParameters ++ parameters) returnType body
            finalState =
                afterCaptures
                    { pendingFunctions = pendingFunctions afterCaptures ++ [lifted]
                    }
         in (capturePrefix, CorePrepMakeClosure closureName preparedCaptures, finalState)

atomizeMany :: PrepState -> [CoreExpression] -> ([CorePrepInstruction], [CorePrepAtom], PrepState)
atomizeMany state [] = ([], [], state)
atomizeMany state (value : remaining) =
    let (prefix, atom, after) = atomize state value; (laterPrefix, atoms, final) = atomizeMany after remaining
     in (prefix ++ laterPrefix, atom : atoms, final)

atomizeCaptures ::
    PrepState ->
    [CoreCapture] ->
    ([CorePrepInstruction], [CorePrepCapture], PrepState)
atomizeCaptures state [] = ([], [], state)
atomizeCaptures state (capture : remaining) =
    let (prefix, atom, afterValue) = atomize state (coreCaptureValue capture)
        prepared =
            CorePrepCapture
                (coreCaptureMode capture)
                (coreCaptureName capture)
                (coreCaptureType capture)
                atom
        (laterPrefix, laterCaptures, final) = atomizeCaptures afterValue remaining
     in (prefix ++ laterPrefix, prepared : laterCaptures, final)
