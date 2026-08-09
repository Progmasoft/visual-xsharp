-- SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
-- SPDX-License-Identifier: MPL-2.0

module Visual.XSharp.Parser
    ( TokenKind (..)
    , Token (..)
    , ParserInput (..)
    , Parser (..)
    , runParser
    ) where

import Visual.XSharp.AST (ParsedAST, SourceSpan)
import Visual.XSharp.Diagnostic (Diagnostic)

data TokenKind
    = IdentifierToken
    | KeywordToken
    | SymbolToken
    | LiteralToken
    | EndOfFileToken
    deriving (Bounded, Enum, Eq, Ord, Read, Show)

data Token = Token
    { tokenKind :: TokenKind
    , tokenText :: String
    , tokenSpan :: SourceSpan
    }
    deriving (Eq, Ord, Read, Show)

data ParserInput = ParserInput
    { parserSourceFile :: FilePath
    , parserTokens :: [Token]
    }
    deriving (Eq, Ord, Read, Show)

-- | Concrete grammar implementations satisfy this stable parser boundary.
newtype Parser = Parser
    { parseTokens :: ParserInput -> Either [Diagnostic] ParsedAST
    }

runParser :: Parser -> ParserInput -> Either [Diagnostic] ParsedAST
runParser = parseTokens
