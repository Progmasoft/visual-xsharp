# Visual X# specification examples

The `.vxs` files in this directory are topic-oriented language design examples. Each numbered fragment is independent and
includes context that explains the intended rule, whether the fragment is valid or invalid, and which surrounding declarations
may be omitted for brevity.

These files are not concatenated programs. Repeated declarations, incomplete context, and deliberately rejected examples are
intentional. A file may therefore be useful as a design reference without being directly compilable as one application.

The directory records current language intent; it is not an implementation-completeness claim. The production compiler is
still moving to the Haskell-through-CorePrep and C++20 Xpp/Xmm architecture, so some examples describe behavior that has not
yet reached the production `vxs` route.

When a language rule is implemented, it should gain a focused compiler test in addition to its explanatory example here.
Compiler behavior must not be inferred from historical fixtures when it conflicts with the current examples.

See [the specification guide](../docs/SPECIFICATION.md) for the topic map and maintenance rules.
