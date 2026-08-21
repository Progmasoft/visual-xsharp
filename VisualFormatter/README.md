# Visual Formatter

Visual Formatter provides the `vfmt` command and validates every input with the canonical Visual X# lexer and parser before
changing it. The initial formatter normalizes line endings, trailing horizontal whitespace, and the final newline without
reconstructing comments or string literals.

```text
vfmt Program.vxs
vfmt -In-Place Program.vxs
vfmt -Dry-Run Program.vxs
vfmt -Help
```

Standard mode writes formatted source to standard output. `-In-Place` overwrites the file, while `-Dry-Run` produces no
output and returns a failing exit status when formatting would change the file.
