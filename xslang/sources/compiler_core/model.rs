/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 */

use std::{slice, str};

use super::{hir_lowering, xlil_lowering};

pub const SYNTAX_ABI_VERSION: u32 = 0;
pub const NO_NODE: u64 = u64::MAX;

#[repr(u32)]
#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub enum FfiStatus
{
    Ok = 0,
    NullArgument = 1,
    InvalidPacket = 2,
}

#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct RawSyntaxNode
{
    pub kind: u32,
    pub token_kind: u32,
    pub visibility: u32,
    pub flags: u32,
    pub parent_index: u64,
    pub first_child: u64,
    pub child_count: u64,
    pub text_offset: u64,
    pub text_length: u64,
    pub file_id: u64,
    pub start_offset: u64,
    pub end_offset: u64,
    pub start_line: u64,
    pub start_column: u64,
    pub end_line: u64,
    pub end_column: u64,
}

#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct RawSyntaxPacket
{
    pub abi_version: u32,
    pub reserved: u32,
    pub root_index: u64,
    pub nodes: *const RawSyntaxNode,
    pub node_count: u64,
    pub child_indices: *const u64,
    pub child_index_count: u64,
    pub text_bytes: *const u8,
    pub text_byte_count: u64,
}

#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub enum PacketError
{
    UnsupportedVersion,
    NonZeroReserved,
    CountExceedsAddressSpace,
    NullTable,
    EmptyNodes,
    InvalidRoot,
    InvalidParent,
    InvalidChildRange,
    InvalidChildIndex,
    InvalidChildParent,
    InvalidTextRange,
    InvalidUtf8,
}

#[derive(Clone, Copy, Debug)]
pub struct SyntaxPacketView<'a>
{
    nodes: &'a [RawSyntaxNode],
    child_indices: &'a [u64],
    text_bytes: &'a [u8],
    root_index: usize,
}

#[derive(Clone, Debug, Eq, PartialEq)]
pub struct SourceSpan
{
    pub file_id: u64,
    pub start_offset: u64,
    pub end_offset: u64,
    pub start_line: u64,
    pub start_column: u64,
    pub end_line: u64,
    pub end_column: u64,
}

#[derive(Clone, Debug, Eq, PartialEq)]
pub struct SyntaxNode
{
    pub kind: u32,
    pub token_kind: u32,
    pub visibility: u32,
    pub flags: u32,
    pub parent: Option<usize>,
    pub children: Vec<usize>,
    pub text: String,
    pub span: SourceSpan,
}

#[derive(Clone, Debug, Eq, PartialEq)]
pub struct SyntaxTree
{
    pub root: usize,
    pub nodes: Vec<SyntaxNode>,
}

pub struct CompilerCoreSession
{
    pub(super) syntax: Vec<SyntaxTree>,
    pub(super) declarations: crate::hir::declarations::Module,
    pub(super) tests: Vec<super::test_harness::TestCase>,
    xhir_text: Option<Vec<u8>>,
    mir_functions: Vec<crate::mir::Function>,
    xmir_text: Option<Vec<u8>>,
    xlil_text: Option<Vec<u8>>,
    diagnostics: Vec<String>,
}

fn failed_lowering_session(syntax: Vec<SyntaxTree>, error: hir_lowering::LoweringError) -> CompilerCoreSession
{
    CompilerCoreSession {
        syntax,
        declarations: crate::hir::declarations::Module {
            name: None,
            nominal_types: Vec::new(),
            functions: Vec::new(),
        },
        tests: Vec::new(),
        xhir_text: None,
        mir_functions: Vec::new(),
        xmir_text: None,
        xlil_text: None,
        diagnostics: vec![error.to_string()],
    }
}

fn build_session(syntax: Vec<SyntaxTree>) -> CompilerCoreSession
{
    let declarations = match hir_lowering::lower_program(&syntax)
    {
        Ok(declarations) => declarations,
        Err(error) => return failed_lowering_session(syntax, error),
    };
    let (tests, test_diagnostics) = super::test_harness::discover(&syntax, &declarations);
    build_session_from_declarations(syntax, declarations, tests, test_diagnostics)
}

pub(super) fn build_session_from_declarations(
    syntax: Vec<SyntaxTree>,
    declarations: crate::hir::declarations::Module,
    tests: Vec<super::test_harness::TestCase>,
    mut diagnostics: Vec<String>,
) -> CompilerCoreSession
{
    let layout_diagnostics = crate::hir::declarations::validate_layouts(&declarations.nominal_types);
    let aggregate_registry = crate::hir::aggregate_registry::build_module(&declarations).unwrap_or_default();
    let collection_registry = crate::hir::collection_registry::build(&declarations, &aggregate_registry);
    let body_count = declarations
        .functions
        .iter()
        .filter(|function| function.body_present)
        .count();
    let mut hir_functions = Vec::new();
    let mut hir_parameter_counts = Vec::new();
    let mut mir_functions = Vec::new();
    diagnostics.extend(
        layout_diagnostics
            .into_iter()
            .map(|(name, error)| format!("nominal type '{name}' {error}")),
    );
    for declaration in declarations.functions.iter().filter(|function| function.body_present)
    {
        let Some(function) = declaration.as_type_checked_input()
        else
        {
            diagnostics.push(format!(
                "function '{}' could not be represented as typed HIR",
                declaration.name
            ));
            continue;
        };
        let type_diagnostics = crate::hir::type_check::TypeChecker::new()
            .with_nominal_types(&declarations.nominal_types)
            .check_function(&function);
        if !type_diagnostics.is_empty()
        {
            diagnostics.push(format!(
                "function '{}' failed type checking: {type_diagnostics:?}",
                declaration.name
            ));
            continue;
        }
        let desugared = match crate::hir::result_desugar::ResultDesugar::new().desugar_function(&function)
        {
            Ok(function) => function,
            Err(errors) =>
            {
                diagnostics.push(format!(
                    "function '{}' failed Result desugaring: {errors:?}",
                    declaration.name
                ));
                continue;
            }
        };
        hir_functions.push(function.clone());
        hir_parameter_counts.push(declaration.parameters.len());
        let mir = match crate::hir::mir_lowering::HirToMirLowerer::new()
            .with_nominal_types(&declarations.nominal_types)
            .with_aggregate_types(&aggregate_registry)
            .with_collection_types(&collection_registry)
            .lower_desugared_function_with_parameters(&desugared, declaration.parameters.len())
        {
            Ok(mir) => mir,
            Err(errors) =>
            {
                diagnostics.push(format!(
                    "function '{}' failed MIR lowering: {errors:?}",
                    declaration.name
                ));
                continue;
            }
        };
        let borrow_diagnostics = crate::mir::BorrowChecker::new().check_function(&mir);
        if !borrow_diagnostics.is_empty()
        {
            diagnostics.push(format!(
                "function '{}' failed borrow checking: {borrow_diagnostics:?}",
                declaration.name
            ));
            continue;
        }
        match crate::mir::optimizer::optimize_verified_function(mir)
        {
            Ok(optimized) => mir_functions.push(optimized.function),
            Err(errors) => diagnostics.push(format!(
                "function '{}' failed MIR optimization: {errors:?}",
                declaration.name
            )),
        }
    }
    let program_name = declarations.name.as_deref().unwrap_or("root");
    let xhir_text = (hir_functions.len() == body_count).then(|| {
        crate::hir::text::program_to_xhir_with_declarations(
            program_name,
            &declarations.nominal_types,
            &hir_functions,
            &hir_parameter_counts,
        )
        .into_bytes()
    });
    let xmir_text = (mir_functions.len() == body_count).then(|| {
        let aggregates = aggregate_registry
            .layouts
            .iter()
            .map(|layout| crate::xlil::AggregateType {
                id: layout.value_type.registry_id,
                name: layout.name.clone(),
                fields: layout.fields.clone(),
            })
            .collect::<Vec<_>>();
        let arrays = collection_registry
            .arrays
            .iter()
            .map(|layout| crate::xlil::ArrayType {
                id: layout.value_type.registry_id,
                element_type: layout.element_type,
                length: layout.length,
            })
            .collect::<Vec<_>>();
        crate::mir::text::program_to_xmir_with_types(program_name, &aggregates, &arrays, &mir_functions).into_bytes()
    });
    let xlil_text = if diagnostics.is_empty()
    {
        match xlil_lowering::lower_module(&declarations, &mir_functions)
        {
            Some(module) => Some(crate::xlil::module_to_string(&module).into_bytes()),
            None =>
            {
                diagnostics.push("verified MIR could not be assembled into a valid XLIL module".to_string());
                None
            }
        }
    }
    else
    {
        None
    };
    CompilerCoreSession {
        syntax,
        declarations,
        tests,
        xhir_text,
        mir_functions,
        xmir_text,
        xlil_text,
        diagnostics,
    }
}

fn table_length(value: u64) -> Result<usize, PacketError>
{
    usize::try_from(value).map_err(|_| PacketError::CountExceedsAddressSpace)
}

unsafe fn raw_table<'a, T>(pointer: *const T, length: usize) -> Result<&'a [T], PacketError>
{
    if length == 0
    {
        return Ok(&[]);
    }
    if pointer.is_null()
    {
        return Err(PacketError::NullTable);
    }
    // SAFETY: The caller of `from_raw` guarantees that non-null packet tables
    // reference `length` initialized elements for the returned view lifetime.
    Ok(unsafe { slice::from_raw_parts(pointer, length) })
}

impl<'a> SyntaxPacketView<'a>
{
    /// Creates and structurally validates a borrowed view over the C23 frontend packet.
    ///
    /// # Safety
    ///
    /// Every non-null table pointer in `packet` must reference its declared number of
    /// initialized elements and remain alive and immutable for `'a`.
    pub unsafe fn from_raw(packet: &'a RawSyntaxPacket) -> Result<Self, PacketError>
    {
        if packet.abi_version != SYNTAX_ABI_VERSION
        {
            return Err(PacketError::UnsupportedVersion);
        }
        if packet.reserved != 0
        {
            return Err(PacketError::NonZeroReserved);
        }
        let node_count = table_length(packet.node_count)?;
        let child_count = table_length(packet.child_index_count)?;
        let text_count = table_length(packet.text_byte_count)?;
        // SAFETY: Forwarded from this function's table lifetime contract.
        let nodes = unsafe { raw_table(packet.nodes, node_count)? };
        // SAFETY: Forwarded from this function's table lifetime contract.
        let child_indices = unsafe { raw_table(packet.child_indices, child_count)? };
        // SAFETY: Forwarded from this function's table lifetime contract.
        let text_bytes = unsafe { raw_table(packet.text_bytes, text_count)? };
        if nodes.is_empty()
        {
            return Err(PacketError::EmptyNodes);
        }
        let root_index = table_length(packet.root_index)?;
        if root_index >= nodes.len() || nodes[root_index].parent_index != NO_NODE
        {
            return Err(PacketError::InvalidRoot);
        }
        let view = Self {
            nodes,
            child_indices,
            text_bytes,
            root_index,
        };
        view.validate()?;
        Ok(view)
    }

    fn validate(&self) -> Result<(), PacketError>
    {
        for (node_index, node) in self.nodes.iter().enumerate()
        {
            if node_index != self.root_index
            {
                let parent = table_length(node.parent_index)?;
                if parent >= self.nodes.len() || parent == node_index
                {
                    return Err(PacketError::InvalidParent);
                }
            }
            let child_start = table_length(node.first_child)?;
            let child_count = table_length(node.child_count)?;
            let child_end = child_start
                .checked_add(child_count)
                .ok_or(PacketError::InvalidChildRange)?;
            if child_end > self.child_indices.len()
            {
                return Err(PacketError::InvalidChildRange);
            }
            for child in &self.child_indices[child_start..child_end]
            {
                let child_index = table_length(*child)?;
                if child_index >= self.nodes.len()
                {
                    return Err(PacketError::InvalidChildIndex);
                }
                if self.nodes[child_index].parent_index != node_index as u64
                {
                    return Err(PacketError::InvalidChildParent);
                }
            }
            self.node_text(node_index)?;
        }
        Ok(())
    }

    #[must_use]
    pub const fn nodes(&self) -> &'a [RawSyntaxNode]
    {
        self.nodes
    }

    #[must_use]
    pub const fn root_index(&self) -> usize
    {
        self.root_index
    }

    pub fn children(&self, node_index: usize) -> Result<&'a [u64], PacketError>
    {
        let node = self.nodes.get(node_index).ok_or(PacketError::InvalidChildIndex)?;
        let start = table_length(node.first_child)?;
        let count = table_length(node.child_count)?;
        let end = start.checked_add(count).ok_or(PacketError::InvalidChildRange)?;
        self.child_indices.get(start..end).ok_or(PacketError::InvalidChildRange)
    }

    pub fn node_text(&self, node_index: usize) -> Result<&'a str, PacketError>
    {
        let node = self.nodes.get(node_index).ok_or(PacketError::InvalidTextRange)?;
        let start = table_length(node.text_offset)?;
        let count = table_length(node.text_length)?;
        let end = start.checked_add(count).ok_or(PacketError::InvalidTextRange)?;
        let bytes = self.text_bytes.get(start..end).ok_or(PacketError::InvalidTextRange)?;
        str::from_utf8(bytes).map_err(|_| PacketError::InvalidUtf8)
    }

    pub fn to_owned_tree(&self) -> Result<SyntaxTree, PacketError>
    {
        let mut nodes = Vec::with_capacity(self.nodes.len());
        for (index, raw) in self.nodes.iter().enumerate()
        {
            let parent = if raw.parent_index == NO_NODE
            {
                None
            }
            else
            {
                Some(table_length(raw.parent_index)?)
            };
            let children = self
                .children(index)?
                .iter()
                .map(|value| table_length(*value))
                .collect::<Result<_, _>>()?;
            nodes.push(SyntaxNode {
                kind: raw.kind,
                token_kind: raw.token_kind,
                visibility: raw.visibility,
                flags: raw.flags,
                parent,
                children,
                text: self.node_text(index)?.to_owned(),
                span: SourceSpan {
                    file_id: raw.file_id,
                    start_offset: raw.start_offset,
                    end_offset: raw.end_offset,
                    start_line: raw.start_line,
                    start_column: raw.start_column,
                    end_line: raw.end_line,
                    end_column: raw.end_column,
                },
            });
        }
        Ok(SyntaxTree {
            root: self.root_index,
            nodes,
        })
    }
}

/// Creates a compiler-core session from a C23 frontend syntax packet.
///
/// This is the narrow compiler-core FFI ownership boundary. A successful call
/// writes a session which owns its syntax tree and must later be released with
/// [`xslang_compiler_core_session_free`].
///
/// # Safety
///
/// `packet` must point to a live [`RawSyntaxPacket`] whose tables satisfy the
/// contract of [`SyntaxPacketView::from_raw`]. `session` must be writable.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn xslang_compiler_core_session_create(
    packet: *const RawSyntaxPacket,
    session: *mut *mut CompilerCoreSession,
) -> FfiStatus
{
    // SAFETY: This forwards the exact caller-owned packet and output pointer with
    // an absent external module name.
    unsafe { create_session(packet, None, session) }
}

/// Creates a compiler-core session and assigns its source tree to a project
/// module supplied by the Kotlin project registry.
///
/// # Safety
///
/// The packet and session requirements match
/// [`xslang_compiler_core_session_create`]. `module_name` must reference
/// `module_name_length` bytes of valid UTF-8.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn xslang_compiler_core_session_create_in_module(
    packet: *const RawSyntaxPacket,
    module_name: *const u8,
    module_name_length: u64,
    session: *mut *mut CompilerCoreSession,
) -> FfiStatus
{
    let Ok(length) = table_length(module_name_length)
    else
    {
        return FfiStatus::InvalidPacket;
    };
    if length == 0 || module_name.is_null()
    {
        return FfiStatus::NullArgument;
    }
    // SAFETY: The caller contract requires this byte range to be live.
    let bytes = unsafe { slice::from_raw_parts(module_name, length) };
    let Ok(name) = str::from_utf8(bytes)
    else
    {
        return FfiStatus::InvalidPacket;
    };
    // SAFETY: This function has the same packet and output contract as the
    // shared implementation.
    unsafe { create_session(packet, Some(name), session) }
}

unsafe fn create_session(
    packet: *const RawSyntaxPacket,
    module_name: Option<&str>,
    session: *mut *mut CompilerCoreSession,
) -> FfiStatus
{
    let Some(session) = (unsafe { session.as_mut() })
    else
    {
        return FfiStatus::NullArgument;
    };
    *session = std::ptr::null_mut();
    let Some(packet) = (unsafe { packet.as_ref() })
    else
    {
        return FfiStatus::NullArgument;
    };
    // SAFETY: The function's caller contract covers every packet table.
    let Ok(view) = (unsafe { SyntaxPacketView::from_raw(packet) })
    else
    {
        return FfiStatus::InvalidPacket;
    };
    let Ok(mut syntax) = view.to_owned_tree()
    else
    {
        return FfiStatus::InvalidPacket;
    };
    if let Some(name) = module_name &&
        attach_project_module(&mut syntax, name).is_err()
    {
        return FfiStatus::InvalidPacket;
    }
    let built = build_session(vec![syntax]);
    *session = Box::into_raw(Box::new(built));
    FfiStatus::Ok
}

fn attach_project_module(tree: &mut SyntaxTree, name: &str) -> Result<(), ()>
{
    const FILE: u32 = 0;
    const DECL_MODULE: u32 = 1;
    const IDENTIFIER: u32 = 24;
    const PATH: u32 = 25;
    if name.is_empty() || tree.nodes.get(tree.root).is_none_or(|root| root.kind != FILE)
    {
        return Err(());
    }
    let segments = name.split("::").collect::<Vec<_>>();
    if segments.iter().any(|segment| segment.is_empty())
    {
        return Err(());
    }
    let span = tree.nodes[tree.root].span.clone();
    let module_index = tree.nodes.len();
    let path_index = module_index + 1;
    let identifiers = (0..segments.len())
        .map(|offset| path_index + 1 + offset)
        .collect::<Vec<_>>();
    tree.nodes.push(SyntaxNode {
        kind: DECL_MODULE,
        token_kind: 0,
        visibility: 0,
        flags: 0,
        parent: Some(tree.root),
        children: vec![path_index],
        text: String::new(),
        span: span.clone(),
    });
    tree.nodes.push(SyntaxNode {
        kind: PATH,
        token_kind: 0,
        visibility: 0,
        flags: 0,
        parent: Some(module_index),
        children: identifiers,
        text: String::new(),
        span: span.clone(),
    });
    for segment in segments
    {
        tree.nodes.push(SyntaxNode {
            kind: IDENTIFIER,
            token_kind: 0,
            visibility: 0,
            flags: 0,
            parent: Some(path_index),
            children: vec![],
            text: segment.to_string(),
            span: span.clone(),
        });
    }
    tree.nodes[tree.root].children.insert(0, module_index);
    Ok(())
}

/// Merges existing per-file sessions into one program-wide compiler-core session.
///
/// The merged session re-runs declaration/body lowering with signatures from every
/// input tree, enabling direct calls across source-file boundaries.
///
/// # Safety
///
/// `sessions` must reference `session_count` live session pointers and `merged`
/// must be writable. Input sessions remain owned by the caller.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn xslang_compiler_core_session_merge(
    sessions: *const *const CompilerCoreSession,
    session_count: u64,
    merged: *mut *mut CompilerCoreSession,
) -> FfiStatus
{
    let Some(merged) = (unsafe { merged.as_mut() })
    else
    {
        return FfiStatus::NullArgument;
    };
    *merged = std::ptr::null_mut();
    let Ok(count) = table_length(session_count)
    else
    {
        return FfiStatus::InvalidPacket;
    };
    if count == 0 || sessions.is_null()
    {
        return FfiStatus::NullArgument;
    }
    // SAFETY: The caller guarantees a live table with `count` session pointers.
    let sessions = unsafe { slice::from_raw_parts(sessions, count) };
    let mut syntax = Vec::new();
    for session in sessions
    {
        let Some(session) = (unsafe { session.as_ref() })
        else
        {
            return FfiStatus::NullArgument;
        };
        syntax.extend(session.syntax.iter().cloned());
    }
    let built = build_session(syntax);
    *merged = Box::into_raw(Box::new(built));
    FfiStatus::Ok
}

/// Returns the imported syntax-node count, or zero for a null session.
///
/// # Safety
///
/// `session` must be null or point to a live session returned by
/// [`xslang_compiler_core_session_create`].
#[unsafe(no_mangle)]
pub unsafe extern "C" fn xslang_compiler_core_session_syntax_node_count(session: *const CompilerCoreSession) -> u64
{
    // SAFETY: The pointer is only borrowed when it is non-null.
    unsafe { session.as_ref() }.map_or(0, |value| value.syntax.iter().map(|tree| tree.nodes.len() as u64).sum())
}

/// Returns the number of top-level function declarations imported into HIR.
///
/// # Safety
///
/// `session` must be null or point to a live compiler-core session.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn xslang_compiler_core_session_function_count(session: *const CompilerCoreSession) -> u64
{
    // SAFETY: The pointer is only borrowed when it is non-null.
    unsafe { session.as_ref() }.map_or(0, |value| value.declarations.functions.len() as u64)
}

/// Returns the number of function bodies currently lowered through Rust MIR.
///
/// # Safety
///
/// `session` must be null or point to a live compiler-core session.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn xslang_compiler_core_session_mir_function_count(session: *const CompilerCoreSession) -> u64
{
    // SAFETY: The pointer is only borrowed when it is non-null.
    unsafe { session.as_ref() }.map_or(0, |value| value.mir_functions.len() as u64)
}

/// Returns the number of compiler-core stage diagnostics retained by a session.
///
/// # Safety
///
/// `session` must be null or point to a live compiler-core session.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn xslang_compiler_core_session_diagnostic_count(session: *const CompilerCoreSession) -> u64
{
    // SAFETY: The pointer is only borrowed when it is non-null.
    unsafe { session.as_ref() }.map_or(0, |value| value.diagnostics.len() as u64)
}

/// Returns one borrowed UTF-8 diagnostic message.
///
/// The bytes remain valid until the session is released. An invalid index or
/// null session returns null and writes zero to `length` when non-null.
///
/// # Safety
///
/// `session` must be null or point to a live compiler-core session. `length`
/// must be null or writable for one `u64`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn xslang_compiler_core_session_diagnostic_text(
    session: *const CompilerCoreSession,
    index: u64,
    length: *mut u64,
) -> *const u8
{
    if !length.is_null()
    {
        // SAFETY: The caller contract requires a writable length pointer.
        unsafe { *length = 0 };
    }
    let Some(text) = (unsafe { session.as_ref() }).and_then(|value| {
        usize::try_from(index)
            .ok()
            .and_then(|index| value.diagnostics.get(index))
    })
    else
    {
        return std::ptr::null();
    };
    if !length.is_null()
    {
        // SAFETY: The caller contract requires a writable length pointer.
        unsafe { *length = text.len() as u64 };
    }
    text.as_ptr()
}

unsafe fn session_text(text: Option<&[u8]>, length: *mut u64) -> *const u8
{
    if !length.is_null()
    {
        // SAFETY: The caller provides the writable length pointer.
        unsafe { *length = text.map_or(0, |value| value.len() as u64) };
    }
    text.map_or(std::ptr::null(), <[u8]>::as_ptr)
}

/// Returns borrowed XHIR v0 program text when every source body reached typed HIR.
///
/// # Safety
///
/// `session` must be null or live; `length` must be null or writable.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn xslang_compiler_core_session_xhir_text(
    session: *const CompilerCoreSession,
    length: *mut u64,
) -> *const u8
{
    let text = unsafe { session.as_ref() }.and_then(|value| value.xhir_text.as_deref());
    unsafe { session_text(text, length) }
}

/// Returns borrowed XMIR v0 program text when every source body reached verified MIR.
///
/// # Safety
///
/// `session` must be null or live; `length` must be null or writable.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn xslang_compiler_core_session_xmir_text(
    session: *const CompilerCoreSession,
    length: *mut u64,
) -> *const u8
{
    let text = unsafe { session.as_ref() }.and_then(|value| value.xmir_text.as_deref());
    unsafe { session_text(text, length) }
}

/// Returns borrowed XLIL v0 text when every source body reached verified XLIL.
///
/// The returned bytes remain valid until the session is released. On failure or
/// a null session this returns null and writes zero to `length` when non-null.
///
/// # Safety
///
/// `session` must be null or point to a live compiler-core session. `length`
/// must be null or writable for one `u64`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn xslang_compiler_core_session_xlil_text(
    session: *const CompilerCoreSession,
    length: *mut u64,
) -> *const u8
{
    if !length.is_null()
    {
        // SAFETY: The caller contract requires a writable length pointer.
        unsafe { *length = 0 };
    }
    let Some(text) = (unsafe { session.as_ref() }).and_then(|value| value.xlil_text.as_ref())
    else
    {
        return std::ptr::null();
    };
    if !length.is_null()
    {
        // SAFETY: The caller contract requires a writable length pointer.
        unsafe { *length = text.len() as u64 };
    }
    text.as_ptr()
}

/// Releases a compiler-core session. Null is accepted.
///
/// # Safety
///
/// `session` must be null or a pointer returned exactly once by
/// [`xslang_compiler_core_session_create`].
#[unsafe(no_mangle)]
pub unsafe extern "C" fn xslang_compiler_core_session_free(session: *mut CompilerCoreSession)
{
    if !session.is_null()
    {
        // SAFETY: Guaranteed by the function's ownership contract.
        drop(unsafe { Box::from_raw(session) });
    }
}

include!("model/tests.rs");
