/*
 * SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
 * SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.0
 */

use crate::hir::{
    async_check::Span,
    declarations::{self, TypeRef},
    type_check::{Expression, Literal, Statement, Type},
};

use super::{CompilerCoreSession, FfiStatus, SourceSpan, SyntaxNode, SyntaxTree, model};

const FILE: u32 = 0;
const DECL_FUNCTION: u32 = 4;
const ATTRIBUTE_LIST: u32 = 13;
const ATTRIBUTE: u32 = 14;
const IDENTIFIER: u32 = 24;
const PATH: u32 = 25;

pub(super) const TEST_IGNORED: u32 = 1 << 0;
pub(super) const TEST_SHOULD_PANIC: u32 = 1 << 1;

#[derive(Clone, Debug)]
pub(super) struct TestCase
{
    pub name: String,
    pub flags: u32,
    pub span: SourceSpan,
}

fn child<'a>(tree: &'a SyntaxTree, node: &'a SyntaxNode, kind: u32) -> Option<&'a SyntaxNode>
{
    node.children
        .iter()
        .filter_map(|index| tree.nodes.get(*index))
        .find(|value| value.kind == kind)
}

fn path_name(tree: &SyntaxTree, node: &SyntaxNode) -> String
{
    node.children
        .iter()
        .filter_map(|index| tree.nodes.get(*index))
        .filter(|value| value.kind == IDENTIFIER)
        .map(|value| value.text.as_str())
        .collect::<Vec<_>>()
        .join("::")
}

fn attribute_names(tree: &SyntaxTree, function: &SyntaxNode) -> Vec<String>
{
    let Some(attributes) = child(tree, function, ATTRIBUTE_LIST)
    else
    {
        return Vec::new();
    };
    attributes
        .children
        .iter()
        .filter_map(|index| tree.nodes.get(*index))
        .filter(|attribute| attribute.kind == ATTRIBUTE)
        .filter_map(|attribute| child(tree, attribute, PATH))
        .map(|path| path_name(tree, path))
        .map(|name| name.rsplit("::").next().unwrap_or(&name).to_string())
        .collect()
}

pub(super) fn discover(trees: &[SyntaxTree], declarations: &declarations::Module) -> (Vec<TestCase>, Vec<String>)
{
    let mut tests = Vec::new();
    let mut diagnostics = Vec::new();
    for tree in trees
    {
        let Some(root) = tree.nodes.get(tree.root).filter(|root| root.kind == FILE)
        else
        {
            continue;
        };
        for function in root
            .children
            .iter()
            .filter_map(|index| tree.nodes.get(*index))
            .filter(|node| node.kind == DECL_FUNCTION)
        {
            let attributes = attribute_names(tree, function);
            if !attributes.iter().any(|name| name == "Test")
            {
                continue;
            }
            let Some(name) = child(tree, function, IDENTIFIER).map(|value| value.text.clone())
            else
            {
                diagnostics.push("#[Test] function is missing its name".to_string());
                continue;
            };
            let matches = declarations
                .functions
                .iter()
                .filter(|declaration| declaration.name == name)
                .collect::<Vec<_>>();
            if matches.len() != 1
            {
                diagnostics.push(format!(
                    "#[Test] function '{name}' must resolve to one top-level function"
                ));
                continue;
            }
            let declaration = matches[0];
            if !declaration.body_present ||
                !declaration.parameters.is_empty() ||
                declaration.return_type != TypeRef::Unit
            {
                diagnostics.push(format!(
                    "#[Test] function '{name}' must have a body and signature fn {name}()"
                ));
                continue;
            }
            let mut flags = 0;
            if attributes.iter().any(|attribute| attribute == "Ignore")
            {
                flags |= TEST_IGNORED;
            }
            if attributes.iter().any(|attribute| attribute == "ShouldPanic")
            {
                flags |= TEST_SHOULD_PANIC;
            }
            tests.push(TestCase {
                name,
                flags,
                span: function.span.clone(),
            });
        }
    }
    (tests, diagnostics)
}

fn hir_span(span: &SourceSpan) -> Span
{
    Span::new(
        span.file_id,
        u32::try_from(span.start_offset).unwrap_or(u32::MAX),
        u32::try_from(span.end_offset).unwrap_or(u32::MAX),
    )
}

fn harness_declaration(test: &TestCase) -> declarations::Function
{
    let span = hir_span(&test.span);
    declarations::Function {
        name: "main".to_string(),
        parameters: Vec::new(),
        return_type: TypeRef::Primitive(crate::hir::type_check::PrimitiveType::Long),
        flags: 0,
        span: test.span.clone(),
        body_present: true,
        body: Some(vec![
            Statement::Expr(Expression::Call {
                function: test.name.clone(),
                arguments: Vec::new(),
                parameter_types: Vec::new(),
                return_type: Box::new(Type::Unit),
                span,
            }),
            Statement::Return {
                value: Some(Expression::Literal {
                    literal: Literal::Integer("0".to_string()),
                    span,
                }),
                span,
            },
        ]),
    }
}

/// Returns the number of valid top-level `#[Test]` functions retained by a session.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn xslang_compiler_core_session_test_count(session: *const CompilerCoreSession) -> u64
{
    // SAFETY: The pointer is borrowed only when non-null.
    unsafe { session.as_ref() }.map_or(0, |session| session.tests.len() as u64)
}

/// Returns one borrowed UTF-8 test name and writes its byte length.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn xslang_compiler_core_session_test_name(
    session: *const CompilerCoreSession,
    index: u64,
    length: *mut u64,
) -> *const u8
{
    if !length.is_null()
    {
        // SAFETY: The caller supplies writable storage when the pointer is non-null.
        unsafe { *length = 0 };
    }
    let Some(test) = (unsafe { session.as_ref() })
        .and_then(|session| usize::try_from(index).ok().and_then(|index| session.tests.get(index)))
    else
    {
        return std::ptr::null();
    };
    if !length.is_null()
    {
        // SAFETY: The caller supplies writable storage when the pointer is non-null.
        unsafe { *length = test.name.len() as u64 };
    }
    test.name.as_ptr()
}

/// Returns `Ignore`/`ShouldPanic` flag bits for one test, or zero for invalid input.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn xslang_compiler_core_session_test_flags(session: *const CompilerCoreSession, index: u64)
-> u32
{
    unsafe { session.as_ref() }
        .and_then(|session| usize::try_from(index).ok().and_then(|index| session.tests.get(index)))
        .map_or(0, |test| test.flags)
}

/// Creates a compiler-core session whose native `main` invokes exactly one test.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn xslang_compiler_core_test_harness_create(
    session: *const CompilerCoreSession,
    index: u64,
    harness: *mut *mut CompilerCoreSession,
) -> FfiStatus
{
    let Some(output) = (unsafe { harness.as_mut() })
    else
    {
        return FfiStatus::NullArgument;
    };
    *output = std::ptr::null_mut();
    let Some(session) = (unsafe { session.as_ref() })
    else
    {
        return FfiStatus::NullArgument;
    };
    let Some(test) = usize::try_from(index)
        .ok()
        .and_then(|index| session.tests.get(index))
        .cloned()
    else
    {
        return FfiStatus::InvalidPacket;
    };
    let mut declarations = session.declarations.clone();
    declarations.functions.retain(|function| function.name != "main");
    declarations.functions.push(harness_declaration(&test));
    let built = model::build_session_from_declarations(session.syntax.clone(), declarations, Vec::new(), Vec::new());
    *output = Box::into_raw(Box::new(built));
    FfiStatus::Ok
}

#[cfg(test)]
mod tests
{
    use super::*;

    #[test]
    fn flag_values_are_stable()
    {
        assert_eq!(TEST_IGNORED, 1);
        assert_eq!(TEST_SHOULD_PANIC, 2);
    }
}
