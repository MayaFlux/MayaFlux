MayaFlux 0.4.1 Patch Release
===========================

Targets correctness fixes in ResonatorNetwork output normalization, Node mock-process dispatch, and NDimensionalContainer reference semantics.

Fixes
-----

### ResonatorNetwork RMS normalization

`ResonatorNetwork::process_batch` divided the summed output by resonator count (1/N) before `output_scale` was applied. This was worst-case-safe against resonators peaking in phase, but made `output_scale = 1.0` quieter than intuitive: perceived loudness dropped as resonators were added, and users had no documented way to reason about scale values without knowing resonator count.

Switched to RMS normalization (1/sqrt(N)), matching standard practice for summing multiple correlated-but-not-fully-coherent signal paths and keeping perceived loudness roughly stable as resonator count changes. This is a default, not a guarantee: resonators sharing a single exciter and tuned closely can peak more coherently than RMS assumes, so true worst-case amplitude can still exceed what RMS implies at `output_scale = 1.0`. `FinalLimiterProcessor` at the root audio buffer remains the actual safety backstop.

Documented the relationship to `get_node_count()` directly on `set_output_scale`, making the tradeoff and compensation path discoverable rather than requiring reasoning through `process_batch`.

### Node mock-process flag dispatch

`enable_mock_process()` and `should_mock_process()` lived on `Generator`, and `RootNode::process_sample()` reached them via `dynamic_pointer_cast<Generator>`. Any `Node` that isn't a `Generator`—notably the entire `GpuSync` family (`GpuComputeNode`, texture/geometry writer nodes)—failed that cast unconditionally, so `should_mock_process()` was unreachable regardless of flag state. Mock processing silently never applied outside `Generator` subtypes.

Moved both methods onto `Node` and call `should_mock_process()` directly in `RootNode` without cast. `GpuSync` nodes already return `0.0` from `process_sample()` unconditionally, so this has no behavioral effect there beyond making the flag legitimately queryable. `Node` subtypes outside `Generator` (e.g., `Constant`) can now be correctly mock-processed at the root level, which `map_parameter`-only usage depends on to stay driven without contributing to channel output.

### NDimensionalContainer region group dangling reference

`NDimensionalContainer::get_region_group()` returned `const RegionGroup&`, creating a dangling reference when the group was not found (returned to static fallback) or pointing to moved memory. Changed return type to `RegionGroup` (by value), eliminating undefined behavior and simplifying caller code. Leverages move semantics for zero overhead with RVO. Affects all six concrete implementations: `PlotContainer`, `SoundStreamContainer`, `TextureContainer`, `VideoStreamContainer`, `WindowContainer`, and mock. All call sites continue working unchanged due to implicit move.

Upgrade Notes
-------------

No API changes. Drop-in replacement for 0.4.0.

The RMS normalization corrects the existing documented contract (`output_scale` unity) and aligns perceived loudness with intuitive expectations. Existing sessions may sound slightly louder; rebalance `output_scale` values if needed. The node mock-process and container reference fixes are transparent correctness corrections with no user-facing API impact.
