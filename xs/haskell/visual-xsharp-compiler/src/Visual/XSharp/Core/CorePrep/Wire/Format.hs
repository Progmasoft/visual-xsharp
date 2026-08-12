-- SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
-- SPDX-License-Identifier: MPL-2.0
module Visual.XSharp.Core.CorePrep.Wire.Format
    ( WireVersion (..), currentWireVersion, wireMagic, WireLimits (..), defaultWireLimits
    , WireErrorKind (..), WireError (..), wireError ) where

import Data.Word (Word8, Word16)

newtype WireVersion = WireVersion { wireVersionNumber :: Word16 }
    deriving (Eq, Ord, Read, Show)

currentWireVersion :: WireVersion
currentWireVersion = WireVersion 1

wireMagic :: [Word8]
wireMagic = map (fromIntegral . fromEnum) "VXCP"

data WireLimits = WireLimits
    { maximumWireBytes :: Int
    , maximumStringCodePoints :: Int
    , maximumFunctions :: Int
    , maximumParametersPerFunction :: Int
    , maximumBlocksPerFunction :: Int
    , maximumInstructionsPerBlock :: Int
    , maximumOperandsPerInstruction :: Int
    , maximumTypeDepth :: Int
    } deriving (Eq, Ord, Read, Show)

defaultWireLimits :: WireLimits
defaultWireLimits = WireLimits
    { maximumWireBytes = 64 * 1024 * 1024
    , maximumStringCodePoints = 1024 * 1024
    , maximumFunctions = 65535
    , maximumParametersPerFunction = 65535
    , maximumBlocksPerFunction = 1048576
    , maximumInstructionsPerBlock = 1048576
    , maximumOperandsPerInstruction = 65535
    , maximumTypeDepth = 128
    }

data WireErrorKind
    = InvalidMagic | UnsupportedVersion | TruncatedInput | TrailingInput
    | InvalidTag | InvalidBoolean | InvalidCodePoint | InvalidCount
    | InvalidSymbol | InvalidInteger | UnsupportedType | LimitExceeded
    deriving (Eq, Ord, Read, Show)

data WireError = WireError
    { wireErrorKind :: WireErrorKind
    , wireErrorOffset :: Int
    , wireErrorContext :: String
    , wireErrorMessage :: String
    } deriving (Eq, Ord, Read, Show)

wireError :: WireErrorKind -> Int -> String -> String -> WireError
wireError = WireError
