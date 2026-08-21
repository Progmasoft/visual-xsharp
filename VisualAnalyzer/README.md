# Visual Analyzer

Visual Analyzer is the compiler-backed language analysis layer used by Visual X# editor integrations. It does not ship a
standalone executable: editor hosts own transport and process lifecycle while this package owns syntax, semantic, and full
frontend analysis results.

The first implemented boundary reuses the compiler's lexer, parser, resolver, type checker, and CorePrep pipeline. Compiler
diagnostics are translated to zero-based protocol positions without maintaining a second language implementation.
