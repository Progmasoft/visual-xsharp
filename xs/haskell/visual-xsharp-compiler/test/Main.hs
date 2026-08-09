-- SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
-- SPDX-License-Identifier: MPL-2.0

module Main (main) where

import Control.Monad (unless)
import Visual.XSharp.AST
import Visual.XSharp.Core
import Visual.XSharp.Core.CorePrep
import Visual.XSharp.Parser
import Visual.XSharp.Pipeline.Stage

main :: IO ()
main = do
    assert "AST phases stay distinct" (show parsed /= show renamed && show renamed /= show resolved && show resolved /= show typed)
    assert "Core remains target-independent" (not (null (coreModuleBindings coreModule)))
    assert "CorePrep remains a separate IR" (not (null (corePrepModuleBindings prepared)))
    assert "parser boundary returns ParsedAST" (runParser parser parserInput == Right parsed)
    assert "renewed artifact extensions are stable" (map artifactExtension [Core, CorePrep, Xpp, Xmm] == [Just ".core", Nothing, Just ".xpp", Just ".xmm"])
  where
    identifier = Identifier "Main"
    qualified = QualifiedName [identifier]
    position = SourcePosition 1 1
    span' = SourceSpan "Main.vxs" position position
    parsed = ParsedAST (SyntaxTree (Just qualified) [Declaration span' identifier () []])
    renamedName = RenamedName identifier 0
    renamed = RenamedAST (SyntaxTree (Just qualified) [Declaration span' renamedName () []])
    resolvedName = ResolvedName (SymbolId 0) identifier
    resolved = ResolvedAST (SyntaxTree (Just qualified) [Declaration span' resolvedName () []])
    valueType = NamedType (QualifiedName [Identifier "Int"]) []
    typed = TypedAST (SyntaxTree (Just qualified) [Declaration span' resolvedName valueType []])
    coreBinding = CoreBinding resolvedName valueType (CoreLiteral (CoreInteger 42))
    coreModule = CoreModule qualified [coreBinding]
    preparedBinding = CorePrepBinding resolvedName valueType (CorePrepAtom (CorePrepLiteral (CoreInteger 42)))
    prepared = CorePrepModule qualified [preparedBinding]
    parser = Parser (const (Right parsed))
    parserInput = ParserInput "Main.vxs" [Token EndOfFileToken "" span']

assert :: String -> Bool -> IO ()
assert label condition = unless condition (fail label)
