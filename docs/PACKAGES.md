<!--
SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
SPDX-License-Identifier: MPL-2.0
-->

# Packages and registry commands

The package registry is an early infrastructure track and is not yet available for production use. The compiler remains
usable without a registry.

The command contract is:

```text
xs resolve
xs login
xs publish
xs update
xs yank <module>@<version>
```

`xs resolve` is implemented for local KTS project coordinates. It reevaluates the project and atomically replaces
`Visual.XSharp.Lockfile.sqlite3` with a real binary SQLite database. It never writes a text lock or SQL dump. Registry-backed version
selection and downloads are not active yet.

`xs login` prompts without echo for a GitHub personal access token. The service uses it once to establish the GitHub
identity, does not store it, and returns a revocable Visual X# registry token. `xs publish` requires the evaluated project to set
`PUBLISH` to `true`; versions are immutable after publication. `xs update` resolves dependencies and replaces
`Visual.XSharp.Lockfile.sqlite3` atomically only after the complete solution succeeds. `vxs yank` excludes a version from new solutions
without breaking projects that already pin it in a lock file.

Registry transport, artifact signing, namespace policy, and public service availability are still under development.

## Package archive v0

The public C23 package API writes and verifies `.xspkg.tar.zst` archives. Version 0 requires an `xspkg.json` regular-file
entry, rejects unsafe or duplicate paths and non-regular entries, and imposes explicit entry-count and unpacked-size
limits. Writer output is lexicographically ordered and uses deterministic tar metadata. Verification reports the SHA-256
digest of the compressed artifact plus compressed/unpacked sizes. Manifest schema validation and registry upload are
separate later layers.
