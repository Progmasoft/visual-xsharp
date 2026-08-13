-- SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
-- SPDX-License-Identifier: MPL-2.0

module Main (main) where

import Control.Exception (IOException, try)
import System.Environment (getArgs)
import System.Exit (exitFailure)
import System.IO (hPutStrLn, stderr)
import Visual.XSharp.Compiler
import Visual.XSharp.Core.Artifact
import Visual.XSharp.Diagnostic

data FrontendCommand = FrontendCommand
    { outputPath :: FilePath
    , sourcePath :: FilePath
    }

main :: IO ()
main = do
    arguments <- getArgs
    case parseCommand arguments of
        Left problem -> failWith problem
        Right command -> compileCommand command

parseCommand :: [String] -> Either String FrontendCommand
-- This executable is a deliberately narrow compiler boundary, not a second
-- user-facing CLI. The C++ driver owns public command semantics and passes one
-- source path plus one private Core artifact destination here.
parseCommand ["--output", output, source]
    | null output = Left "the Core output path is empty"
    | null source = Left "the Visual X# source path is empty"
    | otherwise = Right (FrontendCommand output source)
parseCommand _ = Left "usage: vxs-frontend --output OUTPUT.core SOURCE.vxs"

compileCommand :: FrontendCommand -> IO ()
compileCommand command = do
    -- readFile preserves the compiler's current scalar-oriented String model;
    -- byte decoding policy will move into a dedicated source loader later.
    loaded <- try (readFile (sourcePath command)) :: IO (Either IOException String)
    case loaded of
        Left issue -> failWith (sourcePath command ++ ": " ++ show issue)
        Right source ->
            case compileToCorePrep (CompilerInput (sourcePath command) source) of
                Left diagnostics -> do
                    mapM_ printDiagnostic diagnostics
                    exitFailure
                Right artifacts -> do
                    -- CorePrep remains internal. The process boundary carries verified,
                    -- optimized Core so C++ can perform the adapting CorePrep step in RAM.
                    written <- writeCoreArtifact (outputPath command) (artifactOptimizedCore artifacts)
                    case written of
                        Left issue -> failWith (show issue)
                        Right () -> pure ()

printDiagnostic :: Diagnostic -> IO ()
printDiagnostic diagnostic =
    hPutStrLn stderr (location ++ diagnosticCode diagnostic ++ ": " ++ diagnosticMessage diagnostic)
    where
        location = maybe "" ((++ ": ") . show) (diagnosticSpan diagnostic)

failWith :: String -> IO a
failWith message = hPutStrLn stderr ("vxs-frontend: " ++ message) >> exitFailure
