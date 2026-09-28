-- SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
-- SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.1

{- | Public facade combining CorePrep wire encoding, decoding, and schema types.
The binary format is an internal compiler boundary and is not a user-facing
emission format.
-}
module Visual.XSharp.Core.CorePrep.Wire
    ( encodeCorePrep
    , encodeCorePrepWith
    , decodeCorePrep
    , decodeCorePrepWith
    , WireVersion (..)
    , currentWireVersion
    , WireLimits (..)
    , defaultWireLimits
    , WireErrorKind (..)
    , WireError (..)
    ) where

import Visual.XSharp.Core.CorePrep.Wire.Decode
import Visual.XSharp.Core.CorePrep.Wire.Encode
import Visual.XSharp.Core.CorePrep.Wire.Format
