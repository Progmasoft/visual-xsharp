-- SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
-- SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.0

{- | Conservative formatting on compiler-validated Visual X# source.

The first formatter slice deliberately changes only physical line layout. It
does not reconstruct strings or comments from tokens, because doing so would
discard their original spelling. Deeper token spacing and indentation will be
added only with a trivia-preserving compiler source model.
-}
module Visual.Formatter
    ( LineEnding (..)
    , FormatOptions (..)
    , FormatResult (..)
    , defaultFormatOptions
    , formatSource
    ) where

import Data.List (intercalate)
import Visual.XSharp.Diagnostic
import Visual.XSharp.Frontend

data LineEnding = Auto | CrLf | Lf
    deriving (Bounded, Enum, Eq, Ord, Read, Show)

data FormatOptions = FormatOptions
    { formatLineEnding :: LineEnding
    , formatTrimTrailingWhitespace :: Bool
    , formatInsertFinalNewline :: Bool
    }
    deriving (Eq, Ord, Read, Show)

data FormatResult = FormatResult
    { formattedSource :: String
    , formattingChanged :: Bool
    }
    deriving (Eq, Ord, Read, Show)

defaultFormatOptions :: FormatOptions
defaultFormatOptions = FormatOptions Auto True True

formatSource :: FormatOptions -> CompilerInput -> Either [Diagnostic] FormatResult
formatSource options input = do
    -- Syntax validation is the safety gate. Formatting malformed input would
    -- otherwise make recovery choices that belong to the parser and editor.
    _ <- analyzeSyntax input
    let source = compilerSourceText input
        newline = selectedLineEnding (formatLineEnding options) source
        normalized = normalizeLineEndings source
        sourceEndsWithNewline = not (null normalized) && last normalized == '\n'
        physicalLines = splitLogicalLines normalized
        bodyLines = if sourceEndsWithNewline then dropLast physicalLines else physicalLines
        formattedLines =
            if formatTrimTrailingWhitespace options
                then map trimHorizontalEnd bodyLines
                else bodyLines
        requiresFinalNewline = formatInsertFinalNewline options && not (null source)
        output = intercalate newline formattedLines ++ if requiresFinalNewline then newline else ""
    pure (FormatResult output (output /= source))

selectedLineEnding :: LineEnding -> String -> String
selectedLineEnding CrLf _ = "\r\n"
selectedLineEnding Lf _ = "\n"
selectedLineEnding Auto source = case firstLineEnding source of
    Just value -> value
    Nothing -> "\n"

firstLineEnding :: String -> Maybe String
firstLineEnding [] = Nothing
firstLineEnding ('\r' : '\n' : _) = Just "\r\n"
firstLineEnding ('\r' : _) = Just "\r"
firstLineEnding ('\n' : _) = Just "\n"
firstLineEnding (_ : remaining) = firstLineEnding remaining

normalizeLineEndings :: String -> String
normalizeLineEndings [] = []
normalizeLineEndings ('\r' : '\n' : remaining) = '\n' : normalizeLineEndings remaining
normalizeLineEndings ('\r' : remaining) = '\n' : normalizeLineEndings remaining
normalizeLineEndings (character : remaining) = character : normalizeLineEndings remaining

splitLogicalLines :: String -> [String]
splitLogicalLines source = case break (== '\n') source of
    (line, []) -> [line]
    (line, _ : remaining) -> line : splitLogicalLines remaining

dropLast :: [value] -> [value]
dropLast [] = []
dropLast [_] = []
dropLast (value : remaining) = value : dropLast remaining

trimHorizontalEnd :: String -> String
trimHorizontalEnd = reverse . dropWhile isHorizontalSpace . reverse
    where
        isHorizontalSpace character = character == ' ' || character == '\t'
