-- SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
-- SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.1

{- | Public syntax and phase-indexed tree model shared by the Visual X#
frontend. Parsed trees retain source spelling, renamed trees attach stable
identities, resolved trees attach declarations, and typed trees attach
semantic types without changing the syntax structure.
-}
module Visual.XSharp.AST
    ( Identifier (..)
    , QualifiedName (..)
    , SourcePosition (..)
    , SourceSpan (..)
    , SyntaxTree (..)
    , Declaration (..)
    , TemplateParameter (..)
    , TemplateParameterKind (..)
    , TemplateParameterShape (..)
    , TemplateParameterShapeKind (..)
    , TemplateDefault (..)
    , Parameter (..)
    , Block (..)
    , Statement (..)
    , Expression (..)
    , Pattern (..)
    , RelationalPatternOperator (..)
    , CallableBody (..)
    , Capture (..)
    , CaptureMode (..)
    , Literal (..)
    , UnaryOperator (..)
    , BinaryOperator (..)
    , TypeSyntax (..)
    , TemplateValueSyntax (..)
    , TemplateArgumentSyntax (..)
    , BindingKind (..)
    , Access (..)
    , ParsedAST (..)
    , RenamedName (..)
    , RenamedAST (..)
    , SymbolId (..)
    , ResolvedName (..)
    , ResolvedAST (..)
    , Type (..)
    , TemplateArgument (..)
    , TemplateValue (..)
    , TypedAST (..)
    , boolType
    , intType
    , unitType
    , voidType
    , stringType
    , namedType
    ) where

-- | One source-language identifier in its original spelling.
newtype Identifier = Identifier {identifierText :: String}
    deriving stock (Eq, Ord, Read, Show)

-- | Name split into ordered namespace, type, and member segments.
newtype QualifiedName = QualifiedName {qualifiedNameParts :: [Identifier]}
    deriving stock (Eq, Ord, Read, Show)

-- | Zero-based source coordinate used by parser and diagnostic spans.
data SourcePosition = SourcePosition {sourceLine :: Int, sourceColumn :: Int}
    deriving stock (Eq, Ord, Read, Show)

-- | Half-open source range with the identity of the source file.
data SourceSpan = SourceSpan
    {sourceFile :: FilePath, sourceStart :: SourcePosition, sourceEnd :: SourcePosition}
    deriving stock (Eq, Ord, Read, Show)

-- | Type expression written in source before semantic resolution.
data TypeSyntax
    = ExplicitType Identifier
    | QualifiedTypeSyntax QualifiedName [TemplateArgumentSyntax]
    | BuiltinArrayTypeSyntax TypeSyntax
    | ArrayTypeSyntax TypeSyntax
    | FixedArrayTypeSyntax TypeSyntax TemplateValueSyntax
    | DictionaryTypeSyntax TypeSyntax TypeSyntax
    | CallableTypeSyntax [TypeSyntax] TypeSyntax
    | AutoType
    deriving stock (Eq, Ord, Read, Show)

{- | Restricted compile-time expression used for generic value arguments.

It deliberately excludes calls, closures, and other runtime expressions,
preventing runtime behavior from leaking into a specialization identity.
The type checker evaluates this tree before constructing Core.
-}
data TemplateValueSyntax
    = TemplateIntegerSyntax SourceSpan Integer
    | TemplateCharacterSyntax SourceSpan Integer
    | TemplateBooleanSyntax SourceSpan Bool
    | TemplateNameSyntax SourceSpan QualifiedName
    | TemplateUnarySyntax SourceSpan UnaryOperator TemplateValueSyntax
    | TemplateBinarySyntax SourceSpan BinaryOperator TemplateValueSyntax TemplateValueSyntax
    deriving stock (Eq, Ord, Read, Show)

-- | One generic argument, preserving the distinction between types and values.
data TemplateArgumentSyntax
    = TemplateTypeSyntax TypeSyntax
    | TemplateValueArgumentSyntax TemplateValueSyntax
    deriving stock (Eq, Ord, Read, Show)

-- | Source access modifier recorded on a declaration.
data Access = DefaultAccess | PublicAccess | InternalAccess | ProtectedAccess | PrivateAccess
    deriving stock (Eq, Ord, Read, Show)

-- | Module-level namespace and ordered declarations, parameterized by phase.
data SyntaxTree name annotation = SyntaxTree
    { syntaxNamespace :: Maybe QualifiedName
    , syntaxDeclarations :: [Declaration name annotation]
    }
    deriving stock (Eq, Ord, Read, Show)

-- | Type, function, or generic type declaration in a syntax tree.
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
    | TemplateTypeDeclaration
        { declarationSpan :: SourceSpan
        , declarationName :: name
        , declarationAnnotation :: annotation
        , declarationTemplateParameters :: [TemplateParameter name annotation]
        , typeMembers :: [Declaration name annotation]
        }
    deriving stock (Eq, Ord, Read, Show)

{- | Generic parameter with its declaration span, category, pack flag, and default.

Template metadata remains present through @TypedAST@: renaming assigns each
parameter a stable symbol and type checking uses it in bodies and array sizes.
-}
data TemplateParameter name annotation = TemplateParameter
    { templateParameterSpan :: SourceSpan
    , templateParameterName :: name
    , templateParameterAnnotation :: annotation
    , templateParameterKind :: TemplateParameterKind
    , templateParameterIsPack :: Bool
    , templateParameterDefault :: Maybe TemplateDefault
    }
    deriving stock (Eq, Ord, Read, Show)

-- | Generic parameter category: type, value, or nested template signature.
data TemplateParameterKind
    = TemplateTypeParameter
    | TemplateValueParameterKind TypeSyntax
    | TemplateTemplateParameter [TemplateParameterShape]
    deriving stock (Eq, Ord, Read, Show)

{- | Anonymous shape of one parameter in a template-template signature.

These shapes describe accepted argument categories without introducing
artificial names into lexical scope.
-}
data TemplateParameterShape = TemplateParameterShape
    { templateParameterShapeKind :: TemplateParameterShapeKind
    , templateParameterShapeIsPack :: Bool
    }
    deriving stock (Eq, Ord, Read, Show)

-- | Accepted argument category at one level of a template-template signature.
data TemplateParameterShapeKind
    = TemplateTypeParameterShape
    | TemplateValueParameterShape TypeSyntax
    | TemplateTemplateParameterShape [TemplateParameterShape]
    deriving stock (Eq, Ord, Read, Show)

-- | Default argument attached to a generic parameter.
data TemplateDefault
    = TemplateTypeDefault TypeSyntax
    | TemplateValueDefault TemplateValueSyntax
    deriving stock (Eq, Ord, Read, Show)

-- | Callable parameter with source location and unresolved type syntax.
data Parameter name annotation = Parameter
    { parameterSpan :: SourceSpan
    , parameterName :: name
    , parameterAnnotation :: annotation
    , parameterTypeSyntax :: TypeSyntax
    }
    deriving stock (Eq, Ord, Read, Show)

-- | Ordered statement sequence representing a lexical block.
newtype Block name annotation = Block {blockStatements :: [Statement name annotation]}
    deriving stock (Eq, Ord, Read, Show)

-- | Mutability requested for a local or foreach binding.
data BindingKind = ImmutableBinding | MutableBinding
    deriving stock (Eq, Ord, Read, Show)

{- | Ownership behavior requested for a callable capture.

'StrongCapture' is also the default for ordinary captures; type checking
refines storage according to the captured value category.
-}
data CaptureMode = StrongCapture | WeakCapture | UnownedCapture
    deriving stock (Eq, Ord, Read, Show)

{- | Captured binding with an inner name and an optional outer-scope initializer.

A shorthand capture initially has no initializer. The renamer materializes
its outer-name read so later phases do not need to recover source spelling.
-}
data Capture name annotation = Capture
    { captureSpan :: SourceSpan
    , captureMode :: CaptureMode
    , captureName :: name
    , captureAnnotation :: annotation
    , captureInitializer :: Maybe (Expression name annotation)
    }
    deriving stock (Eq, Ord, Read, Show)

-- | Expression-bodied or block-bodied callable body.
data CallableBody name annotation
    = CallableExpressionBody (Expression name annotation)
    | CallableBlockBody (Block name annotation)
    deriving stock (Eq, Ord, Read, Show)

-- | Statement forms supported by the parsed and typed Visual X# trees.
data Statement name annotation
    = BindingStatement SourceSpan BindingKind TypeSyntax name annotation (Expression name annotation)
    | AssignmentStatement SourceSpan name annotation (Expression name annotation)
    | ReturnStatement SourceSpan (Maybe (Expression name annotation))
    | IfStatement SourceSpan (Expression name annotation) (Block name annotation) (Maybe (Block name annotation))
    | WhileStatement SourceSpan (Expression name annotation) (Block name annotation)
    | DoWhileStatement SourceSpan (Block name annotation) (Expression name annotation)
    | ForStatement
        SourceSpan
        (Maybe (Statement name annotation))
        (Maybe (Expression name annotation))
        [Statement name annotation]
        (Block name annotation)
    | ForEachStatement
        SourceSpan
        BindingKind
        TypeSyntax
        name
        annotation
        (Expression name annotation)
        (Block name annotation)
    | IncrementStatement SourceSpan name annotation Bool
    | -- @target op= value@ reads and writes one named storage location. The
      -- annotation is the target type; the operator is the binary operation
      -- whose result is stored back.
      CompoundAssignmentStatement SourceSpan BinaryOperator name annotation (Expression name annotation)
    | -- @_ = value;@ evaluates its operand and drops the result. It is a
      -- statement of its own, not an assignment to a binding named @_@.
      DiscardStatement SourceSpan (Expression name annotation)
    | BreakStatement SourceSpan (Maybe (Expression name annotation))
    | ContinueStatement SourceSpan
    | ExpressionStatement SourceSpan (Expression name annotation) Bool
    deriving stock (Eq, Ord, Read, Show)

-- | Expression forms retained from parsing through type checking.
data Expression name annotation
    = NameExpression SourceSpan name annotation
    | LiteralExpression SourceSpan Literal annotation
    | -- A member selector preserves the receiver and the member spelling until
      -- the checker knows whether the receiver denotes a type or a value. The
      -- current executable subset accepts only type-qualified method calls.
      MemberAccessExpression SourceSpan (Expression name annotation) Identifier annotation
    | CallExpression SourceSpan (Expression name annotation) [Expression name annotation] annotation
    | UnaryExpression SourceSpan UnaryOperator (Expression name annotation) annotation
    | BinaryExpression SourceSpan BinaryOperator (Expression name annotation) (Expression name annotation) annotation
    | IsPatternExpression SourceSpan (Expression name annotation) (Pattern name annotation) annotation
    | -- @condition ? first : second@ evaluates exactly one arm.
      ConditionalExpression
        SourceSpan
        (Expression name annotation)
        (Expression name annotation)
        (Expression name annotation)
        annotation
    | -- @left ?: fallback@ yields the left value when it is true in Boolean
      -- context and evaluates the fallback only otherwise. The left operand
      -- is evaluated once.
      CoalesceExpression SourceSpan (Expression name annotation) (Expression name annotation) annotation
    | CallableExpression
        SourceSpan
        Bool
        [Capture name annotation]
        [Parameter name annotation]
        (CallableBody name annotation)
        annotation
    deriving stock (Eq, Ord, Read, Show)

{- | Pattern tested by an @is@ expression.

Pattern @and@ and @or@ combine tests beneath @is@; they are distinct from
both eager and short-circuit Boolean operators in 'Expression'.
-}
data Pattern name annotation
    = WildcardPattern SourceSpan annotation
    | NullPattern SourceSpan annotation
    | LiteralPattern SourceSpan Literal annotation
    | TypePattern SourceSpan TypeSyntax annotation
    | RelationalPattern SourceSpan RelationalPatternOperator Literal annotation
    | NotPattern SourceSpan (Pattern name annotation) annotation
    | AndPattern SourceSpan (Pattern name annotation) (Pattern name annotation) annotation
    | OrPattern SourceSpan (Pattern name annotation) (Pattern name annotation) annotation
    deriving stock (Eq, Ord, Read, Show)

-- | Relational comparison used by a relational pattern.
data RelationalPatternOperator
    = PatternLessThan
    | PatternLessEqual
    | PatternGreaterThan
    | PatternGreaterEqual
    | PatternEqual
    | PatternNotEqual
    deriving stock (Eq, Ord, Read, Show)

-- | Literal token value after lexical escape decoding.
data Literal
    = IntegerLiteral Integer
    | FloatingLiteral String
    | CharacterLiteral Integer
    | BooleanLiteral Bool
    | StringLiteral String
    | UnitLiteral
    deriving stock (Eq, Ord, Read, Show)

-- | Unary operator recognized by the parser.
data UnaryOperator = UnaryPlus | UnaryNegate | LogicalNot | BitwiseNot
    deriving stock (Eq, Ord, Read, Show)

-- | Binary arithmetic, comparison, bitwise, or Boolean operator.
data BinaryOperator
    = Add
    | Subtract
    | Multiply
    | Divide
    | FloorDivide
    | Remainder
    | Power
    | ShiftLeft
    | ShiftRight
    | BitwiseAnd
    | BitwiseXor
    | BitwiseOr
    | LessThan
    | LessEqual
    | GreaterThan
    | GreaterEqual
    | Equal
    | NotEqual
    | LogicalAnd
    | LogicalOr
    deriving stock (Eq, Ord, Read, Show)

-- | Parser output with source identifiers and no semantic annotations.
newtype ParsedAST = ParsedAST {parsedSyntaxTree :: SyntaxTree Identifier ()}
    deriving stock (Eq, Ord, Read, Show)

{- | Renamed source name carrying its preserved spelling and unique identity.

Zero is reserved as the native/wire no-symbol sentinel. The renamer assigns
positive identities; negative identities exist only to diagnose unresolved
names during resolution.
-}
data RenamedName = RenamedName {renamedSpelling :: Identifier, renamedUnique :: Int}
    deriving stock (Eq, Ord, Read, Show)

-- | Syntax tree after declarations and references receive unique identities.
newtype RenamedAST = RenamedAST {renamedSyntaxTree :: SyntaxTree RenamedName ()}
    deriving stock (Eq, Ord, Read, Show)

-- | Stable numeric symbol identity used by resolved and native representations.
newtype SymbolId = SymbolId {symbolIdValue :: Int}
    deriving stock (Eq, Ord, Read, Show)

-- | Reference to a resolved declaration with its original spelling.
data ResolvedName = ResolvedName {resolvedSymbol :: SymbolId, resolvedSpelling :: Identifier}
    deriving stock (Eq, Ord, Read, Show)

-- | Syntax tree whose names point to declarations but are not yet type-checked.
newtype ResolvedAST = ResolvedAST {resolvedSyntaxTree :: SyntaxTree ResolvedName ()}
    deriving stock (Eq, Ord, Read, Show)

-- | Semantic type attached to resolved expressions and declarations.
data Type
    = NamedType QualifiedName [TemplateArgument]
    | FunctionType [Type] Type
    | TypeVariable ResolvedName
    | ErrorType
    deriving stock (Eq, Ord, Read, Show)

{- | Evaluated generic argument, retaining its original position in the list.

The ordered sum keeps @Example<int, 4, String>@ distinct from
@Example<int, String, 4>@ when forming specialization identities.
-}
data TemplateArgument
    = TypeTemplateArgument Type
    | ValueTemplateArgument TemplateValue
    deriving stock (Eq, Ord, Read, Show)

{- | Canonical value accepted as a generic argument after type checking.

Integers are unbounded so specialization does not depend on host word size.
Parameter references remain representable for generic bodies.
-}
data TemplateValue
    = IntegerTemplateValue Integer
    | BooleanTemplateValue Bool
    | CharacterTemplateValue Integer
    | TemplateValueParameter ResolvedName
    deriving stock (Eq, Ord, Read, Show)

-- | Resolved syntax tree with semantic types attached to its annotations.
newtype TypedAST = TypedAST {typedSyntaxTree :: SyntaxTree ResolvedName Type}
    deriving stock (Eq, Ord, Read, Show)

-- | Construct a one-segment nominal type without generic arguments.
namedType :: String -> Type
namedType value = NamedType (QualifiedName [Identifier value]) []

-- | Canonical built-in Boolean type.
boolType :: Type
boolType = namedType "bool"

-- | Canonical built-in signed integer type used by the current frontend.
intType :: Type
intType = namedType "int"

-- | Canonical unit type for expressions that produce no value.
unitType :: Type
unitType = namedType "unit"

-- | Canonical void type for procedure declarations and return checking.
voidType :: Type
voidType = namedType "void"

-- | Canonical standard string type.
stringType :: Type
stringType = namedType "String"
