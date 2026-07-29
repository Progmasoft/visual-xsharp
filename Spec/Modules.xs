// SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
// SPDX-License-Identifier: MPL-2.0

// X# module system
//
// `module` is not an X# keyword. A .xs file never declares its own module.
// Module identity and file membership are project metadata supplied by
// xs-project through xs.module.kts.
//
// xs-project is required when a project uses xs.module.kts. xs-compiler does
// not interpret Kotlin scripts and does not guess module membership from a
// directory or file name.
//
// Project source selection in xs.project.kts:
//
// project("Example", "BETA", "0.1.0")
//
// source {
//   include("Sources")
// }
//
// module {
//   include("Modules")
// }
//
// Module membership in xs.module.kts:
//
// module {
//   name("Math")
//   members {
//     add("Modules/Math/*.xs")
//   }
// }
//
// module {
//   name("Application")
//   add("Modules/Application/start.xs")
//   submodule {
//     name("util")
//     add("Modules/Application/Utils/**/*.xs")
//   }
// }
//
// `add` accepts either one concrete .xs path or a glob. Direct add/members
// records belong to the named module. A submodule record belongs to the
// qualified module path formed with `::`.
//
// - Math members are named as Math::<item>.
// - Application members are named as Application::<item>.
// - Application util members are named as Application::util::<item>.
//
// A module source must be selected by module.include(...) and assigned exactly
// once by xs.module.kts. An unassigned file, duplicate membership, missing path,
// empty glob, or source/module registry overlap is a project error.


// ============================================================
// Import
// ============================================================

// `import` makes a module available through its qualified name. It does not
// copy public declarations into local scope.

import Math;

fn use_qualified_module() -> Int
{
  return Math::add(10, 20);
}

// Multiple modules may be imported by one declaration.

import Geometry, Text;

// Duplicate import records are valid and behave as one import.

import Math;


// ============================================================
// Using
// ============================================================

// `using` makes one qualified item available under a local name. Glob is not
// allowed with using.

using Math::add;
using sum = Math::add;

fn use_selected_item() -> Int
{
  return add(1, 2) + sum(3, 4);
}

// `using namespace` makes the public members of a namespace/module path
// available. Glob is allowed only with using namespace.

using namespace Geometry::*;

// `using namespace Geometry;` is also valid. `using Geometry::*;` is invalid;
// only using namespace may carry a trailing glob.


// ============================================================
// Namespace
// ============================================================

// namespace remains a source declaration. It organizes declarations inside
// the module assigned by project metadata. It does not assign module identity.

namespace detail;

internal fn normalize(value: Int) -> Int
{
  return value;
}

// If this file belongs to module Math, the function above is named
// Math::detail::normalize.
//
// Namespace syntax is optional. A file may instead contain multiple block
// namespaces:

// namespace algebra
// {
//   public fn identity(value: Int) -> Int
//   {
//     return value;
//   }
// }
//
// namespace geometry
// {
//   public fn origin() -> (Int, Int)
//   {
//     return (0, 0);
//   }
// }

// One file may use one source-scoped `namespace path;` followed by one or more
// block-scoped namespaces. Block namespaces may nest. A file may also omit the
// source-scoped form and use only block namespaces. Namespace syntax itself is
// optional.


// ============================================================
// Visibility
// ============================================================

// The default visibility is internal. Internal declarations are visible to
// every source assigned to the same module, including separate physical files.
// public declarations are visible to importing modules. private/protected
// retain their type/member visibility meanings.

public fn exported(value: Int) -> Int
{
  return value;
}

internal fn module_only(value: Int) -> Int
{
  return value;
}


// ============================================================
// Canonical project layout
// ============================================================

// For X# sources, PascalCase directory names and snake_case .xs file names are
// canonical. This is a style convention, not a compilation requirement.
//
// Canonical:
//   Modules/Math/integer_ops.xs
//   Modules/Application/Utils/path_tools.xs
//
// Accepted but non-canonical:
//   modules/math/IntegerOps.xs


// ============================================================
// Invalid legacy syntax
// ============================================================

// The following is not an X# declaration:
//
// module math;
//
// `module` lexes as an ordinary identifier. Module membership can only come
// from project metadata.
