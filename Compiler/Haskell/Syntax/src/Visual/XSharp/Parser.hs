-- SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
-- SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.0
module Visual.XSharp.Parser (TokenKind (..), Token (..), ParserInput (..), Parser (..), defaultParser, runParser) where

import Visual.XSharp.AST
import Visual.XSharp.CharacterLiteral
import Visual.XSharp.Diagnostic
import Visual.XSharp.FloatingLiteral
import Visual.XSharp.NumericLiteral
import Visual.XSharp.Parser.Cursor
import Visual.XSharp.Parser.Token

data ParserInput = ParserInput {parserSourceFile :: FilePath, parserTokens :: [Token]}
    deriving (Eq, Ord, Read, Show)
newtype Parser = Parser {parseTokens :: ParserInput -> Either [Diagnostic] ParsedAST}
runParser :: Parser -> ParserInput -> Either [Diagnostic] ParsedAST
runParser = parseTokens
defaultParser :: Parser
defaultParser = Parser parseVisualXSharp

parseVisualXSharp :: ParserInput -> Either [Diagnostic] ParsedAST
parseVisualXSharp input = case runP (parseModule <* endOfInput) (parserTokens input) of
    Left problem -> Left [problem]
    Right (tree, []) -> Right (ParsedAST tree)
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
manyUntilEof parser = do
    next <- peekToken
    case next of
        Nothing -> pure []
        Just token | tokenKind token == EndOfFileToken -> pure []
        _ -> (:) <$> parser <*> manyUntilEof parser

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
    (member, spanValue) <- withSpan parseMemberBody
    pure member {declarationSpan = spanValue}

parseMemberBody :: P (Declaration Identifier ())
parseMemberBody = do
    access <- parseAccess
    isStatic <- maybe False (const True) <$> optionalParser (keyword "static")
    returnType <- parseTypeSyntax
    (name, nameSpan) <- identifier
    _ <- symbol "("
    parameters <- separated "," parseParameter
    _ <- symbol ")"
    -- A semicolon-free expression is a function result, not an optionally
    -- terminated statement. Only a value-returning function's outer body may
    -- therefore admit that form; nested control-flow blocks may not.
    body <- parseBlock (returnType /= ExplicitType (Identifier "void"))
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
    _ <- optionalParser (satisfy (\token -> tokenKind token == IdentifierToken && tokenText token == "_") "parameter label")
    parameterType <- parseTypeSyntax
    (name, spanValue) <- identifier
    pure (Parameter spanValue name () parameterType)

parseTypeSyntax :: P TypeSyntax
parseTypeSyntax = do
    token <- satisfy (\candidate -> tokenKind candidate `elem` [IdentifierToken, KeywordToken]) "type"
    case tokenText token of
        "unit" -> failAt (tokenSpan token) "VXP0013" "Visual X# has no source-language unit type; use void for a no-result function"
        "auto" -> pure AutoType
        "void" -> pure (ExplicitType (Identifier "void"))
        value -> pure (ExplicitType (Identifier value))

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
    do
        tokens <- peekTokens 2
        -- Dispatch before parsing: a failed binding must not be retried as an
        -- expression. Two tokens suffice for the current scalar type syntax.
        case tokens of
            first : _ | tokenKind first == KeywordToken && tokenText first == "return" -> parseReturn
            first : _ | tokenKind first == KeywordToken && tokenText first == "if" -> parseIf
            first : _ | tokenKind first == KeywordToken && tokenText first == "final" -> parseBinding
            first : second : _
                | tokenKind first `elem` [IdentifierToken, KeywordToken]
                , tokenKind second == IdentifierToken
                , tokenText first /= "not" ->
                    parseBinding
            _ -> parseAssignmentOrExpression allowFinalExpression

parseReturn :: P (Statement Identifier ())
parseReturn = do
    start <- keyword "return"
    empty <- peekText ";"
    if empty
        then do end <- symbol ";"; pure (ReturnStatement (mergeSpan (tokenSpan start) (tokenSpan end)) Nothing)
        else do
            value <- parseExpression
            end <- symbol ";"
            pure (ReturnStatement (mergeSpan (tokenSpan start) (tokenSpan end)) (Just value))

parseIf :: P (Statement Identifier ())
parseIf = do
    ((condition, trueBlock, falseBlock), spanValue) <- withSpan $ do
        _ <- keyword "if"
        _ <- symbol "("
        condition <- parseExpression
        _ <- symbol ")"
        trueBlock <- parseBlock False
        falseBlock <- optionalParser $ do
            _ <- keyword "else"
            chained <- peekText "if"
            if chained then Block . (: []) <$> parseIf else parseBlock False
        pure (condition, trueBlock, falseBlock)
    pure (IfStatement spanValue condition trueBlock falseBlock)

parseBinding :: P (Statement Identifier ())
parseBinding = do
    start <- peekToken
    finalToken <- optionalParser (keyword "final")
    bindingType <- parseTypeSyntax
    (name, nameSpan) <- identifier
    _ <- symbol "="
    value <- parseExpression
    end <- symbol ";"
    let startSpan = maybe nameSpan tokenSpan start
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
parseEquality = nonAssociative "equality" parseComparison [("==", Equal), ("\\=", NotEqual)]
parseComparison =
    nonAssociative "relational" parseAdditive [("<", LessThan), ("<=", LessEqual), (">", GreaterThan), (">=", GreaterEqual)]
parseAdditive = chainLeft parseMultiplicative [("+", Add), ("-", Subtract)]
parseMultiplicative = chainLeft parseUnary [("*", Multiply), ("/", Divide), ("//", FloorDivide), ("%", Remainder)]

chainLeft :: P (Expression Identifier ()) -> [(String, BinaryOperator)] -> P (Expression Identifier ())
chainLeft operand operators = operand >>= continue
    where
        continue left = do
            next <- peekToken
            case next >>= operatorToken operators of
                Nothing -> pure left
                Just operator -> do
                    _ <- takeToken
                    right <- operand
                    continue (BinaryExpression (mergeSpan (expressionSpan left) (expressionSpan right)) operator left right ())

parseUnary :: P (Expression Identifier ())
parseUnary = do
    next <- peekToken
    case next
        >>= operatorToken [("+", UnaryPlus), ("-", UnaryNegate), ("not", LogicalNot)] of
        Just operator -> do
            start <- takeToken
            value <- parseUnary
            pure (UnaryExpression (mergeSpan (tokenSpan start) (expressionSpan value)) operator value ())
        Nothing -> parsePostfix

-- Equality and relational groups are non-associative. A parenthesized operand
-- starts a fresh level; a second operator at this level is a syntax error.
nonAssociative :: String -> P (Expression Identifier ()) -> [(String, BinaryOperator)] -> P (Expression Identifier ())
nonAssociative group operand operators = do
    left <- operand
    next <- peekToken
    case next >>= operatorToken operators of
        Nothing -> pure left
        Just operator -> do
            _ <- takeToken
            right <- operand
            following <- peekToken
            case following >>= operatorToken operators of
                Just _ -> failCurrent "VXP0014" (group ++ " operators cannot be chained; use parentheses or an explicit logical conjunction")
                Nothing -> pure (BinaryExpression (mergeSpan (expressionSpan left) (expressionSpan right)) operator left right ())

operatorToken :: [(String, a)] -> Token -> Maybe a
operatorToken operators token
    | tokenKind token `elem` [SymbolToken, KeywordToken] = lookup (tokenText token) operators
    | otherwise = Nothing

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
        Just token | tokenKind token == SymbolToken && tokenText token `elem` ["\\", "["] -> parseCallable
        Just token | tokenKind token == IntegerToken -> do
            _ <- takeToken
            case parseIntegerSpelling (tokenText token) of
                Right parsed -> pure (LiteralExpression (tokenSpan token) (IntegerLiteral (parsedIntegerValue parsed)) ())
                Left issue -> failAt (tokenSpan token) "VXP0010" (renderIntegerLiteralError issue)
        Just token | tokenKind token == FloatingToken -> do
            _ <- takeToken
            case validateFloatingSpelling (tokenText token) of
                Right normalized -> pure (LiteralExpression (tokenSpan token) (FloatingLiteral normalized) ())
                Left issue -> failAt (tokenSpan token) "VXP0012" (renderFloatingLiteralError issue)
        Just token | tokenKind token == CharacterToken -> do
            _ <- takeToken
            case parseCharacterLiteral (tokenText token) of
                Right value -> pure (LiteralExpression (tokenSpan token) (CharacterLiteral value) ())
                Left issue -> failAt (tokenSpan token) "VXP0011" (renderCharacterLiteralError issue)
        Just token | tokenKind token == StringToken -> do _ <- takeToken; pure (LiteralExpression (tokenSpan token) (StringLiteral (tokenText token)) ())
        Just token | tokenKind token == KeywordToken && tokenText token `elem` ["true", "false"] -> do _ <- takeToken; pure (LiteralExpression (tokenSpan token) (BooleanLiteral (tokenText token == "true")) ())
        Just token | tokenKind token == IdentifierToken -> do _ <- takeToken; pure (NameExpression (tokenSpan token) (Identifier (tokenText token)) ())
        Just token | tokenText token == "(" -> do
            _ <- takeToken
            value <- parseExpression
            _ <- symbol ")"
            pure value
        Just token -> failAt (tokenSpan token) "VXP0004" ("expected expression, found " ++ show (tokenText token))
        Nothing -> failCurrent "VXP0005" "expected expression at end of input"

-- A capture list belongs to the callable which follows it.  Keeping this at
-- primary-expression precedence allows immediately invoking a literal while
-- preventing binary operators from becoming part of capture initializers.
parseCallable :: P (Expression Identifier ())
parseCallable = do
    ((explicitCaptures, captures, parameters, body), spanValue) <- withSpan $ do
        (explicitCaptures, captures) <- optionalCaptureList
        _ <- symbol "\\"
        parameters <- parseCallableParameters
        _ <- symbol "->"
        body <- parseCallableBody
        pure (explicitCaptures, captures, parameters, body)
    pure (CallableExpression spanValue explicitCaptures captures parameters body ())

optionalCaptureList :: P (Bool, [Capture Identifier ()])
optionalCaptureList = do
    present <- peekText "["
    if not present
        then pure (False, [])
        else do
            _ <- symbol "["
            empty <- peekText "]"
            captures <- if empty then pure [] else separatedUntil "]" "," parseCapture
            _ <- symbol "]"
            pure (True, captures)

parseCapture :: P (Capture Identifier ())
parseCapture = do
    modeToken <- optionalParser (keyword "weak" <|?> keyword "unowned")
    (name, nameSpan) <- identifier
    hasInitializer <- optionalSymbol "="
    initializer <- if hasInitializer then Just <$> parseExpression else pure Nothing
    let mode = case fmap tokenText modeToken of
            Just "weak" -> WeakCapture
            Just "unowned" -> UnownedCapture
            _ -> StrongCapture
        spanValue = maybe nameSpan (mergeSpan nameSpan . expressionSpan) initializer
    pure (Capture spanValue mode name () initializer)

parseCallableParameters :: P [Parameter Identifier ()]
parseCallableParameters = do
    parenthesized <- optionalSymbol "("
    if parenthesized
        then do
            empty <- peekText ")"
            parameters <- if empty then pure [] else separatedUntil ")" "," parseCallableParameter
            _ <- symbol ")"
            pure parameters
        else do
            arrow <- peekText "->"
            if arrow then pure [] else separatedUntil "->" "," parseInferredCallableParameter

parseCallableParameter :: P (Parameter Identifier ())
parseCallableParameter = do
    tokens <- peekTokens 2
    case tokens of
        first : second : _
            | tokenKind first `elem` [IdentifierToken, KeywordToken]
            , tokenKind second == IdentifierToken ->
                parseParameter
        _ -> parseInferredCallableParameter

parseInferredCallableParameter :: P (Parameter Identifier ())
parseInferredCallableParameter = do
    (name, spanValue) <- identifier
    pure (Parameter spanValue name () AutoType)

parseCallableBody :: P (CallableBody Identifier ())
parseCallableBody = do
    block <- peekText "{"
    if block
        then CallableBlockBody <$> parseBlock True
        else CallableExpressionBody <$> parseExpression

separatedUntil :: String -> String -> P a -> P [a]
separatedUntil closing separator parser = do
    first <- parser
    more <- optionalSymbol separator
    if more
        then (first :) <$> separatedUntil closing separator parser
        else do
            done <- peekText closing
            if done then pure [first] else failCurrent "VXP0009" ("expected " ++ show separator ++ " or " ++ show closing)

expressionSpan :: Expression name annotation -> SourceSpan
expressionSpan expression = case expression of
    NameExpression value _ _ -> value
    LiteralExpression value _ _ -> value
    CallExpression value _ _ _ -> value
    UnaryExpression value _ _ _ -> value
    BinaryExpression value _ _ _ _ -> value
    CallableExpression value _ _ _ _ _ -> value

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
mergeSpan :: SourceSpan -> SourceSpan -> SourceSpan
mergeSpan left right = SourceSpan (sourceFile left) (sourceStart left) (sourceEnd right)
