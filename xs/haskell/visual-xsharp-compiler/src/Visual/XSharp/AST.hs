-- SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
-- SPDX-License-Identifier: MPL-2.0

module Visual.XSharp.AST
    ( Identifier (..), QualifiedName (..), SourcePosition (..), SourceSpan (..)
    , SyntaxTree (..), Declaration (..), Parameter (..), Block (..), Statement (..)
    , Expression (..), Literal (..), UnaryOperator (..), BinaryOperator (..)
    , TypeSyntax (..), BindingKind (..), Access (..), ParsedAST (..), RenamedName (..)
    , RenamedAST (..), SymbolId (..), ResolvedName (..), ResolvedAST (..)
    , Type (..), TypedAST (..), boolType, intType, unitType, stringType
    ) where

newtype Identifier = Identifier { identifierText :: String }
    deriving (Eq, Ord, Read, Show)

newtype QualifiedName = QualifiedName { qualifiedNameParts :: [Identifier] }
    deriving (Eq, Ord, Read, Show)

data SourcePosition = SourcePosition { sourceLine :: Int, sourceColumn :: Int }
    deriving (Eq, Ord, Read, Show)

data SourceSpan = SourceSpan
    { sourceFile :: FilePath, sourceStart :: SourcePosition, sourceEnd :: SourcePosition }
    deriving (Eq, Ord, Read, Show)

data TypeSyntax = ExplicitType Identifier | AutoType
    deriving (Eq, Ord, Read, Show)
data Access = DefaultAccess | PublicAccess | InternalAccess | ProtectedAccess | PrivateAccess
    deriving (Eq, Ord, Read, Show)

data SyntaxTree name annotation = SyntaxTree
    { syntaxNamespace :: Maybe QualifiedName
    , syntaxDeclarations :: [Declaration name annotation]
    } deriving (Eq, Ord, Read, Show)

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
    } deriving (Eq, Ord, Read, Show)

newtype Block name annotation = Block { blockStatements :: [Statement name annotation] }
    deriving (Eq, Ord, Read, Show)

data BindingKind = ImmutableBinding | MutableBinding
    deriving (Eq, Ord, Read, Show)

data Statement name annotation
    = BindingStatement SourceSpan BindingKind TypeSyntax name annotation (Expression name annotation)
    | AssignmentStatement SourceSpan name annotation (Expression name annotation)
    | ReturnStatement SourceSpan (Maybe (Expression name annotation))
    | IfStatement SourceSpan (Expression name annotation) (Block name annotation) (Maybe (Block name annotation))
    | ExpressionStatement SourceSpan (Expression name annotation)
    deriving (Eq, Ord, Read, Show)

data Expression name annotation
    = NameExpression SourceSpan name annotation
    | LiteralExpression SourceSpan Literal annotation
    | CallExpression SourceSpan (Expression name annotation) [Expression name annotation] annotation
    | UnaryExpression SourceSpan UnaryOperator (Expression name annotation) annotation
    | BinaryExpression SourceSpan BinaryOperator (Expression name annotation) (Expression name annotation) annotation
    deriving (Eq, Ord, Read, Show)

data Literal = IntegerLiteral Integer | BooleanLiteral Bool | StringLiteral String | UnitLiteral
    deriving (Eq, Ord, Read, Show)

data UnaryOperator = UnaryPlus | UnaryNegate | LogicalNot
    deriving (Eq, Ord, Read, Show)

data BinaryOperator
    = Add | Subtract | Multiply | Divide | FloorDivide | Remainder
    | LessThan | LessEqual | GreaterThan | GreaterEqual | Equal | NotEqual
    | LogicalAnd | LogicalOr
    deriving (Eq, Ord, Read, Show)

newtype ParsedAST = ParsedAST { parsedSyntaxTree :: SyntaxTree Identifier () }
    deriving (Eq, Ord, Read, Show)

data RenamedName = RenamedName { renamedSpelling :: Identifier, renamedUnique :: Int }
    deriving (Eq, Ord, Read, Show)

newtype RenamedAST = RenamedAST { renamedSyntaxTree :: SyntaxTree RenamedName () }
    deriving (Eq, Ord, Read, Show)

newtype SymbolId = SymbolId { symbolIdValue :: Int }
    deriving (Eq, Ord, Read, Show)

data ResolvedName = ResolvedName { resolvedSymbol :: SymbolId, resolvedSpelling :: Identifier }
    deriving (Eq, Ord, Read, Show)

newtype ResolvedAST = ResolvedAST { resolvedSyntaxTree :: SyntaxTree ResolvedName () }
    deriving (Eq, Ord, Read, Show)

data Type
    = NamedType QualifiedName [Type]
    | FunctionType [Type] Type
    | TypeVariable ResolvedName
    | ErrorType
    deriving (Eq, Ord, Read, Show)

newtype TypedAST = TypedAST { typedSyntaxTree :: SyntaxTree ResolvedName Type }
    deriving (Eq, Ord, Read, Show)

named :: String -> Type
named value = NamedType (QualifiedName [Identifier value]) []

boolType, intType, unitType, stringType :: Type
boolType = named "bool"
intType = named "int"
unitType = named "unit"
stringType = named "string"
