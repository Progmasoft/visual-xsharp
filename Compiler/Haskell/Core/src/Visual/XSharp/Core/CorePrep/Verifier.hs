-- SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
-- SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.0
module Visual.XSharp.Core.CorePrep.Verifier (verifyCorePrep) where

import Data.List (nub)
import Visual.XSharp.AST
import Visual.XSharp.Core qualified as Core
import Visual.XSharp.Core.CorePrep
import Visual.XSharp.Diagnostic

verifyCorePrep :: CorePrepModule -> Either [Diagnostic] CorePrepModule
verifyCorePrep moduleValue =
    let problems =
            duplicateFunctions (corePrepModuleFunctions moduleValue)
                ++ concatMap verifyFunction (corePrepModuleFunctions moduleValue)
     in if null problems then Right moduleValue else Left problems

duplicateFunctions :: [CorePrepFunction] -> [Diagnostic]
duplicateFunctions functions = duplicateIds "VXC0001" "duplicate CorePrep function symbol" (map (resolvedSymbol . corePrepFunctionName) functions)

verifyFunction :: CorePrepFunction -> [Diagnostic]
verifyFunction function =
    let blocks = corePrepFunctionBlocks function
        blockIds = map corePrepBlockId blocks
        parameterIds = map (resolvedSymbol . fst) (corePrepFunctionParameters function)
        definitionIds = concatMap blockDefinitions blocks
     in missingEntry blockIds
            ++ duplicateIds "VXC0003" "duplicate CorePrep block id" blockIds
            ++ duplicateIds "VXC0004" "duplicate CorePrep parameter symbol" parameterIds
            ++ duplicateIds "VXC0005" "CorePrep symbol is defined more than once" (parameterIds ++ definitionIds)
            ++ concatMap (verifyBlock blockIds) blocks
    where
        missingEntry ids =
            if corePrepFunctionEntry function `elem` ids
                then []
                else [problem "VXC0002" "CorePrep function entry block does not exist"]

blockDefinitions :: CorePrepBlock -> [SymbolId]
blockDefinitions block = [resolvedSymbol name | CorePrepBind name _ _ _ <- corePrepBlockInstructions block]

verifyBlock :: [Int] -> CorePrepBlock -> [Diagnostic]
verifyBlock blockIds block =
    concatMap verifyInstruction (corePrepBlockInstructions block)
        ++ verifyTerminator blockIds (corePrepBlockTerminator block)

verifyInstruction :: CorePrepInstruction -> [Diagnostic]
verifyInstruction instruction = case instruction of
    CorePrepBind _ valueType _ operation -> verifyOperation valueType operation
    CorePrepAssign _ atom -> verifyAtom atom
    CorePrepEvaluate operation -> verifyOperation ErrorType operation

verifyOperation :: Type -> CorePrepOperation -> [Diagnostic]
verifyOperation resultType operation = case operation of
    CorePrepCopy atom -> verifyAtom atom ++ resultMismatch (atomType atom)
    CorePrepCall callee arguments -> verifyAtom callee ++ concatMap verifyAtom arguments
    CorePrepPrimitive primitive atoms -> concatMap verifyAtom atoms ++ verifyPrimitive primitive atoms resultType
    CorePrepMakeClosure function captures ->
        invalidFunction function
            ++ concatMap verifyCapture captures
            ++ closureResultMismatch resultType
    where
        resultMismatch valueType =
            if resultType == ErrorType || resultType == valueType
                then []
                else [problem "VXC0006" "CorePrep copy result type does not match its atom"]
        invalidFunction name = [problem "VXC0013" "CorePrep closure function symbol must be positive" | symbolIdValue (resolvedSymbol name) <= 0]
        closureResultMismatch valueType = case valueType of
            FunctionType _ _ -> []
            ErrorType -> []
            _ -> [problem "VXC0014" "CorePrep closure result must have a callable type"]

verifyCapture :: CorePrepCapture -> [Diagnostic]
verifyCapture (CorePrepCapture mode name valueType atom) =
    [problem "VXC0015" "CorePrep capture symbol must be positive" | symbolIdValue (resolvedSymbol name) <= 0]
        ++ [problem "VXC0016" "CorePrep capture has an unresolved type" | valueType == ErrorType]
        ++ [problem "VXC0017" "CorePrep capture atom type does not match its slot" | atomType atom /= valueType]
        ++ [ problem "VXC0018" "non-owning capture requires an AARC reference value"
           | mode /= StrongCapture && not (aarcReference valueType)
           ]
        ++ verifyAtom atom
    where
        -- Core currently lacks nominal-declaration metadata, so every resolved
        -- named type remains conservatively reference-like at this boundary.
        aarcReference value = case value of
            FunctionType _ _ -> True
            NamedType _ _ -> value /= unitType && value /= boolType && value /= intType
            _ -> False

verifyPrimitive :: Core.CorePrimitive -> [CorePrepAtom] -> Type -> [Diagnostic]
verifyPrimitive primitive atoms resultType
    | length atoms /= expectedArity = [problem "VXC0007" "CorePrep primitive has the wrong arity"]
    | any ((== ErrorType) . atomType) atoms = [problem "VXC0008" "CorePrep primitive contains an unresolved type"]
    | otherwise = operandProblems ++ resultProblems
    where
        expectedArity = if primitive `elem` [Core.CoreNegate, Core.CoreLogicalNot] then 1 else 2
        comparisonOrLogical =
            if primitive
                `elem` [ Core.CoreLessThan
                       , Core.CoreLessEqual
                       , Core.CoreGreaterThan
                       , Core.CoreGreaterEqual
                       , Core.CoreEqual
                       , Core.CoreNotEqual
                       , Core.CoreLogicalAnd
                       , Core.CoreLogicalOr
                       , Core.CoreLogicalNot
                       ]
                then True
                else False
        operandTypes = map atomType atoms
        firstType = case operandTypes of valueType : _ -> valueType; [] -> ErrorType
        operandsAgree = all (== firstType) operandTypes
        logical = primitive `elem` [Core.CoreLogicalAnd, Core.CoreLogicalOr, Core.CoreLogicalNot]
        numeric = all isNumericType operandTypes
        booleanContext = all (\valueType -> valueType == boolType || isNumericType valueType) operandTypes
        negatable = isSignedIntegerType firstType || isFloatingType firstType
        operandProblems
            | not operandsAgree = [problem "VXC0019" "CorePrep primitive operand types do not agree"]
            | logical && not booleanContext = [problem "VXC0020" "CorePrep logical primitive requires bool or numeric operands"]
            | primitive == Core.CoreNegate && not negatable =
                [problem "VXC0021" "CorePrep negation requires a signed integer or floating operand"]
            | not logical && not numeric && primitive `notElem` [Core.CoreEqual, Core.CoreNotEqual] =
                [problem "VXC0022" "CorePrep arithmetic or ordering primitive requires numeric operands"]
            | otherwise = []
        expectedResult = if comparisonOrLogical then boolType else firstType
        resultProblems = if resultType == expectedResult then [] else [problem "VXC0009" "CorePrep primitive result type is inconsistent"]

verifyTerminator :: [Int] -> CorePrepTerminator -> [Diagnostic]
verifyTerminator blockIds terminator = case terminator of
    CorePrepReturn atom -> verifyAtom atom
    CorePrepBranch atom trueTarget falseTarget -> verifyAtom atom ++ requireBool atom ++ targets [trueTarget, falseTarget]
    CorePrepJump target -> targets [target]
    CorePrepUnreachable -> []
    where
        targets values = [problem "VXC0010" "CorePrep terminator targets a missing block" | any (`notElem` blockIds) values]
        requireBool atom = [problem "VXC0011" "CorePrep branch condition must be bool" | atomType atom /= boolType]

verifyAtom :: CorePrepAtom -> [Diagnostic]
verifyAtom atom =
    [problem "VXC0012" "CorePrep atom has an unresolved type" | atomType atom == ErrorType]
        ++ case atom of
            CorePrepLiteral literal valueType -> verifyLiteral literal valueType
            CorePrepVariable _ _ -> []

verifyLiteral :: Core.CoreLiteral -> Type -> [Diagnostic]
verifyLiteral literal valueType =
    [problem "VXC0023" "CorePrep literal payload does not match its declared scalar type" | not matches]
    where
        matches = case literal of
            Core.CoreInteger value -> integerFits valueType value
            Core.CoreFloating spelling -> isFloatingType valueType && validFloatingSpelling spelling
            Core.CoreString _ -> valueType == stringType
            Core.CoreBoolean _ -> valueType == boolType
            Core.CoreUnit -> valueType == unitType

typeSpelling :: Type -> String
typeSpelling (NamedType (QualifiedName [Identifier name]) []) = name
typeSpelling _ = ""

integerWidths :: [(String, Bool, Int)]
integerWidths =
    [ ("char", False, 32)
    , ("byte", True, 8)
    , ("short", True, 16)
    , ("long", True, 32)
    , ("int", True, 64)
    , ("longint", True, 128)
    , ("ubyte", False, 8)
    , ("ushort", False, 16)
    , ("ulong", False, 32)
    , ("uint", False, 64)
    , ("ulongint", False, 128)
    ]

integerFits :: Type -> Integer -> Bool
integerFits valueType value = case [(signed, width) | (name, signed, width) <- integerWidths, name == typeSpelling valueType] of
    [(True, width)] -> value >= negate (2 ^ (width - 1)) && value <= 2 ^ (width - 1) - 1
    [(False, width)] -> value >= 0 && value <= 2 ^ width - 1
    _ -> False

isIntegerType :: Type -> Bool
isIntegerType valueType = any (\(name, _, _) -> name == typeSpelling valueType) integerWidths

isSignedIntegerType :: Type -> Bool
isSignedIntegerType valueType = any (\(name, signed, _) -> signed && name == typeSpelling valueType) integerWidths

isFloatingType :: Type -> Bool
isFloatingType valueType = typeSpelling valueType `elem` ["sfloat", "lfloat", "float", "double"]

isNumericType :: Type -> Bool
isNumericType valueType = isIntegerType valueType || isFloatingType valueType

validFloatingSpelling :: String -> Bool
validFloatingSpelling spelling
    | spelling `elem` ["nan", "+nan", "-nan", "inf", "+inf", "-inf"] = True
    | otherwise = case stripSign spelling of
        [] -> False
        unsignedSpelling ->
            let (mantissa, exponentPart) = break (`elem` "eE") unsignedSpelling
             in validSignificand mantissa && validExponent exponentPart
    where
        stripSign ('+' : remaining) = remaining
        stripSign ('-' : remaining) = remaining
        stripSign value = value
        validSignificand value = case break (== '.') value of
            (whole, []) -> not (null whole) && all digit whole
            (whole, _ : fraction) -> (not (null whole) || not (null fraction)) && all digit whole && all digit fraction
        validExponent [] = True
        validExponent (_ : remaining) = case stripSign remaining of
            [] -> False
            digits -> all digit digits
        digit character = character >= '0' && character <= '9'

atomType :: CorePrepAtom -> Type
atomType atom = case atom of CorePrepVariable _ valueType -> valueType; CorePrepLiteral _ valueType -> valueType

duplicateIds :: (Eq a) => String -> String -> [a] -> [Diagnostic]
duplicateIds code message values = [problem code message | length values /= length (nub values)]

problem :: String -> String -> Diagnostic
problem code message = Diagnostic CorePrepStage Error code Nothing message
