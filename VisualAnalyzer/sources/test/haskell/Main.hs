-- SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
-- SPDX-License-Identifier: MPL-2.0

module Main (main) where

import System.Exit (exitFailure)
import Visual.Analyzer
import Visual.XSharp.Frontend

main :: IO ()
main = do
    check "syntax analysis returns compiler-owned syntax artifacts" syntaxAnalysis
    check "full analysis reaches CorePrep" fullAnalysis
    check "compiler positions are translated to zero-based protocol positions" protocolPositions

check :: String -> Bool -> IO ()
check label passed = if passed then putStrLn ("PASS: " ++ label) else putStrLn ("FAIL: " ++ label) >> exitFailure

validSource :: String
validSource =
    unlines
        [ "namespace Example;"
        , "public class Program {"
        , "    public static void Main() {"
        , "        return;"
        , "    }"
        , "}"
        ]

syntaxAnalysis :: Bool
syntaxAnalysis = case analyzeDocument Syntax (CompilerInput "Program.vxs" validSource) of
    Right SyntaxResult {} -> True
    _ -> False

fullAnalysis :: Bool
fullAnalysis = case analyzeDocument Full (CompilerInput "Program.vxs" validSource) of
    Right FullResult {} -> True
    _ -> False

protocolPositions :: Bool
protocolPositions = case analyzeDocument Syntax (CompilerInput "Broken.vxs" "@") of
    Left [problem] -> case analyzerRange problem of
        Just (ProtocolRange (ProtocolPosition 0 0) (ProtocolPosition 0 1)) -> analyzerCode problem == "VXL0001"
        _ -> False
    _ -> False
