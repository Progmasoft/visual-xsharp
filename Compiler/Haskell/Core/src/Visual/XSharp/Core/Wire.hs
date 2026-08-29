-- SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
-- SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.0

module Visual.XSharp.Core.Wire
    ( encodeCore
    , decodeCore
    , CoreWireVersion (..)
    , currentCoreWireVersion
    , CoreWireLimits (..)
    , defaultCoreWireLimits
    , CoreWireErrorKind (..)
    , CoreWireError (..)
    ) where

import Data.Bits (Bits, shiftL, shiftR, (.&.), (.|.))
import Data.Char (chr, ord)
import Data.Int (Int64)
import Data.Word (Word16, Word32, Word64, Word8)
import Visual.XSharp.AST
import Visual.XSharp.Core

newtype CoreWireVersion = CoreWireVersion {coreWireVersionNumber :: Word16}
    deriving (Eq, Ord, Read, Show)

currentCoreWireVersion :: CoreWireVersion
currentCoreWireVersion = CoreWireVersion 1

data CoreWireLimits = CoreWireLimits
    { maximumCoreWireBytes :: Int
    , maximumCoreTextScalars :: Int
    , maximumCoreFunctions :: Int
    , maximumCoreParameters :: Int
    , maximumCoreStatements :: Int
    , maximumCoreOperands :: Int
    , maximumCoreTypeDepth :: Int
    , maximumCoreExpressionDepth :: Int
    }
    deriving (Eq, Ord, Read, Show)

defaultCoreWireLimits :: CoreWireLimits
defaultCoreWireLimits =
    CoreWireLimits
        { maximumCoreWireBytes = 64 * 1024 * 1024
        , maximumCoreTextScalars = 1024 * 1024
        , maximumCoreFunctions = 65535
        , maximumCoreParameters = 65535
        , maximumCoreStatements = 1048576
        , maximumCoreOperands = 65535
        , maximumCoreTypeDepth = 128
        , maximumCoreExpressionDepth = 4096
        }

data CoreWireErrorKind
    = CoreInvalidMagic
    | CoreUnsupportedVersion
    | CoreTruncatedInput
    | CoreTrailingInput
    | CoreInvalidTag
    | CoreInvalidBoolean
    | CoreInvalidScalar
    | CoreInvalidCount
    | CoreInvalidSymbol
    | CoreInvalidInteger
    | CoreUnsupportedType
    | CoreLimitExceeded
    deriving (Eq, Ord, Read, Show)

data CoreWireError = CoreWireError
    { coreWireErrorKind :: CoreWireErrorKind
    , coreWireErrorOffset :: Int
    , coreWireErrorContext :: String
    , coreWireErrorMessage :: String
    }
    deriving (Eq, Ord, Read, Show)

type Encoder = Either CoreWireError [Word8]

encodeCore :: CoreWireLimits -> CoreModule -> Either CoreWireError [Word8]
encodeCore limits moduleValue = do
    payload <- encodeModule limits moduleValue
    let bytes = magic ++ word16 (coreWireVersionNumber currentCoreWireVersion) ++ word16 0 ++ payload
    requireEncode limits "wire byte length" (maximumCoreWireBytes limits) (length bytes)
    pure bytes

encodeModule :: CoreWireLimits -> CoreModule -> Encoder
encodeModule limits moduleValue = do
    name <- encodeQualifiedName limits (coreModuleName moduleValue)
    functions <-
        encodeVector
            limits
            "function count"
            (maximumCoreFunctions limits)
            (encodeFunction limits)
            (coreModuleFunctions moduleValue)
    pure (name ++ functions)

encodeFunction :: CoreWireLimits -> CoreFunction -> Encoder
encodeFunction limits function = do
    name <- encodeResolvedName limits "function symbol" (coreFunctionName function)
    parameters <-
        encodeVector
            limits
            "parameter count"
            (maximumCoreParameters limits)
            (encodeParameter limits)
            (coreFunctionParameters function)
    result <- encodeType limits 0 (coreFunctionReturnType function)
    body <-
        encodeVector
            limits
            "statement count"
            (maximumCoreStatements limits)
            (encodeStatement limits)
            (coreFunctionBody function)
    pure (name ++ parameters ++ result ++ body)

encodeParameter :: CoreWireLimits -> (ResolvedName, Type) -> Encoder
encodeParameter limits (name, valueType) =
    (++)
        <$> encodeResolvedName limits "parameter symbol" name
        <*> encodeType limits 0 valueType

encodeStatement :: CoreWireLimits -> CoreStatement -> Encoder
encodeStatement limits statement = case statement of
    CoreBind binding -> do
        name <- encodeResolvedName limits "binding symbol" (coreBindingName binding)
        valueType <- encodeType limits 0 (coreBindingType binding)
        value <- encodeExpression limits 0 (coreBindingValue binding)
        pure ([0] ++ name ++ valueType ++ encodeBool (coreBindingMutable binding) ++ value)
    CoreAssign name expression ->
        taggedExpression 1
            <$> encodeResolvedName limits "assignment symbol" name
            <*> encodeExpression limits 0 expression
    CoreReturn expression -> (2 :) <$> encodeExpression limits 0 expression
    CoreIf condition trueBranch falseBranch -> do
        encodedCondition <- encodeExpression limits 0 condition
        encodedTrue <-
            encodeVector
                limits
                "true branch statement count"
                (maximumCoreStatements limits)
                (encodeStatement limits)
                trueBranch
        encodedFalse <-
            encodeVector
                limits
                "false branch statement count"
                (maximumCoreStatements limits)
                (encodeStatement limits)
                falseBranch
        pure ([3] ++ encodedCondition ++ encodedTrue ++ encodedFalse)
    CoreEvaluate expression -> (4 :) <$> encodeExpression limits 0 expression
    where
        taggedExpression tag left right = [tag] ++ left ++ right

encodeExpression :: CoreWireLimits -> Int -> CoreExpression -> Encoder
encodeExpression limits depth expression
    | depth > maximumCoreExpressionDepth limits = failure CoreLimitExceeded "expression" "expression nesting exceeds limit"
    | otherwise = case expression of
        CoreVariable name valueType -> do
            encodedType <- encodeType limits 0 valueType
            encodedName <- encodeResolvedName limits "variable symbol" name
            pure ([0] ++ encodedType ++ encodedName)
        CoreLiteral literal valueType -> do
            encodedType <- encodeType limits 0 valueType
            encodedLiteral <- encodeLiteral limits literal
            pure ([1] ++ encodedType ++ encodedLiteral)
        CoreApply callee arguments valueType -> do
            encodedType <- encodeType limits 0 valueType
            encodedCallee <- encodeExpression limits (depth + 1) callee
            encodedArguments <-
                encodeVector
                    limits
                    "call argument count"
                    (maximumCoreOperands limits)
                    (encodeExpression limits (depth + 1))
                    arguments
            pure ([2] ++ encodedType ++ encodedCallee ++ encodedArguments)
        CorePrimitive primitive arguments valueType -> do
            encodedType <- encodeType limits 0 valueType
            encodedArguments <-
                encodeVector
                    limits
                    "primitive operand count"
                    (maximumCoreOperands limits)
                    (encodeExpression limits (depth + 1))
                    arguments
            pure ([3, primitiveTag primitive] ++ encodedType ++ encodedArguments)

encodeLiteral :: CoreWireLimits -> CoreLiteral -> Encoder
encodeLiteral limits literal = case literal of
    CoreUnit -> pure [0]
    CoreBoolean value -> pure (1 : encodeBool value)
    CoreInteger value -> (2 :) <$> encodeInteger value
    CoreString value -> (3 :) <$> encodeText limits "string literal" value

encodeType :: CoreWireLimits -> Int -> Type -> Encoder
encodeType limits depth valueType
    | depth > maximumCoreTypeDepth limits = failure CoreLimitExceeded "type" "type nesting exceeds limit"
    | valueType == unitType = pure [0]
    | valueType == boolType = pure [1]
    | valueType == intType = pure [2]
    | valueType == stringType = pure [3]
    | NamedType name arguments <- valueType = do
        encodedName <- encodeQualifiedName limits name
        encodedArguments <-
            encodeVector
                limits
                "type argument count"
                (maximumCoreOperands limits)
                (encodeType limits (depth + 1))
                arguments
        pure ([4] ++ encodedName ++ encodedArguments)
    | FunctionType parameters result <- valueType = do
        encodedParameters <-
            encodeVector
                limits
                "function type parameter count"
                (maximumCoreParameters limits)
                (encodeType limits (depth + 1))
                parameters
        encodedResult <- encodeType limits (depth + 1) result
        pure ([5] ++ encodedParameters ++ encodedResult)
    | TypeVariable name <- valueType = (6 :) <$> encodeResolvedName limits "type variable symbol" name
    | ErrorType <- valueType = failure CoreUnsupportedType "type" "ErrorType cannot cross the Core boundary"

encodeResolvedName :: CoreWireLimits -> String -> ResolvedName -> Encoder
encodeResolvedName limits context name
    | symbolIdValue (resolvedSymbol name) <= 0 = failure CoreInvalidSymbol context "symbol id must be positive"
    | otherwise = do
        spelling <- encodeText limits (context ++ " spelling") (identifierText (resolvedSpelling name))
        pure (word64 (fromIntegral (symbolIdValue (resolvedSymbol name))) ++ spelling)

encodeQualifiedName :: CoreWireLimits -> QualifiedName -> Encoder
encodeQualifiedName limits (QualifiedName parts) =
    encodeVector
        limits
        "qualified name parts"
        65535
        (encodeText limits "qualified name part" . identifierText)
        parts

encodeVector :: CoreWireLimits -> String -> Int -> (a -> Encoder) -> [a] -> Encoder
encodeVector limits context maximumValue encode values = do
    requireEncode limits context maximumValue (length values)
    encoded <- traverse encode values
    pure (word32 (fromIntegral (length values)) ++ concat encoded)

encodeText :: CoreWireLimits -> String -> String -> Encoder
encodeText limits context value = do
    requireEncode limits context (maximumCoreTextScalars limits) (length value)
    codePoints <- traverse encodeScalar value
    pure (word32 (fromIntegral (length value)) ++ concat codePoints)
    where
        encodeScalar character
            | code >= 0xd800 && code <= 0xdfff = failure CoreInvalidScalar context "surrogate is not a Unicode scalar"
            | code > 0x10ffff = failure CoreInvalidScalar context "code point exceeds Unicode range"
            | otherwise = pure (word32 (fromIntegral code))
            where
                code = ord character

encodeInteger :: Integer -> Encoder
encodeInteger value
    | value < fromIntegral (minBound :: Int64) || value > fromIntegral (maxBound :: Int64) =
        failure CoreInvalidInteger "integer literal" "integer does not fit signed 64-bit Core representation"
    | otherwise = pure (word64 (fromIntegral (fromIntegral value :: Int64)))

requireEncode :: CoreWireLimits -> String -> Int -> Int -> Either CoreWireError ()
requireEncode _ context maximumValue actual
    | actual > maximumValue = failure CoreLimitExceeded context ("count exceeds configured limit: " ++ show actual)
    | otherwise = Right ()

data DecoderState = DecoderState [Word8] Int CoreWireLimits
newtype Decoder a = Decoder {runDecoder :: DecoderState -> Either CoreWireError (a, DecoderState)}

instance Functor Decoder where
    fmap transform parser = Decoder $ \state -> do
        (value, next) <- runDecoder parser state
        pure (transform value, next)
instance Applicative Decoder where
    pure value = Decoder $ \state -> Right (value, state)
    functionParser <*> valueParser = Decoder $ \state -> do
        (function, afterFunction) <- runDecoder functionParser state
        (value, afterValue) <- runDecoder valueParser afterFunction
        pure (function value, afterValue)
instance Monad Decoder where
    parser >>= nextParser = Decoder $ \state -> do
        (value, afterValue) <- runDecoder parser state
        runDecoder (nextParser value) afterValue

decodeCore :: CoreWireLimits -> [Word8] -> Either CoreWireError CoreModule
decodeCore limits bytes
    | length bytes > maximumCoreWireBytes limits =
        failure CoreLimitExceeded "wire byte length" "input exceeds configured limit"
    | otherwise = do
        (moduleValue, DecoderState remaining offset _) <- runDecoder decodeDocument (DecoderState bytes 0 limits)
        if null remaining
            then Right moduleValue
            else Left (CoreWireError CoreTrailingInput offset "document" "bytes remain after Core module")

decodeDocument :: Decoder CoreModule
decodeDocument = do
    header <- takeBytes "magic" 4
    requireDecode (header == magic) CoreInvalidMagic "magic" "input is not a Visual X# Core document"
    version <- readWord16 "version"
    requireDecode
        (version == coreWireVersionNumber currentCoreWireVersion)
        CoreUnsupportedVersion
        "version"
        "unsupported Core wire version"
    flags <- readWord16 "flags"
    requireDecode (flags == 0) CoreInvalidTag "flags" "reserved flags must be zero"
    CoreModule <$> decodeQualifiedName <*> decodeVector "function count" maximumCoreFunctions decodeFunction

decodeFunction :: Decoder CoreFunction
decodeFunction =
    CoreFunction
        <$> decodeResolvedName "function symbol"
        <*> decodeVector "parameter count" maximumCoreParameters decodeParameter
        <*> decodeType 0
        <*> decodeVector "statement count" maximumCoreStatements decodeStatement

decodeParameter :: Decoder (ResolvedName, Type)
decodeParameter = (,) <$> decodeResolvedName "parameter symbol" <*> decodeType 0

decodeStatement :: Decoder CoreStatement
decodeStatement = do
    tag <- readWord8 "statement tag"
    case tag of
        0 ->
            CoreBind
                <$> ( CoreBinding
                        <$> decodeResolvedName "binding symbol"
                        <*> decodeType 0
                        <*> decodeBool "binding mutability"
                        <*> decodeExpression 0
                    )
        1 -> CoreAssign <$> decodeResolvedName "assignment symbol" <*> decodeExpression 0
        2 -> CoreReturn <$> decodeExpression 0
        3 ->
            CoreIf
                <$> decodeExpression 0
                <*> decodeVector "true branch statement count" maximumCoreStatements decodeStatement
                <*> decodeVector "false branch statement count" maximumCoreStatements decodeStatement
        4 -> CoreEvaluate <$> decodeExpression 0
        _ -> invalidTag "statement tag" tag

decodeExpression :: Int -> Decoder CoreExpression
decodeExpression depth = do
    limits <- currentLimits
    requireDecode
        (depth <= maximumCoreExpressionDepth limits)
        CoreLimitExceeded
        "expression"
        "expression nesting exceeds limit"
    tag <- readWord8 "expression tag"
    case tag of
        0 -> flip CoreVariable <$> decodeType 0 <*> decodeResolvedName "variable symbol"
        1 -> do valueType <- decodeType 0; literal <- decodeLiteral; pure (CoreLiteral literal valueType)
        2 -> do
            valueType <- decodeType 0
            callee <- decodeExpression (depth + 1)
            arguments <- decodeVector "call argument count" maximumCoreOperands (decodeExpression (depth + 1))
            pure (CoreApply callee arguments valueType)
        3 -> do
            primitive <- readWord8 "primitive tag" >>= decodePrimitive
            valueType <- decodeType 0
            arguments <- decodeVector "primitive operand count" maximumCoreOperands (decodeExpression (depth + 1))
            pure (CorePrimitive primitive arguments valueType)
        _ -> invalidTag "expression tag" tag

decodeLiteral :: Decoder CoreLiteral
decodeLiteral = do
    tag <- readWord8 "literal tag"
    case tag of
        0 -> pure CoreUnit
        1 -> CoreBoolean <$> decodeBool "boolean literal"
        2 -> CoreInteger . fromIntegral . (fromIntegral :: Word64 -> Int64) <$> readWord64 "integer literal"
        3 -> CoreString <$> decodeText "string literal"
        _ -> invalidTag "literal tag" tag

decodeType :: Int -> Decoder Type
decodeType depth = do
    limits <- currentLimits
    requireDecode (depth <= maximumCoreTypeDepth limits) CoreLimitExceeded "type" "type nesting exceeds limit"
    tag <- readWord8 "type tag"
    case tag of
        0 -> pure unitType
        1 -> pure boolType
        2 -> pure intType
        3 -> pure stringType
        4 ->
            NamedType <$> decodeQualifiedName <*> decodeVector "type argument count" maximumCoreOperands (decodeType (depth + 1))
        5 ->
            FunctionType
                <$> decodeVector "function type parameter count" maximumCoreParameters (decodeType (depth + 1))
                <*> decodeType (depth + 1)
        6 -> TypeVariable <$> decodeResolvedName "type variable symbol"
        _ -> invalidTag "type tag" tag

decodeResolvedName :: String -> Decoder ResolvedName
decodeResolvedName context = do
    raw <- readWord64 context
    requireDecode (raw > 0 && raw <= fromIntegral (maxBound :: Int)) CoreInvalidSymbol context "invalid symbol id"
    ResolvedName (SymbolId (fromIntegral raw)) . Identifier <$> decodeText (context ++ " spelling")

decodeQualifiedName :: Decoder QualifiedName
decodeQualifiedName = QualifiedName <$> decodeVectorMaximum "qualified name parts" 65535 (Identifier <$> decodeText "qualified name part")

decodeVector :: String -> (CoreWireLimits -> Int) -> Decoder a -> Decoder [a]
decodeVector context selectMaximum parser = currentLimits >>= \limits -> decodeVectorMaximum context (selectMaximum limits) parser

decodeVectorMaximum :: String -> Int -> Decoder a -> Decoder [a]
decodeVectorMaximum context maximumValue parser = do
    count <- readWord32 context
    requireDecode (toInteger count <= toInteger maximumValue) CoreLimitExceeded context "count exceeds configured limit"
    sequence (replicate (fromIntegral count) parser)

decodeText :: String -> Decoder String
decodeText context = do
    limits <- currentLimits
    count <- readWord32 (context ++ " length")
    requireDecode
        (toInteger count <= toInteger (maximumCoreTextScalars limits))
        CoreLimitExceeded
        context
        "text exceeds configured limit"
    sequence (replicate (fromIntegral count) decodeScalar)
    where
        decodeScalar = do
            value <- readWord32 context
            requireDecode
                (value <= 0x10ffff && not (value >= 0xd800 && value <= 0xdfff))
                CoreInvalidScalar
                context
                "invalid Unicode scalar"
            pure (chr (fromIntegral value))

decodeBool :: String -> Decoder Bool
decodeBool context =
    readWord8 context >>= \value -> case value of
        0 -> pure False
        1 -> pure True
        _ -> failDecode CoreInvalidBoolean context "invalid boolean byte"

decodePrimitive :: Word8 -> Decoder CorePrimitive
decodePrimitive tag = case drop (fromIntegral tag) primitives of
    primitive : _ -> pure primitive
    [] -> invalidTag "primitive tag" tag
    where
        primitives =
            [ CoreAdd
            , CoreSubtract
            , CoreMultiply
            , CoreDivide
            , CoreFloorDivide
            , CoreRemainder
            , CoreLessThan
            , CoreLessEqual
            , CoreGreaterThan
            , CoreGreaterEqual
            , CoreEqual
            , CoreNotEqual
            , CoreLogicalAnd
            , CoreLogicalOr
            , CoreNegate
            , CoreLogicalNot
            ]

primitiveTag :: CorePrimitive -> Word8
primitiveTag primitive = fromIntegral (index primitive primitives)
    where
        primitives =
            [ CoreAdd
            , CoreSubtract
            , CoreMultiply
            , CoreDivide
            , CoreFloorDivide
            , CoreRemainder
            , CoreLessThan
            , CoreLessEqual
            , CoreGreaterThan
            , CoreGreaterEqual
            , CoreEqual
            , CoreNotEqual
            , CoreLogicalAnd
            , CoreLogicalOr
            , CoreNegate
            , CoreLogicalNot
            ]
        index :: CorePrimitive -> [CorePrimitive] -> Int
        index value (candidate : remaining) = if value == candidate then 0 else 1 + index value remaining
        index _ [] = error "CorePrimitive enumeration is incomplete"

currentLimits :: Decoder CoreWireLimits
currentLimits = Decoder $ \state@(DecoderState _ _ limits) -> Right (limits, state)

requireDecode :: Bool -> CoreWireErrorKind -> String -> String -> Decoder ()
requireDecode condition kind context message = if condition then pure () else failDecode kind context message

invalidTag :: String -> Word8 -> Decoder a
invalidTag context tag = failDecode CoreInvalidTag context ("unknown tag " ++ show tag)

failDecode :: CoreWireErrorKind -> String -> String -> Decoder a
failDecode kind context message = Decoder $ \(DecoderState _ offset _) -> Left (CoreWireError kind offset context message)

takeBytes :: String -> Int -> Decoder [Word8]
takeBytes context count = Decoder $ \(DecoderState bytes offset limits) ->
    let (prefix, suffix) = splitAt count bytes
     in if length prefix == count
            then Right (prefix, DecoderState suffix (offset + count) limits)
            else Left (CoreWireError CoreTruncatedInput offset context "input ended before field was complete")

readWord8 :: String -> Decoder Word8
readWord8 context = do
    bytes <- takeBytes context 1
    case bytes of
        [value] -> pure value
        _ -> failDecode CoreTruncatedInput context "byte reader returned no value"
readWord16 :: String -> Decoder Word16
readWord16 context = assemble <$> takeBytes context 2
readWord32 :: String -> Decoder Word32
readWord32 context = assemble <$> takeBytes context 4
readWord64 :: String -> Decoder Word64
readWord64 context = assemble <$> takeBytes context 8

magic :: [Word8]
magic = map (fromIntegral . fromEnum) "VXCR"

failure :: CoreWireErrorKind -> String -> String -> Either CoreWireError a
failure kind context message = Left (CoreWireError kind 0 context message)

encodeBool :: Bool -> [Word8]
encodeBool False = [0]
encodeBool True = [1]
word16 :: Word16 -> [Word8]
word16 value = [byte value 0, byte value 8]
word32 :: Word32 -> [Word8]
word32 value = [byte value 0, byte value 8, byte value 16, byte value 24]
word64 :: Word64 -> [Word8]
word64 value =
    [byte value 0, byte value 8, byte value 16, byte value 24, byte value 32, byte value 40, byte value 48, byte value 56]
byte :: (Integral a, Bits a) => a -> Int -> Word8
byte value amount = fromIntegral ((value `shiftR` amount) .&. 0xff)
assemble :: (Integral a, Bits a) => [Word8] -> a
assemble bytes = foldr (\(position, value) result -> result .|. (fromIntegral value `shiftL` (position * 8))) 0 (zip [0 ..] bytes)
