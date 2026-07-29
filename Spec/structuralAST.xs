// SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
// SPDX-License-Identifier: MPL-2.0

// Structural AST system:

//
// The structural AST represents parsed X# source code as explicit
// syntax nodes instead of storing complete declarations and bodies only
// as raw source ranges.
//
// The parser constructs structural AST nodes.
//
// The structural AST does not perform:
//
// - Name resolution
// - Import resolution
// - Type checking
// - Send or Sync validation
// - Ownership validation
// - Borrow checking
// - Method resolution
// - Operator resolution
// - Monomorphization
// - Code generation
//
// Those operations belong to later compiler stages.
//


// ============================================================
// Compiler position
// ============================================================

// .xs source code
//     ↓
// Lexing + Parsing
//     ↓
// Structural AST
//     ↓
// Macro expansion
//     ↓
// HIR
//     ↓
// Type checking
//     ↓
// MIR


// Macro declarations and macro calls are represented in the AST.
//
// Macro expansion operates directly on structural AST nodes.
//
// HIR receives the fully expanded AST.


// ============================================================
// Source spans
// ============================================================

// Every AST node stores its source location.

data SourceSpan {
    file_id: UInt
    start_offset: UInt
    end_offset: UInt
    start_line: UInt
    start_column: UInt
    end_line: UInt
    end_column: UInt
}


// SourceSpan is used for:
//
// - Diagnostics
// - Error highlighting
// - Macro expansion locations
// - Dependency tracking
// - Debug information
// - Source mapping


// ============================================================
// Root AST
// ============================================================

data AstFile {
    inner_attributes: [AttributeNode]
    assigned_module: Optional<ProjectModuleAssignment>
    import: [ImportDeclaration]
    declarations: [Declaration]
    span: SourceSpan
}


// assigned_module is external project metadata. It is not parsed from source.
// Direct program sources may have None; xs.module.kts supplies named module
// assignments before semantic analysis.


// ============================================================
// Declaration node family
// ============================================================

enum data Declaration {
    Import: ImportDeclaration,
    Namespace: NamespaceDeclaration,
    Function: FunctionDeclaration,
    Class: ClassDeclaration,
    Interface: InterfaceDeclaration,
    Enum: EnumDeclaration,
    Data: DataDeclaration,
    Variable: VariableDeclaration,
    Macro: MacroDeclaration,
    ExternBlock: ExternBlockDeclaration,
}


// ============================================================
// Attributes
// ============================================================

data AttributeNode {
    path: PathNode
    arguments: [AttributeArgument]
    is_inner: Bool
    span: SourceSpan
}


enum data AttributeArgument {
    Expression: Expression,
    NameValue: AttributeNameValue,
}


data AttributeNameValue {
    name: IdentifierNode
    value: Expression
    span: SourceSpan
}


// Attribute delimiter syntax is built into the parser.
// Official X# attributes live in the Attrs module under std. That module is
// implicitly available for attribute lookup. Explicit `import attrs;` is
// optional.
// Outer attributes use #[...]. Inner file attributes use #![...].
// Attribute semantic names are not keywords.


// ============================================================
// Project module assignment
// ============================================================

data ProjectModuleAssignment {
    name: PathNode
    span: SourceSpan
}


// ============================================================
// Import declarations
// ============================================================

data ImportDeclaration {
    attributes: [AttributeNode]
    kind: ImportKind
    module_path: PathNode
    imported_names: [ImportName]
    alias: IdentifierNode
    span: SourceSpan
}


enum ImportKind {
    ModuleImport,
    SelectedImport,
    WildcardImport,
}


data ImportName {
    name: IdentifierNode
    alias: IdentifierNode
    span: SourceSpan
}


// alias may be None.


// ============================================================
// Namespace declarations
// ============================================================

data NamespaceDeclaration {
    attributes: [AttributeNode]
    path: PathNode
    visibility: VisibilityNode
    declarations: Optional<[Declaration]>
    span: SourceSpan
}


// declarations is None for the one permitted source-scoped `namespace path;`
// form and contains the nested declarations for `namespace path { ... }`.


// ============================================================
// Visibility
// ============================================================

enum VisibilityKind {
    Default,
    Public,
    Private,
    Protected,
    Internal,
}


data VisibilityNode {
    kind: VisibilityKind
    span: SourceSpan
}


// ============================================================
// Identifiers and paths
// ============================================================

data IdentifierNode {
    text: String
    span: SourceSpan
}


data PathNode {
    segments: [IdentifierNode]
    span: SourceSpan
}


// Example:
//
// Http::Response.BodyHandlers

// PathNode
// ├── Http
// ├── Response
// └── BodyHandlers


// ============================================================
// Generic parameters
// ============================================================

data GenericParameter {
    name: IdentifierNode
    constraints: [TypeNode]
    span: SourceSpan
}


// Example:
//
// T: Runnable, Printable

// GenericParameter
// ├── name: T
// └── constraints
//     ├── Runnable
//     └── Printable


// ============================================================
// Extern declarations
// ============================================================

data ExternBlockDeclaration {
    attributes: [AttributeNode]
    abi: TokenNode
    functions: [FunctionDeclaration]
    variables: [VariableDeclaration]
    span: SourceSpan
}


// Source form:
//
// #[repr(C)]
// extern "C" {
//     fn puts(text: std::cffi::CStr) -> Int;
//     static errno: Int;
// }
//
// Functions inside an extern block are body-less external declarations.
// Static variables inside an extern block are foreign global symbols and do not
// have source initializers.


// ============================================================
// Function declarations
// ============================================================

data FunctionDeclaration {
    attributes: [AttributeNode]
    name: IdentifierNode
    visibility: VisibilityNode

    operator_token: TokenNode

    is_async: Bool
    is_static: Bool
    is_incomplete: Bool
    is_extern: Bool
    extern_abi: TokenNode

    generic_parameters: [GenericParameter]
    parameters: [ParameterDeclaration]

    return_type: TypeNode

    body: BlockStatement
    span: SourceSpan
}


// return_type may be None when the function has no explicit
// `-> Type` declaration.
// operator_token may be None. When present, name is operator and the token
// identifies the overloaded operator.
//
// A function without an explicit return type returns no value.


// ============================================================
// Parameters
// ============================================================

data ParameterDeclaration {
    attributes: [AttributeNode]
    name: IdentifierNode
    parameter_type: TypeNode
    span: SourceSpan
}


// ============================================================
// Class declarations
// ============================================================

data ClassDeclaration {
    attributes: [AttributeNode]
    name: IdentifierNode
    visibility: VisibilityNode

    is_incomplete: Bool

    generic_parameters: [GenericParameter]

    base_specifiers: [BaseSpecifier]

    members: [ClassMember]
    span: SourceSpan
}

data BaseSpecifier {
    access: VisibilityNode
    is_virtual: Bool
    base_type: TypeNode
    span: SourceSpan
}


enum data ClassMember {
    Field: FieldDeclaration,
    Method: FunctionDeclaration,
    Constructor: ConstructorDeclaration,
    Destructor: DestructorDeclaration,
    NestedClass: ClassDeclaration,
    NestedEnum: EnumDeclaration,
    NestedData: DataDeclaration,
    NestedMacro: MacroDeclaration,
}


// ============================================================
// Fields
// ============================================================

data FieldDeclaration {
    attributes: [AttributeNode]
    name: IdentifierNode
    field_type: TypeNode
    initializer: Expression
    visibility: VisibilityNode
    is_static: Bool
    is_immutable: Bool
    span: SourceSpan
}


// initializer may be None.


// ============================================================
// Constructors
// ============================================================

data ConstructorDeclaration {
    attributes: [AttributeNode]
    class_name: IdentifierNode
    parameters: [ParameterDeclaration]
    body: BlockStatement
    visibility: VisibilityNode
    span: SourceSpan
}


// A class may contain multiple overloaded constructors.
// Constructors with an identical parameter type list conflict.


// ============================================================
// Destructors
// ============================================================

data DestructorDeclaration {
    attributes: [AttributeNode]
    class_name: IdentifierNode
    body: BlockStatement
    span: SourceSpan
}


// Source form:
//
// File::Drop() {
// }


// ============================================================
// Interface declarations
// ============================================================

data InterfaceDeclaration {
    attributes: [AttributeNode]
    name: IdentifierNode
    visibility: VisibilityNode
    generic_parameters: [GenericParameter]
    base_specifiers: [BaseSpecifier]
    members: [FunctionDeclaration]
    span: SourceSpan
}


// ============================================================
// Enum declarations
// ============================================================

data EnumDeclaration {
    attributes: [AttributeNode]
    name: IdentifierNode
    visibility: VisibilityNode
    is_data_enum: Bool
    base_specifiers: [BaseSpecifier]
    variants: [EnumVariant]
    span: SourceSpan
}


data EnumVariant {
    attributes: [AttributeNode]
    name: IdentifierNode
    payload_type: TypeNode
    is_overload: Bool
    span: SourceSpan
}


// payload_type may be None.
//
// Regular enum variants do not carry values.
//
// enum data variants may carry at most one payload.
// Typed variants with distinct payload types may share a name.
// is_overload is false for the first variant in that overload set and true for later variants.
// Regular enum variants and non-typed enum data variants have unique names.
// base_specifiers must be empty for a regular enum.
// An enum data declaration may contain any number of enum data bases.


// Example:
//
// enum data Token {
//     Identifier: String,
//     Integer: Int,
//     Plus,
// }

// EnumDeclaration
// ├── Identifier -> String
// ├── Integer    -> Int
// └── Plus       -> None


// ============================================================
// Data declarations
// ============================================================

data DataDeclaration {
    attributes: [AttributeNode]
    name: IdentifierNode
    visibility: VisibilityNode
    generic_parameters: [GenericParameter]
    base_specifiers: [BaseSpecifier]
    fields: [DataField]
    constructors: [ConstructorDeclaration]
    methods: [FunctionDeclaration]
    span: SourceSpan
}


data DataField {
    attributes: [AttributeNode]
    name: IdentifierNode
    field_type: TypeNode
    span: SourceSpan
}


// data declarations may contain fields, constructors, methods, and any number of data bases.
// Constructors and methods may be overloaded by parameter type list.
// Operator overloads are FunctionDeclaration nodes whose name is operator
// and whose operator token identifies the overload target.


// ============================================================
// Variable declarations
// ============================================================

enum VariableBindingKind {
    Mutable,
    Immutable,
    Constant,
    StaticConstant,
}


data VariableDeclaration {
    attributes: [AttributeNode]
    name: IdentifierNode
    variable_type: TypeNode?
    initializer: Expression
    binding_kind: VariableBindingKind
    is_type_inferred: Bool
    span: SourceSpan
}


// ============================================================
// Type node family
// ============================================================

enum data TypeNode {
    Named: NamedTypeNode,
    Generic: GenericTypeNode,
    Array: ArrayTypeNode,
    FixedArray: FixedArrayTypeNode,
    Pointer: PointerTypeNode,
    Reference: ReferenceTypeNode,
    MutableReference: MutableReferenceTypeNode,
    Tuple: TupleTypeNode,
    Function: FunctionTypeNode,
    Unit: UnitTypeNode,
}


data NamedTypeNode {
    path: PathNode
    span: SourceSpan
}


data GenericTypeNode {
    base_type: TypeNode
    arguments: [TypeNode]
    span: SourceSpan
}


data ArrayTypeNode {
    element_type: TypeNode
    span: SourceSpan
}


data FixedArrayTypeNode {
    element_type: TypeNode
    maximum_index: Expression
    span: SourceSpan
}


data PointerTypeNode {
    pointee_type: TypeNode
    span: SourceSpan
}


data ReferenceTypeNode {
    referenced_type: TypeNode
    lifetime: LifetimeNode
    span: SourceSpan
}


data MutableReferenceTypeNode {
    referenced_type: TypeNode
    lifetime: LifetimeNode
    span: SourceSpan
}


data TupleTypeNode {
    elements: [TupleTypeElement]
    span: SourceSpan
}


data TupleTypeElement {
    name: Optional<Identifier>
    element_type: TypeNode
    span: SourceSpan
}


data FunctionTypeNode {
    parameter_types: [TypeNode]
    return_type: TypeNode
    span: SourceSpan
}


data UnitTypeNode {
    span: SourceSpan
}


// Example:
//
// Arc<Mutex<Str>>
// fn(Int, Str) -> Bool

// GenericTypeNode
// ├── base_type: Arc
// └── arguments
//     └── GenericTypeNode
//         ├── base_type: Mutex
//         └── arguments
//             └── NamedTypeNode(Str)


// ============================================================
// Lifetimes
// ============================================================

data LifetimeNode {
    name: IdentifierNode
    span: SourceSpan
}


// Example:
//
// &'a User
// &'a mut User
// &'static Str
// &'else User


// ============================================================
// Statement node family
// ============================================================

enum data Statement {
    Block: BlockStatement,
    Expression: ExpressionStatement,
    Variable: VariableDeclarationStatement,
    Return: ReturnStatement,
    If: IfStatement,
    For: ForStatement,
    ForEach: ForEachStatement,
    While: WhileStatement,
    Loop: LoopStatement,
    Match: MatchStatement,
    Break: BreakStatement,
    Continue: ContinueStatement,
    MacroCall: MacroCallStatement,
    Discard: DiscardStatement,
}


// ============================================================
// Block statements
// ============================================================

data BlockStatement {
    statements: [Statement]
    span: SourceSpan
}


// ============================================================
// Expression statements
// ============================================================

data ExpressionStatement {
    expression: Expression
    is_discarded: Bool
    span: SourceSpan
}

// `expression;` sets is_discarded to true.
// A final block expression without `;` sets is_discarded to false and may be
// desugared into the block/function value, like Rust tail expressions.


// ============================================================
// Discard statements
// ============================================================

data DiscardStatement {
    expression: Expression
    span: SourceSpan
}


// `else: expression;` explicitly discards the expression value.


// ============================================================
// Variable declaration statements
// ============================================================

data VariableDeclarationStatement {
    declaration: VariableDeclaration
    span: SourceSpan
}


// ============================================================
// Return statements
// ============================================================

data ReturnStatement {
    value: Expression
    span: SourceSpan
}


// value may be None for:
//
// return;


// ============================================================
// If statements
// ============================================================

data IfStatement {
    condition: Expression
    then_block: BlockStatement
    else_if_branches: [ElseIfBranch]
    else_block: BlockStatement
    span: SourceSpan
}


data ElseIfBranch {
    condition: Expression
    body: BlockStatement
    span: SourceSpan
}


// else_block may be None.


// ============================================================
// For statements
// ============================================================

data ForStatement {
    initializer: Statement
    condition: Expression
    increment: Expression
    body: BlockStatement
    span: SourceSpan
}


// ============================================================
// For-each statements
// ============================================================

data ForEachStatement {
    pattern: PatternNode
    iterable: Expression
    body: BlockStatement
    span: SourceSpan
}


// Example:
//
// for (item in users.iter()) {
// }


// ============================================================
// While statements
// ============================================================

data WhileStatement {
    condition: Expression
    body: BlockStatement
    span: SourceSpan
}


data LoopStatement {
    body: BlockStatement
    span: SourceSpan
}


// ============================================================
// Match statements
// ============================================================

data MatchStatement {
    value: Expression
    arms: [MatchArm]
    span: SourceSpan
}


data MatchArm {
    pattern: PatternNode
    body: Statement
    span: SourceSpan
}


// ============================================================
// Break and continue
// ============================================================

data BreakStatement {
    span: SourceSpan
}


data ContinueStatement {
    span: SourceSpan
}


// ============================================================
// Expression node family
// ============================================================

enum data Expression {
    Identifier: IdentifierExpression,
    Literal: LiteralExpression,
    Binary: BinaryExpression,
    Unary: UnaryExpression,
    Assignment: AssignmentExpression,
    GenericQualifier: GenericQualifierExpression,
    Call: CallExpression,
    MethodCall: MethodCallExpression,
    MemberAccess: MemberAccessExpression,
    Index: IndexExpression,
    New: NewExpression,
    Function: FunctionExpression,
    Await: AwaitExpression,
    Move: MoveExpression,
    Borrow: BorrowExpression,
    MutableBorrow: MutableBorrowExpression,
    Dereference: DereferenceExpression,
    ArrayLiteral: ArrayLiteralExpression,
    ObjectLiteral: ObjectLiteralExpression,
    TypedObjectLiteral: TypedObjectLiteralExpression,
    FieldSet: FieldSetExpression,
    Tuple: TupleExpression,
    If: IfExpression,
    Match: MatchExpression,
    MacroCall: MacroCallExpression,
}


// ============================================================
// Identifier expressions
// ============================================================

data IdentifierExpression {
    identifier: IdentifierNode
    span: SourceSpan
}


data GenericQualifierExpression {
    base: PathNode
    type_arguments: [TypeNode]
    associated_path: PathNode
    span: SourceSpan
}


// associated_path is absent for expression turbofish such as Factory::<Int>().


// ============================================================
// Literal expressions
// ============================================================

enum LiteralKind {
    Integer,
    FloatingPoint,
    String,
    Character,
    Boolean,
    None,
}


data LiteralExpression {
    kind: LiteralKind
    source_text: String
    span: SourceSpan
}


// source_text preserves the original spelling.
//
// Examples:
//
// 1'000
// 3.141'592
// 1e-6
// "Alpha"
// 'A'
// true
// None


// ============================================================
// Binary expressions
// ============================================================

enum BinaryOperator {
    Add,
    Subtract,
    Multiply,
    Divide,
    Remainder,

    Equal,
    NotEqual,
    Greater,
    Less,
    GreaterEqual,
    LessEqual,

    LogicalAnd,
    LogicalOr,

    BitwiseAnd,
    BitwiseOr,
}


data BinaryExpression {
    left: Expression
    operator: BinaryOperator
    right: Expression
    span: SourceSpan
}


// Example:
//
// a + b

// BinaryExpression
// ├── left: IdentifierExpression(a)
// ├── operator: Add
// └── right: IdentifierExpression(b)


// ============================================================
// Unary expressions
// ============================================================

enum UnaryOperator {
    LogicalNot,
    Positive,
    Negative,
}


data UnaryExpression {
    operator: UnaryOperator
    operand: Expression
    span: SourceSpan
}


// ============================================================
// Assignment expressions
// ============================================================

data AssignmentExpression {
    target: Expression
    value: Expression
    span: SourceSpan
}


// ============================================================
// Function calls
// ============================================================

data CallExpression {
    callee: Expression
    arguments: [Expression]
    span: SourceSpan
}


// Example:
//
// add(1, 2)


// ============================================================
// Method calls
// ============================================================

data MethodCallExpression {
    receiver: Expression
    method_name: IdentifierNode
    arguments: [Expression]
    span: SourceSpan
}


// Example:
//
// client.send(request, handler)


// MethodCallExpression
// ├── receiver: client
// ├── method_name: send
// └── arguments
//     ├── request
//     └── handler


// ============================================================
// Member access
// ============================================================

data MemberAccessExpression {
    receiver: Expression
    member_name: IdentifierNode
    span: SourceSpan
}


// ============================================================
// Index expressions
// ============================================================

data IndexExpression {
    receiver: Expression
    index: Expression
    span: SourceSpan
}


// ============================================================
// Object creation
// ============================================================

data NewExpression {
    constructed_type: TypeNode
    arguments: [Expression]
    span: SourceSpan
}


// Class construction always spells its type explicitly as `new Type(...)`.


// ============================================================
// Function expressions
// ============================================================

data FunctionExpression {
    parameters: [Identifier]
    body: BlockStatement
    capture_kind: FunctionCaptureKind
    span: SourceSpan
}


enum FunctionCaptureKind {
    Default,
    Move,
}


// Examples:
//
// fn(a, b) { a + b }
//
// move fn() {
//     std::thread::yield();
// }


// ============================================================
// Await expressions
// ============================================================

data AwaitExpression {
    task: Expression
    span: SourceSpan
}


// ============================================================
// Move expressions
// ============================================================

data MoveExpression {
    value: Expression
    span: SourceSpan
}


// ============================================================
// Borrow expressions
// ============================================================

data BorrowExpression {
    value: Expression
    span: SourceSpan
}


data MutableBorrowExpression {
    value: Expression
    span: SourceSpan
}


// ============================================================
// Dereference expressions
// ============================================================

data DereferenceExpression {
    value: Expression
    span: SourceSpan
}


// ============================================================
// Array literals
// ============================================================

data ArrayLiteralExpression {
    elements: [Expression]
    span: SourceSpan
}


// ============================================================
// Object literals
// ============================================================

data ObjectLiteralExpression {
    fields: [ObjectLiteralField]
    span: SourceSpan
}


data TypedObjectLiteralExpression {
    constructor: Expression
    fields: [ObjectLiteralField]
    span: SourceSpan
}


data ObjectLiteralField {
    name: IdentifierNode
    value: Expression
    span: SourceSpan
}


// Data values use object literals and fields use ordinary member access.


// ============================================================
// Tuple expressions
// ============================================================

data TupleExpression {
    elements: [TupleExpressionElement]
    span: SourceSpan
}


data TupleExpressionElement {
    name: Optional<Identifier>
    value: Expression
    span: SourceSpan
}


// ============================================================
// If expressions
// ============================================================

data IfExpression {
    condition: Expression
    then_block: BlockStatement
    else_block: BlockStatement
    span: SourceSpan
}


// If expression requires else.
// Branch blocks must produce compatible values unless a branch diverges.


// ============================================================
// Match expressions
// ============================================================

data MatchExpression {
    value: Expression
    arms: [MatchExpressionArm]
    span: SourceSpan
}


data MatchExpressionArm {
    pattern: PatternNode
    body: BlockStatement
    span: SourceSpan
}


// Match expression arms must produce compatible values unless an arm diverges.


// ============================================================
// Pattern node family
// ============================================================

enum data PatternNode {
    Identifier: IdentifierPattern,
    Literal: LiteralPattern,
    EnumVariant: EnumVariantPattern,
    Tuple: TuplePattern,
    Typed: TypedPattern,
    Else: ElsePattern,
}


data IdentifierPattern {
    identifier: IdentifierNode
    span: SourceSpan
}


data LiteralPattern {
    literal: LiteralExpression
    span: SourceSpan
}


data EnumVariantPattern {
    path: PathNode
    bindings: [PatternNode]
    span: SourceSpan
}


data TuplePattern {
    elements: [PatternNode]
    span: SourceSpan
}


data TypedPattern {
    pattern: PatternNode
    type: TypeNode
    span: SourceSpan
}


data ElsePattern {
    span: SourceSpan
}


// ============================================================
// Macro declarations
// ============================================================

data MacroDeclaration {
    attributes: [AttributeNode]
    name: IdentifierNode
    rules: [MacroRule]
    span: SourceSpan
}


data MacroRule {
    matcher: MacroMatcher
    expansion: MacroExpansion
    span: SourceSpan
}


// ============================================================
// Macro matchers
// ============================================================

data MacroMatcher {
    elements: [MacroMatcherElement]
    span: SourceSpan
}


enum data MacroMatcherElement {
    Token: MacroTokenMatcher,
    Fragment: MacroFragmentMatcher,
    Repetition: MacroRepetitionMatcher,
}


data MacroTokenMatcher {
    token_text: String
    span: SourceSpan
}


data MacroFragmentMatcher {
    variable_name: IdentifierNode
    fragment_kind: MacroFragmentKind
    span: SourceSpan
}


enum MacroFragmentKind {
    Expr,
    Ident,
    Ty,
    Path,
    Pat,
    Stmt,
    Block,
    Item,
    Literal,
    Meta,
    TokenTree,
    Lifetime,
    Visibility,
}


enum MacroRepetitionKind {
    ZeroOrMore,
    OneOrMore,
}


data MacroRepetitionMatcher {
    elements: [MacroMatcherElement]
    separator: MacroSeparator
    repetition_kind: MacroRepetitionKind
    span: SourceSpan
}


enum MacroSeparator {
    None,
    Comma,
}


// Only comma may be used as a repetition separator.


// ============================================================
// Macro expansions
// ============================================================

data MacroExpansion {
    elements: [MacroExpansionElement]
    span: SourceSpan
}


enum data MacroExpansionElement {
    Token: MacroExpansionToken,
    Variable: MacroExpansionVariable,
    Repetition: MacroExpansionRepetition,
}


data MacroExpansionToken {
    token_text: String
    span: SourceSpan
}


data MacroExpansionVariable {
    variable_name: IdentifierNode
    span: SourceSpan
}


data MacroExpansionRepetition {
    elements: [MacroExpansionElement]
    repetition_kind: MacroRepetitionKind
    span: SourceSpan
}


// ============================================================
// Macro calls
// ============================================================

data MacroCallExpression {
    macro_name: IdentifierNode
    arguments: [MacroArgument]
    span: SourceSpan
}


data MacroCallStatement {
    call: MacroCallExpression
    span: SourceSpan
}


data MacroArgument {
    tokens: [TokenNode]
    span: SourceSpan
}


// Macro calls use parentheses only.


// ============================================================
// Token preservation
// ============================================================

data TokenNode {
    kind: TokenKind
    source_text: String
    span: SourceSpan
}


// TokenNode is used where exact syntax must remain available,
// especially during macro matching and expansion.


// ============================================================
// Parser responsibilities
// ============================================================

// The parser must:
//
// - Construct structural declaration nodes.
// - Parse parameter lists.
// - Parse generic parameter lists and constraints.
// - Parse type syntax.
// - Parse enum variants.
// - Parse class and interface members.
// - Parse statements.
// - Parse expressions.
// - Parse patterns.
// - Parse macro declarations.
// - Parse macro calls.
// - Attach SourceSpan to every node.
// - Preserve source token spelling when required.


// ============================================================
// Parser non-responsibilities
// ============================================================

// The parser must not:
//
// - Resolve names.
// - Resolve modules.
// - Search imported module files.
// - Check whether a type exists.
// - Check whether an identifier is visible.
// - Infer expression types.
// - Resolve method calls.
// - Resolve operators.
// - Validate ownership.
// - Validate borrows.
// - Validate Send or Sync.
// - Perform macro expansion.
// - Perform HIR lowering.
// - Perform MIR lowering.


// ============================================================
// HIR responsibilities
// ============================================================

// HIR receives expanded AST and performs:
//
// - Import resolution
// - Module resolution
// - Namespace resolution
// - Name resolution
// - Method-call normalization
// - Operator-call normalization
// - High-level syntax normalization
// - Dependency-graph construction


// ============================================================
// Type-checking responsibilities
// ============================================================

// Type checking performs:
//
// - Type compatibility checks
// - Generic constraint checks
// - Visibility checks
// - Mutability checks
// - Send checks
// - Sync checks
// - Async and await checks
// - Statically provable synchronization checks


// ============================================================
// MIR responsibilities
// ============================================================

// MIR makes explicit:
//
// - Ownership
// - Moves
// - Borrows
// - Drop points
// - Destructor calls
// - Control-flow edges
// - Match branching
// - Async state machines


// ============================================================
// Complete example
// ============================================================

// Source:
//
// fn add(a: Int, b: Int) -> Int {
//     return a + b;
// }


// Structural AST:
//
// AstFile
// └── FunctionDeclaration
//     ├── name: Add
//     ├── parameters
//     │   ├── ParameterDeclaration
//     │   │   ├── name: a
//     │   │   └── type: Int
//     │   └── ParameterDeclaration
//     │       ├── name: b
//     │       └── type: Int
//     ├── return_type: Int
//     └── body
//         └── BlockStatement
//             └── ReturnStatement
//                 └── BinaryExpression
//                     ├── left: a
//                     ├── operator: Add
//                     └── right: b


// ============================================================
// Implementation guidance for C23
// ============================================================

// The C23 compiler implementation may represent node families
// using tagged unions.


// Conceptual example:
//
// typedef enum {
//     XS_EXPR_IDENTIFIER,
//     XS_EXPR_LITERAL,
//     XS_EXPR_BINARY,
//     XS_EXPR_CALL,
//     XS_EXPR_METHOD_CALL,
//     XS_EXPR_AWAIT,
//     XS_EXPR_ASSIGNMENT
// } XsExprKind;
//
// typedef struct XsExpr XsExpr;
//
// struct XsExpr {
//     XsExprKind kind;
//     XsSourceSpan span;
//
//     union {
//         XsIdentifierExpr identifier;
//         XsLiteralExpr literal;
//         XsBinaryExpr binary;
//         XsCallExpr call;
//         XsMethodCallExpr method_call;
//         XsAwaitExpr await_expr;
//         XsAssignmentExpr assignment;
//     } as;
// };


// AST nodes should normally be allocated in compiler-owned arenas.
//
// AST child collections should use compiler vector structures.
//
// Every parser loop must guarantee token progress.
//
// If a parser iteration consumes no token:
//
// - Emit one diagnostic.
// - Recover by consuming a token or leaving the parse branch.
// - Do not repeatedly append diagnostics or AST nodes without
//   advancing.


// This progress rule prevents unbounded parser loops and memory
// growth.


// ============================================================
// Summary
// ============================================================

//
// Structural AST:
//
//     Explicit syntax-node tree
//     SourceSpan on every node
//     No semantic resolution
//
// Major node groups:
//
//     AstFile
//     Declaration
//     Statement
//     Expression
//     TypeNode
//     PatternNode
//     MacroDeclaration
//     MacroCallExpression
//
// Parser:
//
//     Builds syntax structure
//
// Macro expansion:
//
//     Transforms AST into expanded AST
//
// HIR:
//
//     Resolves names, modules and high-level constructs
//
// Type checking:
//
//     Validates types and language constraints
//
// MIR:
//
//     Resolves ownership and execution behavior
//
