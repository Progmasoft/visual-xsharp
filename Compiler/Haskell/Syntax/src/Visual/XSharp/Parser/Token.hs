-- SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
-- SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.1

module Visual.XSharp.Parser.Token (TokenKind (..), Token (..)) where

import Visual.XSharp.AST (SourceSpan)

-- Shared by the lexer and parser machinery without making the token cursor
-- depend on the concrete declaration grammar. Parser reexports this API.
data TokenKind
    = IdentifierToken
    | KeywordToken
    | SymbolToken
    | IntegerToken
    | FloatingToken
    | CharacterToken
    | StringToken
    | EndOfFileToken
    deriving (Bounded, Enum, Eq, Ord, Read, Show)

data Token = Token
    { tokenKind :: TokenKind
    , tokenText :: String
    , tokenSpan :: SourceSpan
    }
    deriving (Eq, Ord, Read, Show)
