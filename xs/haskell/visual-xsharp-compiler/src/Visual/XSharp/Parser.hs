-- SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
-- SPDX-License-Identifier: MPL-2.0
module Visual.XSharp.Parser (TokenKind (..), Token (..), ParserInput (..), Parser (..), defaultParser, runParser) where

import Visual.XSharp.AST
import Visual.XSharp.Diagnostic

data TokenKind = IdentifierToken | KeywordToken | SymbolToken | IntegerToken | StringToken | EndOfFileToken
    deriving (Bounded, Enum, Eq, Ord, Read, Show)
data Token = Token {tokenKind :: TokenKind, tokenText :: String, tokenSpan :: SourceSpan}
    deriving (Eq, Ord, Read, Show)
data ParserInput = ParserInput {parserSourceFile :: FilePath, parserTokens :: [Token]}
    deriving (Eq, Ord, Read, Show)
newtype Parser = Parser {parseTokens :: ParserInput -> Either [Diagnostic] ParsedAST}
runParser :: Parser -> ParserInput -> Either [Diagnostic] ParsedAST
runParser = parseTokens
defaultParser :: Parser
defaultParser = Parser parseVisualXSharp

newtype P a = P {runP :: [Token] -> Either Diagnostic (a, [Token])}
instance Functor P where fmap f parser = P $ \ts -> do (a, rest) <- runP parser ts; pure (f a, rest)
instance Applicative P where
    pure a = P $ \ts -> Right (a, ts)
    pf <*> pa = P $ \ts -> do (f, rest) <- runP pf ts; (a, final) <- runP pa rest; pure (f a, final)
instance Monad P where p >>= f = P $ \ts -> do (a, rest) <- runP p ts; runP (f a) rest

parseVisualXSharp :: ParserInput -> Either [Diagnostic] ParsedAST
parseVisualXSharp input = case runP parseModule (parserTokens input) of
    Left problem -> Left [problem]
    Right (tree, []) -> Right (ParsedAST tree)
    Right (tree, token : _) | tokenKind token == EndOfFileToken -> Right (ParsedAST tree)
    Right (_, token : _) -> Left [problemAt token "VXP0001" ("unexpected token " ++ show (tokenText token))]
    where
        parseModule = do
            namespace <- optionalParser parseNamespace
            SyntaxTree namespace <$> manyUntilEof parseDeclaration

parseNamespace :: P QualifiedName
parseNamespace = do
    _ <- keyword "namespace"
    (first, _) <- identifier
    remaining <- moreParts
    _ <- symbol ";"
    pure (QualifiedName (first : remaining))
    where
        moreParts = do
            dot <- optionalSymbol "."
            if dot then do (part, _) <- identifier; (part :) <$> moreParts else pure []

manyUntilEof :: P a -> P [a]
manyUntilEof parser = do done <- peekKind EndOfFileToken; if done then pure [] else (:) <$> parser <*> manyUntilEof parser

parseDeclaration :: P (Declaration Identifier ())
parseDeclaration = do
    _ <- parseAccess
    start <- keyword "class"
    (name, _) <- identifier
    _ <- symbol "{"
    members <- manyUntil "}" parseMember
    close <- symbol "}"
    pure (TypeDeclaration (mergeSpan (tokenSpan start) (tokenSpan close)) name () members)

parseMember :: P (Declaration Identifier ())
parseMember = do
    access <- parseAccess
    isStatic <- optionalSymbol "static"
    returnType <- parseTypeSyntax
    (name, nameSpan) <- identifier
    _ <- symbol "("
    parameters <- separated "," parseParameter
    _ <- symbol ")"
    -- A semicolon-free expression is a function result, not an optionally
    -- terminated statement. Only a value-returning function's outer body may
    -- therefore admit that form; nested control-flow blocks may not.
    body <- parseBlock (returnType /= ExplicitType (Identifier "unit"))
    pure
        (FunctionDeclaration (mergeSpan nameSpan (blockSpan body nameSpan)) name () returnType parameters body isStatic access)

parseAccess :: P Access
parseAccess = do
    token <- optionalParser (keyword "public" <|?> keyword "internal" <|?> keyword "private" <|?> keyword "protected")
    pure $ case fmap tokenText token of
        Just "public" -> PublicAccess
        Just "internal" -> InternalAccess
        Just "private" -> PrivateAccess
        Just "protected" -> ProtectedAccess
        _ -> DefaultAccess

manyUntil :: String -> P a -> P [a]
manyUntil closing parser = do
    done <- peekText closing
    eof <- peekKind EndOfFileToken
    if done
        then pure []
        else if eof then failCurrent "VXP0002" "unterminated declaration" else (:) <$> parser <*> manyUntil closing parser

parseParameter :: P (Parameter Identifier ())
parseParameter = do
    _ <- optionalSymbol "_"
    parameterType <- parseTypeSyntax
    (name, spanValue) <- identifier
    pure (Parameter spanValue name () parameterType)

parseTypeSyntax :: P TypeSyntax
parseTypeSyntax = do
    token <- satisfy (\candidate -> tokenKind candidate `elem` [IdentifierToken, KeywordToken]) "type"
    pure $ case tokenText token of
        "auto" -> AutoType
        "void" -> ExplicitType (Identifier "unit")
        value -> ExplicitType (Identifier value)

parseBlock :: Bool -> P (Block Identifier ())
parseBlock allowFinalExpression = do _ <- symbol "{"; statements <- go; _ <- symbol "}"; pure (Block statements)
    where
        go = do
            done <- peekText "}"
            eof <- peekKind EndOfFileToken
            if done
                then pure []
                else
                    if eof
                        then failCurrent "VXP0002" "unterminated block"
                        else (:) <$> parseStatement allowFinalExpression <*> go

blockSpan :: Block name annotation -> SourceSpan -> SourceSpan
blockSpan (Block []) fallback = fallback
blockSpan (Block statements) fallback = foldl mergeSpan fallback (map statementSpan statements)

statementSpan :: Statement name annotation -> SourceSpan
statementSpan statement = case statement of
    BindingStatement value _ _ _ _ _ -> value
    AssignmentStatement value _ _ _ -> value
    ReturnStatement value _ -> value
    IfStatement value _ _ _ -> value
    ExpressionStatement value _ _ -> value

parseStatement :: Bool -> P (Statement Identifier ())
parseStatement allowFinalExpression =
    parseReturn <|?> parseIf <|?> parseBinding <|?> parseAssignmentOrExpression allowFinalExpression

parseReturn :: P (Statement Identifier ())
parseReturn = do
    start <- keyword "return"
    empty <- peekText ";"
    if empty
        then do _ <- symbol ";"; pure (ReturnStatement (tokenSpan start) Nothing)
        else do
            value <- parseExpression
            end <- symbol ";"
            pure (ReturnStatement (mergeSpan (tokenSpan start) (tokenSpan end)) (Just value))

parseIf :: P (Statement Identifier ())
parseIf = do
    start <- keyword "if"
    hasParen <- optionalSymbol "("
    condition <- parseExpression
    if hasParen then do _ <- symbol ")"; pure () else pure ()
    trueBlock <- parseBlock False
    falseBlock <- optionalParser (keyword "else" >> parseBlock False)
    pure
        ( IfStatement
            (mergeSpan (tokenSpan start) (blockSpan (maybe trueBlock id falseBlock) (tokenSpan start)))
            condition
            trueBlock
            falseBlock
        )

parseBinding :: P (Statement Identifier ())
parseBinding = do
    finalToken <- optionalParser (keyword "final")
    bindingType <- parseTypeSyntax
    (name, nameSpan) <- identifier
    _ <- symbol "="
    value <- parseExpression
    end <- symbol ";"
    let startSpan = maybe nameSpan tokenSpan finalToken
        kind = maybe MutableBinding (const ImmutableBinding) finalToken
    pure (BindingStatement (mergeSpan startSpan (tokenSpan end)) kind bindingType name () value)

parseAssignmentOrExpression :: Bool -> P (Statement Identifier ())
parseAssignmentOrExpression allowFinalExpression = do
    expression <- parseExpression
    assignment <- optionalSymbol "="
    if assignment
        then case expression of
            NameExpression start name _ -> do
                value <- parseExpression
                end <- symbol ";"
                pure (AssignmentStatement (mergeSpan start (tokenSpan end)) name () value)
            _ -> failAt (expressionSpan expression) "VXP0003" "assignment target must be a name"
        else do
            terminated <- peekText ";"
            if terminated
                then do
                    end <- symbol ";"
                    pure (ExpressionStatement (mergeSpan (expressionSpan expression) (tokenSpan end)) expression True)
                else do
                    closesBody <- peekText "}"
                    if allowFinalExpression && closesBody
                        then pure (ExpressionStatement (expressionSpan expression) expression False)
                        else do
                            _ <- symbol ";"
                            pure (ExpressionStatement (expressionSpan expression) expression True)

parseExpression :: P (Expression Identifier ())
parseExpression = parseLogicalOr
parseLogicalOr
    , parseLogicalAnd
    , parseEquality
    , parseComparison
    , parseAdditive
    , parseMultiplicative ::
        P (Expression Identifier ())
parseLogicalOr = chainLeft parseLogicalAnd [("||", LogicalOr)]
parseLogicalAnd = chainLeft parseEquality [("&&", LogicalAnd)]
parseEquality = chainLeft parseComparison [("==", Equal), ("\\=", NotEqual)]
parseComparison = chainLeft parseAdditive [("<", LessThan), ("<=", LessEqual), (">", GreaterThan), (">=", GreaterEqual)]
parseAdditive = chainLeft parseMultiplicative [("+", Add), ("-", Subtract)]
parseMultiplicative = chainLeft parseUnary [("*", Multiply), ("/", Divide), ("//", FloorDivide), ("%", Remainder)]

chainLeft :: P (Expression Identifier ()) -> [(String, BinaryOperator)] -> P (Expression Identifier ())
chainLeft operand operators = operand >>= continue
    where
        continue left = do
            next <- peekToken
            case next >>= (\token -> lookup (tokenText token) operators) of
                Nothing -> pure left
                Just operator -> do
                    _ <- takeToken
                    right <- operand
                    continue (BinaryExpression (mergeSpan (expressionSpan left) (expressionSpan right)) operator left right ())

parseUnary :: P (Expression Identifier ())
parseUnary = do
    next <- peekToken
    case next
        >>= (\token -> lookup (tokenText token) [("+", UnaryPlus), ("-", UnaryNegate), ("!", LogicalNot), ("not", LogicalNot)]) of
        Just operator -> do
            start <- takeToken
            value <- parseUnary
            pure (UnaryExpression (mergeSpan (tokenSpan start) (expressionSpan value)) operator value ())
        Nothing -> parsePostfix

parsePostfix :: P (Expression Identifier ())
parsePostfix = parsePrimary >>= calls
    where
        calls callee = do
            call <- optionalSymbol "("
            if not call
                then pure callee
                else do
                    arguments <- separated "," parseExpression
                    close <- symbol ")"
                    calls (CallExpression (mergeSpan (expressionSpan callee) (tokenSpan close)) callee arguments ())

parsePrimary :: P (Expression Identifier ())
parsePrimary = do
    next <- peekToken
    case next of
        Just token | tokenKind token == IntegerToken -> do
            _ <- takeToken
            pure (LiteralExpression (tokenSpan token) (IntegerLiteral (read (filter (/= '_') (tokenText token)))) ())
        Just token | tokenKind token == StringToken -> do _ <- takeToken; pure (LiteralExpression (tokenSpan token) (StringLiteral (tokenText token)) ())
        Just token | tokenText token `elem` ["true", "false"] -> do _ <- takeToken; pure (LiteralExpression (tokenSpan token) (BooleanLiteral (tokenText token == "true")) ())
        Just token | tokenKind token == IdentifierToken -> do _ <- takeToken; pure (NameExpression (tokenSpan token) (Identifier (tokenText token)) ())
        Just token | tokenText token == "(" -> do
            open <- takeToken
            unit <- peekText ")"
            if unit
                then do close <- symbol ")"; pure (LiteralExpression (mergeSpan (tokenSpan open) (tokenSpan close)) UnitLiteral ())
                else do value <- parseExpression; _ <- symbol ")"; pure value
        Just token -> failAt (tokenSpan token) "VXP0004" ("expected expression, found " ++ show (tokenText token))
        Nothing -> failCurrent "VXP0005" "expected expression at end of input"

expressionSpan :: Expression name annotation -> SourceSpan
expressionSpan expression = case expression of
    NameExpression value _ _ -> value
    LiteralExpression value _ _ -> value
    CallExpression value _ _ _ -> value
    UnaryExpression value _ _ _ -> value
    BinaryExpression value _ _ _ _ -> value

(<|?>) :: P a -> P a -> P a
left <|?> right = P $ \tokens -> case runP left tokens of Left _ -> runP right tokens; success -> success
optionalParser :: P a -> P (Maybe a)
optionalParser parser = (Just <$> parser) <|?> pure Nothing
separated :: String -> P a -> P [a]
separated separator parser = do
    done <- peekText ")"
    if done
        then pure []
        else do
            first <- parser
            more <- optionalSymbol separator
            if more then (first :) <$> separated separator parser else pure [first]
identifier :: P (Identifier, SourceSpan)
identifier = do
    token <- satisfy ((== IdentifierToken) . tokenKind) "identifier"; pure (Identifier (tokenText token), tokenSpan token)
keyword :: String -> P Token
keyword text = satisfy ((== text) . tokenText) (show text)
symbol :: String -> P Token
symbol text = satisfy ((== text) . tokenText) (show text)
optionalSymbol :: String -> P Bool
optionalSymbol text = do matches <- peekText text; if matches then takeToken >> pure True else pure False
satisfy :: (Token -> Bool) -> String -> P Token
satisfy predicate expectation = P $ \tokens -> case tokens of
    token : rest | predicate token -> Right (token, rest)
    token : _ -> Left (problemAt token "VXP0006" ("expected " ++ expectation ++ ", found " ++ show (tokenText token)))
    [] -> Left (Diagnostic ParserStage Error "VXP0007" Nothing ("expected " ++ expectation ++ " at end of input"))
takeToken :: P Token
takeToken = P $ \tokens -> case tokens of
    token : rest -> Right (token, rest)
    [] -> Left (Diagnostic ParserStage Error "VXP0008" Nothing "unexpected end of input")
peekToken :: P (Maybe Token)
peekToken = P $ \tokens -> Right (case tokens of [] -> Nothing; token : _ -> Just token, tokens)
peekText :: String -> P Bool
peekText text = maybe False ((== text) . tokenText) <$> peekToken
peekKind :: TokenKind -> P Bool
peekKind kind = maybe False ((== kind) . tokenKind) <$> peekToken
failCurrent :: String -> String -> P a
failCurrent code message = P $ \tokens -> Left $ case tokens of token : _ -> problemAt token code message; [] -> Diagnostic ParserStage Error code Nothing message
failAt :: SourceSpan -> String -> String -> P a
failAt value code message = P $ \_ -> Left (Diagnostic ParserStage Error code (Just value) message)
problemAt :: Token -> String -> String -> Diagnostic
problemAt token code message = Diagnostic ParserStage Error code (Just (tokenSpan token)) message
mergeSpan :: SourceSpan -> SourceSpan -> SourceSpan
mergeSpan left right = SourceSpan (sourceFile left) (sourceStart left) (sourceEnd right)
