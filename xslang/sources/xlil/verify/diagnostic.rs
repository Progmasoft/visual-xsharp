/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 */

/// Category of a violated XLIL model invariant.
#[derive(Clone, Debug, Eq, PartialEq)]
pub enum DiagnosticCode
{
    /// Module name is empty.
    EmptyModuleName,
    /// Aggregate registry entry or reference is invalid.
    InvalidAggregateType,
    /// Array registry entry or reference is invalid.
    InvalidArrayType,
    /// Aggregate names are not unique.
    DuplicateAggregateName,
    /// Function name is empty.
    EmptyFunctionName,
    /// Function names are not unique.
    DuplicateFunctionName,
    /// An external declaration contains a body.
    DeclarationHasBody,
    /// A definition contains no block.
    DefinitionHasNoBlocks,
    /// Block identifiers are duplicated.
    DuplicateBlockId,
    /// Block label is empty.
    EmptyBlockLabel,
    /// Definition block has no terminator.
    MissingTerminator,
    /// Instruction result is absent from the value registry.
    InstructionResultUnknown,
    /// Return register is absent.
    ReturnValueUnknown,
    /// Return register type differs from the signature.
    ReturnValueTypeMismatch,
    /// A void function returns a value.
    VoidReturnValue,
    /// A non-void function returns no value.
    NonVoidReturnMissingValue,
    /// Branch target is absent.
    BranchTargetUnknown,
    /// Direct call target is absent.
    CallTargetUnknown,
    /// Call arity differs from the callee signature.
    CallArgumentCountMismatch,
    /// Call argument type differs from the parameter type.
    CallArgumentTypeMismatch,
    /// Call result type differs from the callee result.
    CallResultTypeMismatch,
    /// A void/non-void call result rule is violated.
    CallVoidResultMismatch,
    /// Stack-slot registry or identifier is invalid.
    StackSlotInvalid,
    /// Load/store types do not match their slot.
    MemoryTypeMismatch,
}

/// Diagnostic produced by whole-module verification.
#[derive(Clone, Debug, Eq, PartialEq)]
pub struct Diagnostic
{
    /// Stable diagnostic category.
    pub code: DiagnosticCode,
    /// Human-readable explanation.
    pub message: String,
}
