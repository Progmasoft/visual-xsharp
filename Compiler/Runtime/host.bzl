"""Link options of a program that hosts generated code.

A compiled program calls the Visual X# runtime: a closure allocates through
it, an argument that is passed by need does, a string is one of its objects,
and console output is one of its functions. The JIT resolves a runtime symbol
in the process that hosts it, so every program that runs compiled programs
links the runtime libraries and exports their entry points.

The list is the C ABI of `Visual/XSharp/Runtime/AARC.h` and
`Visual/XSharp/Runtime/Text.h`. A function added to either header is added
here, or a program that calls it fails to resolve it under the JIT on
Windows, where nothing is exported that is not named.
"""

RUNTIME_HOST_DEPS = [
    "//Compiler/Headers/Visual/XSharp/Runtime:aarc_api",
    "//Compiler/Headers/Visual/XSharp/Runtime:text_api",
    "//Compiler/Runtime/AARC:aarc",
    "//Compiler/Runtime/Text:text",
]

_ENTRY_POINTS = [
    "vxs_aarc_abi_version",
    "vxs_aarc_allocate",
    "vxs_aarc_copy_unowned",
    "vxs_aarc_copy_weak",
    "vxs_aarc_is_exact_type",
    "vxs_aarc_load_unowned",
    "vxs_aarc_lock_weak",
    "vxs_aarc_make_unowned",
    "vxs_aarc_make_weak",
    "vxs_aarc_release_strong",
    "vxs_aarc_release_unowned",
    "vxs_aarc_release_weak",
    "vxs_aarc_retain_strong",
    "vxs_aarc_string_literal",
    "vxs_console_write",
    "vxs_text_concat",
    "vxs_text_equals",
    "vxs_text_format_char",
    "vxs_text_format_floating",
    "vxs_text_format_signed",
    "vxs_text_format_string",
    "vxs_text_format_unsigned",
    "vxs_text_from_bool",
    "vxs_text_from_char",
    "vxs_text_from_signed",
    "vxs_text_from_unsigned",
    "vxs_text_newline",
]

RUNTIME_HOST_LINKOPTS = select({
    "@platforms//os:windows": ["/EXPORT:" + name for name in _ENTRY_POINTS],
    "@platforms//os:macos": [],
    "//conditions:default": ["-rdynamic"],
})
