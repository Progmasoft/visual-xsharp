/*
 * SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
 * SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.0
 */

use std::collections::{HashMap, HashSet};

use super::async_check::Span;

#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub enum Visibility
{
    Public,
    Internal,
    Private,
}

#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub enum SymbolKind
{
    Function,
    Class,
    Interface,
    Enum,
    Data,
    Macro,
}

#[derive(Clone, Debug, Eq, PartialEq)]
pub struct Symbol
{
    pub name: String,
    pub kind: SymbolKind,
    pub visibility: Visibility,
    pub span: Span,
}

#[derive(Clone, Debug, Eq, PartialEq)]
pub enum Import
{
    Module
    {
        module: String, span: Span
    },
    All
    {
        module: String, span: Span
    },
    Selected
    {
        module: String,
        name: String,
        alias: Option<String>,
        span: Span,
    },
}

#[derive(Clone, Debug, Eq, PartialEq)]
pub struct Module
{
    pub name: String,
    pub symbols: Vec<Symbol>,
    pub imports: Vec<Import>,
}

#[derive(Clone, Debug, Eq, PartialEq)]
pub enum DiagnosticCode
{
    DuplicateModule,
    DuplicateSymbol,
    MissingImportModule,
    MissingImportSymbol,
    PrivateImport,
}

#[derive(Clone, Debug, Eq, PartialEq)]
pub struct Diagnostic
{
    pub code: DiagnosticCode,
    pub message: String,
    pub span: Span,
}

#[derive(Clone, Debug, Eq, PartialEq)]
pub struct ResolvedModule
{
    pub name: String,
    pub local_symbols: HashMap<String, Symbol>,
    pub available_modules: HashSet<String>,
    pub imported_symbols: HashMap<String, Symbol>,
}

#[derive(Clone, Debug, Eq, PartialEq)]
pub struct Resolution
{
    pub modules: HashMap<String, ResolvedModule>,
    pub diagnostics: Vec<Diagnostic>,
}

#[derive(Default)]
pub struct Resolver
{
    diagnostics: Vec<Diagnostic>,
}

impl Resolver
{
    #[must_use]
    pub fn new() -> Self
    {
        Self::default()
    }

    #[must_use]
    pub fn resolve(mut self, modules: &[Module]) -> Resolution
    {
        let module_index = self.index_modules(modules);
        let mut resolved = HashMap::new();
        for module in modules
        {
            if resolved.contains_key(&module.name)
            {
                continue;
            }
            let local_symbols = self.collect_local_symbols(module);
            let (imported_symbols, available_modules) = self.resolve_imports(module, &module_index);
            resolved.insert(module.name.clone(), ResolvedModule {
                name: module.name.clone(),
                local_symbols,
                available_modules,
                imported_symbols,
            });
        }
        Resolution {
            modules: resolved,
            diagnostics: self.diagnostics,
        }
    }

    fn index_modules<'a>(&mut self, modules: &'a [Module]) -> HashMap<&'a str, &'a Module>
    {
        let mut index = HashMap::new();
        let mut seen = HashSet::new();
        for module in modules
        {
            if !seen.insert(module.name.as_str())
            {
                self.diagnostics.push(Diagnostic {
                    code: DiagnosticCode::DuplicateModule,
                    message: format!("module '{}' is declared more than once", module.name),
                    span: module.symbols.first().map_or(Span::new(0, 0, 0), |symbol| symbol.span),
                });
                continue;
            }
            index.insert(module.name.as_str(), module);
        }
        index
    }

    fn collect_local_symbols(&mut self, module: &Module) -> HashMap<String, Symbol>
    {
        let mut locals = HashMap::new();
        for symbol in &module.symbols
        {
            if locals.contains_key(&symbol.name)
            {
                self.diagnostics.push(Diagnostic {
                    code: DiagnosticCode::DuplicateSymbol,
                    message: format!(
                        "symbol '{}' is declared more than once in module '{}'",
                        symbol.name, module.name
                    ),
                    span: symbol.span,
                });
                continue;
            }
            locals.insert(symbol.name.clone(), symbol.clone());
        }
        locals
    }

    fn resolve_imports(
        &mut self,
        module: &Module,
        module_index: &HashMap<&str, &Module>,
    ) -> (HashMap<String, Symbol>, HashSet<String>)
    {
        let mut imports = HashMap::new();
        let mut available_modules = HashSet::from(["Panic".to_string(), "Result".to_string()]);
        for import in &module.imports
        {
            match import
            {
                Import::Module {
                    module: target,
                    span,
                } =>
                {
                    if self.find_module(module_index, target, *span).is_some() ||
                        target == "Panic" ||
                        target == "Result"
                    {
                        available_modules.insert(target.clone());
                    }
                }
                Import::All {
                    module: target,
                    span,
                } =>
                {
                    if let Some(target_module) = self.find_module(module_index, target, *span)
                    {
                        for symbol in target_module
                            .symbols
                            .iter()
                            .filter(|symbol| symbol.visibility == Visibility::Public)
                        {
                            imports.entry(symbol.name.clone()).or_insert_with(|| symbol.clone());
                        }
                    }
                }
                Import::Selected {
                    module: target,
                    name,
                    alias,
                    span,
                } =>
                {
                    if let Some(target_module) = self.find_module(module_index, target, *span)
                    {
                        self.resolve_selected_import(&mut imports, target_module, name, alias.as_deref(), *span);
                    }
                }
            }
        }
        (imports, available_modules)
    }

    fn find_module<'a>(
        &mut self,
        module_index: &HashMap<&str, &'a Module>,
        name: &str,
        span: Span,
    ) -> Option<&'a Module>
    {
        let module = module_index.get(name).copied();
        if module.is_none()
        {
            self.diagnostics.push(Diagnostic {
                code: DiagnosticCode::MissingImportModule,
                message: format!("import target module '{name}' was not found"),
                span,
            });
        }
        module
    }

    fn resolve_selected_import(
        &mut self,
        imports: &mut HashMap<String, Symbol>,
        target_module: &Module,
        name: &str,
        alias: Option<&str>,
        span: Span,
    )
    {
        let symbol = target_module.symbols.iter().find(|symbol| symbol.name == name);
        let Some(symbol) = symbol
        else
        {
            self.diagnostics.push(Diagnostic {
                code: DiagnosticCode::MissingImportSymbol,
                message: format!("symbol '{name}' was not found in module '{}'", target_module.name),
                span,
            });
            return;
        };

        if symbol.visibility != Visibility::Public
        {
            self.diagnostics.push(Diagnostic {
                code: DiagnosticCode::PrivateImport,
                message: format!("symbol '{name}' is not public"),
                span,
            });
            return;
        }

        imports.insert(alias.unwrap_or(name).to_string(), symbol.clone());
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

    fn symbol(name: &str, visibility: Visibility) -> Symbol
    {
        Symbol {
            name: name.to_string(),
            kind: SymbolKind::Function,
            visibility,
            span: span(0, 1),
        }
    }

    #[test]
    fn selected_import_resolves_public_symbol_with_alias()
    {
        let library = Module {
            name: "Math".to_string(),
            symbols: vec![symbol("Add", Visibility::Public)],
            imports: vec![],
        };
        let app = Module {
            name: "App".to_string(),
            symbols: vec![],
            imports: vec![Import::Selected {
                module: "Math".to_string(),
                name: "Add".to_string(),
                alias: Some("Plus".to_string()),
                span: span(10, 20),
            }],
        };

        let resolution = Resolver::new().resolve(&[library, app]);
        let app = resolution.modules.get("App").expect("App module should exist");

        assert!(resolution.diagnostics.is_empty());
        assert!(app.imported_symbols.contains_key("Plus"));
    }

    #[test]
    fn wildcard_import_opens_only_public_symbols()
    {
        let library = Module {
            name: "Model".to_string(),
            symbols: vec![
                symbol("User", Visibility::Public),
                symbol("Secret", Visibility::Private),
            ],
            imports: vec![],
        };
        let app = Module {
            name: "App".to_string(),
            symbols: vec![],
            imports: vec![Import::All {
                module: "Model".to_string(),
                span: span(10, 20),
            }],
        };

        let resolution = Resolver::new().resolve(&[library, app]);
        let app = resolution.modules.get("App").expect("App module should exist");

        assert!(resolution.diagnostics.is_empty());
        assert!(app.imported_symbols.contains_key("User"));
        assert!(!app.imported_symbols.contains_key("Secret"));
    }

    #[test]
    fn module_import_makes_module_available_without_scope_importing_symbols()
    {
        let library = Module {
            name: "Math".to_string(),
            symbols: vec![symbol("Add", Visibility::Public)],
            imports: vec![],
        };
        let app = Module {
            name: "App".to_string(),
            symbols: vec![],
            imports: vec![Import::Module {
                module: "Math".to_string(),
                span: span(10, 20),
            }],
        };

        let resolution = Resolver::new().resolve(&[library, app]);
        let app = resolution.modules.get("App").expect("App module should exist");

        assert!(resolution.diagnostics.is_empty());
        assert!(app.available_modules.contains("Math"));
        assert!(app.available_modules.contains("Panic"));
        assert!(app.available_modules.contains("Result"));
        assert!(!app.imported_symbols.contains_key("Math.Add"));
        assert!(!app.imported_symbols.contains_key("Add"));
    }

    #[test]
    fn reports_duplicate_symbol_in_module()
    {
        let app = Module {
            name: "App".to_string(),
            symbols: vec![symbol("Main", Visibility::Public), symbol("Main", Visibility::Public)],
            imports: vec![],
        };

        let resolution = Resolver::new().resolve(&[app]);

        assert_eq!(resolution.diagnostics.len(), 1);
        assert_eq!(resolution.diagnostics[0].code, DiagnosticCode::DuplicateSymbol);
    }

    #[test]
    fn reports_missing_import_module()
    {
        let app = Module {
            name: "App".to_string(),
            symbols: vec![],
            imports: vec![Import::All {
                module: "Missing".to_string(),
                span: span(10, 20),
            }],
        };

        let resolution = Resolver::new().resolve(&[app]);

        assert_eq!(resolution.diagnostics.len(), 1);
        assert_eq!(resolution.diagnostics[0].code, DiagnosticCode::MissingImportModule);
    }

    #[test]
    fn reports_private_selected_import()
    {
        let library = Module {
            name: "Model".to_string(),
            symbols: vec![symbol("Secret", Visibility::Private)],
            imports: vec![],
        };
        let app = Module {
            name: "App".to_string(),
            symbols: vec![],
            imports: vec![Import::Selected {
                module: "Model".to_string(),
                name: "Secret".to_string(),
                alias: None,
                span: span(10, 20),
            }],
        };

        let resolution = Resolver::new().resolve(&[library, app]);

        assert_eq!(resolution.diagnostics.len(), 1);
        assert_eq!(resolution.diagnostics[0].code, DiagnosticCode::PrivateImport);
    }
}
