-- SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
-- SPDX-License-Identifier: MPL-2.0

module Visual.XSharp.Core.Artifact
    ( CoreArtifactError (..)
    , readCoreArtifact
    , readCoreArtifactWith
    , writeCoreArtifact
    , writeCoreArtifactWith
    ) where

import Control.Exception (IOException, try)
import Data.ByteString qualified as ByteString
import System.FilePath (takeExtension)
import Visual.XSharp.Core
import Visual.XSharp.Core.Verifier
import Visual.XSharp.Core.Wire
import Visual.XSharp.Diagnostic (diagnosticCode, diagnosticMessage)

data CoreArtifactError
    = InvalidCoreArtifactPath FilePath
    | CoreArtifactWireError CoreWireError
    | CoreArtifactVerificationError [String]
    | CoreArtifactIOError FilePath String
    deriving (Eq, Ord, Read, Show)

readCoreArtifact :: FilePath -> IO (Either CoreArtifactError CoreModule)
readCoreArtifact = readCoreArtifactWith defaultCoreWireLimits

readCoreArtifactWith :: CoreWireLimits -> FilePath -> IO (Either CoreArtifactError CoreModule)
readCoreArtifactWith limits path
    | takeExtension path /= ".core" = pure (Left (InvalidCoreArtifactPath path))
    | otherwise = do
        loaded <- try (ByteString.readFile path) :: IO (Either IOException ByteString.ByteString)
        pure $ case loaded of
            Left issue -> Left (CoreArtifactIOError path (show issue))
            Right bytes -> case decodeCore limits (ByteString.unpack bytes) of
                Left issue -> Left (CoreArtifactWireError issue)
                Right moduleValue -> verify moduleValue

writeCoreArtifact :: FilePath -> CoreModule -> IO (Either CoreArtifactError ())
writeCoreArtifact = writeCoreArtifactWith defaultCoreWireLimits

writeCoreArtifactWith :: CoreWireLimits -> FilePath -> CoreModule -> IO (Either CoreArtifactError ())
writeCoreArtifactWith limits path moduleValue
    | takeExtension path /= ".core" = pure (Left (InvalidCoreArtifactPath path))
    | otherwise = case verify moduleValue >>= encode of
        Left issue -> pure (Left issue)
        Right bytes -> do
            written <- try (ByteString.writeFile path (ByteString.pack bytes)) :: IO (Either IOException ())
            pure $ either (Left . CoreArtifactIOError path . show) (const (Right ())) written
    where
        encode value = either (Left . CoreArtifactWireError) Right (encodeCore limits value)

verify :: CoreModule -> Either CoreArtifactError CoreModule
verify moduleValue = case verifyCore moduleValue of
    Left problems -> Left (CoreArtifactVerificationError (map diagnosticText problems))
    Right verified -> Right verified
    where
        diagnosticText diagnostic = diagnosticCode diagnostic ++ ": " ++ diagnosticMessage diagnostic
