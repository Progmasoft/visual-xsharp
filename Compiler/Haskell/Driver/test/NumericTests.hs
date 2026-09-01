-- SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
-- SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.0

module NumericTests (numericTests) where

import Data.List (isInfixOf)
import Visual.XSharp.AST
import Visual.XSharp.BuiltinTypes
import Visual.XSharp.CharacterLiteral
import Visual.XSharp.Compiler
import Visual.XSharp.Core
import Visual.XSharp.Diagnostic
import Visual.XSharp.FloatingLiteral
import Visual.XSharp.Lexer
import Visual.XSharp.NumericLiteral
import Visual.XSharp.NumericSemantics
import Visual.XSharp.Parser

numericTests :: [(String, Bool)]
numericTests =
    lexicalAcceptanceTests
        ++ lexicalRejectionTests
        ++ parserValueTests
        ++ characterLiteralTests
        ++ floatingLiteralTests
        ++ scalarMetadataTests
        ++ targetBoundaryTests
        ++ numericContextTests
        ++ corePropagationTests
        ++ semanticRuleTests

lexicalAcceptanceTests :: [(String, Bool)]
lexicalAcceptanceTests =
    [ accepts "decimal zero" "0" IntegerToken
    , accepts "decimal digits" "26" IntegerToken
    , accepts "decimal separator" "1'000'000" IntegerToken
    , accepts "hexadecimal lowercase prefix" "0xff" IntegerToken
    , accepts "hexadecimal uppercase prefix" "0XFF" IntegerToken
    , accepts "hexadecimal separator" "0xFF'EE" IntegerToken
    , accepts "binary lowercase prefix" "0b1010" IntegerToken
    , accepts "binary uppercase prefix" "0B1010" IntegerToken
    , accepts "binary separator" "0b1010'1100" IntegerToken
    , accepts "maximum u8 spelling" "255" IntegerToken
    , accepts "maximum i16 spelling" "32'767" IntegerToken
    , accepts "maximum u16 spelling" "65'535" IntegerToken
    , accepts "maximum i32 spelling" "2'147'483'647" IntegerToken
    , accepts "maximum u32 spelling" "4'294'967'295" IntegerToken
    , accepts "maximum i64 spelling" "9'223'372'036'854'775'807" IntegerToken
    , accepts "maximum u64 spelling" "18'446'744'073'709'551'615" IntegerToken
    , accepts "maximum i128 spelling" "170'141'183'460'469'231'731'687'303'715'884'105'727" IntegerToken
    , accepts "maximum u128 spelling" "340'282'366'920'938'463'463'374'607'431'768'211'455" IntegerToken
    ]

lexicalRejectionTests :: [(String, Bool)]
lexicalRejectionTests =
    [ rejectsLexically "octal lowercase prefix" "0o377" "octal"
    , rejectsLexically "octal uppercase prefix" "0O377" "octal"
    , rejectsLexically "hexadecimal without digits" "0x" "at least one digit"
    , rejectsLexically "binary without digits" "0b" "at least one digit"
    , rejectsLexically "binary digit two" "0b2" "not a valid binary digit"
    , rejectsLexically "binary hexadecimal tail" "0bA" "not a valid binary digit"
    , rejectsLexically "hexadecimal letter G" "0xG" "not a valid hexadecimal digit"
    , rejectsLexically "decimal identifier tail" "123abc" "not a valid decimal digit"
    , rejectsLexically "leading radix separator" "0x'FF" "cannot begin"
    , rejectsLexically "trailing radix separator" "0b1010'" "cannot end"
    , rejectsLexically "consecutive decimal separators" "1''000" "consecutive"
    , rejectsLexically "consecutive hexadecimal separators" "0xAB''CD" "consecutive"
    , rejectsLexically "underscore is not a separator" "1_000" "not a valid decimal digit"
    , rejectsLexically "separator before invalid digit" "0b1'2" "between two valid binary digits"
    , rejectsLexically "separator after invalid digit" "0xG'0" "between two valid hexadecimal digits"
    ]

parserValueTests :: [(String, Bool)]
parserValueTests =
    [ parsedValue "decimal parser value" "26" 26
    , parsedValue "separated decimal parser value" "1'000'000" 1000000
    , parsedValue "hexadecimal parser value" "0xFF" 255
    , parsedValue "separated hexadecimal parser value" "0xFF'EE" 65518
    , parsedValue "binary parser value" "0b1010" 10
    , parsedValue "separated binary parser value" "0b1010'1100" 172
    , directParse "direct decimal radix" "12'345" DecimalRadix 12345
    , directParse "direct hexadecimal radix" "0xCA'FE" HexadecimalRadix 51966
    , directParse "direct binary radix" "0b1101'0010" BinaryRadix 210
    ]

characterLiteralTests :: [(String, Bool)]
characterLiteralTests =
    [ characterValue "single ASCII character" "'A'" 0x00000041
    , characterValue "two packed characters" "'AB'" 0x00004142
    , characterValue "three packed characters" "'ABC'" 0x00414243
    , characterValue "four packed characters" "'ABCD'" 0x41424344
    , characterValue "escaped newline participates in packing" "'A\\n'" 0x0000410a
    , characterValue "escaped quote" "'\\\''" 0x27
    , characterValue "escaped double quote" "'\\\"'" 0x22
    , characterValue "escaped backslash" "'\\\\'" 0x5c
    , characterValue "escaped null" "'\\0'" 0x00
    , characterValue "escaped alert" "'\\a'" 0x07
    , characterValue "escaped backspace" "'\\b'" 0x08
    , characterValue "escaped escape" "'\\e'" 0x1b
    , characterValue "escaped form feed" "'\\f'" 0x0c
    , characterValue "escaped carriage return" "'\\r'" 0x0d
    , characterValue "escaped horizontal tab" "'\\t'" 0x09
    , characterValue "escaped vertical tab" "'\\v'" 0x0b
    , characterValue "variable hexadecimal escape" "'\\x41'" 0x41
    , characterValue "four digit Unicode escape" "'\\u0041'" 0x41
    , characterValue "eight digit Unicode escape" "'\\U00000041'" 0x41
    , characterValue "Unicode scalar occupies required bytes" "'λ'" 0x03bb
    , characterFailure "empty character literal" "''" "at least one"
    , characterFailure "five ASCII values overflow u32" "'ABCDE'" "does not fit u32"
    , characterFailure "unsupported escape" "'\\q'" "unsupported character escape"
    , characterFailure "hexadecimal escape requires digits" "'\\x'" "requires at least one digit"
    , characterFailure "short u escape" "'\\u041'" "requires exactly 4 digits"
    , characterFailure "short U escape" "'\\U000041'" "requires exactly 8 digits"
    , characterFailure "surrogate escape" "'\\uD800'" "not a Unicode scalar"
    , characterFailure "out of range scalar" "'\\U00110000'" "not a Unicode scalar"
    , characterLexFailure "unterminated character literal" "'A" "unterminated"
    , characterLexFailure "physical newline character literal" "'A\nB'" "line break"
    , compiles "character literal targets char" "char letter = 'A';"
    , rejectedWith "character literal does not target int" "int letter = 'A';" "VXT0002"
    , ("character literal reaches Core as u32 payload", coreCharacterValue)
    ]

floatingLiteralTests :: [(String, Bool)]
floatingLiteralTests =
    [ floatingValue "decimal fraction" "1.5" "1.5"
    , floatingValue "lowercase scientific notation" "1e3" "1e3"
    , floatingValue "fraction with exponent" "1.5e2" "1.5e2"
    , floatingValue "uppercase signed exponent" "2E-4" "2E-4"
    , floatingValue "positive exponent sign" "2E+4" "2E+4"
    , floatingValue "separated integer and fraction" "1'000.250'000" "1000.250000"
    , floatingFailure "fraction requires right digits" "1." "both sides"
    , floatingFailure "fraction requires left digits" ".5" "both sides"
    , floatingFailure "exponent requires digits" "1e" "requires decimal digits"
    , floatingFailure "signed exponent requires digits" "1e-" "requires decimal digits"
    , floatingFailure "multiple decimal points" "1.2.3" "more than one decimal point"
    , floatingFailure "multiple exponent markers" "1e2e3" "more than one exponent"
    , floatingFailure "separator cannot touch decimal point on left" "1'.5" "between two digits"
    , floatingFailure "separator cannot touch decimal point on right" "1.'5" "between two digits"
    , floatingFailure "separator is forbidden in exponent" "1.5e1'00" "not allowed"
    , floatingFailure "fundamental suffixes are forbidden" "1.5f" "suffixes are not supported"
    , compiles "untargeted floating literal defaults to float" "auto value = 1.5;"
    , compiles "sfloat target selects f16 semantics" "sfloat value = 1.5;"
    , compiles "lfloat target selects f32 semantics" "lfloat value = 1.5;"
    , compiles "float target selects f64 semantics" "float value = 1.5;"
    , compiles "double target selects f128 semantics" "double value = 1.5;"
    , rejectedWith "integer target rejects floating literal" "int value = 1.5;" "VXT0002"
    , ("floating spelling reaches Core without host rounding", coreFloatingSpelling)
    ]

scalarMetadataTests :: [(String, Bool)]
scalarMetadataTests =
    [ scalarMetadata "char is u32 storage" CharacterScalar CharacterFamily 32 False
    , scalarMetadata "bool is u8 storage" BooleanScalar BooleanFamily 8 False
    , scalarMetadata "byte is i8" ByteScalar SignedIntegerFamily 8 True
    , scalarMetadata "short is i16" ShortScalar SignedIntegerFamily 16 True
    , scalarMetadata "long is i32" LongScalar SignedIntegerFamily 32 True
    , scalarMetadata "int is i64" IntScalar SignedIntegerFamily 64 True
    , scalarMetadata "longint is i128" LongIntScalar SignedIntegerFamily 128 True
    , scalarMetadata "ubyte is u8" UByteScalar UnsignedIntegerFamily 8 False
    , scalarMetadata "ushort is u16" UShortScalar UnsignedIntegerFamily 16 False
    , scalarMetadata "ulong is u32" ULongScalar UnsignedIntegerFamily 32 False
    , scalarMetadata "uint is u64" UIntScalar UnsignedIntegerFamily 64 False
    , scalarMetadata "ulongint is u128" ULongIntScalar UnsignedIntegerFamily 128 False
    , scalarMetadata "sfloat is f16" SFloatScalar FloatingFamily 16 False
    , scalarMetadata "lfloat is f32" LFloatScalar FloatingFamily 32 False
    , scalarMetadata "float is f64" FloatScalar FloatingFamily 64 False
    , scalarMetadata "double is f128" DoubleScalar FloatingFamily 128 False
    , ("default integer is int", defaultIntegerScalar == IntScalar)
    , ("default floating point is float", defaultFloatingScalar == FloatScalar)
    , ("every scalar name round-trips through Type", all scalarRoundTrips scalarTypes)
    , ("every wrapper name round-trips to its scalar", all wrapperRoundTrips scalarTypes)
    , ("signed width rank selects int over long", widerScalarType LongScalar IntScalar == Just IntScalar)
    , ("unsigned width rank selects ulongint", widerScalarType UIntScalar ULongIntScalar == Just ULongIntScalar)
    , ("floating width rank selects double", widerScalarType SFloatScalar DoubleScalar == Just DoubleScalar)
    , ("different scalar families have no wider common scalar", widerScalarType IntScalar UIntScalar == Nothing)
    ]

targetBoundaryTests :: [(String, Bool)]
targetBoundaryTests =
    concat
        [ signedBoundary "byte" ByteScalar (-128) 127
        , signedBoundary "short" ShortScalar (-32768) 32767
        , signedBoundary "long" LongScalar (-2147483648) 2147483647
        , signedBoundary "int" IntScalar (-9223372036854775808) 9223372036854775807
        , signedBoundary
            "longint"
            LongIntScalar
            (-170141183460469231731687303715884105728)
            170141183460469231731687303715884105727
        , unsignedBoundary "ubyte" UByteScalar 255
        , unsignedBoundary "ushort" UShortScalar 65535
        , unsignedBoundary "ulong" ULongScalar 4294967295
        , unsignedBoundary "uint" UIntScalar 18446744073709551615
        , unsignedBoundary "ulongint" ULongIntScalar 340282366920938463463374607431768211455
        ]
        ++ [ compiles "targeted byte literal" "byte value = 127;"
           , rejectedWith "byte overflow diagnostic" "byte value = 128;" "VXT0016"
           , compiles "targeted unsigned literal" "ubyte value = 255;"
           , rejectedWith "unsigned overflow diagnostic" "ubyte value = 256;" "VXT0016"
           , rejectedWith "unsigned negative diagnostic" "ubyte value = -1;" "VXT0011"
           , compiles "targeted longint beyond int" "longint value = 9'223'372'036'854'775'808;"
           , rejectedWith
                "un-targeted literal never widens silently"
                "auto value = 9'223'372'036'854'775'808;"
                "VXT0017"
           , compiles "constant expression fits byte target" "byte value = 100 + 27;"
           , rejectedWith "constant expression overflow is diagnosed" "byte value = 100 + 28;" "VXT0018"
           , rejectedWith "constant division by zero is diagnosed" "int value = 1 / 0;" "VXT0019"
           , rejectedWith "constant floor division by zero is diagnosed" "int value = 1 // 0;" "VXT0019"
           , rejectedWith "constant remainder by zero is diagnosed" "int value = 1 % 0;" "VXT0019"
           ]

numericContextTests :: [(String, Bool)]
numericContextTests =
    [ compiles "numeric zero is a valid condition" "if (0) { return; }"
    , compiles "numeric one is a valid condition" "if (1) { return; }"
    , compiles "numeric negative one is a valid condition" "if (-1) { return; }"
    , compiles "integer literal targets bool" "bool enabled = 1;"
    , compiles "integer zero targets bool" "bool disabled = 0;"
    , rejectedWith "string is not a condition" "if (\"yes\") { return; }" "VXT0006"
    , compiles "same-width long arithmetic" "long value = 2 + 3;"
    , compiles "same-width byte arithmetic" "byte value = 2 + 3;"
    , rejectedWith
        "mixed computed widths are not implicit conversions"
        "long left = 1; int right = 2; int sum = left + right;"
        "VXT0012"
    ]

corePropagationTests :: [(String, Bool)]
corePropagationTests =
    [ ("targeted long literal reaches Core with long type", coreLiteralHasType "long" 42)
    , ("targeted byte literal reaches Core with byte type", coreLiteralHasType "byte" 42)
    , ("targeted uint literal reaches Core with uint type", coreLiteralHasType "uint" 42)
    , ("numeric bool lowering emits a Core boolean", numericBoolLowers)
    ]

semanticRuleTests :: [(String, Bool)]
semanticRuleTests =
    [ ruleSucceeds "int addition preserves int" intType (binaryNumericRule Add intType intType)
    , ruleSucceeds
        "long addition preserves long"
        (namedType "long")
        (binaryNumericRule Add (namedType "long") (namedType "long"))
    , ruleSucceeds
        "uint multiplication preserves uint"
        (namedType "uint")
        (binaryNumericRule Multiply (namedType "uint") (namedType "uint"))
    , ruleSucceeds
        "floating division preserves float"
        (namedType "float")
        (binaryNumericRule Divide (namedType "float") (namedType "float"))
    , ruleSucceeds "numeric comparison returns bool" boolType (binaryNumericRule LessThan intType intType)
    , ruleSucceeds "integer equality returns bool" boolType (binaryNumericRule Equal intType intType)
    , ruleSucceeds "boolean logical and returns bool" boolType (binaryNumericRule LogicalAnd boolType boolType)
    , ruleSucceeds "numeric logical or returns bool" boolType (binaryNumericRule LogicalOr intType intType)
    , ruleFails "mixed signed widths fail" (binaryNumericRule Add (namedType "long") intType)
    , ruleFails "mixed signedness fails" (binaryNumericRule Add intType (namedType "uint"))
    , ruleFails "string arithmetic fails" (binaryNumericRule Add stringType stringType)
    , ruleFails "mismatched equality fails" (binaryNumericRule Equal intType (namedType "long"))
    , ruleSucceeds "unary plus preserves signed integer" intType (unaryNumericRule UnaryPlus intType)
    , ruleSucceeds "unary plus preserves unsigned integer" (namedType "uint") (unaryNumericRule UnaryPlus (namedType "uint"))
    , ruleSucceeds "unary negate preserves float" (namedType "float") (unaryNumericRule UnaryNegate (namedType "float"))
    , ruleFails "unary negate rejects unsigned integer" (unaryNumericRule UnaryNegate (namedType "uint"))
    , ruleSucceeds "logical not accepts integer context" boolType (unaryNumericRule LogicalNot intType)
    , ruleFails "logical not rejects string" (unaryNumericRule LogicalNot stringType)
    , ruleSucceeds "untargeted small integer selects int" intType (integerLiteralRule NoNumericContext 42)
    , ruleFails "untargeted oversized integer fails" (integerLiteralRule NoNumericContext 9223372036854775808)
    , ruleSucceeds "boolean integer context selects bool" boolType (integerLiteralRule BooleanNumericContext 99)
    , ruleSucceeds
        "targeted ubyte selects ubyte"
        (namedType "ubyte")
        (integerLiteralRule (TargetNumericType (namedType "ubyte")) 255)
    , ruleFails "targeted ubyte rejects overflow" (integerLiteralRule (TargetNumericType (namedType "ubyte")) 256)
    , ruleSucceeds "untargeted floating selects float" (namedType "float") (floatingLiteralRule NoNumericContext)
    , ruleSucceeds
        "targeted double selects double"
        (namedType "double")
        (floatingLiteralRule (TargetNumericType (namedType "double")))
    , ("bool accepts boolean context", acceptsBooleanContext boolType)
    , ("integer accepts boolean context", acceptsBooleanContext intType)
    , ("floating point accepts boolean context", acceptsBooleanContext (namedType "float"))
    , ("string rejects boolean context", not (acceptsBooleanContext stringType))
    ]

ruleSucceeds :: String -> Type -> NumericRuleResult -> (String, Bool)
ruleSucceeds label expected rule =
    (label, numericRuleType rule == expected && numericRuleError rule == Nothing)

ruleFails :: String -> NumericRuleResult -> (String, Bool)
ruleFails label rule = (label, case numericRuleError rule of Just _ -> True; Nothing -> False)

accepts :: String -> String -> TokenKind -> (String, Bool)
accepts label source expectedKind =
    (label, case lexOne source of Right [token] -> tokenKind token == expectedKind && tokenText token == source; _ -> False)

rejectsLexically :: String -> String -> String -> (String, Bool)
rejectsLexically label source fragment =
    (label, case lexOne source of Left problems -> any (isInfixOf fragment . diagnosticMessage) problems; Right _ -> False)

parsedValue :: String -> String -> Integer -> (String, Bool)
parsedValue label spelling expected =
    ( label
    , case lexWithEof spelling >>= parseExpressionModule of
        Right value -> value == expected
        Left _ -> False
    )

directParse :: String -> String -> IntegerRadix -> Integer -> (String, Bool)
directParse label spelling radix expected =
    ( label
    , case parseIntegerSpelling spelling of
        Right parsed -> parsedIntegerRadix parsed == radix && parsedIntegerValue parsed == expected
        Left _ -> False
    )

characterValue :: String -> String -> Integer -> (String, Bool)
characterValue label spelling expected =
    (label, parseCharacterLiteral spelling == Right expected)

characterFailure :: String -> String -> String -> (String, Bool)
characterFailure label spelling fragment =
    ( label
    , case parseCharacterLiteral spelling of
        Left issue -> fragment `isInfixOf` renderCharacterLiteralError issue
        Right _ -> False
    )

characterLexFailure :: String -> String -> String -> (String, Bool)
characterLexFailure label spelling fragment =
    ( label
    , case lexOne spelling of Left problems -> any (isInfixOf fragment . diagnosticMessage) problems; Right _ -> False
    )

floatingValue :: String -> String -> String -> (String, Bool)
floatingValue label spelling expected =
    (label, validateFloatingSpelling spelling == Right expected)

floatingFailure :: String -> String -> String -> (String, Bool)
floatingFailure label spelling fragment =
    ( label
    , case validateFloatingSpelling spelling of
        Left issue -> fragment `isInfixOf` renderFloatingLiteralError issue
        Right _ -> False
    )

scalarMetadata :: String -> ScalarType -> ScalarFamily -> Int -> Bool -> (String, Bool)
scalarMetadata label scalar family width signed =
    ( label
    , scalarTypeFamily scalar == family
        && scalarTypeWidth scalar == width
        && scalarTypeSigned scalar == signed
    )

scalarRoundTrips :: ScalarType -> Bool
scalarRoundTrips scalar = typeToScalarType (scalarTypeToType scalar) == Just scalar

wrapperRoundTrips :: ScalarType -> Bool
wrapperRoundTrips scalar = wrapperNameToScalarType (scalarWrapperName scalar) == Just scalar

signedBoundary :: String -> ScalarType -> Integer -> Integer -> [(String, Bool)]
signedBoundary name scalar minimumValue maximumValue =
    [ (name ++ " minimum fits", integerFits scalar minimumValue)
    , (name ++ " maximum fits", integerFits scalar maximumValue)
    , (name ++ " below minimum is rejected", not (integerFits scalar (minimumValue - 1)))
    , (name ++ " above maximum is rejected", not (integerFits scalar (maximumValue + 1)))
    , (name ++ " reported minimum is exact", integerMinimum scalar == Just minimumValue)
    , (name ++ " reported maximum is exact", integerMaximum scalar == Just maximumValue)
    ]

unsignedBoundary :: String -> ScalarType -> Integer -> [(String, Bool)]
unsignedBoundary name scalar maximumValue =
    [ (name ++ " zero fits", integerFits scalar 0)
    , (name ++ " maximum fits", integerFits scalar maximumValue)
    , (name ++ " negative one is rejected", not (integerFits scalar (-1)))
    , (name ++ " above maximum is rejected", not (integerFits scalar (maximumValue + 1)))
    , (name ++ " reported minimum is zero", integerMinimum scalar == Just 0)
    , (name ++ " reported maximum is exact", integerMaximum scalar == Just maximumValue)
    ]

compiles :: String -> String -> (String, Bool)
compiles label body = (label, case compileBody body of Right _ -> True; Left _ -> False)

rejectedWith :: String -> String -> String -> (String, Bool)
rejectedWith label body code =
    (label, case compileBody body of Left problems -> any ((== code) . diagnosticCode) problems; Right _ -> False)

compileBody :: String -> Either [Diagnostic] FrontendArtifacts
compileBody body = compile ("class Numeric { void Run() { " ++ body ++ " return; } }")

compile :: String -> Either [Diagnostic] FrontendArtifacts
compile source = compileToCorePrep (CompilerInput "numeric-test.vxs" source)

lexOne :: String -> Either [Diagnostic] [Token]
lexOne source = do
    tokens <- runLexer defaultLexer (LexerInput "numeric-test.vxs" source)
    pure (filter ((/= EndOfFileToken) . tokenKind) tokens)

lexWithEof :: String -> Either [Diagnostic] [Token]
lexWithEof source = runLexer defaultLexer (LexerInput "numeric-test.vxs" ("class N { int V() { " ++ source ++ " } }"))

parseExpressionModule :: [Token] -> Either [Diagnostic] Integer
parseExpressionModule tokens = do
    ParsedAST tree <- runParser defaultParser (ParserInput "numeric-test.vxs" tokens)
    case syntaxDeclarations tree of
        [ TypeDeclaration {typeMembers = [FunctionDeclaration {declarationBody = Block [ExpressionStatement _ expression False]}]}
            ] ->
                case expression of
                    LiteralExpression _ (IntegerLiteral value) _ -> Right value
                    _ -> Left []
        _ -> Left []

coreLiteralHasType :: String -> Integer -> Bool
coreLiteralHasType typeName expected = case compileBody (typeName ++ " value = " ++ show expected ++ ";") of
    Right artifacts -> any matches (concatMap coreFunctionBody (coreModuleFunctions (artifactCore artifacts)))
    Left _ -> False
    where
        matches (CoreBind binding) =
            coreBindingType binding == namedType typeName
                && case coreBindingValue binding of
                    CoreLiteral (CoreInteger value) valueType -> value == expected && valueType == namedType typeName
                    _ -> False
        matches _ = False

numericBoolLowers :: Bool
numericBoolLowers = case compileBody "bool enabled = 1;" of
    Right artifacts -> any matches (concatMap coreFunctionBody (coreModuleFunctions (artifactCore artifacts)))
    Left _ -> False
    where
        matches (CoreBind binding) = case coreBindingValue binding of CoreLiteral (CoreBoolean True) valueType -> valueType == boolType; _ -> False
        matches _ = False

coreCharacterValue :: Bool
coreCharacterValue = case compileBody "char letter = 'A';" of
    Right artifacts -> any matches (concatMap coreFunctionBody (coreModuleFunctions (artifactCore artifacts)))
    Left _ -> False
    where
        matches (CoreBind binding) = case coreBindingValue binding of
            CoreLiteral (CoreInteger 0x41) valueType -> valueType == namedType "char"
            _ -> False
        matches _ = False

coreFloatingSpelling :: Bool
coreFloatingSpelling = case compileBody "double precise = 1'000.250'000;" of
    Right artifacts -> any matches (concatMap coreFunctionBody (coreModuleFunctions (artifactCore artifacts)))
    Left _ -> False
    where
        matches (CoreBind binding) = case coreBindingValue binding of
            CoreLiteral (CoreFloating "1000.250000") valueType -> valueType == namedType "double"
            _ -> False
        matches _ = False
