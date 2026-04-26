MayaFlux 0.3.1 Patch Release
===========================

Targets removal of deprecated creational API and correctness fix in the Lila JIT compiler subsystem.

Fixes
-----

### Lila FreeType include path injection

`ClangInterpreter` was missing the `-isystem` flag for FreeType headers, causing runtime evaluation of code including `<ft2build.h>` to fail with unresolved dependencies. The JIT compiler now mirrors the existing Eigen include injection pattern, resolving the path via `Config::FREETYPE_HINT` (populated at configure time from `find_package(Freetype)`) with fallback to well-known system paths in `HostEnvironment`. Live coding workflows using Portal text rendering can now evaluate user code without manual include path specification.

Breaking Changes
----------------

### Removal of CreationProxy and pipe operator overloads

`CreationProxy<T>`, `PendingChannel<T>`, and all `operator|` overloads for domain binding have been removed. This was announced at 0.3.0 and documented throughout the 0.3 development cycle.

Migrate all call sites from the deprecated bracket-pipe syntax to the committed pipe-bracket syntax:

```cpp
// Old (removed)
object[ch] | Audio
object[{0,1}] | Audio

// New (required)
object | Audio[ch]
object | Audio[{0,1}]
```

Creator methods now return `std::shared_ptr<T>` directly without intermediary proxy objects. The domain binding is resolved at the call site via `DomainSpec::operator[]`.

Removed symbols:
- `CreationProxy<T>`
- `PendingChannel<T>`
- `operator|(shared_ptr<Node>, Domain)` and typed variants
- `operator|(PendingChannel<T>, CreationContext)`
- All `MAYAFLUX_API Domain operator|` declarations

Upgrade Notes
-------------

**Not a drop-in replacement.** Any code using the bracket-pipe syntax (`[ch] | Audio`) must be migrated to pipe-bracket syntax (`| Audio[ch]`) before compilation. The upgrade is mechanical: swap the order of the bracket and pipe operators, keeping the channel argument within the brackets.

No functional changes to committed systems. The `DomainSpec` pipe syntax and channel binding semantics remain stable.

Source: github.com/MayaFlux/MayaFlux
Docs: mayaflux.org
License: GPLv3
