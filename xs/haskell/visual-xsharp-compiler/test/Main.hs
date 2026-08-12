-- SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
-- SPDX-License-Identifier: MPL-2.0
module Main (main) where

import Data.List (isInfixOf)
import Control.Exception (finally)
import Data.Word (Word8)
import System.Directory (doesFileExist, getTemporaryDirectory, removeFile)
import System.Exit (exitFailure)
import System.FilePath ((</>))
import Visual.XSharp.AST
import Visual.XSharp.Compiler
import Visual.XSharp.Core
import Visual.XSharp.Core.CorePrep
import Visual.XSharp.Core.CorePrep.Artifact
import Visual.XSharp.Core.CorePrep.Verifier
import Visual.XSharp.Core.CorePrep.Wire
import Visual.XSharp.Diagnostic
import Visual.XSharp.Pipeline.Stage

main :: IO ()
main = do
    check "full frontend compiles renewed declarations through CorePrep" fullPipeline
    check "operator precedence is preserved and constants fold" precedence
    check "unterminated final expression supplies the function result" finalExpression
    check "semicolon-terminated pure expression is rejected" pureExpressionStatement
    check "CorePrep atomizes calls and creates explicit control flow" preparedControlFlow
    check "project entry resolves namespace, class, and public static void Main" entryContract
    check "int Main is rejected for the selected project entry" wrongEntryReturn
    check "top-level runtime functions are rejected" topLevelFunction
    check "unknown names are diagnosed by name resolution" unknownName
    check "immutable assignment is rejected" immutableAssignment
    check "wrong condition types are rejected" wrongCondition
    check "wrong call arity is rejected" wrongArity
    check "lexer rejects unsupported input" badCharacter
    check "renewed artifact extensions remain stable" (map artifactExtension [Core, CorePrep, Xpp, Xmm] == [Just ".core", Nothing, Just ".xpp", Just ".xmm"])
    check "Haskell CorePrep verifier rejects duplicate blocks" invalidCorePrep
    check "CorePrep wire codec round-trips the frontend result" wireRoundTrip
    check "CorePrep wire codec rejects truncated input" wireRejectsTruncation
    check "CorePrep wire codec rejects trailing input" wireRejectsTrailingInput
    check "CorePrep wire codec rejects unsupported types" wireRejectsUnsupportedType
    check "CorePrep wire codec preserves Unicode scalar values" wirePreservesUnicode
    check "CorePrep wire v1 golden bytes remain stable" wireGoldenDocument
    checkIO "explicit .core artifact round-trips through binary I/O" artifactRoundTrip
    checkIO "artifact API rejects a non-.core path" artifactRejectsExtension

check :: String -> Bool -> IO ()
check label passed = if passed then putStrLn ("PASS: " ++ label) else putStrLn ("FAIL: " ++ label) >> exitFailure

checkIO :: String -> IO Bool -> IO ()
checkIO label action = action >>= check label

compile :: String -> Either [Diagnostic] FrontendArtifacts
compile = compileToCorePrep . CompilerInput "test.vxs"

sample :: String
sample = unlines
    [ "namespace Name;"
    , "class Main {"
    , "  public static int Sum(_ int left, _ int right) {"
    , "    return left + right;"
    , "  }"
    , "  public static void Main() {"
    , "    int value = Sum(20, 22);"
    , "    if (value >= 40) {"
    , "        value = value + 1;"
    , "    } else {"
    , "        value = 0;"
    , "    }"
    , "    return;"
    , "  }"
    , "}"
    ]

fullPipeline :: Bool
fullPipeline = case compileEntryToCorePrep (QualifiedName [Identifier "Name", Identifier "Main"]) (CompilerInput "test.vxs" sample) of
    Right artifacts -> length (syntaxDeclarations (typedSyntaxTree (artifactTypedAST artifacts))) == 1
        && length (coreModuleFunctions (artifactOptimizedCore artifacts)) == 2
        && length (corePrepModuleFunctions (artifactCorePrep artifacts)) == 2
    Left _ -> False

precedence :: Bool
precedence = case compile "class Math { public static int Calculate() { return 2 + 3 * 4; } }" of
    Right artifacts -> case coreModuleFunctions (artifactOptimizedCore artifacts) of
      function:_ -> case coreFunctionBody function of
        [CoreReturn (CoreLiteral (CoreInteger 14) _)] -> True
        _ -> False
      [] -> False
    Left _ -> False

finalExpression :: Bool
finalExpression = case compile "class Math { auto Calculate() { 2 + 3 * 4 } }" of
    Right artifacts -> case coreModuleFunctions (artifactOptimizedCore artifacts) of
        function:_ -> case coreFunctionBody function of [CoreReturn (CoreLiteral (CoreInteger 14) _)] -> True; _ -> False
        [] -> False
    Left _ -> False

pureExpressionStatement :: Bool
pureExpressionStatement = hasCode "VXT0013" (compile "class App { void Run() { 42; } }")

preparedControlFlow :: Bool
preparedControlFlow = case compileEntryToCorePrep (QualifiedName [Identifier "Name", Identifier "Main"]) (CompilerInput "test.vxs" sample) of
    Right artifacts ->
        let functions = corePrepModuleFunctions (artifactCorePrep artifacts)
            mainFunction = functions !! 1
            blocks = corePrepFunctionBlocks mainFunction
        in length blocks >= 4 && any isBranch blocks && any hasCall blocks
    Left _ -> False
  where
    isBranch block = case corePrepBlockTerminator block of CorePrepBranch _ _ _ -> True; _ -> False
    hasCall block = any callInstruction (corePrepBlockInstructions block)
    callInstruction instruction = case instruction of CorePrepBind _ _ _ (CorePrepCall _ _) -> True; _ -> False

entryContract :: Bool
entryContract = case compile sample of
    Right artifacts -> validateEntryPoint (QualifiedName [Identifier "Name", Identifier "Main"]) (artifactTypedAST artifacts) == Right ()
    Left _ -> False
wrongEntryReturn :: Bool
wrongEntryReturn = hasCode "VXE0008" (compileEntryToCorePrep (QualifiedName [Identifier "Name", Identifier "Main"])
    (CompilerInput "entry.vxs" "namespace Name; class Main { public static int Main() { return 0; } }"))
topLevelFunction :: Bool
topLevelFunction = hasCode "VXP0006" (compile "int Main() { return 0; }")

hasCode :: String -> Either [Diagnostic] a -> Bool
hasCode code result = case result of Left problems -> any ((== code) . diagnosticCode) problems; Right _ -> False
unknownName :: Bool
unknownName = hasCode "VXN0001" (compile "class App { int Read() { return missing; } }")
immutableAssignment :: Bool
immutableAssignment = hasCode "VXT0003" (compile "class App { int Read() { final int value = 1; value = 2; return value; } }")
wrongCondition :: Bool
wrongCondition = hasCode "VXT0006" (compile "class App { void Run() { if (1) { return; } return; } }")
wrongArity :: Bool
wrongArity = hasCode "VXT0008" (compile "class App { int Sum(_ int a, _ int b) { return a+b; } void Run() { Sum(1); } }")
badCharacter :: Bool
badCharacter = case compile "class App { void Run() { @; } }" of Left problems -> any (isInfixOf "unsupported character" . diagnosticMessage) problems; Right _ -> False

invalidCorePrep :: Bool
invalidCorePrep = case compile sample of
    Right artifacts -> case corePrepModuleFunctions (artifactCorePrep artifacts) of
        function:rest -> hasCode "VXC0003" (verifyCorePrep (artifactCorePrep artifacts)
            { corePrepModuleFunctions = function { corePrepFunctionBlocks = corePrepFunctionBlocks function ++ take 1 (corePrepFunctionBlocks function) } : rest })
        [] -> False
    Left _ -> False

wireRoundTrip :: Bool
wireRoundTrip = case compile sample of
    Right artifacts -> case encodeCorePrep (artifactCorePrep artifacts) >>= decodeCorePrep of
        Right decoded -> decoded == artifactCorePrep artifacts
        Left _ -> False
    Left _ -> False

wireRejectsTruncation :: Bool
wireRejectsTruncation = case compile sample of
    Right artifacts -> case encodeCorePrep (artifactCorePrep artifacts) of
        Right bytes -> case decodeCorePrep (take (length bytes - 1) bytes) of
            Left problem -> wireErrorKind problem == TruncatedInput
            Right _ -> False
        Left _ -> False
    Left _ -> False

wireRejectsTrailingInput :: Bool
wireRejectsTrailingInput = case compile sample of
    Right artifacts -> case encodeCorePrep (artifactCorePrep artifacts) of
        Right bytes -> case decodeCorePrep (bytes ++ [0]) of
            Left problem -> wireErrorKind problem == TrailingInput
            Right _ -> False
        Left _ -> False
    Left _ -> False

wireRejectsUnsupportedType :: Bool
wireRejectsUnsupportedType = case compile sample of
    Right artifacts ->
        let prepared = artifactCorePrep artifacts
        in case corePrepModuleFunctions prepared of
            function:remaining ->
                let changed = prepared { corePrepModuleFunctions = function { corePrepFunctionReturnType = ErrorType } : remaining }
                in case encodeCorePrep changed of
                    Left problem -> wireErrorKind problem == UnsupportedType
                    Right _ -> False
            [] -> False
    Left _ -> False

wirePreservesUnicode :: Bool
wirePreservesUnicode =
    let name = ResolvedName (SymbolId 1) (Identifier "Text")
        literal = CorePrepLiteral (CoreString "Visual X# λ 😀") stringType
        function = CorePrepFunction name [] stringType 0 [CorePrepBlock 0 [] (CorePrepReturn literal)]
        prepared = CorePrepModule (QualifiedName [Identifier "Unicode"]) [function]
    in case encodeCorePrep prepared >>= decodeCorePrep of
        Right decoded -> decoded == prepared
        Left _ -> False

goldenModule :: CorePrepModule
goldenModule =
    let mainName = ResolvedName (SymbolId 1) (Identifier "Main")
        mainBlock = CorePrepBlock 0 [] (CorePrepReturn (CorePrepLiteral CoreUnit unitType))
        mainFunction = CorePrepFunction mainName [] unitType 0 [mainBlock]
    in CorePrepModule (QualifiedName [Identifier "Demo"]) [mainFunction]

goldenBytes :: [Word8]
goldenBytes =
    [ 0x56, 0x58, 0x43, 0x50, 0x01, 0x00, 0x00, 0x00
    , 0x01, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00
    , 0x44, 0x00, 0x00, 0x00, 0x65, 0x00, 0x00, 0x00
    , 0x6d, 0x00, 0x00, 0x00, 0x6f, 0x00, 0x00, 0x00
    , 0x01, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00
    , 0x00, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00
    , 0x4d, 0x00, 0x00, 0x00, 0x61, 0x00, 0x00, 0x00
    , 0x69, 0x00, 0x00, 0x00, 0x6e, 0x00, 0x00, 0x00
    , 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
    , 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
    , 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00
    ]

wireGoldenDocument :: Bool
wireGoldenDocument = encodeCorePrep goldenModule == Right goldenBytes
    && decodeCorePrep goldenBytes == Right goldenModule

artifactRoundTrip :: IO Bool
artifactRoundTrip = do
    temporary <- getTemporaryDirectory
    let path = temporary </> "visual-xsharp-coreprep-wire-v1.core"
        cleanup = doesFileExist path >>= \exists -> if exists then removeFile path else pure ()
    (do written <- writeCorePrepArtifact path goldenModule
        loaded <- readCorePrepArtifact path
        pure (written == Right () && loaded == Right goldenModule)) `finally` cleanup

artifactRejectsExtension :: IO Bool
artifactRejectsExtension = do
    result <- writeCorePrepArtifact "invalid.xpp" goldenModule
    pure (result == Left (InvalidArtifactPath "invalid.xpp"))
