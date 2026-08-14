-- SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
-- SPDX-License-Identifier: MPL-2.0

module Main (main) where

import Data.Char (isAlpha, isAlphaNum)
import System.Environment (getArgs)
import System.Exit (exitFailure)
import System.IO (hPutStrLn, stderr)
import Visual.XSharp.AST
import Visual.XSharp.Compiler
import Visual.XSharp.Core (CoreModule)
import Visual.XSharp.Core.Artifact
import Visual.XSharp.Diagnostic
import Visual.XSharp.SourceSet

data FrontendCommand
    = CompileFile FilePath FilePath
    | CompileProject FilePath FilePath QualifiedName [FilePath] [FilePath]

data CommandOptions = CommandOptions
    { optionOutput :: Maybe FilePath
    , optionSourceFile :: Maybe FilePath
    , optionProjectRoot :: Maybe FilePath
    , optionEntry :: Maybe String
    , optionSourceRoots :: [FilePath]
    , optionExcludes :: [FilePath]
    }

emptyOptions :: CommandOptions
emptyOptions = CommandOptions Nothing Nothing Nothing Nothing [] []

main :: IO ()
main = do
    arguments <- getArgs
    case parseCommand arguments of
        Left problem -> failWith problem
        Right command -> compileCommand command

-- This executable is a deliberately narrow process boundary, not a second
-- public CLI. C++ owns user-facing command semantics. Named private options are
-- still used here so repeated source roots and exclusions cannot be confused
-- with positional source files or shell-expanded globs.
parseCommand :: [String] -> Either String FrontendCommand
parseCommand arguments = parseOptions emptyOptions arguments >>= finishCommand

parseOptions :: CommandOptions -> [String] -> Either String CommandOptions
parseOptions options [] = Right options
parseOptions options (name : remaining) = case name of
    "--output" -> uniqueValue "--output" optionOutput setOutput options remaining
    "--source-file" -> uniqueValue "--source-file" optionSourceFile setSourceFile options remaining
    "--project-root" -> uniqueValue "--project-root" optionProjectRoot setProjectRoot options remaining
    "--entry" -> uniqueValue "--entry" optionEntry setEntry options remaining
    "--source-root" -> repeatedValue "--source-root" addSourceRoot options remaining
    "--exclude" -> repeatedValue "--exclude" addExclude options remaining
    _ -> Left ("unknown private frontend option: " ++ name)

uniqueValue ::
    String ->
    (CommandOptions -> Maybe String) ->
    (String -> CommandOptions -> CommandOptions) ->
    CommandOptions ->
    [String] ->
    Either String CommandOptions
uniqueValue name getter setter options remaining = case remaining of
    [] -> Left (name ++ " requires one value")
    value : rest
        | null value -> Left (name ++ " value is empty")
        | getter options /= Nothing -> Left (name ++ " may be specified only once")
        | otherwise -> parseOptions (setter value options) rest

repeatedValue ::
    String ->
    (String -> CommandOptions -> CommandOptions) ->
    CommandOptions ->
    [String] ->
    Either String CommandOptions
repeatedValue name setter options remaining = case remaining of
    [] -> Left (name ++ " requires one value")
    value : rest
        | null value -> Left (name ++ " value is empty")
        | otherwise -> parseOptions (setter value options) rest

setOutput, setSourceFile, setProjectRoot, setEntry :: String -> CommandOptions -> CommandOptions
setOutput value options = options {optionOutput = Just value}
setSourceFile value options = options {optionSourceFile = Just value}
setProjectRoot value options = options {optionProjectRoot = Just value}
setEntry value options = options {optionEntry = Just value}

addSourceRoot, addExclude :: String -> CommandOptions -> CommandOptions
addSourceRoot value options = options {optionSourceRoots = optionSourceRoots options ++ [value]}
addExclude value options = options {optionExcludes = optionExcludes options ++ [value]}

finishCommand :: CommandOptions -> Either String FrontendCommand
finishCommand options = case (optionOutput options, optionSourceFile options, optionProjectRoot options, optionEntry options) of
    (Nothing, _, _, _) -> Left "--output is required"
    (Just output, Just source, Nothing, Nothing)
        | null (optionSourceRoots options) && null (optionExcludes options) ->
            Right (CompileFile output source)
        | otherwise -> Left "file compilation cannot declare source roots or exclusions"
    (Just output, Nothing, Just root, Just entryText) -> do
        entry <- parseQualifiedName entryText
        if null (optionSourceRoots options)
            then Left "project compilation requires at least one --source-root"
            else Right (CompileProject output root entry (optionSourceRoots options) (optionExcludes options))
    (Just _, Just _, Just _, _) -> Left "--source-file and --project-root are mutually exclusive"
    (Just _, Nothing, _, _) -> Left "project compilation requires --project-root and --entry"
    (Just _, Just _, _, _) -> Left "file compilation accepts only --output and --source-file"

parseQualifiedName :: String -> Either String QualifiedName
parseQualifiedName text =
    let parts = splitDot text
     in if length parts < 2
            then Left "--entry must name a namespace-qualified class"
            else
                if all validIdentifier parts
                    then Right (QualifiedName (map Identifier parts))
                    else Left ("--entry contains an invalid identifier: " ++ text)

validIdentifier :: String -> Bool
validIdentifier [] = False
validIdentifier (first : remaining) =
    (isAlpha first || first == '_') && all (\value -> isAlphaNum value || value == '_') remaining

splitDot :: String -> [String]
splitDot [] = [""]
splitDot ('.' : remaining) = "" : splitDot remaining
splitDot (value : remaining) = case splitDot remaining of
    [] -> [[value]]
    first : rest -> (value : first) : rest

compileCommand :: FrontendCommand -> IO ()
compileCommand command = case command of
    CompileFile output source -> compileFile output source
    CompileProject output root entry roots excludes -> compileProject output root entry roots excludes

compileFile :: FilePath -> FilePath -> IO ()
compileFile output source = do
    loaded <- loadSourceFile source
    case loaded of
        Left diagnostics -> failWithDiagnostics diagnostics
        Right document ->
            case compileToCorePrep (toCompilerInput document) of
                Left diagnostics -> failWithDiagnostics diagnostics
                Right artifacts -> writeArtifact output (artifactOptimizedCore artifacts)

compileProject :: FilePath -> FilePath -> QualifiedName -> [FilePath] -> [FilePath] -> IO ()
compileProject output root entry roots excludes = do
    loaded <- loadSourceSet (SourceSetRequest root roots excludes)
    case loaded of
        Left diagnostics -> failWithDiagnostics diagnostics
        Right documents ->
            case compileProjectToCorePrep entry (map toCompilerInput documents) of
                Left diagnostics -> failWithDiagnostics diagnostics
                Right artifacts -> writeArtifact output (projectEntryCore artifacts)

toCompilerInput :: LoadedSource -> CompilerInput
toCompilerInput source = CompilerInput (loadedSourcePath source) (loadedSourceText source)

writeArtifact :: FilePath -> CoreModule -> IO ()
writeArtifact output core = do
    -- CorePrep remains internal. The process boundary carries verified,
    -- optimized Core so C++ can perform the adapting CorePrep step in RAM.
    written <- writeCoreArtifact output core
    case written of
        Left issue -> failWith (show issue)
        Right () -> pure ()

failWithDiagnostics :: [Diagnostic] -> IO a
failWithDiagnostics diagnostics = do
    mapM_ printDiagnostic diagnostics
    exitFailure

printDiagnostic :: Diagnostic -> IO ()
printDiagnostic diagnostic =
    hPutStrLn stderr (location ++ diagnosticCode diagnostic ++ ": " ++ diagnosticMessage diagnostic)
    where
        location = maybe "" ((++ ": ") . show) (diagnosticSpan diagnostic)

failWith :: String -> IO a
failWith message = hPutStrLn stderr ("vxs-frontend: " ++ message) >> exitFailure
