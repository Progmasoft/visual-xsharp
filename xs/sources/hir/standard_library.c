/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 */

#include "standard_library.h"

#include <string.h>

typedef struct
{
    const char *name;
    const char *required_import;
    const char *alternate_import;
} StandardCallInfo;

static const XsHirStandardTypeInfo standard_types[] = {
    {.canonical_name = "String", .min_arity = 0, .max_arity = 0},
    {.canonical_name = "std.optional.Optional", .short_name = "Optional", .min_arity = 1, .max_arity = 1},
    {.canonical_name = "std.result.Result", .short_name = "Result", .min_arity = 1, .max_arity = 2},
    {.canonical_name = "std.result.Error", .short_name = "Error", .min_arity = 0, .max_arity = 0},
    {.canonical_name = "Task", .min_arity = 1, .max_arity = 1},
    {.canonical_name = "ArrayList", .min_arity = 1, .max_arity = 1},
    {.canonical_name = "std.fs.File", .short_name = "File", .required_import = "fs"},
    {.canonical_name = "std.fs.OpenOptions", .short_name = "OpenOptions", .required_import = "fs"},
    {.canonical_name = "std.process.Args", .short_name = "Args", .required_import = "process"},
    {.canonical_name = "std.http.Client", .short_name = "Client", .required_import = "http"},
    {.canonical_name = "std.http.Request", .short_name = "Request", .required_import = "http"},
    {.canonical_name = "std.http.Response",
     .short_name = "Response",
     .required_import = "http",
     .min_arity = 1,
     .max_arity = 1},
    {.canonical_name = "std.http.Uri", .short_name = "Uri", .required_import = "http"},
    {.canonical_name = "std.http.BodyHandlers", .short_name = "BodyHandlers", .required_import = "http"},
    {.canonical_name = "std.http.BodyPublishers", .short_name = "BodyPublishers", .required_import = "http"},
    {.canonical_name = "std.net.TcpListener", .short_name = "TcpListener", .required_import = "net"},
    {.canonical_name = "std.net.TcpStream", .short_name = "TcpStream", .required_import = "net"},
    {.canonical_name = "std.thread.Channel",
     .short_name = "Channel",
     .required_import = "thread",
     .min_arity = 1,
     .max_arity = 1},
    {.canonical_name = "std.thread.Sender",
     .short_name = "Sender",
     .required_import = "thread",
     .min_arity = 1,
     .max_arity = 1},
    {.canonical_name = "std.sync.Mutex",
     .short_name = "Mutex",
     .required_import = "mutex",
     .alternate_import = "sync",
     .min_arity = 1,
     .max_arity = 1},
    {.canonical_name = "std.sync.RwLock",
     .short_name = "RwLock",
     .required_import = "rw_lock",
     .alternate_import = "sync",
     .min_arity = 1,
     .max_arity = 1},
    {.canonical_name = "std.sync.Arc",
     .short_name = "Arc",
     .required_import = "arc",
     .alternate_import = "sync",
     .min_arity = 1,
     .max_arity = 1},
    {.canonical_name = "std.sync.Weak",
     .short_name = "Weak",
     .required_import = "arc",
     .alternate_import = "sync",
     .min_arity = 1,
     .max_arity = 1},
    {.canonical_name = "std.sync.Atomic",
     .short_name = "Atomic",
     .required_import = "atomic",
     .alternate_import = "sync",
     .min_arity = 1,
     .max_arity = 1},
    {.canonical_name = "std.sync.CancellationToken", .short_name = "CancellationToken", .required_import = "sync"},
    {.canonical_name = "std.cffi.CStr", .required_import = "cffi"},
    {.canonical_name = "std.cffi.CString", .required_import = "cffi"},
    {.canonical_name = "std.cffi.File", .required_import = "cffi"},
    {.canonical_name = "std.cffi.VarArgs", .required_import = "cffi"},
    {.canonical_name = "std.cffi.RawPtr", .required_import = "cffi", .min_arity = 1, .max_arity = 1},
    {.canonical_name = "std.cffi.NonNull", .required_import = "cffi", .min_arity = 1, .max_arity = 1},
    {.canonical_name = "std.cffi.Slice", .required_import = "cffi", .min_arity = 1, .max_arity = 1},
    {.canonical_name = "std.cffi.Handle", .required_import = "cffi", .min_arity = 1, .max_arity = 1},
    {.canonical_name = "std.cffi.Owned", .required_import = "cffi", .min_arity = 1, .max_arity = 1},
    {.canonical_name = "std.cffi.Borrowed", .required_import = "cffi", .min_arity = 1, .max_arity = 1},
    {.canonical_name = "std.cffi.Out", .required_import = "cffi", .min_arity = 1, .max_arity = 1},
    {.canonical_name = "std.cffi.DynamicLibrary", .required_import = "cffi"},
    {.canonical_name = "std.cffi.Symbol", .required_import = "cffi", .min_arity = 1, .max_arity = 1},
};

static const StandardCallInfo standard_calls[] = {
    {.name = "Ok"},
    {.name = "Error"},
    {.name = "Some"},
    {.name = "Int.parse"},
    {.name = "Long.parse"},
    {.name = "Integer.parse"},
    {.name = "Float.parse"},
    {.name = "SFloat.parse"},
    {.name = "std.result.Ok"},
    {.name = "std.result.Error"},
    {.name = "std.optional.Some"},
    {.name = "std.stdout", .required_import = "stdio"},
    {.name = "std.stderr", .required_import = "stdio"},
    {.name = "std.stdin", .required_import = "stdio"},
    {.name = "std.fmt.format", .required_import = "stdio"},
    {.name = "std._print", .required_import = "stdio"},
    {.name = "std._eprint", .required_import = "stdio"},
    {.name = "std.fs.write", .required_import = "fs"},
    {.name = "std.fs.read", .required_import = "fs"},
    {.name = "std.fs.read_to_str", .required_import = "fs"},
    {.name = "std.fs.create_dir", .required_import = "fs"},
    {.name = "std.fs.create_file", .required_import = "fs"},
    {.name = "std.fs.copy_file", .required_import = "fs"},
    {.name = "std.fs.copy_dir", .required_import = "fs"},
    {.name = "std.fs.move", .required_import = "fs"},
    {.name = "std.fs.remove_file", .required_import = "fs"},
    {.name = "std.fs.remove_dir", .required_import = "fs"},
    {.name = "std.fs.list_dir", .required_import = "fs"},
    {.name = "std.fs.walk", .required_import = "fs"},
    {.name = "std.fs.walk_dir", .required_import = "fs"},
    {.name = "std.fs.exists", .required_import = "fs"},
    {.name = "std.fs.is_dir", .required_import = "fs"},
    {.name = "std.fs.is_file", .required_import = "fs"},
    {.name = "std.fs.relative_path", .required_import = "fs"},
    {.name = "std.fs.join_path", .required_import = "fs"},
    {.name = "std.fs.parent_dir", .required_import = "fs"},
    {.name = "std.fs.size", .required_import = "fs"},
    {.name = "std.fs.modified_ticks", .required_import = "fs"},
    {.name = "std.process.run", .required_import = "process"},
    {.name = "std.process.execute", .required_import = "process"},
    {.name = "std.thread.channel", .required_import = "thread"},
    {.name = "std.thread.spawn", .required_import = "thread"},
    {.name = "std.thread.sleep", .required_import = "thread"},
    {.name = "std.thread.yield", .required_import = "thread"},
    {.name = "hardware.read_float", .required_import = "hardware"},
    {.name = "std.net.listen", .required_import = "net"},
    {.name = "std.fs.File.open", .required_import = "fs"},
    {.name = "std.fs.OpenOptions.new", .required_import = "fs"},
    {.name = "std.http.Request.builder", .required_import = "http"},
    {.name = "std.http.Uri.create", .required_import = "http"},
    {.name = "std.http.BodyHandlers.of_str", .required_import = "http"},
    {.name = "std.http.BodyPublishers.of_str", .required_import = "http"},
    {.name = "std.sync.Arc.new", .required_import = "arc", .alternate_import = "sync"},
    {.name = "std.sync.Arc.clone", .required_import = "arc", .alternate_import = "sync"},
    {.name = "std.sync.Mutex.new", .required_import = "mutex", .alternate_import = "sync"},
    {.name = "std.sync.RwLock.new", .required_import = "rw_lock", .alternate_import = "sync"},
    {.name = "std.sync.Atomic.new", .required_import = "atomic", .alternate_import = "sync"},
    {.name = "std.sync.CancellationToken.timeout", .required_import = "sync"},
    {.name = "Arc.new", .required_import = "arc", .alternate_import = "sync"},
    {.name = "Arc.clone", .required_import = "arc", .alternate_import = "sync"},
    {.name = "Mutex.new", .required_import = "mutex", .alternate_import = "sync"},
    {.name = "RwLock.new", .required_import = "rw_lock", .alternate_import = "sync"},
    {.name = "Atomic.new", .required_import = "atomic", .alternate_import = "sync"},
};

static bool import_available(const XsHirImportScope *import, const char *required, const char *alternate)
{
    return required == nullptr || xs_hir_import_scope_has_module(import, required) ||
           (alternate != nullptr && xs_hir_import_scope_has_module(import, alternate));
}

const XsHirStandardTypeInfo *xs_hir_standard_type_find(const char *name)
{
    if(name == nullptr)
        return nullptr;
    for(size_t i = 0; i < sizeof(standard_types) / sizeof(standard_types[0]); ++i)
    {
        if(strcmp(standard_types[i].canonical_name, name) == 0 ||
           (standard_types[i].short_name != nullptr && strcmp(standard_types[i].short_name, name) == 0))
            return &standard_types[i];
    }
    return nullptr;
}

XsHirStandardLookup xs_hir_standard_type_lookup(const XsHirStandardTypeInfo *type, const XsHirImportScope *import)
{
    if(type == nullptr)
        return XS_HIR_STANDARD_UNKNOWN;
    return import_available(import, type->required_import, type->alternate_import) ? XS_HIR_STANDARD_AVAILABLE
                                                                                   : XS_HIR_STANDARD_MISSING_IMPORT;
}

XsHirStandardLookup xs_hir_standard_call_lookup(const char *name, const XsHirImportScope *import)
{
    if(name == nullptr)
        return XS_HIR_STANDARD_UNKNOWN;
    for(size_t i = 0; i < sizeof(standard_calls) / sizeof(standard_calls[0]); ++i)
    {
        if(strcmp(standard_calls[i].name, name) != 0)
            continue;
        return import_available(import, standard_calls[i].required_import, standard_calls[i].alternate_import)
                   ? XS_HIR_STANDARD_AVAILABLE
                   : XS_HIR_STANDARD_MISSING_IMPORT;
    }
    return XS_HIR_STANDARD_UNKNOWN;
}

bool xs_hir_standard_module_name(const char *name)
{
    static const char *const modules[] = {
        "arc",   "atomic", "attrs",    "cffi",         "fs",         "hardware", "http",
        "mutex", "net",    "optional", "panic",        "process",    "result",   "rw_lock",
        "stdio", "sync",   "thread",   "std.optional", "std.result",
    };
    for(size_t i = 0; i < sizeof(modules) / sizeof(modules[0]); ++i)
    {
        if(strcmp(name, modules[i]) == 0)
            return true;
    }
    return false;
}
