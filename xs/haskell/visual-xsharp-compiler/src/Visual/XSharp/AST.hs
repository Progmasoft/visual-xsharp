-- SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
-- SPDX-License-Identifier: MPL-2.0

module Visual.XSharp.AST
    ( Identifier (..)
    , QualifiedName (..)
    , SourcePosition (..)
    , SourceSpan (..)
    , SyntaxTree (..)
    , Declaration (..)
    , ParsedAST (..)
    , RenamedName (..)
    , RenamedAST (..)
    , SymbolId (..)
    , ResolvedName (..)
    , ResolvedAST (..)
    , Type (..)
    , TypedAST (..)
    ) where

-- | A case-sensitive source identifier.
newtype Identifier = Identifier
    { identifierText :: String
    }
    deriving (Eq, Ord, Read, Show)

-- | A namespace-qualified Visual X# name.
newtype QualifiedName = QualifiedName
    { qualifiedNameParts :: [Identifier]
    }
    deriving (Eq, Ord, Read, Show)

data SourcePosition = SourcePosition
    { sourceLine :: Int
    , sourceColumn :: Int
    }
    deriving (Eq, Ord, Read, Show)

data SourceSpan = SourceSpan
    { sourceFile :: FilePath
    , sourceStart :: SourcePosition
    , sourceEnd :: SourcePosition
    }
    deriving (Eq, Ord, Read, Show)

-- | Phase-independent tree shape. Names and annotations change at each phase.
data SyntaxTree name annotation = SyntaxTree
    { syntaxNamespace :: Maybe QualifiedName
    , syntaxDeclarations :: [Declaration name annotation]
    }
    deriving (Eq, Ord, Read, Show)

data Declaration name annotation = Declaration
    { declarationSpan :: SourceSpan
    , declarationName :: name
    , declarationAnnotation :: annotation
    , nestedDeclarations :: [Declaration name annotation]
    }
    deriving (Eq, Ord, Read, Show)

-- | Parser output. Names have source spelling and no semantic annotation.
newtype ParsedAST = ParsedAST
    { parsedSyntaxTree :: SyntaxTree Identifier ()
    }
    deriving (Eq, Ord, Read, Show)

data RenamedName = RenamedName
    { renamedSpelling :: Identifier
    , renamedUnique :: Int
    }
    deriving (Eq, Ord, Read, Show)

-- | Renamer output. Locally unique names precede global name resolution.
newtype RenamedAST = RenamedAST
    { renamedSyntaxTree :: SyntaxTree RenamedName ()
    }
    deriving (Eq, Ord, Read, Show)

newtype SymbolId = SymbolId
    { symbolIdValue :: Int
    }
    deriving (Eq, Ord, Read, Show)

data ResolvedName = ResolvedName
    { resolvedSymbol :: SymbolId
    , resolvedSpelling :: Identifier
    }
    deriving (Eq, Ord, Read, Show)

-- | Renamer and name-resolution output.
newtype ResolvedAST = ResolvedAST
    { resolvedSyntaxTree :: SyntaxTree ResolvedName ()
    }
    deriving (Eq, Ord, Read, Show)

data Type
    = NamedType QualifiedName [Type]
    | FunctionType [Type] Type
    | TypeVariable ResolvedName
    deriving (Eq, Ord, Read, Show)

-- | Type-checker output. Every declaration carries its checked type.
newtype TypedAST = TypedAST
    { typedSyntaxTree :: SyntaxTree ResolvedName Type
    }
    deriving (Eq, Ord, Read, Show)
