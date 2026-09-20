-- SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
-- SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.1

module Main (main) where

import Control.DeepSeq (NFData (rnf))
import Criterion.Main
import Data.Word (Word8)
import System.IO (hSetEncoding, stdout, utf8)
import Visual.XSharp.AST
import Visual.XSharp.Core
import Visual.XSharp.Core.CorePrep
import Visual.XSharp.Core.CorePrep.Verifier
import Visual.XSharp.Core.CorePrep.Wire
import Visual.XSharp.Core.Optimizer
import Visual.XSharp.Core.Verifier
import Visual.XSharp.Core.Wire

-- Production IR intentionally does not carry evaluation-strategy instances.
-- Fixture owners force the complete derived representation before Criterion
-- starts sampling without adding benchmark-only instances to compiler types.
newtype CoreModules = CoreModules [(Int, CoreModule)]

newtype CorePrepModules = CorePrepModules [(Int, CorePrepModule)]

instance NFData CoreModules where
    rnf (CoreModules values) = rnf (show values)

instance NFData CorePrepModules where
    rnf (CorePrepModules values) = rnf (show values)

main :: IO ()
main = do
    -- Criterion uses the microsecond symbol in its progress output. Windows can
    -- otherwise inherit a legacy console encoding and abort after a successful
    -- sample, so the harness owns its output encoding explicitly.
    hSetEncoding stdout utf8
    defaultMain
        [ bgroup
            "Core"
            [ env (pure (CoreModules (fixtures sizes))) $ \modules ->
                bgroup "Verify" [benchAt size verifyDigest (coreModuleAt size modules) | size <- sizes]
            , env (pure (CoreModules (fixtures sizes))) $ \modules ->
                bgroup "Encode" [benchAt size encodeDigest (coreModuleAt size modules) | size <- sizes]
            , env (traverse encodedFixture sizes) $ \documents ->
                bgroup "Decode" [benchAt size decodeDigest (documentAt size documents) | size <- sizes]
            , env (pure (CoreModules (inlineFixtures inlineSizes))) $ \modules ->
                bgroup "InlineLinearBody" [benchAt size optimizeDigest (coreModuleAt size modules) | size <- inlineSizes]
            ]
        , bgroup
            "CorePrep"
            [ env (pure (CoreModules (preparedFixtures sizes))) $ \modules ->
                bgroup "Prepare" [benchAt size prepareDigest (coreModuleAt size modules) | size <- sizes]
            , env (CorePrepModules <$> traverse preparedFixture sizes) $ \modules ->
                bgroup "Verify" [benchAt size verifyPrepDigest (corePrepModuleAt size modules) | size <- sizes]
            , env (CorePrepModules <$> traverse preparedFixture sizes) $ \modules ->
                bgroup "Encode" [benchAt size encodePrepDigest (corePrepModuleAt size modules) | size <- sizes]
            , env (traverse preparedDocument sizes) $ \documents ->
                bgroup "Decode" [benchAt size decodePrepDigest (documentAt size documents) | size <- sizes]
            ]
        ]
    where
        sizes = [8, 32, 128, 512]
        inlineSizes = [8, 32, 128, 256]

benchAt :: Int -> (a -> Int) -> a -> Benchmark
benchAt size measure input = bench (show size) (whnf measure input)

-- The size key selects a stable value owned by one Criterion environment. This
-- keeps fixture construction out of the timed expression without relying on
-- global CAF evaluation order.
moduleAt :: Int -> [(Int, a)] -> a
moduleAt requested values = case lookup requested values of
    Just value -> value
    Nothing -> error "benchmark fixture size was not prepared"

coreModuleAt :: Int -> CoreModules -> CoreModule
coreModuleAt requested (CoreModules values) = moduleAt requested values

corePrepModuleAt :: Int -> CorePrepModules -> CorePrepModule
corePrepModuleAt requested (CorePrepModules values) = moduleAt requested values

documentAt :: Int -> [(Int, [a])] -> [a]
documentAt = moduleAt

fixtures :: [Int] -> [(Int, CoreModule)]
fixtures = map (\size -> (size, makeCoreModule size))

preparedFixtures :: [Int] -> [(Int, CoreModule)]
preparedFixtures = fixtures

inlineFixtures :: [Int] -> [(Int, CoreModule)]
inlineFixtures = map (\size -> (size, makeInlineModule size))

encodedFixture :: Int -> IO (Int, [Word8])
encodedFixture size = case verifyCore (makeCoreModule size) of
    Left diagnostics -> fail (show diagnostics)
    Right verified -> case encodeCore defaultCoreWireLimits verified of
        Left failure -> fail (show failure)
        Right bytes -> pure (size, bytes)

preparedFixture :: Int -> IO (Int, CorePrepModule)
preparedFixture size = case prepareCore (makeCoreModule size) of
    Left diagnostics -> fail (show diagnostics)
    Right value -> case verifyCorePrep value of
        Left diagnostics -> fail (show diagnostics)
        Right verified -> pure (size, verified)

preparedDocument :: Int -> IO (Int, [Word8])
preparedDocument size = do
    (_, prepared) <- preparedFixture size
    case encodeCorePrep prepared of
        Left failure -> fail (show failure)
        Right bytes -> pure (size, bytes)

verifyDigest :: CoreModule -> Int
verifyDigest value = either length coreDigest (verifyCore value)

encodeDigest :: CoreModule -> Int
encodeDigest value = either (length . show) length (encodeCore defaultCoreWireLimits value)

decodeDigest :: [Word8] -> Int
decodeDigest bytes = either (length . show) coreDigest (decodeCore defaultCoreWireLimits bytes)

-- Measure the production verifier/effect/inliner fixed point. Fixture creation
-- remains in Criterion's environment and is not charged to the sample.
optimizeDigest :: CoreModule -> Int
optimizeDigest value = case optimizeCoreWith inlineBenchmarkOptions value of
    Left diagnostics -> length diagnostics
    Right result ->
        coreDigest (optimizedCore result)
            + optimizationIterations result
            + sum (map inlineRewrittenCalls (optimizationInlineReports result))

inlineBenchmarkOptions :: OptimizerOptions
inlineBenchmarkOptions =
    defaultOptimizerOptions
        { optimizerConstantPropagation = False
        , optimizerControlFlowSimplification = False
        , optimizerDeadCodeElimination = False
        }

prepareDigest :: CoreModule -> Int
prepareDigest value = either length corePrepDigest (prepareCore value)

verifyPrepDigest :: CorePrepModule -> Int
verifyPrepDigest value = either length corePrepDigest (verifyCorePrep value)

encodePrepDigest :: CorePrepModule -> Int
encodePrepDigest value = either (length . show) length (encodeCorePrep value)

decodePrepDigest :: [Word8] -> Int
decodePrepDigest bytes = either (length . show) corePrepDigest (decodeCorePrep bytes)

-- A structural digest forces every recursive container without allocating the
-- large intermediate String produced by show. The traversal is intentionally
-- simple and shared by operations whose result is an IR value.
coreDigest :: CoreModule -> Int
coreDigest value = sum (map functionDigest (coreModuleFunctions value))

functionDigest :: CoreFunction -> Int
functionDigest value =
    1
        + length (coreFunctionParameters value)
        + sum (map statementDigest (coreFunctionBody value))

statementDigest :: CoreStatement -> Int
statementDigest statement = case statement of
    CoreBind binding -> 1 + expressionDigest (coreBindingValue binding)
    CoreAssign _ expression -> 1 + expressionDigest expression
    CoreReturn expression -> 1 + expressionDigest expression
    CoreIf condition whenTrue whenFalse ->
        1 + expressionDigest condition + sum (map statementDigest whenTrue) + sum (map statementDigest whenFalse)
    CoreEvaluate expression -> 1 + expressionDigest expression

expressionDigest :: CoreExpression -> Int
expressionDigest expression = case expression of
    CoreVariable _ _ -> 1
    CoreLiteral _ _ -> 1
    CoreApply callee arguments _ -> 1 + expressionDigest callee + sum (map expressionDigest arguments)
    CorePrimitive _ arguments _ -> 1 + sum (map expressionDigest arguments)
    CoreLet _ _ value body _ -> 1 + expressionDigest value + expressionDigest body
    CoreClosure captures parameters _ body _ ->
        1
            + length parameters
            + sum (map captureDigest captures)
            + sum (map statementDigest body)

captureDigest :: CoreCapture -> Int
captureDigest capture = 1 + expressionDigest (coreCaptureValue capture)

corePrepDigest :: CorePrepModule -> Int
corePrepDigest value = sum (map prepFunctionDigest (corePrepModuleFunctions value))

prepFunctionDigest :: CorePrepFunction -> Int
prepFunctionDigest value =
    1
        + length (corePrepFunctionParameters value)
        + sum (map blockDigest (corePrepFunctionBlocks value))

blockDigest :: CorePrepBlock -> Int
blockDigest value =
    1
        + sum (map instructionDigest (corePrepBlockInstructions value))
        + terminatorDigest (corePrepBlockTerminator value)

instructionDigest :: CorePrepInstruction -> Int
instructionDigest instruction = case instruction of
    CorePrepBind _ _ _ operation -> 1 + operationDigest operation
    CorePrepAssign _ atom -> 1 + atomDigest atom
    CorePrepEvaluate operation -> 1 + operationDigest operation

operationDigest :: CorePrepOperation -> Int
operationDigest operation = case operation of
    CorePrepCopy atom -> 1 + atomDigest atom
    CorePrepCall callee arguments -> 1 + atomDigest callee + sum (map atomDigest arguments)
    CorePrepPrimitive _ arguments -> 1 + sum (map atomDigest arguments)
    CorePrepMakeClosure _ captures -> 1 + sum (map prepCaptureDigest captures)

prepCaptureDigest :: CorePrepCapture -> Int
prepCaptureDigest (CorePrepCapture _ _ _ atom) = 1 + atomDigest atom

atomDigest :: CorePrepAtom -> Int
atomDigest atom = case atom of
    CorePrepVariable _ _ -> 1
    CorePrepLiteral _ _ -> 1

terminatorDigest :: CorePrepTerminator -> Int
terminatorDigest terminator = case terminator of
    CorePrepReturn atom -> 1 + atomDigest atom
    CorePrepBranch atom _ _ -> 1 + atomDigest atom
    CorePrepJump _ -> 1
    CorePrepUnreachable -> 1

makeCoreModule :: Int -> CoreModule
makeCoreModule count = CoreModule (QualifiedName [Identifier "Benchmark"]) (map makeFunction [0 .. count - 1])

-- Each call expands a two-binding linear helper. Function and local symbols are
-- globally disjoint so the benchmark measures fresh allocation and cloning,
-- not malformed-input rejection.
makeInlineModule :: Int -> CoreModule
makeInlineModule count =
    CoreModule
        (QualifiedName [Identifier "InlineBenchmark"])
        (inlineHelper : map makeCaller [0 .. count - 1])
    where
        helperName = name 1 "ScaleAndBias"
        parameterName = name 2 "input"
        firstName = name 3 "scaled"
        secondName = name 4 "biased"
        inlineHelper =
            CoreFunction
                helperName
                [(parameterName, intType)]
                intType
                [ CoreBind
                    ( CoreBinding
                        firstName
                        intType
                        False
                        (CorePrimitive CoreMultiply [CoreVariable parameterName intType, integer 2] intType)
                    )
                , CoreBind
                    ( CoreBinding
                        secondName
                        intType
                        False
                        (CorePrimitive CoreAdd [CoreVariable firstName intType, integer 1] intType)
                    )
                , CoreReturn (CoreVariable secondName intType)
                ]
        makeCaller index =
            let base = index * 3 + 100
                argumentName = name (base + 1) "argument"
                callerName = name (base + 2) "Caller"
                argument = CorePrimitive CoreAdd [integer (toInteger index), integer 7] intType
                applied =
                    CoreApply
                        (CoreVariable helperName (FunctionType [intType] intType))
                        [argument]
                        intType
             in CoreFunction
                    callerName
                    []
                    intType
                    [ CoreBind (CoreBinding argumentName intType False applied)
                    , CoreReturn (CoreVariable argumentName intType)
                    ]

makeFunction :: Int -> CoreFunction
makeFunction index =
    CoreFunction
        (name base "Function")
        []
        intType
        [ CoreBind (CoreBinding valueName intType True sumValue)
        , CoreIf comparison [CoreAssign valueName (integer 2)] [CoreAssign valueName (integer 0)]
        , CoreReturn (CoreVariable valueName intType)
        ]
    where
        base = index * 4 + 1
        valueName = name (base + 1) "value"
        sumValue = CorePrimitive CoreAdd [integer (toInteger index), integer 1] intType
        comparison = CorePrimitive CoreGreaterEqual [CoreVariable valueName intType, integer 1] boolType

name :: Int -> String -> ResolvedName
name identifier spelling = ResolvedName (SymbolId identifier) (Identifier spelling)

integer :: Integer -> CoreExpression
integer value = CoreLiteral (CoreInteger value) intType
