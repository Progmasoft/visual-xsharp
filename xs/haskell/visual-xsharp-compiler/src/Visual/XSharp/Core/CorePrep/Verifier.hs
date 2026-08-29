-- SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
-- SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.0
module Visual.XSharp.Core.CorePrep.Verifier (verifyCorePrep) where

import Data.List (nub)
import Visual.XSharp.AST
import qualified Visual.XSharp.Core as Core
import Visual.XSharp.Core.CorePrep
import Visual.XSharp.Diagnostic

verifyCorePrep :: CorePrepModule -> Either [Diagnostic] CorePrepModule
verifyCorePrep moduleValue =
    let problems = duplicateFunctions (corePrepModuleFunctions moduleValue) ++ concatMap verifyFunction (corePrepModuleFunctions moduleValue)
    in if null problems then Right moduleValue else Left problems

duplicateFunctions :: [CorePrepFunction] -> [Diagnostic]
duplicateFunctions functions = duplicateIds "VXC0001" "duplicate CorePrep function symbol" (map (resolvedSymbol . corePrepFunctionName) functions)

verifyFunction :: CorePrepFunction -> [Diagnostic]
verifyFunction function =
    let blocks = corePrepFunctionBlocks function
        blockIds = map corePrepBlockId blocks
        parameterIds = map (resolvedSymbol . fst) (corePrepFunctionParameters function)
        definitionIds = concatMap blockDefinitions blocks
    in missingEntry blockIds ++ duplicateIds "VXC0003" "duplicate CorePrep block id" blockIds
        ++ duplicateIds "VXC0004" "duplicate CorePrep parameter symbol" parameterIds
        ++ duplicateIds "VXC0005" "CorePrep symbol is defined more than once" (parameterIds ++ definitionIds)
        ++ concatMap (verifyBlock blockIds) blocks
  where
    missingEntry ids = if corePrepFunctionEntry function `elem` ids then [] else [problem "VXC0002" "CorePrep function entry block does not exist"]

blockDefinitions :: CorePrepBlock -> [SymbolId]
blockDefinitions block = [resolvedSymbol name | CorePrepBind name _ _ _ <- corePrepBlockInstructions block]

verifyBlock :: [Int] -> CorePrepBlock -> [Diagnostic]
verifyBlock blockIds block = concatMap verifyInstruction (corePrepBlockInstructions block) ++ verifyTerminator blockIds (corePrepBlockTerminator block)

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
  where resultMismatch valueType = if resultType == ErrorType || resultType == valueType then [] else [problem "VXC0006" "CorePrep copy result type does not match its atom"]

verifyPrimitive :: Core.CorePrimitive -> [CorePrepAtom] -> Type -> [Diagnostic]
verifyPrimitive primitive atoms resultType
    | length atoms /= expectedArity = [problem "VXC0007" "CorePrep primitive has the wrong arity"]
    | any ((== ErrorType) . atomType) atoms = [problem "VXC0008" "CorePrep primitive contains an unresolved type"]
    | otherwise = resultProblems
  where
    expectedArity = if primitive `elem` [Core.CoreNegate, Core.CoreLogicalNot] then 1 else 2
    expectedResult = if primitive `elem` [Core.CoreLessThan, Core.CoreLessEqual,
        Core.CoreGreaterThan, Core.CoreGreaterEqual, Core.CoreEqual,
        Core.CoreNotEqual, Core.CoreLogicalAnd, Core.CoreLogicalOr,
        Core.CoreLogicalNot] then boolType else intType
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
verifyAtom atom = [problem "VXC0012" "CorePrep atom has an unresolved type" | atomType atom == ErrorType]

atomType :: CorePrepAtom -> Type
atomType atom = case atom of CorePrepVariable _ valueType -> valueType; CorePrepLiteral _ valueType -> valueType

duplicateIds :: Eq a => String -> String -> [a] -> [Diagnostic]
duplicateIds code message values = [problem code message | length values /= length (nub values)]

problem :: String -> String -> Diagnostic
problem code message = Diagnostic CorePrepStage Error code Nothing message
