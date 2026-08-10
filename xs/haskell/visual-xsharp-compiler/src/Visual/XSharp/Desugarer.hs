-- SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
-- SPDX-License-Identifier: MPL-2.0
module Visual.XSharp.Desugarer
    ( Desugarer (..), defaultDesugarer, runDesugarer ) where

import Visual.XSharp.AST
import Visual.XSharp.Core
import Visual.XSharp.Diagnostic (Diagnostic)

newtype Desugarer = Desugarer { desugarTypedAST :: TypedAST -> Either [Diagnostic] CoreModule }
runDesugarer :: Desugarer -> TypedAST -> Either [Diagnostic] CoreModule
runDesugarer = desugarTypedAST
defaultDesugarer :: Desugarer
defaultDesugarer = Desugarer (Right . lowerTree)

lowerTree :: TypedAST -> CoreModule
lowerTree (TypedAST (SyntaxTree namespace declarations)) = CoreModule (maybe defaultName id namespace) (concatMap lowerTop declarations)
  where defaultName = QualifiedName [Identifier "Main"]

lowerTop :: Declaration ResolvedName Type -> [CoreFunction]
lowerTop TypeDeclaration{typeMembers = members} = map lowerDeclaration members
lowerTop function@FunctionDeclaration{} = [lowerDeclaration function]

lowerDeclaration :: Declaration ResolvedName Type -> CoreFunction
lowerDeclaration declaration@FunctionDeclaration{} = CoreFunction (declarationName declaration)
    [(parameterName parameter, parameterAnnotation parameter) | parameter <- declarationParameters declaration]
    returnType (lowerFunctionBlock returnType (declarationBody declaration))
  where returnType = case declarationAnnotation declaration of FunctionType _ result -> result; value -> value
lowerDeclaration TypeDeclaration{} = error "type declarations are lowered through lowerTop"

lowerBlock :: Block ResolvedName Type -> [CoreStatement]
lowerBlock (Block statements) = map lowerStatement statements

lowerFunctionBlock :: Type -> Block ResolvedName Type -> [CoreStatement]
lowerFunctionBlock returnType (Block statements) = case reverse statements of
    ExpressionStatement _ expression False:remaining | returnType /= unitType ->
        map lowerStatement (reverse remaining) ++ [CoreReturn (lowerExpression expression)]
    _ -> map lowerStatement statements

lowerStatement :: Statement ResolvedName Type -> CoreStatement
lowerStatement statement = case statement of
    BindingStatement _ kind _ name valueType value -> CoreBind (CoreBinding name valueType (kind == MutableBinding) (lowerExpression value))
    AssignmentStatement _ name _ value -> CoreAssign name (lowerExpression value)
    ReturnStatement _ value -> CoreReturn (maybe (CoreLiteral CoreUnit unitType) lowerExpression value)
    IfStatement _ condition trueBlock falseBlock -> CoreIf (lowerExpression condition) (lowerBlock trueBlock) (maybe [] lowerBlock falseBlock)
    ExpressionStatement _ value _ -> CoreEvaluate (lowerExpression value)

lowerExpression :: Expression ResolvedName Type -> CoreExpression
lowerExpression expression = case expression of
    NameExpression _ name valueType -> CoreVariable name valueType
    LiteralExpression _ literal valueType -> CoreLiteral (lowerLiteral literal) valueType
    CallExpression _ callee arguments valueType -> CoreApply (lowerExpression callee) (map lowerExpression arguments) valueType
    UnaryExpression _ UnaryPlus value _ -> lowerExpression value
    UnaryExpression _ operator value valueType -> CorePrimitive (lowerUnary operator) [lowerExpression value] valueType
    BinaryExpression _ operator left right valueType -> CorePrimitive (lowerBinary operator) [lowerExpression left, lowerExpression right] valueType

lowerLiteral :: Literal -> CoreLiteral
lowerLiteral literal = case literal of IntegerLiteral value -> CoreInteger value; BooleanLiteral value -> CoreBoolean value; StringLiteral value -> CoreString value; UnitLiteral -> CoreUnit
lowerUnary :: UnaryOperator -> CorePrimitive
lowerUnary UnaryNegate = CoreNegate
lowerUnary LogicalNot = CoreLogicalNot
lowerUnary UnaryPlus = CoreAdd
lowerBinary :: BinaryOperator -> CorePrimitive
lowerBinary operator = case operator of
    Add -> CoreAdd; Subtract -> CoreSubtract; Multiply -> CoreMultiply; Divide -> CoreDivide; FloorDivide -> CoreFloorDivide; Remainder -> CoreRemainder
    LessThan -> CoreLessThan; LessEqual -> CoreLessEqual; GreaterThan -> CoreGreaterThan; GreaterEqual -> CoreGreaterEqual
    Equal -> CoreEqual; NotEqual -> CoreNotEqual; LogicalAnd -> CoreLogicalAnd; LogicalOr -> CoreLogicalOr
