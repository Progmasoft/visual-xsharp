-- SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
-- SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.0

module Visual.XSharp.AST
    ( Identifier (..)
    , QualifiedName (..)
    , SourcePosition (..)
    , SourceSpan (..)
    , SyntaxTree (..)
    , Declaration (..)
    , Parameter (..)
    , Block (..)
    , Statement (..)
    , Expression (..)
    , CallableBody (..)
    , Capture (..)
    , CaptureMode (..)
    , Literal (..)
    , UnaryOperator (..)
    , BinaryOperator (..)
    , TypeSyntax (..)
    , BindingKind (..)
    , Access (..)
    , ParsedAST (..)
    , RenamedName (..)
    , RenamedAST (..)
    , SymbolId (..)
    , ResolvedName (..)
    , ResolvedAST (..)
    , Type (..)
    , TypedAST (..)
    , boolType
    , intType
    , unitType
    , voidType
    , stringType
    , namedType
    ) where

newtype Identifier = Identifier {identifierText :: String}
    deriving (Eq, Ord, Read, Show)

newtype QualifiedName = QualifiedName {qualifiedNameParts :: [Identifier]}
    deriving (Eq, Ord, Read, Show)

data SourcePosition = SourcePosition {sourceLine :: Int, sourceColumn :: Int}
    deriving (Eq, Ord, Read, Show)

data SourceSpan = SourceSpan
    {sourceFile :: FilePath, sourceStart :: SourcePosition, sourceEnd :: SourcePosition}
    deriving (Eq, Ord, Read, Show)

data TypeSyntax = ExplicitType Identifier | AutoType
    deriving (Eq, Ord, Read, Show)
data Access = DefaultAccess | PublicAccess | InternalAccess | ProtectedAccess | PrivateAccess
    deriving (Eq, Ord, Read, Show)

data SyntaxTree name annotation = SyntaxTree
    { syntaxNamespace :: Maybe QualifiedName
    , syntaxDeclarations :: [Declaration name annotation]
    }
    deriving (Eq, Ord, Read, Show)

data Declaration name annotation
    = TypeDeclaration
        { declarationSpan :: SourceSpan
        , declarationName :: name
        , declarationAnnotation :: annotation
        , typeMembers :: [Declaration name annotation]
        }
    | FunctionDeclaration
        { declarationSpan :: SourceSpan
        , declarationName :: name
        , declarationAnnotation :: annotation
        , declarationReturnSyntax :: TypeSyntax
        , declarationParameters :: [Parameter name annotation]
        , declarationBody :: Block name annotation
        , declarationIsStatic :: Bool
        , declarationAccess :: Access
        }
    deriving (Eq, Ord, Read, Show)

data Parameter name annotation = Parameter
    { parameterSpan :: SourceSpan
    , parameterName :: name
    , parameterAnnotation :: annotation
    , parameterTypeSyntax :: TypeSyntax
    }
    deriving (Eq, Ord, Read, Show)

newtype Block name annotation = Block {blockStatements :: [Statement name annotation]}
    deriving (Eq, Ord, Read, Show)

data BindingKind = ImmutableBinding | MutableBinding
    deriving (Eq, Ord, Read, Show)

-- Capture modes are source-level ownership requests.  StrongCapture is also
-- used for ordinary implicit and explicit captures; the type checker later
-- refines its storage behavior from the captured value category.
data CaptureMode = StrongCapture | WeakCapture | UnownedCapture
    deriving (Eq, Ord, Read, Show)

-- A capture binding owns a name visible inside the callable and an optional
-- initializer evaluated in the surrounding scope.  During parsing the
-- initializer is absent for `[value]`; the renamer materializes the outer-name
-- read so later stages never need to recover lexical spelling.
data Capture name annotation = Capture
    { captureSpan :: SourceSpan
    , captureMode :: CaptureMode
    , captureName :: name
    , captureAnnotation :: annotation
    , captureInitializer :: Maybe (Expression name annotation)
    }
    deriving (Eq, Ord, Read, Show)

data CallableBody name annotation
    = CallableExpressionBody (Expression name annotation)
    | CallableBlockBody (Block name annotation)
    deriving (Eq, Ord, Read, Show)

data Statement name annotation
    = BindingStatement SourceSpan BindingKind TypeSyntax name annotation (Expression name annotation)
    | AssignmentStatement SourceSpan name annotation (Expression name annotation)
    | ReturnStatement SourceSpan (Maybe (Expression name annotation))
    | IfStatement SourceSpan (Expression name annotation) (Block name annotation) (Maybe (Block name annotation))
    | ExpressionStatement SourceSpan (Expression name annotation) Bool
    deriving (Eq, Ord, Read, Show)

data Expression name annotation
    = NameExpression SourceSpan name annotation
    | LiteralExpression SourceSpan Literal annotation
    | CallExpression SourceSpan (Expression name annotation) [Expression name annotation] annotation
    | UnaryExpression SourceSpan UnaryOperator (Expression name annotation) annotation
    | BinaryExpression SourceSpan BinaryOperator (Expression name annotation) (Expression name annotation) annotation
    | CallableExpression
        SourceSpan
        Bool
        [Capture name annotation]
        [Parameter name annotation]
        (CallableBody name annotation)
        annotation
    deriving (Eq, Ord, Read, Show)

data Literal
    = IntegerLiteral Integer
    | FloatingLiteral String
    | CharacterLiteral Integer
    | BooleanLiteral Bool
    | StringLiteral String
    | UnitLiteral
    deriving (Eq, Ord, Read, Show)

data UnaryOperator = UnaryPlus | UnaryNegate | LogicalNot
    deriving (Eq, Ord, Read, Show)

data BinaryOperator
    = Add
    | Subtract
    | Multiply
    | Divide
    | FloorDivide
    | Remainder
    | LessThan
    | LessEqual
    | GreaterThan
    | GreaterEqual
    | Equal
    | NotEqual
    | LogicalAnd
    | LogicalOr
    deriving (Eq, Ord, Read, Show)

newtype ParsedAST = ParsedAST {parsedSyntaxTree :: SyntaxTree Identifier ()}
    deriving (Eq, Ord, Read, Show)

-- Zero is reserved as the wire/native "no symbol" sentinel.  The renamer
-- allocates positive identities; negative values exist only long enough for
-- name resolution to diagnose a missing source name.
data RenamedName = RenamedName {renamedSpelling :: Identifier, renamedUnique :: Int}
    deriving (Eq, Ord, Read, Show)

newtype RenamedAST = RenamedAST {renamedSyntaxTree :: SyntaxTree RenamedName ()}
    deriving (Eq, Ord, Read, Show)

newtype SymbolId = SymbolId {symbolIdValue :: Int}
    deriving (Eq, Ord, Read, Show)

data ResolvedName = ResolvedName {resolvedSymbol :: SymbolId, resolvedSpelling :: Identifier}
    deriving (Eq, Ord, Read, Show)

newtype ResolvedAST = ResolvedAST {resolvedSyntaxTree :: SyntaxTree ResolvedName ()}
    deriving (Eq, Ord, Read, Show)

data Type
    = NamedType QualifiedName [Type]
    | FunctionType [Type] Type
    | TypeVariable ResolvedName
    | ErrorType
    deriving (Eq, Ord, Read, Show)

newtype TypedAST = TypedAST {typedSyntaxTree :: SyntaxTree ResolvedName Type}
    deriving (Eq, Ord, Read, Show)

namedType :: String -> Type
namedType value = NamedType (QualifiedName [Identifier value]) []

boolType, intType, unitType, voidType, stringType :: Type
boolType = namedType "bool"
intType = namedType "int"
unitType = namedType "unit"
voidType = namedType "void"
stringType = namedType "string"
