-- SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
-- SPDX-License-Identifier: MPL-2.0
module Visual.XSharp.Core.CorePrep.Artifact
    ( ArtifactError (..), readCorePrepArtifact, readCorePrepArtifactWith
    , writeCorePrepArtifact, writeCorePrepArtifactWith ) where

import Control.Exception (IOException, try)
import qualified Data.ByteString as ByteString
import Data.Word (Word8)
import System.FilePath (takeExtension)
import Visual.XSharp.Core.CorePrep
import Visual.XSharp.Core.CorePrep.Wire

data ArtifactError
    = InvalidArtifactPath FilePath
    | ArtifactWireError WireError
    | ArtifactIOError FilePath String
    deriving (Eq, Ord, Read, Show)

readCorePrepArtifact :: FilePath -> IO (Either ArtifactError CorePrepModule)
readCorePrepArtifact = readCorePrepArtifactWith defaultWireLimits

readCorePrepArtifactWith :: WireLimits -> FilePath -> IO (Either ArtifactError CorePrepModule)
readCorePrepArtifactWith limits path
    | takeExtension path /= ".core" = pure (Left (InvalidArtifactPath path))
    | otherwise = do
        loaded <- try (ByteString.readFile path) :: IO (Either IOException ByteString.ByteString)
        pure $ case loaded of
            Left problem -> Left (ArtifactIOError path (show problem))
            Right bytes -> case decodeCorePrepWith limits (ByteString.unpack bytes) of
                Left problem -> Left (ArtifactWireError problem)
                Right moduleValue -> Right moduleValue

writeCorePrepArtifact :: FilePath -> CorePrepModule -> IO (Either ArtifactError ())
writeCorePrepArtifact = writeCorePrepArtifactWith defaultWireLimits

writeCorePrepArtifactWith :: WireLimits -> FilePath -> CorePrepModule -> IO (Either ArtifactError ())
writeCorePrepArtifactWith limits path moduleValue
    | takeExtension path /= ".core" = pure (Left (InvalidArtifactPath path))
    | otherwise = case encodeCorePrepWith limits moduleValue of
        Left problem -> pure (Left (ArtifactWireError problem))
        Right bytes -> writeBytes path bytes

writeBytes :: FilePath -> [Word8] -> IO (Either ArtifactError ())
writeBytes path bytes = do
    written <- try (ByteString.writeFile path (ByteString.pack bytes)) :: IO (Either IOException ())
    pure $ case written of
        Left problem -> Left (ArtifactIOError path (show problem))
        Right () -> Right ()
