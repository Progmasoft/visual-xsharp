-- SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
-- SPDX-License-Identifier: MPL-2.0

module SourceSetTests (sourceSetTests) where

import Control.Exception (finally)
import Data.ByteString qualified as ByteString
import System.Directory
    ( createDirectory
    , createDirectoryIfMissing
    , getTemporaryDirectory
    , removeDirectoryRecursive
    , removeFile
    )
import System.FilePath (takeDirectory, (</>))
import System.IO (hClose, openTempFile)
import System.Info (os)
import Visual.XSharp.AST
import Visual.XSharp.Compiler
import Visual.XSharp.Core
import Visual.XSharp.Diagnostic
import Visual.XSharp.SourceSet

sourceSetTests :: [(String, IO Bool)]
sourceSetTests =
    [ ("source globs distinguish segments and recursive wildcards", pure globSemantics)
    , ("source loader discovers deterministic project-relative order", discoveryOrder)
    , ("source loader applies project-relative exclusions", exclusionPolicy)
    , ("overlapping source roots do not duplicate compilation units", overlappingRoots)
    , ("uppercase source extensions are not treated as .vxs", caseSensitiveExtension)
    , ("malformed UTF-8 is rejected before lexing", invalidUtf8)
    , ("explicit file mode uses the strict source decoder", explicitFileDecoder)
    , ("explicit file mode requires the exact .vxs extension", explicitFileExtension)
    , ("a configured source root cannot escape the project", escapingRoot)
    , ("an empty discovered source set is diagnosed", emptySourceSet)
    , ("project compiler merges files that declare one namespace", namespaceMerge)
    , ("project compiler selects entry by namespace and class", entrySelection)
    , ("project compiler validates every namespace", validatesEveryNamespace)
    , ("project compiler accumulates independent file diagnostics", accumulatesFileDiagnostics)
    , ("duplicate types across physical files are rejected", duplicateAcrossFiles)
    , ("missing entry namespaces are diagnosed", missingEntryNamespace)
    , ("entry names must include a namespace", unqualifiedEntry)
    , ("entry Main remains public static void across merged units", mergedEntryContract)
    ]

globSemantics :: Bool
globSemantics =
    and
        [ matchesGlob "Sources/**/*.vxs" "Sources/Main.vxs"
        , matchesGlob "Sources/**/*.vxs" "Sources/Compiler/Parser.vxs"
        , matchesGlob "Sources/Generated/**" "Sources/Generated/Nested/File.vxs"
        , matchesGlob "Sources/?ain.vxs" "Sources/Main.vxs"
        , matchesGlob "Sources/*.vxs" "Sources/Main.vxs"
        , not (matchesGlob "Sources/*.vxs" "Sources/Nested/Main.vxs")
        , if os == "mingw32"
            then matchesGlob "Sources/Main.vxs" "Sources/main.vxs"
            else not (matchesGlob "Sources/Main.vxs" "Sources/main.vxs")
        , not (matchesGlob "Tests/**" "Sources/Tests/Main.vxs")
        ]

discoveryOrder :: IO Bool
discoveryOrder = withTemporaryTree $ \root -> do
    writeSource root "Sources/Zeta.vxs" minimalSource
    writeSource root "Sources/Alpha.vxs" minimalSource
    writeSource root "Sources/Nested/Middle.vxs" minimalSource
    result <- loadSourceSet (SourceSetRequest root ["Sources"] [])
    pure $ case result of
        Right sources ->
            map loadedSourceRelativePath sources
                == ["Sources/Alpha.vxs", "Sources/Nested/Middle.vxs", "Sources/Zeta.vxs"]
        Left _ -> False

exclusionPolicy :: IO Bool
exclusionPolicy = withTemporaryTree $ \root -> do
    writeSource root "Sources/Main.vxs" minimalSource
    writeSource root "Sources/Generated/Bindings.vxs" "this would not parse"
    writeSource root "Sources/Skip.vxs" "this would not parse"
    writeSource root "Sources/Nested/Keep.vxs" minimalSource
    result <-
        loadSourceSet
            ( SourceSetRequest
                root
                ["Sources"]
                ["Sources/Generated/**", "Sources/Skip.vxs"]
            )
    pure $ case result of
        Right sources ->
            map loadedSourceRelativePath sources
                == ["Sources/Main.vxs", "Sources/Nested/Keep.vxs"]
        Left _ -> False

overlappingRoots :: IO Bool
overlappingRoots = withTemporaryTree $ \root -> do
    writeSource root "Sources/Main.vxs" minimalSource
    writeSource root "Sources/Nested/Helper.vxs" minimalSource
    result <- loadSourceSet (SourceSetRequest root ["Sources", "Sources/Nested"] [])
    pure $ case result of
        Right sources ->
            map loadedSourceRelativePath sources
                == ["Sources/Main.vxs", "Sources/Nested/Helper.vxs"]
        Left _ -> False

caseSensitiveExtension :: IO Bool
caseSensitiveExtension = withTemporaryTree $ \root -> do
    writeSource root "Sources/Main.VXS" minimalSource
    result <- loadSourceSet (SourceSetRequest root ["Sources"] [])
    pure (hasDiagnostic "VXS0007" result)

invalidUtf8 :: IO Bool
invalidUtf8 = withTemporaryTree $ \root -> do
    let path = root </> "Sources" </> "Invalid.vxs"
    createDirectoryIfMissing True (root </> "Sources")
    ByteString.writeFile path (ByteString.pack [0x63, 0x6c, 0x61, 0x73, 0x73, 0x20, 0xc3, 0x28])
    result <- loadSourceSet (SourceSetRequest root ["Sources"] [])
    pure (hasDiagnostic "VXS0020" result)

explicitFileDecoder :: IO Bool
explicitFileDecoder = withTemporaryTree $ \root -> do
    let path = root </> "Türkçe.vxs"
    ByteString.writeFile path (ByteString.pack [0xc3, 0x28])
    result <- loadSourceFile path
    pure (hasDiagnostic "VXS0020" result)

explicitFileExtension :: IO Bool
explicitFileExtension = withTemporaryTree $ \root -> do
    let path = root </> "Main.VXS"
    writeFile path minimalSource
    result <- loadSourceFile path
    pure (hasDiagnostic "VXS0003" result)

escapingRoot :: IO Bool
escapingRoot = withTemporaryTree $ \root -> do
    let project = root </> "Project"
        outside = root </> "Outside"
    createDirectory project
    createDirectory outside
    writeSource outside "Escape.vxs" minimalSource
    result <- loadSourceSet (SourceSetRequest project ["../Outside"] [])
    pure (hasDiagnostic "VXS0009" result)

emptySourceSet :: IO Bool
emptySourceSet = withTemporaryTree $ \root -> do
    createDirectoryIfMissing True (root </> "Sources")
    writeFile (root </> "Sources" </> "README.txt") "not source"
    result <- loadSourceSet (SourceSetRequest root ["Sources"] [])
    pure (hasDiagnostic "VXS0007" result)

namespaceMerge :: IO Bool
namespaceMerge = pure $ case compileProjectToCorePrep applicationEntry inputs of
    Right project ->
        length (projectNamespaces project) == 1
            && artifactSourceFiles (projectEntryNamespace project) == ["helper.vxs", "main.vxs"]
            && length (coreModuleFunctions (projectEntryCore project)) == 2
    Left _ -> False
    where
        inputs =
            [ CompilerInput "main.vxs" applicationMain
            , CompilerInput "helper.vxs" applicationHelper
            ]

entrySelection :: IO Bool
entrySelection = pure $ case compileProjectToCorePrep applicationEntry inputs of
    Right project ->
        length (projectNamespaces project) == 2
            && artifactNamespace (projectEntryNamespace project) == Just applicationNamespace
            && coreModuleName (projectEntryCore project) == applicationNamespace
    Left _ -> False
    where
        inputs =
            [ CompilerInput "library.vxs" "namespace Library; class Utility { public static int Value() { return 7; } }"
            , CompilerInput "main.vxs" applicationMain
            ]

validatesEveryNamespace :: IO Bool
validatesEveryNamespace =
    pure $
        hasDiagnostic
            "VXN0001"
            ( compileProjectToCorePrep
                applicationEntry
                [ CompilerInput "main.vxs" applicationMain
                , CompilerInput "broken.vxs" "namespace Broken; class Utility { int Value() { return missing; } }"
                ]
            )

accumulatesFileDiagnostics :: IO Bool
accumulatesFileDiagnostics = pure $ case compileProjectToCorePrep applicationEntry inputs of
    Left diagnostics ->
        length diagnostics == 2
            && map (fmap sourceFile . diagnosticSpan) diagnostics
                == [Just "first.vxs", Just "second.vxs"]
    Right _ -> False
    where
        inputs =
            [ CompilerInput "first.vxs" "namespace First; class One { void Run() { @; } }"
            , CompilerInput "second.vxs" "namespace Second; class Two { void Run() { #; } }"
            ]

duplicateAcrossFiles :: IO Bool
duplicateAcrossFiles =
    pure $
        hasDiagnostic
            "VXR0001"
            ( compileProjectToCorePrep
                applicationEntry
                [ CompilerInput "first.vxs" applicationMain
                , CompilerInput "second.vxs" "namespace Application; class Program { public static void Main() { return; } }"
                ]
            )

missingEntryNamespace :: IO Bool
missingEntryNamespace =
    pure $
        hasDiagnostic
            "VXE0011"
            ( compileProjectToCorePrep
                (QualifiedName [Identifier "Missing", Identifier "Program"])
                [CompilerInput "main.vxs" applicationMain]
            )

unqualifiedEntry :: IO Bool
unqualifiedEntry =
    pure $
        hasDiagnostic
            "VXE0001"
            ( compileProjectToCorePrep
                (QualifiedName [Identifier "Program"])
                [CompilerInput "main.vxs" applicationMain]
            )

mergedEntryContract :: IO Bool
mergedEntryContract =
    pure $
        hasDiagnostic
            "VXE0008"
            ( compileProjectToCorePrep
                applicationEntry
                [ CompilerInput "helper.vxs" applicationHelper
                , CompilerInput
                    "main.vxs"
                    "namespace Application; class Program { public static int Main() { return 0; } }"
                ]
            )

applicationNamespace :: QualifiedName
applicationNamespace = QualifiedName [Identifier "Application"]

applicationEntry :: QualifiedName
applicationEntry = QualifiedName [Identifier "Application", Identifier "Program"]

applicationMain :: String
applicationMain =
    unlines
        [ "namespace Application;"
        , "class Program {"
        , "  public static void Main() {"
        , "    return;"
        , "  }"
        , "}"
        ]

applicationHelper :: String
applicationHelper =
    unlines
        [ "namespace Application;"
        , "class Helper {"
        , "  public static int Value() {"
        , "    return 42;"
        , "  }"
        , "}"
        ]

minimalSource :: String
minimalSource = "namespace Example; class Value { public static int Get() { return 1; } }"

hasDiagnostic :: String -> Either [Diagnostic] value -> Bool
hasDiagnostic code result = case result of
    Left diagnostics -> any ((== code) . diagnosticCode) diagnostics
    Right _ -> False

withTemporaryTree :: (FilePath -> IO Bool) -> IO Bool
withTemporaryTree action = do
    temporary <- getTemporaryDirectory
    (path, handle) <- openTempFile temporary "visual-xsharp-source-set-"
    hClose handle
    removeFile path
    createDirectory path
    action path `finally` removeDirectoryRecursive path

writeSource :: FilePath -> FilePath -> String -> IO ()
writeSource root relative content = do
    let path = root </> relative
    createDirectoryIfMissing True (takeDirectory path)
    writeFile path content
