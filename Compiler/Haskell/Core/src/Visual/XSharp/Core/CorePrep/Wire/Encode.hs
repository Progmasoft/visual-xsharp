-- SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
-- SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.0
module Visual.XSharp.Core.CorePrep.Wire.Encode (encodeCorePrep, encodeCorePrepWith) where

import Data.Bits (Bits, (.&.), shiftR)
import Data.Char (ord)
import Data.Int (Int64)
import Data.Word (Word8, Word16, Word32, Word64)
import Visual.XSharp.AST
import Visual.XSharp.Core
import Visual.XSharp.Core.CorePrep
import Visual.XSharp.Core.CorePrep.Wire.Format

type Encoder = Either WireError [Word8]

encodeCorePrep :: CorePrepModule -> Either WireError [Word8]
encodeCorePrep = encodeCorePrepWith defaultWireLimits

encodeCorePrepWith :: WireLimits -> CorePrepModule -> Either WireError [Word8]
encodeCorePrepWith limits moduleValue = do
    payload <- encodeModule limits moduleValue
    let bytes = wireMagic ++ word16 (wireVersionNumber currentWireVersion) ++ word16 0 ++ payload
    requireLimit limits "wire byte length" (maximumWireBytes limits) (length bytes)
    pure bytes

encodeModule :: WireLimits -> CorePrepModule -> Encoder
encodeModule limits moduleValue = do
    name <- encodeQualifiedName limits (corePrepModuleName moduleValue)
    functions <- encodeVector limits "function count" (maximumFunctions limits)
        (encodeFunction limits) (corePrepModuleFunctions moduleValue)
    pure (name ++ functions)

encodeFunction :: WireLimits -> CorePrepFunction -> Encoder
encodeFunction limits function = do
    name <- encodeResolvedName limits "function symbol" (corePrepFunctionName function)
    parameters <- encodeVector limits "parameter count" (maximumParametersPerFunction limits)
        (encodeParameter limits) (corePrepFunctionParameters function)
    result <- encodeType limits (corePrepFunctionReturnType function)
    entry <- encodeNonNegative32 "entry block" (corePrepFunctionEntry function)
    blocks <- encodeVector limits "block count" (maximumBlocksPerFunction limits)
        (encodeBlock limits) (corePrepFunctionBlocks function)
    pure (name ++ parameters ++ result ++ entry ++ blocks)

encodeParameter :: WireLimits -> (ResolvedName, Type) -> Encoder
encodeParameter limits (name, valueType) = do
    encodedName <- encodeResolvedName limits "parameter symbol" name
    encodedType <- encodeType limits valueType
    pure (encodedName ++ encodedType)

encodeBlock :: WireLimits -> CorePrepBlock -> Encoder
encodeBlock limits block = do
    identifier <- encodeNonNegative32 "block id" (corePrepBlockId block)
    instructions <- encodeVector limits "instruction count" (maximumInstructionsPerBlock limits)
        (encodeInstruction limits) (corePrepBlockInstructions block)
    terminator <- encodeTerminator limits (corePrepBlockTerminator block)
    pure (identifier ++ instructions ++ terminator)

encodeInstruction :: WireLimits -> CorePrepInstruction -> Encoder
encodeInstruction limits instruction = case instruction of
    CorePrepBind name valueType mutable operation -> do
        encodedName <- encodeResolvedName limits "binding symbol" name
        encodedType <- encodeType limits valueType
        encodedOperation <- encodeOperation limits operation
        pure ([0] ++ encodedName ++ encodedType ++ encodeBool mutable ++ encodedOperation)
    CorePrepAssign name atom -> do
        encodedName <- encodeResolvedName limits "assignment symbol" name
        encodedAtom <- encodeAtom limits atom
        pure ([1] ++ encodedName ++ encodedAtom)
    CorePrepEvaluate operation -> do
        encodedOperation <- encodeOperation limits operation
        pure ([2] ++ encodedOperation)

encodeOperation :: WireLimits -> CorePrepOperation -> Encoder
encodeOperation limits operation = case operation of
    CorePrepCopy atom -> encodeTaggedAtoms limits 0 [atom]
    CorePrepCall callee arguments -> encodeTaggedAtoms limits 1 (callee : arguments)
    CorePrepPrimitive primitive arguments -> encodeTaggedAtoms limits (primitiveTag primitive) arguments

encodeTaggedAtoms :: WireLimits -> Word8 -> [CorePrepAtom] -> Encoder
encodeTaggedAtoms limits tag atoms = do
    encoded <- encodeVector limits "operand count" (maximumOperandsPerInstruction limits) (encodeAtom limits) atoms
    pure (tag : encoded)

encodeAtom :: WireLimits -> CorePrepAtom -> Encoder
encodeAtom limits atom = case atom of
    CorePrepVariable name valueType -> do
        encodedName <- encodeResolvedName limits "variable symbol" name
        encodedType <- encodeType limits valueType
        pure ([0] ++ encodedType ++ encodedName)
    CorePrepLiteral literal valueType -> do
        encodedType <- encodeType limits valueType
        encodedLiteral <- encodeLiteral limits valueType literal
        pure ([1] ++ encodedType ++ encodedLiteral)

encodeLiteral :: WireLimits -> Type -> CoreLiteral -> Encoder
encodeLiteral limits valueType literal = case (primitiveTypeTag valueType, literal) of
    (Just 0, CoreUnit) -> pure []
    (Just 1, CoreBoolean value) -> pure (encodeBool value)
    (Just 2, CoreInteger value) -> encodeInteger64 value
    (Just 4, CoreString value) -> encodeText limits "string literal" value
    _ -> Left (wireError UnsupportedType 0 "literal" "literal payload does not match its declared type")

encodeTerminator :: WireLimits -> CorePrepTerminator -> Encoder
encodeTerminator limits terminator = case terminator of
    CorePrepReturn atom -> do encoded <- encodeAtom limits atom; pure (0 : encoded)
    CorePrepBranch atom trueTarget falseTarget -> do
        encoded <- encodeAtom limits atom
        trueBlock <- encodeNonNegative32 "true target" trueTarget
        falseBlock <- encodeNonNegative32 "false target" falseTarget
        pure ([1] ++ encoded ++ trueBlock ++ falseBlock)
    CorePrepJump target -> do encoded <- encodeNonNegative32 "jump target" target; pure (2 : encoded)
    CorePrepUnreachable -> pure [3]

encodeResolvedName :: WireLimits -> String -> ResolvedName -> Encoder
encodeResolvedName limits context name = do
    symbol <- encodePositive64 context (symbolIdValue (resolvedSymbol name))
    spelling <- encodeText limits (context ++ " spelling") (identifierText (resolvedSpelling name))
    pure (symbol ++ spelling)

encodeQualifiedName :: WireLimits -> QualifiedName -> Encoder
encodeQualifiedName limits (QualifiedName parts) =
    encodeVector limits "qualified name part count" 65535
        (encodeText limits "qualified name part" . identifierText) parts

encodeType :: WireLimits -> Type -> Encoder
encodeType limits = encodeTypeAt limits 0

encodeTypeAt :: WireLimits -> Int -> Type -> Encoder
encodeTypeAt limits depth valueType
    | depth > maximumTypeDepth limits = Left (wireError LimitExceeded 0 "type" "type nesting exceeds configured limit")
    | Just tag <- primitiveTypeTag valueType = pure [tag]
    | NamedType name arguments <- valueType = do
        encodedName <- encodeQualifiedName limits name
        encodedArguments <- encodeVector limits "type argument count" 65535 (encodeTypeAt limits (depth + 1)) arguments
        pure ([6] ++ encodedName ++ encodedArguments)
    | FunctionType parameters result <- valueType = do
        encodedParameters <- encodeVector limits "function type parameter count" 65535
            (encodeTypeAt limits (depth + 1)) parameters
        encodedResult <- encodeTypeAt limits (depth + 1) result
        pure ([5] ++ encodedParameters ++ encodedResult)
    | TypeVariable name <- valueType = do
        encodedName <- encodeResolvedName limits "type variable symbol" name
        pure ([7] ++ encodedName)
    | ErrorType <- valueType = Left (wireError UnsupportedType 0 "type" "unresolved ErrorType cannot cross the CorePrep boundary")

primitiveTypeTag :: Type -> Maybe Word8
primitiveTypeTag valueType
    | valueType == unitType = Just 0
    | valueType == boolType = Just 1
    | valueType == intType = Just 2
    | valueType == stringType = Just 4
    | otherwise = Nothing

primitiveTag :: CorePrimitive -> Word8
primitiveTag primitive = case primitive of
    CoreAdd -> 2; CoreSubtract -> 3; CoreMultiply -> 4; CoreDivide -> 5
    CoreFloorDivide -> 6; CoreRemainder -> 7; CoreLessThan -> 8; CoreLessEqual -> 9
    CoreGreaterThan -> 10; CoreGreaterEqual -> 11; CoreEqual -> 12; CoreNotEqual -> 13
    CoreLogicalAnd -> 14; CoreLogicalOr -> 15; CoreNegate -> 16; CoreLogicalNot -> 17

encodeVector :: WireLimits -> String -> Int -> (a -> Encoder) -> [a] -> Encoder
encodeVector limits context maximumCount encode values = do
    requireLimit limits context maximumCount (length values)
    encodedCount <- encodeNonNegative32 context (length values)
    encodedValues <- traverse encode values
    pure (encodedCount ++ concat encodedValues)

encodeText :: WireLimits -> String -> String -> Encoder
encodeText limits context value = do
    requireLimit limits context (maximumStringCodePoints limits) (length value)
    count <- encodeNonNegative32 context (length value)
    codePoints <- traverse encodeCodePoint value
    pure (count ++ concat codePoints)
  where
    encodeCodePoint character
        | code >= 0xD800 && code <= 0xDFFF = Left (wireError InvalidCodePoint 0 context "surrogate code point is not a scalar value")
        | code > 0x10FFFF = Left (wireError InvalidCodePoint 0 context "code point exceeds Unicode range")
        | otherwise = Right (word32 (fromIntegral code))
      where code = ord character

encodeInteger64 :: Integer -> Encoder
encodeInteger64 value
    | value < fromIntegral (minBound :: Int64) || value > fromIntegral (maxBound :: Int64) =
        Left (wireError InvalidInteger 0 "integer literal" "integer does not fit signed 64-bit CorePrep wire representation")
    | otherwise = pure (word64 (fromIntegral (fromIntegral value :: Int64)))

encodePositive64 :: String -> Int -> Encoder
encodePositive64 context value
    | value <= 0 = Left (wireError InvalidSymbol 0 context "symbol id must be positive")
    | otherwise = pure (word64 (fromIntegral value))

encodeNonNegative32 :: String -> Int -> Encoder
encodeNonNegative32 context value
    | value < 0 || toInteger value > toInteger (maxBound :: Word32) =
        Left (wireError InvalidCount 0 context "value does not fit unsigned 32-bit wire field")
    | otherwise = pure (word32 (fromIntegral value))

encodeBool :: Bool -> [Word8]
encodeBool False = [0]
encodeBool True = [1]

requireLimit :: WireLimits -> String -> Int -> Int -> Either WireError ()
requireLimit _ context maximumValue actual
    | actual > maximumValue = Left (wireError LimitExceeded 0 context ("count " ++ show actual ++ " exceeds limit " ++ show maximumValue))
    | otherwise = Right ()

word16 :: Word16 -> [Word8]
word16 value = [byte value 0, byte value 8]

word32 :: Word32 -> [Word8]
word32 value = [byte value 0, byte value 8, byte value 16, byte value 24]

word64 :: Word64 -> [Word8]
word64 value = [byte value 0, byte value 8, byte value 16, byte value 24,
                byte value 32, byte value 40, byte value 48, byte value 56]

byte :: (Integral a, Bits a) => a -> Int -> Word8
byte value amount = fromIntegral ((value `shiftR` amount) .&. 0xff)
