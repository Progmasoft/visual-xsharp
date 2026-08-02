/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 */

use std::collections::{HashMap, HashSet};

use super::async_check::Span;
use super::symbols::SymbolKind;

#[derive(Clone, Debug, Eq, PartialEq)]
pub struct TypeRef
{
    pub name: String,
    pub span: Span,
}

#[derive(Clone, Debug, Eq, PartialEq)]
pub struct GenericParameter
{
    pub name: String,
    pub constraints: Vec<TypeRef>,
    pub span: Span,
}

#[derive(Clone, Debug, Eq, PartialEq)]
pub struct GenericUse
{
    pub generic_name: String,
    pub parameters: Vec<GenericParameter>,
    pub arguments: Vec<TypeRef>,
    pub span: Span,
}

#[derive(Clone, Debug, Eq, PartialEq)]
pub struct NominalEnvironment
{
    pub symbols: HashMap<String, SymbolKind>,
    pub implements: HashMap<String, HashSet<String>>,
}

impl NominalEnvironment
{
    #[must_use]
    pub fn new() -> Self
    {
        Self {
            symbols: HashMap::new(),
            implements: HashMap::new(),
        }
    }

    pub fn add_symbol(&mut self, name: impl Into<String>, kind: SymbolKind)
    {
        self.symbols.insert(name.into(), kind);
    }

    pub fn add_implementation(&mut self, type_name: impl Into<String>, interface_name: impl Into<String>)
    {
        self.implements
            .entry(type_name.into())
            .or_default()
            .insert(interface_name.into());
    }
}

impl Default for NominalEnvironment
{
    fn default() -> Self
    {
        Self::new()
    }
}

#[derive(Clone, Debug, Eq, PartialEq)]
pub enum DiagnosticCode
{
    DuplicateGenericParameter,
    ConstraintIsNotInterface,
    GenericArityMismatch,
    UnsatisfiedConstraint,
}

#[derive(Clone, Debug, Eq, PartialEq)]
pub struct Diagnostic
{
    pub code: DiagnosticCode,
    pub message: String,
    pub span: Span,
}

#[derive(Default)]
pub struct GenericChecker
{
    diagnostics: Vec<Diagnostic>,
}

impl GenericChecker
{
    #[must_use]
    pub fn new() -> Self
    {
        Self::default()
    }

    #[must_use]
    pub fn check_parameters(
        mut self,
        parameters: &[GenericParameter],
        environment: &NominalEnvironment,
    ) -> Vec<Diagnostic>
    {
        self.check_duplicate_parameters(parameters);
        self.check_constraints_are_interfaces(parameters, environment);
        self.diagnostics
    }

    #[must_use]
    pub fn check_use(mut self, generic_use: &GenericUse, environment: &NominalEnvironment) -> Vec<Diagnostic>
    {
        self.check_duplicate_parameters(&generic_use.parameters);
        self.check_constraints_are_interfaces(&generic_use.parameters, environment);
        if generic_use.parameters.len() != generic_use.arguments.len()
        {
            self.diagnostics.push(Diagnostic {
                code: DiagnosticCode::GenericArityMismatch,
                message: format!(
                    "generic '{}' expects {} type argument(s), got {}",
                    generic_use.generic_name,
                    generic_use.parameters.len(),
                    generic_use.arguments.len()
                ),
                span: generic_use.span,
            });
            return self.diagnostics;
        }
        for (parameter, argument) in generic_use.parameters.iter().zip(&generic_use.arguments)
        {
            self.check_argument_constraints(parameter, argument, environment);
        }
        self.diagnostics
    }

    fn check_duplicate_parameters(&mut self, parameters: &[GenericParameter])
    {
        let mut seen = HashSet::new();
        for parameter in parameters
        {
            if !seen.insert(parameter.name.as_str())
            {
                self.diagnostics.push(Diagnostic {
                    code: DiagnosticCode::DuplicateGenericParameter,
                    message: format!("generic parameter '{}' is already defined", parameter.name),
                    span: parameter.span,
                });
            }
        }
    }

    fn check_constraints_are_interfaces(&mut self, parameters: &[GenericParameter], environment: &NominalEnvironment)
    {
        for parameter in parameters
        {
            for constraint in &parameter.constraints
            {
                if environment.symbols.get(&constraint.name) != Some(&SymbolKind::Interface)
                {
                    self.diagnostics.push(Diagnostic {
                        code: DiagnosticCode::ConstraintIsNotInterface,
                        message: format!("generic constraint '{}' must resolve to an interface", constraint.name),
                        span: constraint.span,
                    });
                }
            }
        }
    }

    fn check_argument_constraints(
        &mut self,
        parameter: &GenericParameter,
        argument: &TypeRef,
        environment: &NominalEnvironment,
    )
    {
        let implemented = environment.implements.get(&argument.name);
        for constraint in &parameter.constraints
        {
            if environment.symbols.get(&constraint.name) != Some(&SymbolKind::Interface)
            {
                continue;
            }
            if !implemented.is_some_and(|interfaces| interfaces.contains(&constraint.name))
            {
                self.diagnostics.push(Diagnostic {
                    code: DiagnosticCode::UnsatisfiedConstraint,
                    message: format!(
                        "type '{}' does not satisfy interface constraint '{}'",
                        argument.name, constraint.name
                    ),
                    span: argument.span,
                });
            }
        }
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

    fn type_ref(name: &str) -> TypeRef
    {
        TypeRef {
            name: name.to_string(),
            span: span(10, 20),
        }
    }

    fn parameter(name: &str, constraints: Vec<TypeRef>) -> GenericParameter
    {
        GenericParameter {
            name: name.to_string(),
            constraints,
            span: span(0, 1),
        }
    }

    fn environment() -> NominalEnvironment
    {
        let mut environment = NominalEnvironment::new();
        environment.add_symbol("Reader", SymbolKind::Interface);
        environment.add_symbol("Writer", SymbolKind::Interface);
        environment.add_symbol("User", SymbolKind::Data);
        environment.add_symbol("Box", SymbolKind::Class);
        environment.add_implementation("User", "Reader");
        environment
    }

    #[test]
    fn accepts_satisfied_interface_constraint()
    {
        let generic_use = GenericUse {
            generic_name: "Read".to_string(),
            parameters: vec![parameter("T", vec![type_ref("Reader")])],
            arguments: vec![type_ref("User")],
            span: span(0, 30),
        };

        let diagnostics = GenericChecker::new().check_use(&generic_use, &environment());

        assert!(diagnostics.is_empty());
    }

    #[test]
    fn rejects_duplicate_generic_parameter()
    {
        let parameters = vec![parameter("T", vec![]), parameter("T", vec![])];

        let diagnostics = GenericChecker::new().check_parameters(&parameters, &environment());

        assert_eq!(diagnostics.len(), 1);
        assert_eq!(diagnostics[0].code, DiagnosticCode::DuplicateGenericParameter);
    }

    #[test]
    fn rejects_non_interface_constraint()
    {
        let parameters = vec![parameter("T", vec![type_ref("Box")])];

        let diagnostics = GenericChecker::new().check_parameters(&parameters, &environment());

        assert_eq!(diagnostics.len(), 1);
        assert_eq!(diagnostics[0].code, DiagnosticCode::ConstraintIsNotInterface);
    }

    #[test]
    fn rejects_generic_arity_mismatch()
    {
        let generic_use = GenericUse {
            generic_name: "Pair".to_string(),
            parameters: vec![parameter("T", vec![]), parameter("U", vec![])],
            arguments: vec![type_ref("User")],
            span: span(0, 30),
        };

        let diagnostics = GenericChecker::new().check_use(&generic_use, &environment());

        assert_eq!(diagnostics.len(), 1);
        assert_eq!(diagnostics[0].code, DiagnosticCode::GenericArityMismatch);
    }

    #[test]
    fn rejects_unsatisfied_interface_constraint()
    {
        let generic_use = GenericUse {
            generic_name: "Write".to_string(),
            parameters: vec![parameter("T", vec![type_ref("Writer")])],
            arguments: vec![type_ref("User")],
            span: span(0, 30),
        };

        let diagnostics = GenericChecker::new().check_use(&generic_use, &environment());

        assert_eq!(diagnostics.len(), 1);
        assert_eq!(diagnostics[0].code, DiagnosticCode::UnsatisfiedConstraint);
    }
}
