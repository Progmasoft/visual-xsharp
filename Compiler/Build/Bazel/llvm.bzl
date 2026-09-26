"""Discovers an installed LLVM development tree for the C++20 backend."""

_COMPONENTS = [
    "core",
    "bitwriter",
    "passes",
    "support",
    "targetparser",
    "nativecodegen",
    "bitreader",
    "orcjit",
    "executionengine",
    "runtimedyld",
    "object",
]

def _quote(value):
    return "\"{}\"".format(value.replace("\\", "/"))

def _llvm_repository_impl(repository_ctx):
    root = repository_ctx.os.environ.get("LLVM_ROOT")
    is_windows = repository_ctx.os.name.lower().startswith("windows")
    config = None
    if root:
        executable = "llvm-config.exe" if is_windows else "llvm-config"
        candidate = repository_ctx.path(root).get_child("bin").get_child(executable)
        if candidate.exists:
            config = candidate
    if config == None:
        config = repository_ctx.which("llvm-config") or repository_ctx.which("llvm-config.exe")
    if config == None:
        fail("LLVM was not found. Set LLVM_ROOT to the LLVM development tree or put llvm-config on PATH.")

    # Distribution packages do not have to put headers and archives directly
    # below --prefix. Fedora, for example, uses a versioned lib64 LLVM tree.
    # Query the selected llvm-config for each location so its headers and
    # libraries always belong to the same installation.
    include_location = repository_ctx.execute([config, "--includedir"], quiet = True)
    library_location = repository_ctx.execute([config, "--libdir"], quiet = True)
    if include_location.return_code != 0 or library_location.return_code != 0:
        fail("llvm-config could not resolve its include/library directories:\n{}\n{}".format(
            include_location.stderr,
            library_location.stderr,
        ))
    include_dir = repository_ctx.path(include_location.stdout.strip())
    library_dir = repository_ctx.path(library_location.stdout.strip())
    if not include_dir.exists or not library_dir.exists:
        fail("llvm-config reported missing include/library directories: {} and {}".format(
            include_dir,
            library_dir,
        ))

    libraries = repository_ctx.execute(
        [config, "--link-static", "--libnames"] + _COMPONENTS,
        quiet = True,
    )
    if libraries.return_code != 0:
        fail("llvm-config could not resolve backend components:\n{}".format(libraries.stderr))

    library_suffix = ".lib" if is_windows else ".a"
    library_names = [
        name
        for name in libraries.stdout.replace("\r", " ").replace("\n", " ").split(" ")
        if name.endswith(library_suffix)
    ]
    if not library_names:
        fail("llvm-config returned no static libraries for the native backend")

    # Query non-LLVM dependencies from the chosen development package. LLVM's
    # compression, terminal, and XML dependencies differ between installations,
    # so a checked-in macOS library list would be brittle.
    if is_windows:
        system_linkopts = [
            "/DEFAULTLIB:advapi32.lib",
            "/DEFAULTLIB:ntdll.lib",
            "/DEFAULTLIB:ole32.lib",
            "/DEFAULTLIB:psapi.lib",
            "/DEFAULTLIB:shell32.lib",
            "/DEFAULTLIB:uuid.lib",
            "/DEFAULTLIB:ws2_32.lib",
        ]
    else:
        system_libraries = repository_ctx.execute(
            [config, "--link-static", "--system-libs"] + _COMPONENTS,
            quiet = True,
        )
        if system_libraries.return_code != 0:
            fail("llvm-config could not resolve platform libraries:\n{}".format(system_libraries.stderr))
        system_linkopts = [
            option
            for option in system_libraries.stdout.replace("\r", " ").replace("\n", " ").split(" ")
            if option
        ]

        # Homebrew keeps zstd keg-only on Apple Silicon and Intel hosts. LLVM's
        # system library list therefore names -lzstd without making its cellar
        # visible to Apple's linker. Discover that prefix at repository analysis
        # time instead of committing an architecture-specific /opt path.
        if "-lzstd" in system_linkopts:
            brew = repository_ctx.which("brew")
            if brew:
                zstd_prefix = repository_ctx.execute(
                    [brew, "--prefix", "zstd"],
                    quiet = True,
                )
                if zstd_prefix.return_code == 0:
                    zstd_library = repository_ctx.path(zstd_prefix.stdout.strip()).get_child("lib")
                    if zstd_library.exists:
                        system_linkopts.insert(0, "-L{}".format(zstd_library))

    repository_ctx.symlink(include_dir, "include")
    repository_ctx.symlink(library_dir, "lib")

    imports = []
    dependencies = [":headers"]
    for index, library_name in enumerate(library_names):
        target = "component_{}".format(index)
        imports.append("cc_import(name = {}, static_library = {})".format(
            _quote(target),
            _quote("lib/{}".format(library_name)),
        ))
        dependencies.append(":{}".format(target))

    build = """load(\"@rules_cc//cc:cc_import.bzl\", \"cc_import\")
load(\"@rules_cc//cc:cc_library.bzl\", \"cc_library\")

package(default_visibility = [\"//visibility:public\"])

cc_library(
    name = \"headers\",
    hdrs = glob([\"include/**/*.def\", \"include/**/*.h\", \"include/**/*.inc\"]),
    includes = [\"include\"],
)

{}

cc_library(
    name = \"llvm\",
    deps = {},
    linkopts = {},
)
""".format("\n".join(imports), repr(dependencies), repr(system_linkopts))
    repository_ctx.file("BUILD.bazel", build)

_llvm_repository = repository_rule(
    implementation = _llvm_repository_impl,
    environ = ["LLVM_ROOT", "PATH"],
    local = True,
)

def _llvm_extension_impl(module_ctx):
    _llvm_repository(name = "llvm")

llvm = module_extension(implementation = _llvm_extension_impl)
