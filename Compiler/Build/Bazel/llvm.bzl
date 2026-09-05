"""Discovers an installed LLVM development tree for the C++20 backend."""

_COMPONENTS = [
    "core",
    "bitwriter",
    "passes",
    "support",
    "targetparser",
    "nativecodegen",
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

    if not root:
        prefix = repository_ctx.execute([config, "--prefix"], quiet = True)
        if prefix.return_code != 0:
            fail("llvm-config --prefix failed:\n{}".format(prefix.stderr))
        root = prefix.stdout.strip()

    include_dir = repository_ctx.path(root).get_child("include")
    library_dir = repository_ctx.path(root).get_child("lib")
    if not include_dir.exists or not library_dir.exists:
        fail("LLVM_ROOT must contain include/ and lib/: {}".format(root))

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
