-- SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
-- SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.1

module Visual.XSharp.Diagnostic.SideChannel
    ( SideChannelError (..)
    , writeDiagnosticFile
    ) where

import Control.Exception (IOException, try)
import Data.ByteString qualified as ByteString
import Visual.XSharp.Diagnostic
import Visual.XSharp.Diagnostic.Protocol

data SideChannelError
    = SideChannelModelError DiagnosticProtocolError
    | SideChannelEncodingError DiagnosticProtocolError
    | SideChannelWriteError FilePath String
    deriving (Eq, Ord, Read, Show)

-- The caller selects a fresh path for each compiler process. This function
-- converts the frontend's one-based source model, encodes the complete VXDG
-- document in memory, and performs only one write after all validation passes.
-- It returns failures instead of printing because the native driver owns the
-- public presentation policy.
writeDiagnosticFile :: FilePath -> [Diagnostic] -> IO (Either SideChannelError ())
writeDiagnosticFile path diagnostics
    | null path = pure (Left (SideChannelWriteError path "diagnostic path is empty"))
    | otherwise = case diagnosticDocument diagnostics of
        Left issue -> pure (Left (SideChannelModelError issue))
        Right document -> case encodeDiagnosticDocument defaultDiagnosticProtocolLimits document of
            Left issue -> pure (Left (SideChannelEncodingError issue))
            Right bytes -> do
                result <- try (ByteString.writeFile path bytes) :: IO (Either IOException ())
                pure $ case result of
                    Left issue -> Left (SideChannelWriteError path (show issue))
                    Right () -> Right ()
