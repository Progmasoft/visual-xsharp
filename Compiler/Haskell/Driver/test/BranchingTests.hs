-- SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
-- SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.1

{- | Vertical tests for @match@, for @if@ used as an expression, for
@guard@, and for a block written as a statement.

The parser tests pin the shape of each form and the diagnostics of the
spellings that are recognized but not accepted. The type tests pin every rule
that decides whether a match has a value: one type for all arms, an arm that
always accepts, no arm that can never be selected. The evaluation tests state
the value each program must compute and run the reference evaluator of
"CoreInterpreter" on the unoptimized and on the optimized Core, so a lowering
that selects the wrong arm, evaluates a subject twice, or runs a guard that
should not run is observed as a wrong result.
-}
module BranchingTests (branchingTests) where

import CoreInterpreter
import Data.List (isInfixOf, isPrefixOf)
import Visual.XSharp.AST
import Visual.XSharp.Compiler
import Visual.XSharp.Core
import Visual.XSharp.Core.CorePrep.Verifier
import Visual.XSharp.Core.Verifier
import Visual.XSharp.Core.Wire
import Visual.XSharp.Diagnostic
import Visual.XSharp.Frontend
import Visual.XSharp.Lexer
import Visual.XSharp.Parser
import Visual.XSharp.Template.Application
import Visual.XSharp.Template.Specialization

branchingTests :: [(String, Bool)]
branchingTests = parserTests ++ typeTests ++ loweringTests ++ evaluationTests ++ pipelineTests

-- ---------------------------------------------------------------- sources

body :: String -> String
body statements =
    unlines
        [ "class Program {"
        , "    public static int Evaluate(_ bool flag, _ bool other, _ int left, _ int right) {"
        , "        " ++ statements
        , "    }"
        , "    public static int Twice(_ int value) { return value + value; }"
        , "}"
        ]

parseSource :: String -> Either [Diagnostic] ParsedAST
parseSource text = do
    tokens <- runLexer defaultLexer (LexerInput "branching.vxs" text)
    runParser defaultParser (ParserInput "branching.vxs" tokens)

parses :: String -> Bool
parses text = either (const False) (const True) (parseSource text)

-- | Whether parsing stops with the given diagnostic.
parseFailsWith :: String -> String -> Bool
parseFailsWith code text = case parseSource text of
    Left problems -> any ((== code) . diagnosticCode) problems
    Right _ -> False

firstStatements :: String -> Maybe [Statement Identifier ()]
firstStatements text = case parseSource text of
    Right (ParsedAST (SyntaxTree _ (TypeDeclaration {typeMembers = member : _} : _))) -> case member of
        FunctionDeclaration {declarationBody = Block statements} -> Just statements
        _ -> Nothing
    _ -> Nothing

compileSource :: String -> Either [Diagnostic] FrontendArtifacts
compileSource text = compileToCorePrep (CompilerInput "branching.vxs" text)

accepted :: String -> Bool
accepted text = either (const False) (const True) (compileSource text)

rejectedWith :: String -> String -> Bool
rejectedWith code text = case compileSource text of
    Left problems -> any ((== code) . diagnosticCode) problems
    Right _ -> False

-- | The codes a source is rejected with, or none when it is accepted.
codesOf :: String -> [String]
codesOf text = either (map diagnosticCode) (const []) (compileSource text)

-- ---------------------------------------------------------------- parser

parserTests :: [(String, Bool)]
parserTests =
    [ ("a match at the start of a statement is a terminated expression statement", matchStatementParses)
    , ("a match in a binding initializer is a match expression", matchExpressionParses)
    , ("every subject of a match has its own parentheses", multipleSubjectsParse)
    , ("an arm has one pattern for each subject", multiplePatternsParse)
    , ("a guard follows the patterns of its arm", guardedArmParses)
    , ("a type pattern binds a name or discards the value", typePatternsParse)
    , ("the null and enum case patterns are recognized", otherPatternsParse)
    , ("a block body is a block expression and any other body is the expression itself", armBodiesParse)
    , ("the comma after a block body is optional", parses (body "match (left) { 1 -> { } 2 -> { } } return 0;"))
    , ("the comma after the last arm is optional", parses (body "return match (left) { 1 -> 10, _ -> 20, };"))
    , ("a match without arms parses", parses (body "match (left) { } return 0;"))
    ,
        ( "an expression body must be separated from the next arm"
        , parseFailsWith "VXP0038" (body "return match (left) { 1 -> 10 _ -> 20 };")
        )
    , ("a bare name is not a pattern", parseFailsWith "VXP0036" (body "match (left) { right -> { } } return 0;"))
    , ("a negative literal is not a pattern", parseFailsWith "VXP0036" (body "match (left) { -1 -> { } } return 0;"))
    , ("an unterminated match is reported", not (parses (body "match (left) { 1 -> { }")))
    , ("an if in operand position is a conditional over two value blocks", ifExpressionParses)
    , ("an if at the start of a statement stays an if statement", ifStatementKeepsItsNode)
    ,
        ( "an if used as an expression requires an else branch"
        , parseFailsWith "VXP0033" (body "int r = if (flag) { 1 }; return r;")
        )
    ,
        ( "the else branch of an if expression must be a block"
        , parseFailsWith "VXP0034" (body "int r = if (flag) { 1 } else if (other) { 2 } else { 3 }; return r;")
        )
    , ("a guard is a statement with a condition and an else block", guardParses)
    , ("a guard requires an else block", parseFailsWith "VXP0037" (body "guard (flag) { return 0; } return 1;"))
    ,
        ( "a binding condition in an if is recognized and rejected"
        , parseFailsWith "VXP0035" (body "if (auto user = Twice(left)) { return 1; } return 0;")
        )
    ,
        ( "a binding condition in a guard is recognized and rejected"
        , parseFailsWith "VXP0035" (body "guard (int user = Twice(left)) else { return 1; } return 0;")
        )
    ,
        ( "a binding condition in a while is recognized and rejected"
        , parseFailsWith "VXP0035" (body "while (auto user = Twice(left)) { return 1; } return 0;")
        )
    ,
        ( "a binding condition in an if expression is recognized and rejected"
        , parseFailsWith "VXP0035" (body "int r = if (auto user = Twice(left)) { 1 } else { 2 }; return r;")
        )
    , ("an assignment in a condition is still an expression", parses (body "int n = 0; if (n = left) { return 1; } return 0;"))
    , ("a brace at the start of a statement opens a nested block", nestedBlockParses)
    , ("nested blocks nest", parses (body "{ { { return 1; } } }"))
    , ("an empty nested block parses", parses (body "{ } return 0;"))
    ,
        ( "an unterminated nested block is reported"
        , parseFailsWith "VXP0002" "class Program { public static int Evaluate() { { int a = 1;"
        )
    , ("match is reserved and cannot name a local", not (parses (body "int match = 1; return match;")))
    , ("guard is reserved and cannot name a local", not (parses (body "int guard = 1; return guard;")))
    ]

matchStatementParses :: Bool
matchStatementParses = case firstStatements (body "match (left) { 1 -> { return 1; }, _ -> { return 2; } }") of
    Just [ExpressionStatement _ (MatchExpression _ [NameExpression _ (Identifier "left") ()] [first, second] ()) True] ->
        length (matchArmPatterns first) == 1 && length (matchArmPatterns second) == 1
    _ -> False

matchExpressionParses :: Bool
matchExpressionParses = case firstStatements (body "int r = match (left) { 1 -> 10, _ -> 20 }; return r;") of
    Just [BindingStatement _ _ _ (Identifier "r") () (MatchExpression _ [_] [_, _] ()), _] -> True
    _ -> False

multipleSubjectsParse :: Bool
multipleSubjectsParse = case firstStatements (body "match (left), (right + 1), (flag) { } return 0;") of
    Just [ExpressionStatement _ (MatchExpression _ [NameExpression {}, BinaryExpression {}, NameExpression {}] [] ()) _, _] -> True
    _ -> False

multiplePatternsParse :: Bool
multiplePatternsParse = case firstStatements (body "match (left), (right) { (1), (2) -> { }, 3, _ -> { } } return 0;") of
    Just [ExpressionStatement _ (MatchExpression _ _ [first, second] ()) _, _] ->
        case (matchArmPatterns first, matchArmPatterns second) of
            ( [MatchLiteralPattern _ (IntegerLiteral 1) (), MatchLiteralPattern _ (IntegerLiteral 2) ()]
                , [MatchLiteralPattern _ (IntegerLiteral 3) (), MatchWildcardPattern _ ()]
                ) -> True
            _ -> False
    _ -> False

guardedArmParses :: Bool
guardedArmParses = case firstStatements (body "match (left) { 2 if flag -> { }, _ -> { } } return 0;") of
    Just [ExpressionStatement _ (MatchExpression _ _ [first, second] ()) _, _] ->
        case (matchArmGuard first, matchArmGuard second) of
            (Just (NameExpression _ (Identifier "flag") ()), Nothing) -> True
            _ -> False
    _ -> False

typePatternsParse :: Bool
typePatternsParse = case firstStatements (body "match (left) { int value -> { }, int _ -> { } } return 0;") of
    Just [ExpressionStatement _ (MatchExpression _ _ [first, second] ()) _, _] ->
        case (matchArmPatterns first, matchArmPatterns second) of
            ( [MatchTypePattern _ (ExplicitType (Identifier "int")) (Just (Identifier "value")) ()]
                , [MatchTypePattern _ (ExplicitType (Identifier "int")) Nothing ()]
                ) -> True
            _ -> False
    _ -> False

otherPatternsParse :: Bool
otherPatternsParse = case firstStatements (body "match (left) { null -> { }, .Ready -> { }, true -> { }, 'a' -> { } } return 0;") of
    Just [ExpressionStatement _ (MatchExpression _ _ arms ()) _, _] -> case map matchArmPatterns arms of
        [ [MatchNullPattern _ ()]
            , [MatchCasePattern _ (Identifier "Ready") ()]
            , [MatchLiteralPattern _ (BooleanLiteral True) ()]
            , [MatchLiteralPattern _ (CharacterLiteral 97) ()]
            ] -> True
        _ -> False
    _ -> False

armBodiesParse :: Bool
armBodiesParse = case firstStatements (body "int r = match (left) { 1 -> { int t = 2; t }, _ -> left + 1 }; return r;") of
    Just [BindingStatement _ _ _ _ () (MatchExpression _ _ [first, second] ()), _] ->
        case (matchArmBody first, matchArmBody second) of
            (BlockExpression _ (Block [BindingStatement {}, ExpressionStatement _ _ False]) (), BinaryExpression {}) -> True
            _ -> False
    _ -> False

ifExpressionParses :: Bool
ifExpressionParses = case firstStatements (body "int r = if (flag) { 1 } else { int t = 2; t }; return r;") of
    Just
        [ BindingStatement
                _
                _
                _
                _
                ()
                ( ConditionalExpression
                        _
                        (NameExpression _ (Identifier "flag") ())
                        (BlockExpression _ (Block [ExpressionStatement _ _ False]) ())
                        (BlockExpression _ (Block [BindingStatement {}, ExpressionStatement _ _ False]) ())
                        ()
                    )
            , _
            ] ->
        True
    _ -> False

ifStatementKeepsItsNode :: Bool
ifStatementKeepsItsNode = case firstStatements (body "if (flag) { return 1; } else { return 2; }") of
    Just [IfStatement _ _ _ (Just _)] -> True
    _ -> False

nestedBlockParses :: Bool
nestedBlockParses = case firstStatements (body "int a = 1; { int b = 2; a = b; } return a;") of
    Just [BindingStatement {}, BlockStatement _ (Block [BindingStatement {}, AssignmentStatement {}]), ReturnStatement {}] -> True
    _ -> False

guardParses :: Bool
guardParses = case firstStatements (body "guard (left > 0) else { return 0; } return 1;") of
    Just [GuardStatement _ (BinaryExpression _ GreaterThan _ _ ()) (Block [ReturnStatement {}]), _] -> True
    _ -> False

-- ---------------------------------------------------------------- types

typeTests :: [(String, Bool)]
typeTests =
    [ ("a match expression has the type of its arms", accepted (body "int r = match (left) { 1 -> 10, _ -> 20 }; return r;"))
    , ("a match expression types its literals from the receiver", accepted (body "long wide = match (left) { 1 -> 10, _ -> 20 }; return wide > 15 ? 1 : 0;"))
    , ("a literal arm takes its type from an earlier arm", accepted (body "long wide = 3; long r = match (left) { 1 -> wide, _ -> 20 }; return r > 15 ? 1 : 0;"))
    , ("a literal pattern is typed from its subject", accepted (body "long wide = 5; return match (wide) { 5 -> 1, _ -> 0 };"))
    , ("a bool subject is covered by true and false", accepted (body "return match (flag) { true -> 1, false -> 2 };"))
    , ("a type pattern that binds accepts every value", accepted (body "return match (left) { int value -> value + 1 };"))
    , ("a statement match needs no arm that always accepts", accepted (body "match (left) { 1 -> { return 1; } } return 0;"))
    , ("an accepted match reports nothing", codesOf (body "return match (left) { 1 -> 10, _ -> 20 };") == [])
    ,
        ( "a match expression that may accept nothing is rejected"
        , rejectedWith "VXT0052" (body "return match (left) { 1 -> 10, 2 -> 20 };")
        )
    ,
        ( "a guarded catch-all does not make a match expression complete"
        , rejectedWith "VXT0052" (body "return match (left) { _ if flag -> 1 };")
        )
    ,
        ( "one bool literal does not cover a bool subject"
        , rejectedWith "VXT0052" (body "return match (flag) { true -> 1 };")
        )
    ,
        ( "a guarded bool literal does not cover its value"
        , rejectedWith "VXT0052" (body "return match (flag) { true if other -> 1, false -> 2 };")
        )
    , ("a match expression without arms is rejected", rejectedWith "VXT0052" (body "return match (left) { };"))
    ,
        ( "two bool subjects are covered by their four combinations"
        , accepted
            (body "return match (flag), (other) { (true), (true) -> 3, (true), (false) -> 2, (false), (true) -> 1, (false), (false) -> 0 };")
        )
    ,
        ( "a wildcard covers both values of its bool subject"
        , accepted (body "return match (flag), (other) { (true), (_) -> 1, (false), (true) -> 2, (false), (false) -> 3 };")
        )
    ,
        ( "a missing combination of two bool subjects is rejected"
        , rejectedWith
            "VXT0052"
            (body "return match (flag), (other) { (true), (true) -> 3, (true), (false) -> 2, (false), (true) -> 1 };")
        )
    ,
        ( "a guarded combination does not count as covered"
        , rejectedWith
            "VXT0052"
            (body "return match (flag), (other) { (true), (_) -> 1, (false), (true) -> 2, (false), (false) if left > 0 -> 3 };")
        )
    ,
        ( "a bool subject beside an int subject needs a catch-all"
        , rejectedWith "VXT0052" (body "return match (flag), (left) { (true), (_) -> 1, (false), (1) -> 2 };")
        )
    ,
        ( "arms of different types are rejected"
        , rejectedWith "VXT0050" (body "long wide = 3; long r = match (left) { 1 -> wide, _ -> left }; return 0;")
        )
    ,
        ( "a string match result is not lowered yet"
        , rejectedWith "VXT0051" (body "auto text = match (left) { 1 -> \"a\", _ -> \"b\" }; return 0;")
        )
    ,
        ( "a string subject is not lowered yet"
        , rejectedWith "VXT0058" (body "match (\"a\") { _ -> { } } return 0;")
        )
    ,
        ( "an arm with too few patterns is rejected"
        , rejectedWith "VXT0048" (body "match (left), (right) { 1 -> { } } return 0;")
        )
    ,
        ( "an arm with too many patterns is rejected"
        , rejectedWith "VXT0048" (body "match (left) { 1, 2 -> { } } return 0;")
        )
    ,
        ( "a second arm for the same literal can never be selected"
        , rejectedWith "VXT0053" (body "match (left) { 1 -> { }, 1 -> { } } return 0;")
        )
    ,
        ( "an arm after a catch-all can never be selected"
        , rejectedWith "VXT0053" (body "match (left) { _ -> { }, 1 -> { } } return 0;")
        )
    ,
        ( "an arm after a binding catch-all can never be selected"
        , rejectedWith "VXT0053" (body "match (left) { int value -> { }, _ -> { } } return 0;")
        )
    ,
        ( "an arm shadowed in every position can never be selected"
        , rejectedWith "VXT0053" (body "match (left), (right) { (1), (_) -> { }, (1), (2) -> { } } return 0;")
        )
    ,
        ( "a guarded arm does not shadow a later arm"
        , accepted (body "match (left) { 1 if flag -> { }, 1 -> { } } return 0;")
        )
    ,
        ( "an arm that differs in one position is reachable"
        , accepted (body "match (left), (right) { (1), (2) -> { }, (1), (_) -> { } } return 0;")
        )
    , ("a null pattern needs a reference subject", rejectedWith "VXT0055" (body "match (left) { null -> { } } return 0;"))
    , ("an enum case pattern needs enum declarations", rejectedWith "VXT0056" (body "match (left) { .Ready -> { } } return 0;"))
    ,
        ( "a type pattern must name the type of its subject"
        , rejectedWith "VXT0057" (body "match (left) { long value -> { } } return 0;")
        )
    , ("a literal pattern of another type is rejected", rejectedWith "VXT0054" (body "match (left) { \"a\" -> { } } return 0;"))
    , ("a guard must be bool or numeric", rejectedWith "VXT0049" (body "match (left) { 1 if \"x\" -> { } } return 0;"))
    , ("a numeric guard is accepted", accepted (body "match (left) { 1 if right -> { } } return 0;"))
    ,
        ( "a pure expression body in a statement match is rejected"
        , rejectedWith "VXT0013" (body "match (left) { 1 -> 5 } return 0;")
        )
    ,
        ( "an effectful expression body in a statement match is accepted"
        , accepted (body "int r = 0; match (left) { 1 -> r = 5, _ -> r = Twice(left) } return r;")
        )
    , ("a pattern binding is immutable", rejectedWith "VXT0003" (body "match (left) { int value -> { value = 5; } } return 0;"))
    ,
        ( "a pattern binding is in scope in its guard"
        , accepted (body "return match (left) { int value if value > 3 -> value, _ -> 0 };")
        )
    ,
        ( "a pattern binding is not in scope after the match"
        , not (accepted (body "match (left) { int value -> { } } return value;"))
        )
    ,
        ( "a pattern binding is not in scope in a later arm"
        , not (accepted (body "return match (left) { int value if flag -> value, _ -> value };"))
        )
    ,
        ( "two arms may bind the same name"
        , accepted (body "return match (left) { int value if flag -> value, int value -> value + 1 };")
        )
    ,
        ( "one arm cannot bind a name twice"
        , rejectedWith "VXR0008" (body "match (left), (right) { (int value), (int value) -> { } } return 0;")
        )
    ,
        ( "a pattern binding cannot reuse a name in scope"
        , rejectedWith "VXR0008" (body "match (left) { int flag -> { } } return 0;")
        )
    , ("an if expression has the type of its blocks", accepted (body "int r = if (flag) { left } else { right }; return r;"))
    , ("an if expression types its literals from the receiver", accepted (body "long wide = if (flag) { 1 } else { 2 }; return wide > 1 ? 1 : 0;"))
    ,
        ( "the blocks of an if expression must have one type"
        , rejectedWith "VXT0037" (body "long wide = 3; long r = if (flag) { wide } else { left }; return 0;")
        )
    , ("the test of an if expression must be bool or numeric", rejectedWith "VXT0036" (body "int r = if (\"s\") { 1 } else { 2 }; return r;"))
    ,
        ( "a value block must end with an expression"
        , rejectedWith "VXT0046" (body "int r = if (flag) { Twice(left); } else { 2 }; return r;")
        )
    , ("an empty value block has no value", rejectedWith "VXT0046" (body "int r = if (flag) { } else { 2 }; return r;"))
    ,
        ( "a return inside a value block is rejected"
        , rejectedWith "VXT0047" (body "int r = if (flag) { return 1; 2 } else { 3 }; return r;")
        )
    ,
        ( "a return nested inside a value block is rejected"
        , rejectedWith "VXT0047" (body "int r = match (left) { 1 -> { if (flag) { return 1; } 2 }, _ -> 3 }; return r;")
        )
    ,
        ( "a break cannot leave a value block"
        , rejectedWith "VXT0059" (body "while (true) { int r = if (flag) { break; 1 } else { 2 }; } return 0;")
        )
    ,
        ( "a continue cannot leave a value block"
        , rejectedWith "VXT0059" (body "while (flag) { int r = match (left) { 1 -> { continue; 1 }, _ -> 2 }; } return 0;")
        )
    ,
        ( "a loop inside a value block may still be left"
        , accepted (body "int r = if (flag) { int n = 0; while (n < left) { n += 1; if (n > 3) { break; } } n } else { 2 }; return r;")
        )
    , ("a guard that returns is accepted", accepted (body "guard (left > 0) else { return 0; } return left;"))
    , ("a guard condition must be bool or numeric", rejectedWith "VXT0060" (body "guard (\"s\") else { return 0; } return 1;"))
    ,
        ( "the else block of a guard must leave"
        , rejectedWith "VXT0061" (body "guard (flag) else { int unused = Twice(left); } return 0;")
        )
    , ("an empty guard block does not leave", rejectedWith "VXT0061" (body "guard (flag) else { } return 0;"))
    ,
        ( "a guard block that leaves on both branches is accepted"
        , accepted (body "guard (flag) else { if (other) { return 1; } else { return 2; } } return 0;")
        )
    ,
        ( "a guard block that leaves on one branch only is rejected"
        , rejectedWith "VXT0061" (body "guard (flag) else { if (other) { return 1; } } return 0;")
        )
    ,
        ( "a guard may leave a loop"
        , accepted (body "int n = 0; while (true) { guard (n < left) else { break; } n += 1; } return n;")
        )
    , ("a guard cannot break outside a loop", rejectedWith "VXT0025" (body "guard (flag) else { break; } return 0;"))
    , ("a guard block may end with a nested block that leaves", accepted (body "guard (flag) else { { return 1; } } return 0;"))
    , ("a nested block sees the names declared before it", accepted (body "int a = left; { a += 1; } return a;"))
    ,
        ( "a name declared in a nested block ends with the block"
        , not (accepted (body "{ int inner = left; } return inner;"))
        )
    ,
        ( "sibling blocks may declare the same name"
        , accepted (body "int total = 0; { int part = left; total += part; } { int part = right; total += part; } return total;")
        )
    ,
        ( "a nested block cannot redeclare a name that is in scope"
        , rejectedWith "VXR0003" (body "int a = left; { int a = right; } return a;")
        )
    ,
        ( "a return in a nested block is checked against the function type"
        , rejectedWith "VXT0005" (body "{ return flag; } return 0;")
        )
    , ("a break in a nested block needs a loop", rejectedWith "VXT0025" (body "{ break; } return 0;"))
    ,
        ( "a break value in a nested block supplies the loop value"
        , accepted (body "int r = while (true) { { break left; } }; return r;")
        )
    ]

-- ---------------------------------------------------------------- lowering

loweringTests :: [(String, Bool)]
loweringTests =
    [ ("a match expression lowers to a subject, a result slot and nested tests", matchExpressionLowers)
    , ("a statement match lowers without a result slot", matchStatementLowers)
    , ("a catch-all arm ends the chain without a test", catchAllEndsChain)
    , ("patterns for several subjects are tested together", severalSubjectsLower)
    , ("a guard after a literal is decided in its own slot", guardedLiteralLowers)
    , ("a guard on a catch-all is the test itself", guardedCatchAllLowers)
    , ("a pattern binding is bound to the subject before its guard", bindingLowers)
    , ("an if expression over pure blocks is one lazy conditional", pureIfExpressionLowers)
    , ("an if expression whose block has statements selects into a slot", ifExpressionWithStatementsLowers)
    , ("a guard runs its block when the condition is false", guardLowers)
    , ("the statements of a nested block join the enclosing sequence", nestedBlockLowers)
    , ("a match within the nesting bound has no taken slot", not (usesTakenSlot (wideMatch 16)))
    , ("a match beyond the nesting bound records the taken arm in a slot", usesTakenSlot (wideMatch 17))
    , ("a wide match nests no deeper than one group", all ((<= 18) . nestingOf . wideMatch) [17, 40, 200])
    , ("a wide match is split into groups in one sequence", wideMatchIsGrouped)
    ]

-- | A match expression with the given number of literal arms and a catch-all.
wideMatch :: Int -> String
wideMatch count =
    body
        ( "return match (left) { "
            ++ concat [show index ++ " -> " ++ show (index * 3 + 1) ++ ", " | index <- [0 .. count - 2]]
            ++ "_ -> 0 };"
        )

usesTakenSlot :: String -> Bool
usesTakenSlot text = case loweredBody text of
    Just statements -> or [generated "$taken" name | CoreBind (CoreBinding name _ _ _) <- statements]
    Nothing -> False

-- | The deepest nesting of conditional statements in the lowered body.
nestingOf :: String -> Int
nestingOf text = maybe 0 depth (loweredBody text)
    where
        depth :: [CoreStatement] -> Int
        depth statements = maximum (0 : map statementDepth statements)
        statementDepth statement = case statement of
            CoreIf _ whenTrue whenFalse -> 1 + max (depth whenTrue) (depth whenFalse)
            _ -> 0

-- Forty arms are three groups: the first chain, then two guarded groups.
wideMatchIsGrouped :: Bool
wideMatchIsGrouped = case loweredBody (wideMatch 40) of
    Just
        [ CoreBind (CoreBinding subject _ False _)
            , CoreBind (CoreBinding slot _ True _)
            , CoreBind (CoreBinding taken _ True (CoreLiteral (CoreBoolean False) _))
            , CoreIf _ [CoreAssign firstTaken _, CoreAssign firstStore _] _
            , CoreIf (CoreVariable secondTest _) [] [CoreIf {}]
            , CoreIf (CoreVariable thirdTest _) [] [CoreIf {}]
            , CoreReturn (CoreVariable result _)
            ] ->
        generated "$subject" subject
            && generated "$matched" slot
            && generated "$taken" taken
            && firstTaken == taken
            && firstStore == slot
            && secondTest == taken
            && thirdTest == taken
            && result == slot
    _ -> False

loweredBody :: String -> Maybe [CoreStatement]
loweredBody text = case compileSource text of
    Right artifacts -> functionBody (artifactCore artifacts)
    Left _ -> Nothing

functionBody :: CoreModule -> Maybe [CoreStatement]
functionBody moduleValue =
    case [ coreFunctionBody function
         | function <- coreModuleFunctions moduleValue
         , spelling (coreFunctionName function) == "Evaluate"
         ] of
        statements : _ -> Just statements
        [] -> Nothing

spelling :: ResolvedName -> String
spelling = identifierText . resolvedSpelling

generated :: String -> ResolvedName -> Bool
generated prefix name = prefix `isPrefixOf` spelling name

-- | Whether an expression compares the given local with the given integer.
comparesWith :: ResolvedName -> Integer -> CoreExpression -> Bool
comparesWith subject value expression = case expression of
    CorePrimitive CoreEqual [CoreVariable name _, CoreLiteral (CoreInteger literal) _] _ -> name == subject && literal == value
    _ -> False

matchExpressionLowers :: Bool
matchExpressionLowers = case loweredBody (body "return match (left) { 1 -> 10, 2 -> 20, _ -> 30 };") of
    Just
        [ CoreBind (CoreBinding subject _ False (CoreVariable source _))
            , CoreBind (CoreBinding slot _ True _)
            , CoreIf first [CoreAssign firstStore _] [CoreIf second [CoreAssign secondStore _] [CoreAssign lastStore _]]
            , CoreReturn (CoreVariable result _)
            ] ->
        generated "$subject" subject
            && spelling source == "left"
            && generated "$matched" slot
            && comparesWith subject 1 first
            && comparesWith subject 2 second
            && all (== slot) [firstStore, secondStore, lastStore, result]
    _ -> False

matchStatementLowers :: Bool
matchStatementLowers = case loweredBody (body "match (left) { 1 -> { return 1; }, 2 -> { return 2; } } return 0;") of
    Just [CoreBind (CoreBinding subject _ False _), CoreIf first [CoreReturn _] [CoreIf second [CoreReturn _] []], CoreReturn _] ->
        generated "$subject" subject && comparesWith subject 1 first && comparesWith subject 2 second
    _ -> False

catchAllEndsChain :: Bool
catchAllEndsChain = case loweredBody (body "match (left) { _ -> { return 7; } } return 0;") of
    Just [CoreBind (CoreBinding subject _ False _), CoreReturn _, CoreReturn _] -> generated "$subject" subject
    _ -> False

severalSubjectsLower :: Bool
severalSubjectsLower = case loweredBody (body "match (left), (right) { (1), (2) -> { return 1; }, (_), (3) -> { return 2; } } return 0;") of
    Just
        [ CoreBind (CoreBinding first _ False _)
            , CoreBind (CoreBinding second _ False _)
            , CoreIf (CorePrimitive CoreLogicalAnd [firstTest, secondTest] _) [CoreReturn _] [CoreIf laterTest [CoreReturn _] []]
            , CoreReturn _
            ] ->
        comparesWith first 1 firstTest && comparesWith second 2 secondTest && comparesWith second 3 laterTest
    _ -> False

guardedLiteralLowers :: Bool
guardedLiteralLowers = case loweredBody (body "match (left) { 1 if flag -> { return 1; } } return 0;") of
    Just
        [ CoreBind (CoreBinding subject _ False _)
            , CoreBind (CoreBinding decision _ True (CoreLiteral (CoreBoolean False) _))
            , CoreIf test [CoreIf (CoreVariable guard _) [CoreAssign decided _] []] []
            , CoreIf (CoreVariable taken _) [CoreReturn _] []
            , CoreReturn _
            ] ->
        generated "$accepted" decision
            && comparesWith subject 1 test
            && spelling guard == "flag"
            && decided == decision
            && taken == decision
    _ -> False

guardedCatchAllLowers :: Bool
guardedCatchAllLowers = case loweredBody (body "match (left) { _ if flag -> { return 1; } } return 0;") of
    Just [CoreBind _, CoreIf (CoreVariable guard _) [CoreReturn _] [], CoreReturn _] -> spelling guard == "flag"
    _ -> False

bindingLowers :: Bool
bindingLowers = case loweredBody (body "match (left) { int value if value > 3 -> { return value; } } return 0;") of
    Just
        [ CoreBind (CoreBinding subject _ False _)
            , CoreBind (CoreBinding bound _ False (CoreVariable source _))
            , CoreIf (CorePrimitive CoreGreaterThan [CoreVariable tested _, _] _) [CoreReturn (CoreVariable returned _)] []
            , CoreReturn _
            ] ->
        spelling bound == "value" && source == subject && tested == bound && returned == bound
    _ -> False

pureIfExpressionLowers :: Bool
pureIfExpressionLowers = case loweredBody (body "return if (flag) { left } else { right };") of
    Just [CoreReturn (CoreConditional (CoreVariable test _) (CoreVariable first _) (CoreVariable second _) _)] ->
        map spelling [test, first, second] == ["flag", "left", "right"]
    _ -> False

ifExpressionWithStatementsLowers :: Bool
ifExpressionWithStatementsLowers = case loweredBody (body "return if (flag) { int doubled = left * 2; doubled } else { right };") of
    Just
        [ CoreBind (CoreBinding slot _ True _)
            , CoreIf _ [CoreBind (CoreBinding local _ _ _), CoreAssign first (CoreVariable stored _)] [CoreAssign second _]
            , CoreReturn (CoreVariable result _)
            ] ->
        generated "$selected" slot && spelling local == "doubled" && stored == local && all (== slot) [first, second, result]
    _ -> False

guardLowers :: Bool
guardLowers = case loweredBody (body "guard (flag) else { return 0; } return 1;") of
    Just [CoreIf (CoreVariable condition _) [] [CoreReturn _], CoreReturn _] -> spelling condition == "flag"
    _ -> False

nestedBlockLowers :: Bool
nestedBlockLowers = case loweredBody (body "int a = left; { int b = a + 1; a = b; } return a;") of
    Just [CoreBind (CoreBinding outer _ _ _), CoreBind (CoreBinding inner _ _ _), CoreAssign stored _, CoreReturn _] ->
        spelling outer == "a" && spelling inner == "b" && stored == outer
    _ -> False

-- ------------------------------------------------------------- evaluation

-- | Source bodies, the arguments to run them on, and the value each run must return.
evaluationCases :: [(String, [((Bool, Bool, Integer, Integer), Integer)])]
evaluationCases =
    [ ("return match (left) { 1 -> 10, 2 -> 20, _ -> 30 };", [(plain 1 0, 10), (plain 2 0, 20), (plain 5 0, 30)])
    ,
        ( "int r = 0; match (left) { 1 -> { r = 10; }, 2 -> { r = 20; } } return r;"
        , [(plain 1 0, 10), (plain 2 0, 20), (plain 7 0, 0)]
        )
    ,
        ( "return match (left) { int n if n > 10 -> n * 2, int n if n > 5 -> n + 1, _ -> 0 };"
        , [(plain 20 0, 40), (plain 7 0, 8), (plain 3 0, 0)]
        )
    ,
        ( "return match (left), (right) { (1), (1) -> 11, (1), (_) -> 10, (_), (1) -> 1, (_), (_) -> 0 };"
        , [(plain 1 1, 11), (plain 1 5, 10), (plain 4 1, 1), (plain 4 4, 0)]
        )
    , ("return match (flag) { true -> 1, false -> 2 };", [(flags True False 0 0, 1), (flags False False 0 0, 2)])
    , -- Two bool subjects covered without a catch-all arm.
        ( "return match (flag), (other) { (true), (_) -> 1, (false), (true) -> 2, (false), (false) -> 3 };"
        , [(flags True True 0 0, 1), (flags True False 0 0, 1), (flags False True 0 0, 2), (flags False False 0 0, 3)]
        )
    ,
        ( "return match (flag), (other) { (true), (true) -> 3, (true), (_) -> 2, (_), (true) -> 1, (_), (_) -> 0 };"
        , [(flags True True 0 0, 3), (flags True False 0 0, 2), (flags False True 0 0, 1), (flags False False 0 0, 0)]
        )
    , -- The subject is evaluated once, whichever arm accepts.
        ( "int n = left; int r = match (n += 1) { 1 -> 100, 2 -> 200, _ -> 300 }; return r + n;"
        , [(plain 0 0, 101), (plain 1 0, 202), (plain 5 0, 306)]
        )
    , -- Subjects are evaluated left to right, before any arm is tested.
        ( "int n = left; return match (n += 1), (n * 10) { (2), (20) -> 1, (_), (_) -> 0 };"
        , [(plain 1 0, 1), (plain 2 0, 0)]
        )
    , -- A guard runs only when the patterns of its arm accept.
        ( "int calls = 0; int r = match (left) { 1 if (calls += 1) > 0 -> 10, 2 if (calls += 10) > 0 -> 20, _ -> 30 }; return r * 100 + calls;"
        , [(plain 1 0, 1001), (plain 2 0, 2010), (plain 3 0, 3000)]
        )
    , -- A guard that is false passes the value on to the arms after it.
        ( "return match (left) { 1 if flag -> 1, 1 -> 2, _ -> 3 };"
        , [(flags True False 1 0, 1), (flags False False 1 0, 2), (flags True False 9 0, 3)]
        )
    , -- A numeric guard is tested in Boolean context.
        ( "return match (left) { 1 if right -> 1, _ -> 0 };"
        , [(plain 1 5, 1), (plain 1 0, 0), (plain 2 5, 0)]
        )
    , -- Only the body of the accepting arm runs.
        ( "int n = 0; int r = match (left) { 1 -> (n += 1), 2 -> (n += 10), _ -> (n += 100) }; return r * 1000 + n;"
        , [(plain 1 0, 1001), (plain 2 0, 10010), (plain 3 0, 100100)]
        )
    , ("return match (Twice(left)) { 4 -> 1, 6 -> 2, _ -> 0 };", [(plain 2 0, 1), (plain 3 0, 2), (plain 4 0, 0)])
    , ("long wide = 5; return match (wide) { 5 -> 1, _ -> 0 };", [(plain 0 0, 1)])
    , ("long wide = match (left) { 1 -> 10, _ -> 20 }; return wide > 15 ? 1 : 0;", [(plain 1 0, 0), (plain 2 0, 1)])
    , ("int r = 0; match (left) { 1 -> r = 5, _ -> r = Twice(left) } return r;", [(plain 1 0, 5), (plain 4 0, 8)])
    ,
        ( "return match (left) { 1 -> if (flag) { 5 } else { 6 }, _ -> match (right) { 0 -> 7, _ -> 8 } };"
        , [(flags True False 1 0, 5), (flags False False 1 0, 6), (plain 2 0, 7), (plain 2 3, 8)]
        )
    ,
        ( "return match (left) { 1 -> { int t = right * 2; t + 1 }, _ -> { int t = right * 3; t - 1 } };"
        , [(plain 1 4, 9), (plain 2 4, 11)]
        )
    , -- A statement arm may continue or leave the enclosing loop.
        ( "int total = 0; for (int i = 0; i < left; i++) { match (i) { 2 -> { continue; }, 5 -> { break; }, _ -> { total += i; } } } return total;"
        , [(plain 10 0, 8), (plain 3 0, 1), (plain 0 0, 0)]
        )
    , -- A statement arm may supply the value of an enclosing loop expression.
        ( "int n = 0; int r = while (true) { n += 1; match (n) { 4 -> { break n * 10; }, _ -> { } } }; return r;"
        , [(plain 0 0, 40)]
        )
    , ("match (left) { 1 -> { return 100; }, _ -> { } } return 5;", [(plain 1 0, 100), (plain 2 0, 5)])
    , ("match (left) { } return 5;", [(plain 1 0, 5)])
    , ("int r = if (left > right) { left } else { right }; return r;", [(plain 3 5, 5), (plain 9 2, 9)])
    ,
        ( "int r = if (flag) { int t = left * 2; t + 1 } else { int t = right * 3; t - 1 }; return r;"
        , [(flags True False 4 0, 9), (flags False False 0 5, 14)]
        )
    , -- Only the selected block of an if expression runs.
        ( "int n = 0; int r = if (flag) { n += 1; 10 } else { n += 100; 20 }; return r * 1000 + n;"
        , [(flags True False 0 0, 10001), (flags False False 0 0, 20100)]
        )
    ,
        ( "return (if (flag) { left } else { right }) + (if (other) { 100 } else { 200 });"
        , [(flags True True 1 2, 101), (flags False False 1 2, 202)]
        )
    , ("guard (left > 0) else { return 0 - 1; } return left * 2;", [(plain 3 0, 6), (plain 0 0, -1)])
    ,
        ( "int total = 0; for (int i = 0; i < left; i++) { guard (i % 2 == 0) else { continue; } total += i; } return total;"
        , [(plain 6 0, 6), (plain 1 0, 0)]
        )
    ,
        ( "int n = 0; while (true) { guard (n < left) else { break; } n += 1; } return n;"
        , [(plain 4 0, 4), (plain 0 0, 0)]
        )
    ,
        ( "int total = 0; { int part = left * 2; total += part; } { int part = right * 3; total += part; } return total;"
        , [(plain 2 3, 13), (plain 0 0, 0)]
        )
    ,
        ( "int n = 0; while (true) { { n += 1; if (n > left) { break; } } } { { return n * 10; } }"
        , [(plain 3 0, 40), (plain 0 0, 10)]
        )
    ,
        ( "int total = 0; for (int i = 0; i < left; i++) { { if (i == 1) { continue; } } { int step = i * 2; total += step; } } return total;"
        , [(plain 4 0, 10), (plain 1 0, 0)]
        )
    , -- A guard condition with a store runs once, before the block decision.
        ( "int n = left; guard ((n += 1) > 3) else { return n * 10; } return n;"
        , [(plain 5 0, 6), (plain 1 0, 20)]
        )
    ]

plain :: Integer -> Integer -> (Bool, Bool, Integer, Integer)
plain = flags False False

flags :: Bool -> Bool -> Integer -> Integer -> (Bool, Bool, Integer, Integer)
flags flag other left right = (flag, other, left, right)

evaluationTests :: [(String, Bool)]
evaluationTests =
    concat
        [ [ ("unoptimized Core computes " ++ label, runs artifactCore statements arguments expected)
          , ("optimized Core computes " ++ label, runs artifactOptimizedCore statements arguments expected)
          ]
        | (statements, runsOfCase) <- evaluationCases
        , (arguments, expected) <- runsOfCase
        , let label = show expected ++ " for " ++ show arguments ++ ": " ++ statements
        ]

runs :: (FrontendArtifacts -> CoreModule) -> String -> (Bool, Bool, Integer, Integer) -> Integer -> Bool
runs select statements (flag, other, left, right) expected = case compileSource (body statements) of
    Right artifacts ->
        runFunction
            (select artifacts)
            "Evaluate"
            [BooleanValue flag, BooleanValue other, IntegerValue left, IntegerValue right]
            == Just (IntegerValue expected)
    Left _ -> False

-- ---------------------------------------------------------------- pipeline

pipelineTests :: [(String, Bool)]
pipelineTests =
    [ ("every lowered case verifies as Core", all (verifies artifactCore) (map fst evaluationCases))
    , ("every optimized case verifies as Core", all (verifies artifactOptimizedCore) (map fst evaluationCases))
    , ("every case verifies as CorePrep", all corePrepVerifies (map fst evaluationCases))
    , ("Core wire round-trips every lowered case", all wireRoundTrips (map fst evaluationCases))
    , ("a template member keeps its match when instantiated", templateInstantiates)
    , ("a call inside a match arm is reachable from the template member", templateReachesArmCalls)
    , ("a closure body may contain a match and a guard", accepted closureSource)
    ]

verifies :: (FrontendArtifacts -> CoreModule) -> String -> Bool
verifies select statements = case compileSource (body statements) of
    Right artifacts -> verifyCore (select artifacts) == Right (select artifacts)
    Left _ -> False

corePrepVerifies :: String -> Bool
corePrepVerifies statements = case compileSource (body statements) of
    Right artifacts -> verifyCorePrep (artifactCorePrep artifacts) == Right (artifactCorePrep artifacts)
    Left _ -> False

wireRoundTrips :: String -> Bool
wireRoundTrips statements = case compileSource (body statements) of
    Right artifacts ->
        let core = artifactCore artifacts
         in (encodeCore defaultCoreWireLimits core >>= decodeCore defaultCoreWireLimits) == Right core
    Left _ -> False

templateSource :: String
templateSource =
    unwords
        [ "template<typename T> class Box {"
        , "int Entry(_ int seed) {"
        , "guard (seed > 0) else { return Fallback(); }"
        , "return match (seed) { 1 -> Step(seed), int other if other > 9 -> Scale(other), _ -> if (seed > 5) { Step(seed) } else { 0 } };"
        , "}"
        , "int Step(_ int value) { return value + 4; }"
        , "int Scale(_ int value) { return value * 2; }"
        , "int Fallback() { return 0; }"
        , "int Unused() { return 9; }"
        , "}"
        ]

templatePlan :: Maybe TemplateSpecializationPlan
templatePlan = case analyzeSemantics (CompilerInput "branching-template.vxs" templateSource) of
    Right artifacts ->
        either
            (const Nothing)
            Just
            ( planTemplateSpecializations
                defaultTemplateSpecializationLimits
                (semanticTypedAST artifacts)
                [ TemplateSpecializationDemand
                    (TemplateApplication (QualifiedName [Identifier "Box"]) [TypeTemplateArgument stringType])
                    (TemplateMemberDemand [Identifier "Entry"])
                    "branching-template"
                ]
            )
    Left _ -> Nothing

specializedMembers :: [Declaration ResolvedName Type]
specializedMembers = case templatePlan of
    Just plan ->
        concat
            [ members
            | TypeDeclaration {typeMembers = members} <-
                map templateSpecializationDeclaration (plannedTemplateSpecializations plan)
            ]
    Nothing -> []

specializedNames :: [String]
specializedNames = map (identifierText . resolvedSpelling . declarationName) specializedMembers

templateInstantiates :: Bool
templateInstantiates =
    all (`isInfixOf` show specializedMembers) ["MatchExpression", "GuardStatement", "BlockExpression"]

templateReachesArmCalls :: Bool
templateReachesArmCalls =
    all (`elem` specializedNames) ["Entry", "Step", "Scale", "Fallback"] && "Unused" `notElem` specializedNames

closureSource :: String
closureSource =
    unlines
        [ "class Program {"
        , "    public static int Run(_ int seed) {"
        , "        auto pick = \\(int value) -> { guard (value > 0) else { return 0; } return match (value) { 1 -> 10, int other -> other * 2 }; };"
        , "        return pick(seed);"
        , "    }"
        , "}"
        ]
