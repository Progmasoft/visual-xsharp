-- SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
-- SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.1

-- | Structured compiler diagnostics shared by frontend stages.
module Visual.XSharp.Diagnostic
    ( DiagnosticStage (..)
    , DiagnosticSeverity (..)
    , Diagnostic (..)
    ) where

import Visual.XSharp.AST (SourceSpan)

-- | Pipeline stage that produced a diagnostic.
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

-- | User-facing severity currently emitted by the Haskell frontend.
data DiagnosticSeverity = Error | Warning
    deriving (Bounded, Enum, Eq, Ord, Read, Show)

-- | Stable diagnostic code, message, source location, and severity.
data Diagnostic = Diagnostic
    { diagnosticStage :: DiagnosticStage
    , diagnosticSeverity :: DiagnosticSeverity
    , diagnosticCode :: String
    , diagnosticSpan :: Maybe SourceSpan
    , diagnosticMessage :: String
    }
    deriving (Eq, Ord, Read, Show)
