-- SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
-- SPDX-License-Identifier: MPL-2.0
module Visual.XSharp.Compiler (CompilerInput (..), FrontendArtifacts (..), compileToCorePrep, compileEntryToCorePrep, validateEntryPoint) where

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

data CompilerInput = CompilerInput {compilerSourceFile :: FilePath, compilerSourceText :: String}
    deriving (Eq, Ord, Read, Show)
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

compileToCorePrep :: CompilerInput -> Either [Diagnostic] FrontendArtifacts
compileToCorePrep input = do
    tokens <- runLexer defaultLexer (LexerInput (compilerSourceFile input) (compilerSourceText input))
    parsed <- runParser defaultParser (ParserInput (compilerSourceFile input) tokens)
    renamed <- runRenamer defaultRenamer parsed
    resolved <- runNameResolution defaultNameResolution renamed
    typed <- runTypeChecker defaultTypeChecker resolved
    core <- runDesugarer defaultDesugarer typed >>= verifyCore
    optimized <- runCoreOptimizer defaultCoreOptimizer core >>= verifyCore
    prepared <- prepareCore optimized >>= verifyCorePrep
    pure (FrontendArtifacts parsed renamed resolved typed core optimized prepared)

compileEntryToCorePrep :: QualifiedName -> CompilerInput -> Either [Diagnostic] FrontendArtifacts
compileEntryToCorePrep entry input = do
    artifacts <- compileToCorePrep input
    validateEntryPoint entry (artifactTypedAST artifacts)
    pure artifacts

validateEntryPoint :: QualifiedName -> TypedAST -> Either [Diagnostic] ()
validateEntryPoint (QualifiedName []) _ = Left [entryProblem "VXE0001" Nothing "entry must name a namespace-qualified class"]
validateEntryPoint (QualifiedName parts) (TypedAST (SyntaxTree namespace declarations)) =
    let className = last parts
        expectedNamespace = if length parts == 1 then Nothing else Just (QualifiedName (init parts))
        matching =
            [ declaration
            | declaration@TypeDeclaration {} <- declarations
            , resolvedSpelling (declarationName declaration) == className
            ]
     in if namespace /= expectedNamespace
            then Left [entryProblem "VXE0002" Nothing "entry namespace does not match the source namespace"]
            else case matching of
                [] -> Left [entryProblem "VXE0003" Nothing "entry class was not found"]
                declaration : _ -> validateEntryMethod declaration

validateEntryMethod :: Declaration ResolvedName Type -> Either [Diagnostic] ()
validateEntryMethod TypeDeclaration {typeMembers = members} =
    case [ member | member@FunctionDeclaration {} <- members, identifierText (resolvedSpelling (declarationName member)) == "Main"
         ] of
        [] -> Left [entryProblem "VXE0004" Nothing "entry class must declare Main"]
        method : _
            | declarationAccess method /= PublicAccess -> failure method "VXE0005" "entry Main must be public"
            | not (declarationIsStatic method) -> failure method "VXE0006" "entry Main must be static"
            | not (null (declarationParameters method)) -> failure method "VXE0007" "entry Main must not declare parameters"
            | declarationReturnSyntax method /= ExplicitType (Identifier "unit") ->
                failure method "VXE0008" "entry Main must return void"
            | otherwise -> Right ()
    where
        failure method code message = Left [entryProblem code (Just (declarationSpan method)) message]
validateEntryMethod _ = Left [entryProblem "VXE0009" Nothing "entry target is not a class"]

entryProblem :: String -> Maybe SourceSpan -> String -> Diagnostic
entryProblem code spanValue message = Diagnostic TypeCheckerStage Error code spanValue message
