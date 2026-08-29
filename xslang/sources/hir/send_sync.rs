/*
 * SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
 * SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.0
 */

use std::collections::{HashMap, HashSet};

use super::async_check::Span;

#[derive(Clone, Copy, Debug, Eq, PartialEq, Hash)]
pub enum Capability
{
    Send,
    Sync,
}

#[derive(Clone, Debug, Eq, PartialEq)]
pub struct TypeRef
{
    pub name: String,
    pub span: Span,
}

#[derive(Clone, Debug, Eq, PartialEq)]
pub enum UseSite
{
    MoveToThread
    {
        ty: TypeRef, span: Span
    },
    ShareAcrossThreads
    {
        ty: TypeRef, span: Span
    },
    AsyncCapture
    {
        ty: TypeRef, span: Span
    },
}

#[derive(Clone, Debug, Eq, PartialEq)]
pub struct CapabilityEnvironment
{
    type_capabilities: HashMap<String, HashSet<Capability>>,
}

impl CapabilityEnvironment
{
    #[must_use]
    pub fn new() -> Self
    {
        Self {
            type_capabilities: HashMap::new(),
        }
    }

    pub fn add_type(&mut self, name: impl Into<String>, capabilities: impl IntoIterator<Item = Capability>)
    {
        self.type_capabilities
            .insert(name.into(), capabilities.into_iter().collect());
    }

    #[must_use]
    pub fn has(&self, name: &str, capability: Capability) -> bool
    {
        self.type_capabilities
            .get(name)
            .is_some_and(|capabilities| capabilities.contains(&capability))
    }
}

impl Default for CapabilityEnvironment
{
    fn default() -> Self
    {
        Self::new()
    }
}

#[derive(Clone, Debug, Eq, PartialEq)]
pub enum DiagnosticCode
{
    MissingSend,
    MissingSync,
}

#[derive(Clone, Debug, Eq, PartialEq)]
pub struct Diagnostic
{
    pub code: DiagnosticCode,
    pub message: String,
    pub span: Span,
}

#[derive(Default)]
pub struct SendSyncChecker
{
    diagnostics: Vec<Diagnostic>,
}

impl SendSyncChecker
{
    #[must_use]
    pub fn new() -> Self
    {
        Self::default()
    }

    #[must_use]
    pub fn check(mut self, use_sites: &[UseSite], environment: &CapabilityEnvironment) -> Vec<Diagnostic>
    {
        for use_site in use_sites
        {
            match use_site
            {
                UseSite::MoveToThread {
                    ty,
                    span,
                } |
                UseSite::AsyncCapture {
                    ty,
                    span,
                } =>
                {
                    self.require_capability(ty, Capability::Send, *span, environment);
                }
                UseSite::ShareAcrossThreads {
                    ty,
                    span,
                } =>
                {
                    self.require_capability(ty, Capability::Sync, *span, environment);
                }
            }
        }
        self.diagnostics
    }

    fn require_capability(
        &mut self,
        ty: &TypeRef,
        capability: Capability,
        span: Span,
        environment: &CapabilityEnvironment,
    )
    {
        if environment.has(&ty.name, capability)
        {
            return;
        }
        let (code, name) = match capability
        {
            Capability::Send => (DiagnosticCode::MissingSend, "Send"),
            Capability::Sync => (DiagnosticCode::MissingSync, "Sync"),
        };
        self.diagnostics.push(Diagnostic {
            code,
            message: format!("type '{}' does not satisfy {name}", ty.name),
            span,
        });
    }
}

#[cfg(test)]
mod tests
{
    use super::*;

    fn span(start: u32, end: u32) -> Span
    {
        Span::new(1, start, end)
    }

    fn ty(name: &str) -> TypeRef
    {
        TypeRef {
            name: name.to_string(),
            span: span(10, 20),
        }
    }

    fn environment() -> CapabilityEnvironment
    {
        let mut environment = CapabilityEnvironment::new();
        environment.add_type("Int", [Capability::Send, Capability::Sync]);
        environment.add_type("UniqueHandle", [Capability::Send]);
        environment.add_type("ThreadLocalCell", []);
        environment
    }

    #[test]
    fn accepts_send_type_moved_to_thread()
    {
        let diagnostics = SendSyncChecker::new().check(
            &[UseSite::MoveToThread {
                ty: ty("UniqueHandle"),
                span: span(0, 10),
            }],
            &environment(),
        );

        assert!(diagnostics.is_empty());
    }

    #[test]
    fn rejects_non_send_type_moved_to_thread()
    {
        let diagnostics = SendSyncChecker::new().check(
            &[UseSite::MoveToThread {
                ty: ty("ThreadLocalCell"),
                span: span(0, 10),
            }],
            &environment(),
        );

        assert_eq!(diagnostics.len(), 1);
        assert_eq!(diagnostics[0].code, DiagnosticCode::MissingSend);
    }

    #[test]
    fn accepts_sync_type_shared_across_threads()
    {
        let diagnostics = SendSyncChecker::new().check(
            &[UseSite::ShareAcrossThreads {
                ty: ty("Int"),
                span: span(0, 10),
            }],
            &environment(),
        );

        assert!(diagnostics.is_empty());
    }

    #[test]
    fn rejects_send_only_type_shared_across_threads()
    {
        let diagnostics = SendSyncChecker::new().check(
            &[UseSite::ShareAcrossThreads {
                ty: ty("UniqueHandle"),
                span: span(0, 10),
            }],
            &environment(),
        );

        assert_eq!(diagnostics.len(), 1);
        assert_eq!(diagnostics[0].code, DiagnosticCode::MissingSync);
    }

    #[test]
    fn async_capture_requires_send()
    {
        let diagnostics = SendSyncChecker::new().check(
            &[UseSite::AsyncCapture {
                ty: ty("ThreadLocalCell"),
                span: span(0, 10),
            }],
            &environment(),
        );

        assert_eq!(diagnostics.len(), 1);
        assert_eq!(diagnostics[0].code, DiagnosticCode::MissingSend);
    }
}
