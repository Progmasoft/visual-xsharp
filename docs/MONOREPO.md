# Repository layout

## Buildable components

```text
Visual/       C++20 CorePrep, Xpp, and Xmm models and passes
xs/           Native vxs compiler, driver, backend, Haskell frontend, and CMake integration
xslang/       Rust compiler core
xs_kts/       Kotlin project evaluator, VXDC, and Visual.XSharp.kts DSL
xsrt/         Runtime components
Spec/         Public language-design example suites
tests/        Native compiler and integration tests
include/      Shared public headers
third_party/  Pinned source dependencies
```

The ecosystem tools have independent ownership and build systems:

```text
VisualAnalyzer/   Haskell LSP, Kotlin configuration DSL, IntelliJ plugin, and VS Code extension
VisualFormatter/  Haskell vfmt executable and Kotlin configuration DSL
VisualLinter/     Haskell vlint executable and Kotlin configuration DSL
```

These canonical projects replace the retired `xs-analyzer`, `xsfmt`, and `xstidy` prototypes. They are not native CMake
subprojects. Their Haskell components use Cabal, their Kotlin configuration layers use Gradle with an external JRE 25 and
Kotlin Script Runner, and the Visual Analyzer editor integrations additionally use their platform-specific build systems.

## CMake selection

The root CMake project currently treats `xs` as the buildable compiler project and `xsrt` as the buildable runtime. The
selection cache variables are:

```text
XS_ENABLE_PROJECTS
XS_ENABLE_RUNTIMES
```

The default values select `xs` and `xsrt`. Cargo, Cabal, Gradle, IntelliJ Platform, and pnpm projects remain outside this
native selector and are composed by repository-level orchestration and CI.

## Nested repositories

The website and IDE are maintained as separate repositories beneath the local working directory. They are not part of the
root Git index or root CMake build.

## Generated directories

Build products, Cargo targets, Cabal `dist-newstyle`, Gradle output, website distribution files, and local service state are
generated or local-only data and are not source ownership boundaries.
