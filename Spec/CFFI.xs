// SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
// SPDX-License-Identifier: MPL-2.0

// C foreign-function interface:
//
// CFFI is opt-in. There is no automatic import and no automatic scope
// injection. The official module is the standard CFFI module. User code import
// it explicitly:

import cffi;


// extern function declarations

#[repr(C)]
extern "C" {
    #[LinkName("puts")]
    #[NoUnwind]
    fn puts(text: std::cffi::CStr) -> Int;

    #[LinkName("strlen")]
    #[NoUnwind]
    fn strlen(text: std::cffi::CStr) -> ULong;
}


fn print_line(message: &Str) -> Result<()> {
    c_text: std::cffi::CString = std::cffi::CString::from_str(message)@;
    else: puts(c_text.as_c_str());
    return Ok(());
}


// library and header metadata

#[LinkLibrary("c")]
#[Header("stdio.h")]
#[repr(C)]
extern "C" {
    fn printf(format: std::cffi::CStr, args: std::cffi::VarArgs) -> Int;

    #[ThreadLocal]
    #[LinkName("errno")]
    static errno: Int;

    #[LinkName("stdin")]
    static stdin_handle: std::cffi::RawPtr<std::cffi::File>;

    #[LinkName("stdout")]
    static stdout_handle: std::cffi::RawPtr<std::cffi::File>;
}

#[LinkLibrary("m")]
#[Header("math.h")]
#[repr(C)]
extern "C" {
    #[LinkName("cos")]
    fn c_cos(value: Double) -> Double;
}


// explicit calling convention and symbol controls

#[Abi("C")]
#[CallingConvention(Cdecl)]
#[repr(C)]
extern "C" {
    #[ExportName("xs_plugin_init")]
    #[SymbolVisibility(Default)]
    fn plugin_init() -> Int;
}


// C layout types

#[repr(C)]
data CPoint {
    x: Double,
    y: Double,
}

#[repr(C)]
#[packed(1)]
data PackedHeader {
    tag: UShort,
    length: UInt,
}

#[transparent]
data FileDescriptor {
    value: Int,
}

#[Opaque]
data NativeLibrary {
    handle: std::cffi::Handle<NativeLibrary>,
}


// dynamic library loading

#[DynamicLibrary]
#[NoUnwind]
fn open_library(path: &Str) -> Result<NativeLibrary, Error> {
    return std::cffi::DynamicLibrary::open(path);
}


// pointers, ownership, and unsafe boundaries

#[Unsafe]
#[repr(C)]
extern "C" {
    fn malloc(size: ULong) -> std::cffi::RawPtr<std::cffi::Void>;

    fn free(ptr: std::cffi::RawPtr<std::cffi::Void>);
}

fn allocate(size: ULong) -> Result<std::cffi::RawPtr<std::cffi::Void>, Error> {
    ptr: std::cffi::RawPtr<std::cffi::Void> = malloc(size);
    if (ptr.is_null()) {
        return Error({ message: "allocation failed" });
    }
    return Ok(ptr);
}


// callbacks

data CompareContext {
    order: Int,
}

#[CFunctionPointer]
type CompareFn = fn(
    left: std::cffi::RawPtr<std::cffi::Void>,
    right: std::cffi::RawPtr<std::cffi::Void>,
    context: std::cffi::RawPtr<CompareContext>,
) -> Int;

#[repr(C)]
extern "C" {
    #[LinkName("qsort_r")]
    fn qsort_r(
        base: std::cffi::RawPtr<std::cffi::Void>,
        count: ULong,
        width: ULong,
        compare: CompareFn,
        context: std::cffi::RawPtr<CompareContext>,
    );
}


// owned/borrowed/out pointer contracts

#[repr(C)]
extern "C" {
    #[Ownership(ReturnsOwned)]
    fn make_buffer(length: ULong) -> std::cffi::Owned<std::cffi::RawPtr<u8>>;

    #[Ownership(TakesOwnership)]
    fn destroy_buffer(buffer: std::cffi::Owned<std::cffi::RawPtr<u8>>);

    #[Ownership(Borrows)]
    fn inspect_buffer(buffer: std::cffi::Borrowed<std::cffi::RawPtr<u8>>, length: ULong) -> Int;

    #[Ownership(OutParameter)]
    fn read_status(status: std::cffi::Out<Int>) -> Int;
}


// thread/async-aware FFI markers

#[repr(C)]
extern "C" {
    #[ForeignThreadSafe]
    #[NoCallbackIntoRuntime]
    fn thread_safe_poll(handle: std::cffi::RawPtr<std::cffi::Void>) -> Int;

    #[MayBlock]
    #[CancellationUnsafe]
    fn blocking_read(fd: Int, buffer: std::cffi::RawPtr<u8>, length: ULong) -> Long;
}


// Strong CFFI surface model:
//
// - std::cffi::RawPtr<T>: nullable raw pointer.
// - std::cffi::NonNull<T>: non-null raw pointer.
// - std::cffi::CStr: borrowed null-terminated C string.
// - std::cffi::CString: owned null-terminated C string.
// - std::cffi::Slice<T>: pointer plus length view for explicit FFI boundaries.
// - std::cffi::VarArgs: marker for C varargs positions.
// - std::cffi::Handle<T>: opaque native handle with explicit ownership policy.
// - std::cffi::File: opaque C FILE handle marker.
// - std::cffi::Owned<T>, std::cffi::Borrowed<T>, and std::cffi::Out<T>: FFI ownership markers.
// - std::cffi::DynamicLibrary: runtime loader with explicit symbol lookup.
// - std::cffi::Symbol<T>: typed dynamic symbol wrapper.
//
// CFFI does not infer safety. Calls marked Unsafe require an unsafe boundary in
// the completed language. Layout, ABI, unwind, thread, callback, ownership, and
// lifetime checks are verified by HIR/MIR before backend lowering.
