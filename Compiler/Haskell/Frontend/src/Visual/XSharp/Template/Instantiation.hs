-- SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
-- SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.1

{- |
Typed declaration instantiation after template application binding.

This pass does not choose overloads, constraints, or a native symbol spelling.
It has one narrow responsibility: replace the semantic template variables in
an already typed declaration tree.  The output is an ordinary type declaration
that the existing Desugarer can lower without learning template syntax.
-}
module Visual.XSharp.Template.Instantiation
    ( TemplateInstantiationError (..)
    , instantiateTemplateType
    , instantiateMember
    , instantiateParameter
    , instantiateBlock
    , instantiateStatement
    , instantiateExpression
    , instantiateCallableBody
    , renderTemplateInstantiationError
    ) where

import Visual.XSharp.AST
import Visual.XSharp.Template.Application

data TemplateInstantiationError
    = ExpectedTemplateTypeDeclaration
    | TemplateBindingTargetsDifferentDeclaration SymbolId SymbolId
    | TemplateTypeSubstitutionFailed TemplateApplicationError
    deriving (Eq, Ord, Read, Show)

instantiateTemplateType ::
    TemplateBinding ->
    Declaration ResolvedName Type ->
    Either TemplateInstantiationError (Declaration ResolvedName Type)
instantiateTemplateType binding declaration = case declaration of
    TemplateTypeDeclaration spanValue name annotation _ members
        | resolvedSymbol name /= templateDeclarationSymbol descriptor ->
            Left
                ( TemplateBindingTargetsDifferentDeclaration
                    (templateDeclarationSymbol descriptor)
                    (resolvedSymbol name)
                )
        | otherwise -> do
            closedAnnotation <- instantiateType binding annotation
            closedMembers <- traverse (instantiateMember binding) members
            pure (TypeDeclaration spanValue name closedAnnotation closedMembers)
    _ -> Left ExpectedTemplateTypeDeclaration
    where
        descriptor = templateBindingDeclaration binding

instantiateMember ::
    TemplateBinding ->
    Declaration ResolvedName Type ->
    Either TemplateInstantiationError (Declaration ResolvedName Type)
instantiateMember binding declaration = case declaration of
    FunctionDeclaration spanValue name annotation returnSyntax parameters body isStatic access -> do
        closedAnnotation <- instantiateType binding annotation
        closedParameters <- traverse (instantiateParameter binding) parameters
        closedBody <- instantiateBlock binding body
        pure
            ( FunctionDeclaration
                spanValue
                name
                closedAnnotation
                returnSyntax
                closedParameters
                closedBody
                isStatic
                access
            )
    TypeDeclaration spanValue name annotation members -> do
        closedAnnotation <- instantiateType binding annotation
        closedMembers <- traverse (instantiateMember binding) members
        pure (TypeDeclaration spanValue name closedAnnotation closedMembers)
    TemplateTypeDeclaration {} ->
        -- Nested templates own a distinct parameter environment. Applying the
        -- outer binding blindly would capture same-spelled inner parameters.
        -- Nested instantiation will be selected independently by the planner.
        pure declaration

instantiateParameter ::
    TemplateBinding ->
    Parameter ResolvedName Type ->
    Either TemplateInstantiationError (Parameter ResolvedName Type)
instantiateParameter binding parameter = do
    closedType <- instantiateType binding (parameterAnnotation parameter)
    pure
        ( Parameter
            (parameterSpan parameter)
            (parameterName parameter)
            closedType
            (parameterTypeSyntax parameter)
        )

instantiateBlock ::
    TemplateBinding ->
    Block ResolvedName Type ->
    Either TemplateInstantiationError (Block ResolvedName Type)
instantiateBlock binding (Block statements) = Block <$> traverse (instantiateStatement binding) statements

instantiateStatement ::
    TemplateBinding ->
    Statement ResolvedName Type ->
    Either TemplateInstantiationError (Statement ResolvedName Type)
instantiateStatement binding statement = case statement of
    BindingStatement spanValue kind syntax name annotation value -> do
        closedAnnotation <- instantiateType binding annotation
        closedValue <- instantiateExpression binding value
        pure (BindingStatement spanValue kind syntax name closedAnnotation closedValue)
    AssignmentStatement spanValue name annotation value -> do
        closedAnnotation <- instantiateType binding annotation
        closedValue <- instantiateExpression binding value
        pure (AssignmentStatement spanValue name closedAnnotation closedValue)
    ReturnStatement spanValue value ->
        ReturnStatement spanValue <$> traverse (instantiateExpression binding) value
    IfStatement spanValue condition trueBlock falseBlock -> do
        closedCondition <- instantiateExpression binding condition
        closedTrue <- instantiateBlock binding trueBlock
        closedFalse <- traverse (instantiateBlock binding) falseBlock
        pure (IfStatement spanValue closedCondition closedTrue closedFalse)
    ExpressionStatement spanValue value terminated ->
        ExpressionStatement spanValue <$> instantiateExpression binding value <*> pure terminated

instantiateExpression ::
    TemplateBinding ->
    Expression ResolvedName Type ->
    Either TemplateInstantiationError (Expression ResolvedName Type)
instantiateExpression binding expression = case expression of
    NameExpression spanValue name annotation ->
        NameExpression spanValue name <$> instantiateType binding annotation
    LiteralExpression spanValue literal annotation ->
        LiteralExpression spanValue literal <$> instantiateType binding annotation
    CallExpression spanValue callee arguments annotation -> do
        closedCallee <- instantiateExpression binding callee
        closedArguments <- traverse (instantiateExpression binding) arguments
        closedAnnotation <- instantiateType binding annotation
        pure (CallExpression spanValue closedCallee closedArguments closedAnnotation)
    UnaryExpression spanValue operator value annotation -> do
        closedValue <- instantiateExpression binding value
        closedAnnotation <- instantiateType binding annotation
        pure (UnaryExpression spanValue operator closedValue closedAnnotation)
    BinaryExpression spanValue operator left right annotation -> do
        closedLeft <- instantiateExpression binding left
        closedRight <- instantiateExpression binding right
        closedAnnotation <- instantiateType binding annotation
        pure (BinaryExpression spanValue operator closedLeft closedRight closedAnnotation)
    CallableExpression spanValue explicit captures parameters body annotation -> do
        closedCaptures <- traverse (instantiateCapture binding) captures
        closedParameters <- traverse (instantiateParameter binding) parameters
        closedBody <- instantiateCallableBody binding body
        closedAnnotation <- instantiateType binding annotation
        pure
            ( CallableExpression
                spanValue
                explicit
                closedCaptures
                closedParameters
                closedBody
                closedAnnotation
            )

instantiateCapture ::
    TemplateBinding ->
    Capture ResolvedName Type ->
    Either TemplateInstantiationError (Capture ResolvedName Type)
instantiateCapture binding capture = do
    closedAnnotation <- instantiateType binding (captureAnnotation capture)
    closedInitializer <- traverse (instantiateExpression binding) (captureInitializer capture)
    pure
        ( Capture
            (captureSpan capture)
            (captureMode capture)
            (captureName capture)
            closedAnnotation
            closedInitializer
        )

instantiateCallableBody ::
    TemplateBinding ->
    CallableBody ResolvedName Type ->
    Either TemplateInstantiationError (CallableBody ResolvedName Type)
instantiateCallableBody binding body = case body of
    CallableExpressionBody expression ->
        CallableExpressionBody <$> instantiateExpression binding expression
    CallableBlockBody block -> CallableBlockBody <$> instantiateBlock binding block

instantiateType :: TemplateBinding -> Type -> Either TemplateInstantiationError Type
instantiateType binding valueType = case substituteType binding valueType of
    Right closed -> Right closed
    Left issue -> Left (TemplateTypeSubstitutionFailed issue)

renderTemplateInstantiationError :: TemplateInstantiationError -> String
renderTemplateInstantiationError issue = case issue of
    ExpectedTemplateTypeDeclaration -> "template instantiation requires a typed template type declaration"
    TemplateBindingTargetsDifferentDeclaration expected actual ->
        "template binding targets symbol "
            ++ show (symbolIdValue expected)
            ++ ", but the declaration has symbol "
            ++ show (symbolIdValue actual)
    TemplateTypeSubstitutionFailed problem -> renderTemplateApplicationError problem
