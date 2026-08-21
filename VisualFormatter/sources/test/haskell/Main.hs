-- SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
-- SPDX-License-Identifier: MPL-2.0

module Main (main) where

import System.Exit (exitFailure)
import Visual.Formatter
import Visual.XSharp.Frontend

main :: IO ()
main = do
    check "formatter removes trailing whitespace and preserves the first line ending" normalizesLayout
    check "formatter reports an already formatted source as unchanged" unchangedSource
    check "formatter refuses malformed source" rejectsMalformedSource

check :: String -> Bool -> IO ()
check label passed = if passed then putStrLn ("PASS: " ++ label) else putStrLn ("FAIL: " ++ label) >> exitFailure

formattedProgram :: String
formattedProgram =
    "namespace Example;\r\npublic class Program {\r\n    public static void Main() {\r\n        return;\r\n    }\r\n}\r\n"

normalizesLayout :: Bool
normalizesLayout =
    let source =
            "namespace Example;  \r\npublic class Program {\n    public static void Main() {\t\r\n        return;\r\n    }\r\n}\r\n"
     in case formatSource defaultFormatOptions (CompilerInput "Program.vxs" source) of
            Right result -> formattedSource result == formattedProgram && formattingChanged result
            Left _ -> False

unchangedSource :: Bool
unchangedSource = case formatSource defaultFormatOptions (CompilerInput "Program.vxs" formattedProgram) of
    Right result -> not (formattingChanged result)
    Left _ -> False

rejectsMalformedSource :: Bool
rejectsMalformedSource = case formatSource defaultFormatOptions (CompilerInput "Broken.vxs" "@   ") of
    Left _ -> True
    Right _ -> False
