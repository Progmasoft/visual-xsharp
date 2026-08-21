-- SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
-- SPDX-License-Identifier: MPL-2.0

module Main (main) where

import Data.Text qualified as Text
import Data.Text.IO qualified as TextIO
import System.Environment (getArgs)
import System.Exit (exitFailure)
import Visual.Formatter
import Visual.XSharp.Diagnostic
import Visual.XSharp.Frontend

data Command = Help | Standard FilePath | InPlace FilePath | DryRun FilePath

main :: IO ()
main = do
    arguments <- getArgs
    case parseCommand arguments of
        Nothing -> usage >> exitFailure
        Just command -> runCommand command

parseCommand :: [String] -> Maybe Command
parseCommand ["-Help"] = Just Help
parseCommand [path] = Just (Standard path)
parseCommand ["-In-Place", path] = Just (InPlace path)
parseCommand ["-Dry-Run", path] = Just (DryRun path)
parseCommand _ = Nothing

runCommand :: Command -> IO ()
runCommand Help = usage
runCommand command = do
    let path = commandPath command
    source <- Text.unpack <$> TextIO.readFile path
    case formatSource defaultFormatOptions (CompilerInput path source) of
        Left problems -> mapM_ printCompilerDiagnostic problems >> exitFailure
        Right result -> case command of
            Standard _ -> putStr (formattedSource result)
            InPlace _ -> TextIO.writeFile path (Text.pack (formattedSource result))
            DryRun _ -> if formattingChanged result then exitFailure else pure ()

commandPath :: Command -> FilePath
commandPath command = case command of
    Help -> error "Help does not own a source path"
    Standard path -> path
    InPlace path -> path
    DryRun path -> path

printCompilerDiagnostic :: Diagnostic -> IO ()
printCompilerDiagnostic problem =
    putStrLn (diagnosticCode problem ++ ": " ++ diagnosticMessage problem)

usage :: IO ()
usage = putStrLn "usage: vfmt <file.vxs> | vfmt -In-Place <file.vxs> | vfmt -Dry-Run <file.vxs> | vfmt -Help"
