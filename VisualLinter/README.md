# Visual Linter

Visual Linter provides the `vlint` command. Its semantic diagnostics come directly from the canonical Visual X# compiler
frontend. The first independent checks cover trailing whitespace, mixed line endings, and a missing final newline.

```text
vlint Program.vxs
vlint -Fix Program.vxs
vlint -List-Checks
vlint -Help
```

`-Fix` applies only safe physical-source fixes and never rewrites compiler or semantic diagnostics. `-List-Checks` prints
the stable rule identifiers currently implemented by the binary.
