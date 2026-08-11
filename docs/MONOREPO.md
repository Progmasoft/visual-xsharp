# Repository layout

## Buildable components

```text
Visual/       C++20 CorePrep, Xpp, and Xmm models and passes
xs/           Native vxs compiler, driver, backend, Haskell frontend, and CMake integration
xslang/       Rust compiler core
xs_kts/       Kotlin project runtime and Visual.XSharp.kts DSL
xsrt/         Runtime components
Spec/         Public language-design example suites
tests/        Native compiler and integration tests
include/      Shared public headers
third_party/  Pinned source dependencies
```

Additional registered tool directories include `xsfmt`, `xstidy`, and `xs-analyzer`. Their registration in the repository does
not imply that they are built by the default native CMake configuration.

## CMake selection

The root CMake project currently treats `xs` as the buildable compiler project and `xsrt` as the buildable runtime. The
selection cache variables are:

```text
XS_ENABLE_PROJECTS
XS_ENABLE_RUNTIMES
```

The default values select `xs` and `xsrt`. Registered future projects are rejected with an explicit not-buildable-yet error
rather than silently ignored.

## Nested repositories

The website and IDE are maintained as separate repositories beneath the local working directory. They are not part of the
root Git index or root CMake build.

## Generated directories

Build products, Cargo targets, Cabal `dist-newstyle`, Gradle output, website distribution files, and local service state are
generated or local-only data and are not source ownership boundaries.
