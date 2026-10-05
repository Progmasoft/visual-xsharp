-- SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
-- SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.1

{- | Tests for classic enums.

A classic enum is a value type whose members are named integers. The tests
pin its declaration, the numbering of its members, the one comparison its
values have, the absence of any conversion to or from an integer, and
@match@ over it: a case pattern names a member, two members with one value
are one case, and a match that names every value needs no catch-all arm.

The expected values are written by hand. Every accepted program runs in the
reference evaluator on the unoptimized and on the optimized Core and is
verified as Core and as CorePrep; an enum must reach Core as its underlying
integer type and never as a named type.
-}
module EnumTests (enumTests) where

import CoreInterpreter
import Data.List (isInfixOf)
import Visual.XSharp.AST
import Visual.XSharp.Compiler
import Visual.XSharp.Core
import Visual.XSharp.Core.CorePrep.Verifier
import Visual.XSharp.Core.Verifier
import Visual.XSharp.Diagnostic
import Visual.XSharp.Lexer
import Visual.XSharp.Parser

enumTests :: [(String, Bool)]
enumTests =
    parserTests
        ++ concat
            [ [ ("unoptimized Core computes " ++ label, runs artifactCore statements arguments expected)
              , ("optimized Core computes " ++ label, runs artifactOptimizedCore statements arguments expected)
              ]
            | (statements, runsOfCase) <- evaluationCases
            , (arguments, expected) <- runsOfCase
            , let label = show expected ++ " for " ++ show arguments ++ ": " ++ statements
            ]
        ++ [ ("every enum program verifies as Core and as CorePrep", all (verifies . fst) evaluationCases)
           , ("no enum reaches Core as a named type", all (erased . fst) evaluationCases)
           ]
        ++ [(label, rejectedWith code (program statements)) | (label, code, statements) <- rejectedBodies]
        ++ [(label, rejectedWith code (declarations ++ emptyProgram)) | (label, code, declarations) <- rejectedDeclarations]
        ++ [ ("an enum without members is a declaration", accepted ("enum Empty { }\n" ++ emptyProgram))
           , ("an enum may be declared after the class that follows it in use", accepted (emptyProgram ++ "enum Later { ONE }\n"))
           , ("a negative member value is accepted in a signed type", accepted ("enum Signed { LOW = -5, NEXT }\n" ++ emptyProgram))
           ]

-- ---------------------------------------------------------------- sources

enums :: String
enums =
    unlines
        [ "enum Status { NONE, UNKNOWN = 0, READY }"
        , "enum Level = byte { LOW = 1, MID, HIGH = 10, TOP }"
        ]

-- | A program around the given body of @Evaluate@.
program :: String -> String
program statements =
    "namespace Test;\n"
        ++ enums
        ++ unlines
            [ "class Program {"
            , "    public static Status Pick(_ int v) { if (v > 0) { return Status.READY; } return Status.NONE; }"
            , "    public static int Rank(_ Level l) { return match (l) { .LOW -> 1, .MID -> 2, .HIGH -> 10, .TOP -> 11 }; }"
            , "    public static Level Raise(_ Level l) { return match (l) { .LOW -> Level.MID, .MID -> Level.HIGH, _ -> Level.TOP }; }"
            , "    public static int Evaluate(_ int left, _ int right) {"
            , "        " ++ statements
            , "    }"
            , "}"
            ]

emptyProgram :: String
emptyProgram = "class Program { public static int Evaluate(_ int left, _ int right) { return 0; } }\n"

-- ----------------------------------------------------------------- parser

parserTests :: [(String, Bool)]
parserTests =
    [ ("an enum declaration keeps its members in order", memberNames "enum Direction { NORTH, EAST, SOUTH, WEST, }" == Just ["NORTH", "EAST", "SOUTH", "WEST"])
    , ("a member keeps the value written for it", memberValues "enum Value = int { FIRST = 5, SECOND, THIRD = 10, FOURTH }" == Just [Just 5, Nothing, Just 10, Nothing])
    , ("a member value may be negative", memberValues "enum Signed { LOW = -5 }" == Just [Just (-5)])
    , ("the underlying type is kept", underlying "enum Status = byte { PENDING = 0 }" == Just (Just (ExplicitType (Identifier "byte"))))
    , ("the underlying type may be absent", underlying "enum Status { PENDING }" == Just Nothing)
    , ("a member value must be an integer literal", parseFailsWith "VXP0041" "enum Status { PENDING = 1.5 }")
    , ("a member value is not an expression", parseFailsWith "VXP0041" "enum Status { PENDING = left }")
    , ("enum is a reserved word", not (parses "class enum { }"))
    ]
    where
        -- The underlying type and the members of the first declaration.
        declaration text = case parseSource text of
            Right (ParsedAST (SyntaxTree _ (EnumDeclaration _ _ _ written members : _))) -> Just (written, members)
            _ -> Nothing
        memberNames text = map (identifierText . enumCaseName) . snd <$> declaration text
        memberValues text = map enumCaseValue . snd <$> declaration text
        underlying text = fst <$> declaration text

-- -------------------------------------------------------------- evaluation

-- | Bodies of @Evaluate@, the arguments to run them on, and the value each run must return.
evaluationCases :: [(String, [((Integer, Integer), Integer)])]
evaluationCases =
    [ -- Values of an enum are compared with == and \=.
      ("Status s = Pick(left); return s == Status.READY ? 1 : 2;", [((3, 0), 1), ((0, 0), 2)])
    , ("Status s = Pick(left); return s \\= Status.NONE ? 1 : 2;", [((3, 0), 1), ((0, 0), 2)])
    , -- Two members with one value are equal.
      ("return Status.NONE == Status.UNKNOWN ? 1 : 2;", [((0, 0), 1)])
    , -- Members are numbered from zero; one without a value follows the one before it.
      ("return Rank(Level.LOW) + Rank(Level.MID) * 10 + Rank(Level.HIGH) * 100 + Rank(Level.TOP) * 1000;", [((0, 0), 12021)])
    , -- A match that names every value needs no catch-all arm.
      ("return match (Pick(left)) { .NONE -> 10, .READY -> 20 };", [((3, 0), 20), ((0, 0), 10)])
    , -- A member is named through either of its names.
      ("return match (Pick(left)) { .UNKNOWN -> 10, .READY -> 20 };", [((0, 0), 10)])
    , ("Level l = Level.MID; return match (l) { .LOW -> 1, _ -> 9 };", [((0, 0), 9)])
    , -- A guard does not count towards completeness, and a catch-all is then needed.
      ("return match (Pick(left)) { .READY if (right > 0) -> 1, .READY -> 2, .NONE -> 3 };", [((1, 1), 1), ((1, 0), 2), ((0, 5), 3)])
    , -- An enum is assigned, passed, returned and yielded by a match.
      ("Status s = Status.NONE; s = Status.READY; return s == Status.READY ? 7 : 8;", [((0, 0), 7)])
    , ("return Rank(Raise(Raise(Level.LOW))) * 100 + Rank(Raise(Level.HIGH));", [((0, 0), 1011)])
    , ("Status s = match (left) { 1 -> Status.READY, _ -> Status.NONE }; return s == Status.READY ? 1 : 0;", [((1, 0), 1), ((2, 0), 0)])
    , -- Two subjects with closed sets of values are complete together.
        ( "return match (Pick(left)), (right > 0) { (.NONE), (true) -> 1, (.NONE), (false) -> 2, (.READY), (true) -> 3, (.READY), (false) -> 4 };"
        , [((0, 1), 1), ((0, 0), 2), ((1, 1), 3), ((1, 0), 4)]
        )
    , -- An enum in a callable and in a loop.
      ("auto f = \\(Status s) -> s == Status.READY ? 5 : 6; return f(Pick(left)) * 10 + f(Status.NONE);", [((2, 0), 56)])
    , ("Level l = Level.LOW; int n = 0; while (l \\= Level.TOP) { l = Raise(l); n += 1; } return n;", [((0, 0), 3)])
    ]

-- ----------------------------------------------------------------- rejects

rejectedBodies :: [(String, String, String)]
rejectedBodies =
    [ ("a match over an enum must name every value", "VXT0052", "return match (Pick(left)) { .NONE -> 10 };")
    , ("a guarded arm does not complete a match over an enum", "VXT0052", "return match (Pick(left)) { .NONE -> 1, .READY if (right > 0) -> 2 };")
    , ("a second arm for the same value can never be selected", "VXT0053", "return match (Pick(left)) { .NONE -> 10, .UNKNOWN -> 11, .READY -> 20 };")
    , ("an enum has only the members it declares", "VXT0064", "Status s = Status.MISSING; return 0;")
    , ("a case pattern names a member of the subject's enum", "VXT0064", "return match (Pick(left)) { .LOW -> 1, _ -> 2 };")
    , ("a case pattern needs a subject of an enum type", "VXT0056", "return match (left) { .NONE -> 1, _ -> 2 };")
    , ("values of an enum are not ordered", "VXT0065", "return Status.NONE < Status.READY ? 1 : 2;")
    , ("values of an enum are not added", "VXT0065", "Status s = Status.NONE + Status.READY; return 0;")
    , ("values of different enums are not compared", "VXT0065", "return Status.NONE == Level.LOW ? 1 : 2;")
    , ("a value of an enum is not compared with an integer", "VXT0065", "return Status.NONE == 0 ? 1 : 2;")
    , ("a value of an enum is not an integer", "VXT0002", "int x = Status.READY; return x;")
    , ("an integer is not a value of an enum", "VXT0002", "Status s = 1; return 0;")
    , ("a value of an enum is not a condition", "VXT0006", "if (Status.READY) { return 1; } return 2;")
    , ("a value of one enum is not a value of another", "VXT0002", "Level l = Status.NONE; return 0;")
    ]

rejectedDeclarations :: [(String, String, String)]
rejectedDeclarations =
    [ ("the underlying type of an enum is an integer type", "VXT0066", "enum A = float { X }\n")
    , ("a member is named once", "VXT0067", "enum A { X, X }\n")
    , ("a member value fits the underlying type", "VXT0068", "enum A = byte { X = 300 }\n")
    , ("a numbered member fits the underlying type", "VXT0068", "enum A = byte { X = 127, Y }\n")
    , ("an unsigned underlying type has no negative member", "VXT0068", "enum A = ubyte { X = -1 }\n")
    , ("an enum is declared once", "VXR0001", "enum A { X }\nenum A { Y }\n")
    , ("an enum and a class do not share a name", "VXR0001", "enum A { X }\nclass A { }\n")
    ]

-- ----------------------------------------------------------------- helpers

parseSource :: String -> Either [Diagnostic] ParsedAST
parseSource text = do
    tokens <- runLexer defaultLexer (LexerInput "enum.vxs" text)
    runParser defaultParser (ParserInput "enum.vxs" tokens)

parses :: String -> Bool
parses = either (const False) (const True) . parseSource

parseFailsWith :: String -> String -> Bool
parseFailsWith code text = case parseSource text of
    Left problems -> any ((== code) . diagnosticCode) problems
    Right _ -> False

compileSource :: String -> Either [Diagnostic] FrontendArtifacts
compileSource text = compileToCorePrep (CompilerInput "enum.vxs" text)

accepted :: String -> Bool
accepted = either (const False) (const True) . compileSource

rejectedWith :: String -> String -> Bool
rejectedWith code text = case compileSource text of
    Left problems -> any ((== code) . diagnosticCode) problems
    Right _ -> False

runs :: (FrontendArtifacts -> CoreModule) -> String -> (Integer, Integer) -> Integer -> Bool
runs select statements (left, right) expected = case compileSource (program statements) of
    Right artifacts ->
        runFunction (select artifacts) "Evaluate" [IntegerValue left, IntegerValue right] == Just (IntegerValue expected)
    Left _ -> False

verifies :: String -> Bool
verifies statements = case compileSource (program statements) of
    Right artifacts ->
        verifyCore (artifactCore artifacts) == Right (artifactCore artifacts)
            && verifyCore (artifactOptimizedCore artifacts) == Right (artifactOptimizedCore artifacts)
            && verifyCorePrep (artifactCorePrep artifacts) == Right (artifactCorePrep artifacts)
    Left _ -> False

{- | Whether the Core of a program mentions no enum. The reserved root of an
enum type cannot be spelled in source, so its absence from the printed Core
shows that every enum was lowered to its underlying type.
-}
erased :: String -> Bool
erased statements = case compileSource (program statements) of
    Right artifacts -> not ("Identifier \"enum\"" `isInfixOf` show (artifactCore artifacts))
    Left _ -> False
