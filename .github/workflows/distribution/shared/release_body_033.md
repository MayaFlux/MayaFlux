MayaFlux 0.3.3 Release
======================

Unifies temporal channel binding with DomainSpec and fixes library installation paths on multi-lib systems.

Breaking Changes
----------------

### Temporal channel binding unified with DomainSpec

`Time()` now takes duration only. Channel binding moves to the domain site via `DomainSpec::operator[]`, consistent with Creator methods introduced in 0.3.1.

```cpp
// Old (removed)
node   >> Time(2.0, 0) | Audio;
node   >> Time(2.0, {0, 1}) | Audio;
buffer >> Time(2.0, 0) | Audio;

// New (required)
node   >> Time(2.0) | Audio[0];
node   >> Time(2.0) | Audio[{0, 1}];
buffer >> Time(2.0) | Audio[0];
net    >> Time(2.0) | Graphics;
```

Removed symbols:
- `TimeSpec::channels` field
- `Time(double, uint32_t)` constructor
- `Time(double, vector<uint32_t>)` constructor
- `operator|` overloads taking `Domain` in Temporal.hpp/cpp

Changed:
- `operator|` overloads in Temporal now take `const CreationContext&` instead of `Domain`
- Channel extraction moves from `spec.channels` to `ctx.channel` / `ctx.channels`
- Temporal.hpp replaces `#include "Domain.hpp"` with `#include "Creator.hpp"` to gain `CreationContext`, `DomainSpec`, `Audio`, and `Graphics`

This completes the DomainSpec unification announced at 0.3.0, deprecated in previous versions, and documented in all release notes. Temporal scheduling now aligns with the committed pipe-bracket syntax across all domains.

Fixes
-----

### CMake installation paths on multi-lib systems

Hardcoded `lib` install paths in `install.cmake` and `MayaFluxConfig.cmake.in` were incompatible with Fedora and other distributions using `lib64` for 64-bit libraries. Replaced with `CMAKE_INSTALL_LIBDIR`, which resolves correctly across all GNU install directory conventions. Removes the manual `mv` workaround from packaging specs.

Upgrade Notes
-------------

**Not a drop-in replacement.** Any code using `Time(seconds, channel)` or `Time(seconds, {channels})` must migrate to `Time(seconds) | Audio[...]` before compilation. The upgrade is mechanical: extract the channel argument, move it after the pipe, and wrap it in brackets with the domain.

Library installation now targets `lib64` on 64-bit Fedora/RHEL systems without manual correction.
