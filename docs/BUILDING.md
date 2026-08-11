# Building and testing

## Supported host

The supported native development host is Windows in a Visual Studio 2026 developer environment. Both C and C++ are compiled
with `clang-cl`; C uses C23 and C++ uses C++20. Ninja is the supported CMake generator and LLD is the linker.

Required tools:

- CMake 3.31 or newer;
- Ninja;
- ClangCL and LLD from the active Visual Studio/LLVM environment;
- an LLVM development package containing `LLVMConfig.cmake` and the LLVM C library;
- vcpkg;
- Rustup and Cargo;
- GHC 9.10 and Cabal;
- JDK 25; and
- the Kotlin command used by project-runtime tests.

## LLVM discovery

Do not write a machine-specific LLVM path into the repository. Use one of these mechanisms:

- set `LLVM_DIR` to the directory containing `LLVMConfig.cmake`;
- set `LLVM_ROOT` to an LLVM development installation prefix; or
- expose the package through `CMAKE_PREFIX_PATH`.

The CMake build fails at configuration time if the LLVM package or LLVM C library cannot be found.

## Submodules

Initialize all nested dependencies before configuring:

```powershell
git submodule update --init --recursive
```

The native CLI uses DIMCLI and the C++ test suite can use Catch2 from `third_party/`.

## Native dependencies

The vcpkg manifest contains `fmt` and a minimal LibArchive build with zstd support. LLVM is not built through vcpkg.

```powershell
& "$env:VCPKG_ROOT\vcpkg.exe" install `
  --triplet x64-windows `
  --x-manifest-root .
```

## Debug build

```powershell
cmake --preset clangcl-debug `
  -DCMAKE_TOOLCHAIN_FILE="$env:VCPKG_ROOT\scripts\buildsystems\vcpkg.cmake"
cmake --build --preset clangcl-debug --parallel 4
ctest --preset clangcl-debug --output-on-failure --parallel 2
```

## Sanitizer build

```powershell
cmake --preset clangcl-sanitize `
  -DCMAKE_TOOLCHAIN_FILE="$env:VCPKG_ROOT\scripts\buildsystems\vcpkg.cmake"
cmake --build --preset clangcl-sanitize --parallel 4
ctest --preset clangcl-sanitize --output-on-failure --parallel 2
```

The sanitizer configuration dynamically discovers the Clang runtime directory and copies the required AddressSanitizer DLL
beside test executables.

## Language-layer tests

Haskell:

```powershell
Set-Location xs
cabal build all
cabal test all
Set-Location haskell\visual-xsharp-compiler
cabal check
```

Rust:

```powershell
Set-Location xslang
cargo +nightly-2026-07-10 fmt --check
cargo +nightly-2026-07-10 test
cargo +nightly-2026-07-10 clippy -- -D warnings
```

Kotlin:

```powershell
.\xs_kts\gradlew.bat -p xs_kts test
```

## File-size gate

Implementation, test, build, configuration, and internal source files must remain at or below 1500 lines. Topic-oriented
public `Spec/` example suites are exempt because each file aggregates independent examples.
