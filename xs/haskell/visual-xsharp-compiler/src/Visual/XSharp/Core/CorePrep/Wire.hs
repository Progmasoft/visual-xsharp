-- SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
-- SPDX-License-Identifier: MPL-2.0
module Visual.XSharp.Core.CorePrep.Wire
    ( encodeCorePrep, encodeCorePrepWith, decodeCorePrep, decodeCorePrepWith
    , WireVersion (..), currentWireVersion, WireLimits (..), defaultWireLimits
    , WireErrorKind (..), WireError (..)
    ) where

import Visual.XSharp.Core.CorePrep.Wire.Decode
import Visual.XSharp.Core.CorePrep.Wire.Encode
import Visual.XSharp.Core.CorePrep.Wire.Format
