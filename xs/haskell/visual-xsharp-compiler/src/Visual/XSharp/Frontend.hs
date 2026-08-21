-- SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
-- SPDX-License-Identifier: MPL-2.0

{- | Reusable, target-independent frontend analysis boundaries.

Compiler-adjacent tools must observe exactly the same tokens, parsed tree, and
semantic names as a build. Keeping these boundaries in the compiler package
prevents an analyzer, formatter, or linter from growing a second language
implementation merely to stop before Core lowering.
-}
module Visual.XSharp.Frontend
    ( CompilerInput (..)
    , SyntaxArtifacts (..)
    , SemanticArtifacts (..)
    , analyzeSyntax
    , analyzeSemantics
    , analyzeParsedSemantics
    ) where

import Visual.XSharp.AST
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

data SyntaxArtifacts = SyntaxArtifacts
    { syntaxTokens :: [Token]
    , syntaxParsedAST :: ParsedAST
    }
    deriving (Eq, Ord, Read, Show)

data SemanticArtifacts = SemanticArtifacts
    { semanticParsedAST :: ParsedAST
    , semanticRenamedAST :: RenamedAST
    , semanticResolvedAST :: ResolvedAST
    , semanticTypedAST :: TypedAST
    }
    deriving (Eq, Ord, Read, Show)

analyzeSyntax :: CompilerInput -> Either [Diagnostic] SyntaxArtifacts
analyzeSyntax input = do
    tokens <- runLexer defaultLexer (LexerInput (compilerSourceFile input) (compilerSourceText input))
    parsed <- runParser defaultParser (ParserInput (compilerSourceFile input) tokens)
    pure (SyntaxArtifacts tokens parsed)

analyzeSemantics :: CompilerInput -> Either [Diagnostic] SemanticArtifacts
analyzeSemantics input = syntaxParsedAST <$> analyzeSyntax input >>= analyzeParsedSemantics

analyzeParsedSemantics :: ParsedAST -> Either [Diagnostic] SemanticArtifacts
analyzeParsedSemantics parsed = do
    renamed <- runRenamer defaultRenamer parsed
    resolved <- runNameResolution defaultNameResolution renamed
    typed <- runTypeChecker defaultTypeChecker resolved
    pure (SemanticArtifacts parsed renamed resolved typed)
