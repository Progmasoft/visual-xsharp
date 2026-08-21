-- SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
-- SPDX-License-Identifier: MPL-2.0

module Visual.XSharp.Diagnostic
    ( DiagnosticStage (..)
    , DiagnosticSeverity (..)
    , Diagnostic (..)
    ) where

import Visual.XSharp.AST (SourceSpan)

data DiagnosticStage
    = SourceLoaderStage
    | LexerStage
    | ParserStage
    | RenamerStage
    | NameResolutionStage
    | TypeCheckerStage
    | DesugarerStage
    | CoreStage
    | CoreOptimizerStage
    | CorePrepStage
    | XppLoweringStage
    | XppOptimizerStage
    | XmmLoweringStage
    | XmmOptimizerStage
    | LlvmBackendStage
    deriving (Bounded, Enum, Eq, Ord, Read, Show)

data DiagnosticSeverity = Error | Warning
    deriving (Bounded, Enum, Eq, Ord, Read, Show)

data Diagnostic = Diagnostic
    { diagnosticStage :: DiagnosticStage
    , diagnosticSeverity :: DiagnosticSeverity
    , diagnosticCode :: String
    , diagnosticSpan :: Maybe SourceSpan
    , diagnosticMessage :: String
    }
    deriving (Eq, Ord, Read, Show)
