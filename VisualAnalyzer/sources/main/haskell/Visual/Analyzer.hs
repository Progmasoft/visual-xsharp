-- SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
-- SPDX-License-Identifier: MPL-2.0

{- | Compiler-backed document analysis for editor protocol hosts.

The analyzer intentionally has no executable. IntelliJ and VS Code hosts will
own process lifecycle and transport; this module owns the language result and
the zero-based positions expected by editor protocols.
-}
module Visual.Analyzer
    ( AnalysisMode (..)
    , AnalysisResult (..)
    , ProtocolPosition (..)
    , ProtocolRange (..)
    , AnalyzerDiagnostic (..)
    , analyzeDocument
    ) where

import Visual.XSharp.AST
import Visual.XSharp.Compiler
import Visual.XSharp.Diagnostic
import Visual.XSharp.Frontend

data AnalysisMode = Syntax | Semantic | Full
    deriving (Bounded, Enum, Eq, Ord, Read, Show)

data AnalysisResult
    = SyntaxResult SyntaxArtifacts
    | SemanticResult SemanticArtifacts
    | FullResult FrontendArtifacts
    deriving (Eq, Ord, Read, Show)

data ProtocolPosition = ProtocolPosition
    { protocolLine :: Int
    , protocolCharacter :: Int
    }
    deriving (Eq, Ord, Read, Show)

data ProtocolRange = ProtocolRange
    { protocolStart :: ProtocolPosition
    , protocolEnd :: ProtocolPosition
    }
    deriving (Eq, Ord, Read, Show)

data AnalyzerDiagnostic = AnalyzerDiagnostic
    { analyzerCode :: String
    , analyzerSeverity :: DiagnosticSeverity
    , analyzerStage :: DiagnosticStage
    , analyzerRange :: Maybe ProtocolRange
    , analyzerMessage :: String
    }
    deriving (Eq, Ord, Read, Show)

analyzeDocument :: AnalysisMode -> CompilerInput -> Either [AnalyzerDiagnostic] AnalysisResult
analyzeDocument mode input = mapLeft (map toAnalyzerDiagnostic) $ case mode of
    Syntax -> SyntaxResult <$> analyzeSyntax input
    Semantic -> SemanticResult <$> analyzeSemantics input
    Full -> FullResult <$> compileToCorePrep input

toAnalyzerDiagnostic :: Diagnostic -> AnalyzerDiagnostic
toAnalyzerDiagnostic problem =
    AnalyzerDiagnostic
        (diagnosticCode problem)
        (diagnosticSeverity problem)
        (diagnosticStage problem)
        (toProtocolRange <$> diagnosticSpan problem)
        (diagnosticMessage problem)

toProtocolRange :: SourceSpan -> ProtocolRange
toProtocolRange spanValue =
    ProtocolRange
        (toProtocolPosition (sourceStart spanValue))
        (toProtocolPosition (sourceEnd spanValue))

toProtocolPosition :: SourcePosition -> ProtocolPosition
toProtocolPosition position =
    ProtocolPosition
        (max 0 (sourceLine position - 1))
        (max 0 (sourceColumn position - 1))

mapLeft :: (left -> other) -> Either left right -> Either other right
mapLeft transform value = case value of
    Left problem -> Left (transform problem)
    Right result -> Right result
