-- SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
-- SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.0

module CoreOptimizerSourceTests (coreOptimizerSourceTests) where

import Visual.XSharp.AST
import Visual.XSharp.Compiler
import Visual.XSharp.Core
import Visual.XSharp.Core.CorePrep (corePrepModuleFunctions)

-- These cases begin as Visual X# text. The constructor-level optimizer suite
-- can isolate every rewrite, while this group proves that real desugared trees
-- carry the identities and types those rewrites expect.
coreOptimizerSourceTests :: [(String, Bool)]
coreOptimizerSourceTests =
    [ ("source constants fold through immutable locals", immutableSourceChain)
    , ("source mutable assignments remain observable", mutableSourceValue)
    , ("source true conditions select the live branch", sourceTrueBranch)
    , ("source false conditions select the live branch", sourceFalseBranch)
    , ("source numeric zero selects the false branch", sourceNumericFalse)
    , ("source nonzero conditions select the true branch", sourceNumericTrue)
    , ("source dead scalar locals disappear from optimized Core", sourceDeadLocal)
    , ("source calls survive discarded results", sourceDiscardedCall)
    , ("source code after return disappears", sourceUnreachable)
    , ("source closure allocation survives a dead binding", sourceDeadClosure)
    , ("source closure constants fold inside the body", sourceClosureFold)
    , ("source optimizer output remains accepted by CorePrep", sourceReachesCorePrep)
    ]

compiled :: String -> Maybe FrontendArtifacts
compiled body = case compileToCorePrep (CompilerInput "optimizer-source.vxs" (source body)) of
    Left _ -> Nothing
    Right artifacts -> Just artifacts

source :: String -> String
source body =
    unlines
        [ "namespace OptimizerSource;"
        , "class Program {"
        , "  int Value() {"
        , body
        , "  }"
        , "}"
        ]

singleBody :: FrontendArtifacts -> Maybe [CoreStatement]
singleBody artifacts = case coreModuleFunctions (artifactOptimizedCore artifacts) of
    [function] -> Just (coreFunctionBody function)
    _ -> Nothing

singleReturn :: FrontendArtifacts -> Maybe CoreExpression
singleReturn artifacts = do
    body <- singleBody artifacts
    case body of
        [CoreReturn value] -> Just value
        _ -> Nothing

integer :: Integer -> CoreExpression
integer value = CoreLiteral (CoreInteger value) intType

immutableSourceChain :: Bool
immutableSourceChain = case compiled "final int base = 40; final int answer = base + 2; return answer;" of
    Just artifacts -> singleReturn artifacts == Just (integer 42)
    Nothing -> False

mutableSourceValue :: Bool
mutableSourceValue = case compiled "int value = 1; value = 2; return value;" of
    Just artifacts -> case singleBody artifacts of
        Just [CoreBind binding, CoreAssign target assigned, CoreReturn returned] ->
            coreBindingMutable binding
                && resolvedSymbol (coreBindingName binding) == resolvedSymbol target
                && assigned == integer 2
                && returned == CoreVariable target intType
        _ -> False
    Nothing -> False

sourceTrueBranch :: Bool
sourceTrueBranch = case compiled "if (true) { return 1; } else { return 2; }" of
    Just artifacts -> singleReturn artifacts == Just (integer 1)
    Nothing -> False

sourceFalseBranch :: Bool
sourceFalseBranch = case compiled "if (false) { return 1; } else { return 2; }" of
    Just artifacts -> singleReturn artifacts == Just (integer 2)
    Nothing -> False

sourceNumericFalse :: Bool
sourceNumericFalse = case compiled "if (0) { return 1; } else { return 2; }" of
    Just artifacts -> singleReturn artifacts == Just (integer 2)
    Nothing -> False

sourceNumericTrue :: Bool
sourceNumericTrue = case compiled "if (-1) { return 1; } else { return 2; }" of
    Just artifacts -> singleReturn artifacts == Just (integer 1)
    Nothing -> False

sourceDeadLocal :: Bool
sourceDeadLocal = case compiled "int unused = 42; return 1;" of
    Just artifacts -> singleReturn artifacts == Just (integer 1)
    Nothing -> False

sourceDiscardedCall :: Bool
sourceDiscardedCall = case compileToCorePrep (CompilerInput "optimizer-call.vxs" callSource) of
    Right artifacts -> case coreModuleFunctions (artifactOptimizedCore artifacts) of
        [_helper, caller] -> case coreFunctionBody caller of
            [CoreEvaluate CoreApply {}, CoreReturn (CoreLiteral (CoreInteger 1) _)] -> True
            _ -> False
        _ -> False
    Left _ -> False
    where
        callSource =
            unlines
                [ "namespace OptimizerSource;"
                , "class Program {"
                , "  int Helper() { return 42; }"
                , "  int Value() { Helper(); return 1; }"
                , "}"
                ]

sourceUnreachable :: Bool
sourceUnreachable = case compiled "return 1; int unreachable = 2;" of
    Just artifacts -> singleReturn artifacts == Just (integer 1)
    Nothing -> False

sourceDeadClosure :: Bool
sourceDeadClosure = case compiled "auto callable = \\ -> 42; return 1;" of
    Just artifacts -> case singleBody artifacts of
        Just [CoreEvaluate CoreClosure {}, CoreReturn (CoreLiteral (CoreInteger 1) _)] -> True
        _ -> False
    Nothing -> False

sourceClosureFold :: Bool
sourceClosureFold = case compiled "auto callable = \\ -> 40 + 2; return 1;" of
    Just artifacts -> case singleBody artifacts of
        Just [CoreEvaluate (CoreClosure _ _ _ [CoreReturn value] _), CoreReturn _] -> value == integer 42
        _ -> False
    Nothing -> False

sourceReachesCorePrep :: Bool
sourceReachesCorePrep = case compiled "final int left = 20; final int right = 22; return left + right;" of
    Just artifacts ->
        singleReturn artifacts == Just (integer 42)
            && not (null (corePrepModuleFunctions (artifactCorePrep artifacts)))
    Nothing -> False
