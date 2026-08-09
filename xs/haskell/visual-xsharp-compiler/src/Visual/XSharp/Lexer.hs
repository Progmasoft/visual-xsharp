-- SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
-- SPDX-License-Identifier: MPL-2.0

module Visual.XSharp.Lexer
    ( LexerInput (..)
    , Lexer (..)
    , runLexer
    ) where

import Visual.XSharp.Diagnostic (Diagnostic)
import Visual.XSharp.Parser (Token)

data LexerInput = LexerInput
    { lexerSourceFile :: FilePath
    , lexerSourceText :: String
    }
    deriving (Eq, Ord, Read, Show)

-- | Concrete lexical grammars satisfy this boundary and produce parser tokens.
newtype Lexer = Lexer
    { lexSource :: LexerInput -> Either [Diagnostic] [Token]
    }

runLexer :: Lexer -> LexerInput -> Either [Diagnostic] [Token]
runLexer = lexSource
