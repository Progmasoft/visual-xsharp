-- SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
-- SPDX-License-Identifier: MPL-2.0

{- | Composition of the target-independent Visual X# frontend stages.

A physical source file is a compilation unit, not a module. Namespace is the
semantic grouping key and does not need to match the directory layout. The
project compiler therefore parses every file independently, merges units by
namespace, runs each namespace through the complete Haskell pipeline, and
selects the configured namespace/class entry only after type checking.
-}
module Visual.XSharp.Compiler
    ( CompilerInput (..)
    , FrontendArtifacts (..)
    , NamespaceArtifacts (..)
    , ProjectFrontendArtifacts (..)
    , compileToCorePrep
    , compileEntryToCorePrep
    , compileProjectToCorePrep
    , projectEntryCore
    , validateEntryPoint
    ) where

import Data.List (groupBy, sortOn)
import Data.Maybe (mapMaybe)
import Visual.XSharp.AST
import Visual.XSharp.Core
import Visual.XSharp.Core.CorePrep
import Visual.XSharp.Core.CorePrep.Verifier
import Visual.XSharp.Core.Optimizer
import Visual.XSharp.Core.Verifier
import Visual.XSharp.Desugarer
import Visual.XSharp.Diagnostic
import Visual.XSharp.Lexer
import Visual.XSharp.Parser
import Visual.XSharp.Resolver.NameResolution
import Visual.XSharp.Resolver.Renamer
import Visual.XSharp.TypeChecker

data CompilerInput = CompilerInput
    { compilerSourceFile :: FilePath
    , compilerSourceText :: String
    }
    deriving (Eq, Ord, Read, Show)

{- | Artifacts for one semantic namespace after all of its physical source
units have been merged. Keeping every stage visible makes stage ownership and
regression tests explicit; CorePrep remains internal at the process boundary.
-}
data FrontendArtifacts = FrontendArtifacts
    { artifactParsedAST :: ParsedAST
    , artifactRenamedAST :: RenamedAST
    , artifactResolvedAST :: ResolvedAST
    , artifactTypedAST :: TypedAST
    , artifactCore :: CoreModule
    , artifactOptimizedCore :: CoreModule
    , artifactCorePrep :: CorePrepModule
    }
    deriving (Eq, Ord, Read, Show)

data NamespaceArtifacts = NamespaceArtifacts
    { artifactNamespace :: Maybe QualifiedName
    , artifactSourceFiles :: [FilePath]
    , artifactFrontend :: FrontendArtifacts
    }
    deriving (Eq, Ord, Read, Show)

{- | A project may contain several namespaces, but the current private Core wire
carries one module at a time. All namespaces are still parsed, renamed,
resolved, typed, lowered, optimized, and verified. The selected entry
namespace is the module handed to the native CorePrep/Xpp/Xmm boundary.
-}
data ProjectFrontendArtifacts = ProjectFrontendArtifacts
    { projectEntryName :: QualifiedName
    , projectNamespaces :: [NamespaceArtifacts]
    , projectEntryNamespace :: NamespaceArtifacts
    }
    deriving (Eq, Ord, Read, Show)

compileToCorePrep :: CompilerInput -> Either [Diagnostic] FrontendArtifacts
compileToCorePrep input = parseCompilerInput input >>= compileParsedToCorePrep

compileEntryToCorePrep :: QualifiedName -> CompilerInput -> Either [Diagnostic] FrontendArtifacts
compileEntryToCorePrep entry input = do
    artifacts <- compileToCorePrep input
    validateEntryPoint entry (artifactTypedAST artifacts)
    pure artifacts

{- | Compile a complete discovered source set. Diagnostics from independent
files and namespaces are accumulated in deterministic input order instead of
stopping at the first bad file, which makes project output useful without
weakening any stage boundary.
-}
compileProjectToCorePrep :: QualifiedName -> [CompilerInput] -> Either [Diagnostic] ProjectFrontendArtifacts
compileProjectToCorePrep entry inputs = do
    namespaceName <- entryNamespaceName entry
    parsedUnits <- collectResults (map parseUnit inputs)
    if null parsedUnits
        then Left [compilerProblem "VXE0010" Nothing "project source set is empty"]
        else do
            compiled <- collectResults (map compileNamespace (groupParsedUnits parsedUnits))
            selected <- selectNamespace namespaceName compiled
            validateEntryPoint entry (artifactTypedAST (artifactFrontend selected))
            pure (ProjectFrontendArtifacts entry compiled selected)
    where
        parseUnit input = do
            parsed <- parseCompilerInput input
            pure (compilerSourceFile input, parsed)

projectEntryCore :: ProjectFrontendArtifacts -> CoreModule
projectEntryCore = artifactOptimizedCore . artifactFrontend . projectEntryNamespace

parseCompilerInput :: CompilerInput -> Either [Diagnostic] ParsedAST
parseCompilerInput input = do
    tokens <- runLexer defaultLexer (LexerInput (compilerSourceFile input) (compilerSourceText input))
    runParser defaultParser (ParserInput (compilerSourceFile input) tokens)

compileParsedToCorePrep :: ParsedAST -> Either [Diagnostic] FrontendArtifacts
compileParsedToCorePrep parsed = do
    renamed <- runRenamer defaultRenamer parsed
    resolved <- runNameResolution defaultNameResolution renamed
    typed <- runTypeChecker defaultTypeChecker resolved
    core <- runDesugarer defaultDesugarer typed >>= verifyCore
    optimized <- runCoreOptimizer defaultCoreOptimizer core >>= verifyCore
    prepared <- prepareCore optimized >>= verifyCorePrep
    pure (FrontendArtifacts parsed renamed resolved typed core optimized prepared)

data ParsedNamespace = ParsedNamespace
    { parsedNamespaceName :: Maybe QualifiedName
    , parsedNamespaceFiles :: [FilePath]
    , parsedNamespaceDeclarations :: [Declaration Identifier ()]
    }

groupParsedUnits :: [(FilePath, ParsedAST)] -> [ParsedNamespace]
groupParsedUnits units = map mergeGroup grouped
    where
        keyed = sortOn namespaceSortKey [(syntaxNamespace tree, file, syntaxDeclarations tree) | (file, ParsedAST tree) <- units]
        grouped = groupBy sameNamespace keyed
        sameNamespace (left, _, _) (right, _, _) = left == right
        mergeGroup [] = ParsedNamespace Nothing [] []
        mergeGroup values@((namespaceName, _, _) : _) =
            ParsedNamespace
                namespaceName
                [file | (_, file, _) <- values]
                (concat [declarations | (_, _, declarations) <- values])

namespaceSortKey :: (Maybe QualifiedName, FilePath, declarations) -> (Maybe QualifiedName, FilePath)
namespaceSortKey (namespaceName, file, _) = (namespaceName, file)

compileNamespace :: ParsedNamespace -> Either [Diagnostic] NamespaceArtifacts
compileNamespace parsed = do
    let syntaxTree = SyntaxTree (parsedNamespaceName parsed) (parsedNamespaceDeclarations parsed)
    artifacts <- compileParsedToCorePrep (ParsedAST syntaxTree)
    pure
        ( NamespaceArtifacts
            (parsedNamespaceName parsed)
            (parsedNamespaceFiles parsed)
            artifacts
        )

selectNamespace :: QualifiedName -> [NamespaceArtifacts] -> Either [Diagnostic] NamespaceArtifacts
selectNamespace namespaceName artifacts =
    case [candidate | candidate <- artifacts, artifactNamespace candidate == Just namespaceName] of
        [selected] -> Right selected
        [] -> Left [compilerProblem "VXE0011" Nothing ("entry namespace was not found: " ++ renderQualifiedName namespaceName)]
        _ ->
            Left
                [ compilerProblem "VXE0012" Nothing ("entry namespace was produced more than once: " ++ renderQualifiedName namespaceName)
                ]

validateEntryPoint :: QualifiedName -> TypedAST -> Either [Diagnostic] ()
validateEntryPoint entry (TypedAST (SyntaxTree namespace declarations)) = do
    expectedNamespace <- entryNamespaceName entry
    className <- entryClassName entry
    if namespace /= Just expectedNamespace
        then Left [entryProblem "VXE0002" Nothing "entry namespace does not match the selected source namespace"]
        else case matchingTypes className declarations of
            [] -> Left [entryProblem "VXE0003" Nothing ("entry class was not found: " ++ identifierText className)]
            [declaration] -> validateEntryMethod declaration
            _ -> Left [entryProblem "VXE0013" Nothing ("entry class is ambiguous: " ++ identifierText className)]

matchingTypes :: Identifier -> [Declaration ResolvedName Type] -> [Declaration ResolvedName Type]
matchingTypes className declarations =
    [ declaration
    | declaration@TypeDeclaration {} <- declarations
    , resolvedSpelling (declarationName declaration) == className
    ]

validateEntryMethod :: Declaration ResolvedName Type -> Either [Diagnostic] ()
validateEntryMethod TypeDeclaration {typeMembers = members} =
    case matchingMethods of
        [] -> Left [entryProblem "VXE0004" Nothing "entry class must declare Main"]
        [method]
            | declarationAccess method /= PublicAccess -> failure method "VXE0005" "entry Main must be public"
            | not (declarationIsStatic method) -> failure method "VXE0006" "entry Main must be static"
            | not (null (declarationParameters method)) -> failure method "VXE0007" "entry Main must not declare parameters"
            | declarationReturnSyntax method /= ExplicitType (Identifier "unit") ->
                failure method "VXE0008" "entry Main must return void"
            | otherwise -> Right ()
        _ -> Left [entryProblem "VXE0014" (declarationSpan <$> firstMethod) "entry class must declare exactly one Main method"]
    where
        matchingMethods =
            [ member
            | member@FunctionDeclaration {} <- members
            , identifierText (resolvedSpelling (declarationName member)) == "Main"
            ]
        firstMethod = case matchingMethods of
            [] -> Nothing
            method : _ -> Just method
        failure method code message = Left [entryProblem code (Just (declarationSpan method)) message]
validateEntryMethod _ = Left [entryProblem "VXE0009" Nothing "entry target is not a class"]

entryNamespaceName :: QualifiedName -> Either [Diagnostic] QualifiedName
entryNamespaceName (QualifiedName parts)
    | length parts < 2 = Left [entryProblem "VXE0001" Nothing "entry must name a namespace-qualified class"]
    | otherwise = Right (QualifiedName (init parts))

entryClassName :: QualifiedName -> Either [Diagnostic] Identifier
entryClassName (QualifiedName parts) =
    case reverse parts of
        className : _ : _ -> Right className
        _ -> Left [entryProblem "VXE0001" Nothing "entry must name a namespace-qualified class"]

renderQualifiedName :: QualifiedName -> String
renderQualifiedName (QualifiedName parts) = joinWithDot (map identifierText parts)

joinWithDot :: [String] -> String
joinWithDot [] = ""
joinWithDot [value] = value
joinWithDot (value : remaining) = value ++ "." ++ joinWithDot remaining

collectResults :: [Either [Diagnostic] value] -> Either [Diagnostic] [value]
collectResults values =
    case concat [problems | Left problems <- values] of
        [] -> Right (mapMaybe rightValue values)
        problems -> Left problems
    where
        rightValue (Right value) = Just value
        rightValue (Left _) = Nothing

compilerProblem :: String -> Maybe SourceSpan -> String -> Diagnostic
compilerProblem code spanValue message = Diagnostic TypeCheckerStage Error code spanValue message

entryProblem :: String -> Maybe SourceSpan -> String -> Diagnostic
entryProblem = compilerProblem
