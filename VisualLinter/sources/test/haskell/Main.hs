-- SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
-- SPDX-License-Identifier: MPL-2.0

module Main (main) where

import Data.List (sort)
import System.Exit (exitFailure)
import Visual.Formatter
import Visual.Linter
import Visual.XSharp.Frontend

main :: IO ()
main = do
    check "linter reports compiler and physical source diagnostics" reportsDiagnostics
    check "safe fixes remove all implemented source hygiene findings" safeFixes
    check "check catalog exposes stable command output" checkCatalog

check :: String -> Bool -> IO ()
check label passed = if passed then putStrLn ("PASS: " ++ label) else putStrLn ("FAIL: " ++ label) >> exitFailure

validSource :: String
validSource =
    "namespace Example;\r\npublic class Program {\n    public static void Main() {  \r\n        return;\r\n    }\r\n}"

reportsDiagnostics :: Bool
reportsDiagnostics =
    let sourceProblems = lintSource (CompilerInput "Program.vxs" validSource)
        compilerProblems = lintSource (CompilerInput "Broken.vxs" "@")
        ruleIds = map lintRuleId sourceProblems
     in sort ruleIds
            == sort
                [ "format.trailingWhitespace"
                , "format.mixedLineEndings"
                , "format.missingFinalNewline"
                ]
            && any ((== "compiler.VXL0001") . lintRuleId) compilerProblems

safeFixes :: Bool
safeFixes = case applySafeFixes (CompilerInput "Program.vxs" validSource) of
    Left _ -> False
    Right result -> null (lintSource (CompilerInput "Program.vxs" (formattedSource result)))

checkCatalog :: Bool
checkCatalog =
    map fst availableChecks
        == [ "compiler"
           , "format.trailingWhitespace"
           , "format.mixedLineEndings"
           , "format.missingFinalNewline"
           ]
