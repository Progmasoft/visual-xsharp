<!--
SPDX-FileCopyrightText: 2026 Leitwolf <xs-lang.chess031@slmails.com>
SPDX-License-Identifier: MPL-2.0
-->

# Runtime ABI

`xsrt` is the buildable C23 runtime boundary. Its public umbrella header is `<xs/runtime.h>`, and ABI version 0 is
declared by `XS_RUNTIME_ABI_VERSION`.

The first implemented owned value is `Optional<Str>`. `Str` remains an immutable borrowed UTF-16 code-unit view with no
required null terminator. `Optional<Str>` stores either `None` as a null opaque box or `Some` as a heap-owned copy.
`Some("")` has a non-null box and is therefore distinct from `None`. Clone performs a deep copy, borrow returns a view
valid until the owner is dropped, and drop clears the handle. The runtime stores host-order `uint16_t` units; XLIL’s
explicit `utf16le`/`utf16be` representation remains the serialization and backend boundary.

This first slice does not define a general allocator ABI, garbage collection, panic transport, I/O, threading, or owned
`String` lowering. Those surfaces will be added only with matching compiler lowering and tests.
