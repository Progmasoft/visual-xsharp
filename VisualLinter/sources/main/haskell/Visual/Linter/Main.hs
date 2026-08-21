-- SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
-- SPDX-License-Identifier: MPL-2.0

module Main (main) where

import Data.Text qualified as Text
import Data.Text.IO qualified as TextIO
import System.Environment (getArgs)
import System.Exit (exitFailure)
import Visual.Formatter (formattedSource)
import Visual.Linter
import Visual.XSharp.AST
import Visual.XSharp.Frontend

data Command = Help | ListChecks | Check FilePath | Fix FilePath

main :: IO ()
main = do
    arguments <- getArgs
    case parseCommand arguments of
        Nothing -> usage >> exitFailure
        Just command -> runCommand command

parseCommand :: [String] -> Maybe Command
parseCommand ["-Help"] = Just Help
parseCommand ["-List-Checks"] = Just ListChecks
parseCommand ["-Fix", path] = Just (Fix path)
parseCommand [path] = Just (Check path)
parseCommand _ = Nothing

runCommand :: Command -> IO ()
runCommand Help = usage
runCommand ListChecks = mapM_ printCheck availableChecks
runCommand command = do
    let path = commandPath command
    source <- Text.unpack <$> TextIO.readFile path
    finalSource <- case command of
        Fix _ -> applyFixes path source
        _ -> pure source
    let problems = lintSource (CompilerInput path finalSource)
    mapM_ printDiagnostic problems
    if null problems then pure () else exitFailure

applyFixes :: FilePath -> String -> IO String
applyFixes path source = case applySafeFixes (CompilerInput path source) of
    Left _ -> pure source
    Right result -> do
        TextIO.writeFile path (Text.pack (formattedSource result))
        pure (formattedSource result)

commandPath :: Command -> FilePath
commandPath command = case command of
    Check path -> path
    Fix path -> path
    Help -> error "Help does not own a source path"
    ListChecks -> error "ListChecks does not own a source path"

printCheck :: (String, String) -> IO ()
printCheck (ruleId, explanation) = putStrLn (ruleId ++ "\t" ++ explanation)

printDiagnostic :: LintDiagnostic -> IO ()
printDiagnostic problem =
    putStrLn
        ( renderLocation (lintSpan problem)
            ++ renderSeverity (lintSeverity problem)
            ++ " ["
            ++ lintRuleId problem
            ++ "] "
            ++ lintMessage problem
        )

renderLocation :: Maybe SourceSpan -> String
renderLocation Nothing = ""
renderLocation (Just spanValue) =
    sourceFile spanValue
        ++ ":"
        ++ show (sourceLine (sourceStart spanValue))
        ++ ":"
        ++ show (sourceColumn (sourceStart spanValue))
        ++ ": "

renderSeverity :: LintSeverity -> String
renderSeverity severity = case severity of
    LintInfo -> "info"
    LintWarning -> "warning"
    LintError -> "error"

usage :: IO ()
usage = putStrLn "usage: vlint <file.vxs> | vlint -Fix <file.vxs> | vlint -List-Checks | vlint -Help"
