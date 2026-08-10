-- SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
-- SPDX-License-Identifier: MPL-2.0

module Visual.XSharp.Lexer
    ( LexerInput (..), Lexer (..), defaultLexer, runLexer ) where

import Data.Char (isAlpha, isAlphaNum, isDigit, isSpace)
import Visual.XSharp.AST (SourcePosition (..), SourceSpan (..))
import Visual.XSharp.Diagnostic
import Visual.XSharp.Parser (Token (..), TokenKind (..))

data LexerInput = LexerInput { lexerSourceFile :: FilePath, lexerSourceText :: String }
    deriving (Eq, Ord, Read, Show)

newtype Lexer = Lexer { lexSource :: LexerInput -> Either [Diagnostic] [Token] }

runLexer :: Lexer -> LexerInput -> Either [Diagnostic] [Token]
runLexer = lexSource

defaultLexer :: Lexer
defaultLexer = Lexer lexVisualXSharp

lexVisualXSharp :: LexerInput -> Either [Diagnostic] [Token]
lexVisualXSharp (LexerInput file source) = go 1 1 source []
  where
    go line column [] output = Right (reverse (token EndOfFileToken "" line column line column : output))
    go line column input@(character : rest) output
        | character == '\n' = go (line + 1) 1 rest output
        | isSpace character = go line (column + 1) rest output
        | take 2 input == "//" = let (comment, remaining) = break (== '\n') rest
                                  in go line (column + 1 + length comment) remaining output
        | take 2 input == "/*" = skipBlock line column (drop 2 input) output
        | isAlpha character || character == '_' =
            let (tailText, remaining) = span (\c -> isAlphaNum c || c == '_') rest
                text = character : tailText
                kind = if text `elem` keywords then KeywordToken else IdentifierToken
            in go line (column + length text) remaining (token kind text line column line (column + length text) : output)
        | isDigit character =
            let (tailText, remaining) = span (\c -> isDigit c || c == '_') rest
                text = character : tailText
            in go line (column + length text) remaining (token IntegerToken text line column line (column + length text) : output)
        | character == '"' = lexString line column rest output
        | otherwise = case longestSymbol input of
            Just symbol -> go line (column + length symbol) (drop (length symbol) input)
                (token SymbolToken symbol line column line (column + length symbol) : output)
            Nothing -> Left [diagnostic "VXL0001" line column ("unsupported character " ++ show character)]

    skipBlock line column input output = case input of
        [] -> Left [diagnostic "VXL0002" line column "unterminated block comment"]
        ('*':'/':remaining) -> go line (column + 2) remaining output
        ('\n':remaining) -> skipBlock (line + 1) 1 remaining output
        (_:remaining) -> skipBlock line (column + 1) remaining output

    lexString line column input output = collect input [] line (column + 1)
      where
        collect [] _ currentLine currentColumn = Left [diagnostic "VXL0003" currentLine currentColumn "unterminated string literal"]
        collect ('"':remaining) value currentLine currentColumn =
            let text = reverse value
            in go currentLine (currentColumn + 1) remaining
                (token StringToken text line column currentLine (currentColumn + 1) : output)
        collect ('\\':'n':remaining) value currentLine currentColumn = collect remaining ('\n':value) currentLine (currentColumn + 2)
        collect ('\\':'"':remaining) value currentLine currentColumn = collect remaining ('"':value) currentLine (currentColumn + 2)
        collect ('\n':_) _ currentLine currentColumn = Left [diagnostic "VXL0004" currentLine currentColumn "newline in string literal"]
        collect (c:remaining) value currentLine currentColumn = collect remaining (c:value) currentLine (currentColumn + 1)

    token kind text startLine startColumn endLine endColumn =
        Token kind text (SourceSpan file (SourcePosition startLine startColumn) (SourcePosition endLine endColumn))
    diagnostic code line column message = Diagnostic LexerStage Error code
        (Just (SourceSpan file (SourcePosition line column) (SourcePosition line (column + 1)))) message

keywords :: [String]
keywords = ["auto", "bool", "class", "else", "false", "final", "if", "int", "internal", "long", "namespace", "not", "private", "protected", "public", "return", "static", "string", "true", "unit", "void"]

longestSymbol :: String -> Maybe String
longestSymbol source = firstMatch ["==", "\\=", "<=", ">=", "&&", "||", "//", "->", "**", "++", "--", "{", "}", "(", ")", ",", ";", ".", "=", "+", "-", "*", "/", "%", "<", ">", "!"]
  where
    firstMatch [] = Nothing
    firstMatch (candidate:remaining)
        | take (length candidate) source == candidate = Just candidate
        | otherwise = firstMatch remaining
