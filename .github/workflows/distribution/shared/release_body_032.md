MayaFlux 0.3.2 Patch Release
===========================

Targets correctness bugs in mesh loading, engine lifecycle, GLFW platform selection, and Linux shutdown semantics.

Fixes
-----

### MeshNetworkBuffer texture binding collision

Per-slot diffuse textures in `MeshNetworkBuffer` were bound at `binding=2+i`, colliding with the slotIndices SSBO already occupying `binding=2` in `set=0`. The descriptor set reported a type mismatch (STORAGE_BUFFER vs COMBINED_IMAGE_SAMPLER) and crashed on the first textured model load. Per-slot texture bindings have been moved to `binding=3+i`, reserving `binding=2` exclusively for the slotIndices buffer. `set=0, binding=0` remains reserved for the engine-owned ViewTransform UBO.

### MeshNetworkProcessor geometry upload on attachment

`MeshNetworkProcessor::on_attach` called `upload_combined` against an unsorted `sorted_indices` vector, cleared all dirty flags, and then never uploaded geometry on subsequent frames because `rebuild_sort()` was lazy and only fires inside `process_batch`. Added `ensure_sorted()` to `MeshNetwork` and call it at the top of `on_attach` to guarantee slot order before the first GPU upload.

### Engine initialization state conflation

`is_engine_initialized` tracked two distinct concerns: whether `Init()` has completed and managers are ready, and whether the engine is actively processing. Split into `is_engine_configured` (Init() succeeded, managers ready) and the existing `is_running()` (actively processing). Added `Engine::is_configured()`, `Core::is_configured()`, and public API `is_engine_configured()`. All internal call sites updated to query the appropriate state.

### GLFW platform hint resolution

`force_x11_on_wayland` resolution was a separate pre-pass before platform hint application, leaving the hint unapplied when the sentinel matched. Folded resolution into the existing platform switch in `configure()`. `GLFW_ANY_PLATFORM` is now the sentinel for "do not hint"; the hint is only applied when a non-default platform is resolved. Default behaviour (no forced platform) is unchanged.

### Linux shutdown gate robustness

The engine shutdown gate on Linux used `std::cin.get()`, which blocks indefinitely in headless contexts (GUI launchers, capture tools) and returns immediately on EOF (piped input), making graceful shutdown impossible in those environments. Replaced with a `sigaction`/`select` loop: TTY contexts block on `select` and exit on Return or signal; non-TTY contexts block on `sigsuspend` until SIGINT or SIGTERM. Static atomic reset on entry handles engine recreation in the same process.

Upgrade Notes
-------------

No API changes. Drop-in replacement for 0.3.1. The `is_engine_initialized` query remains available but should be migrated to `is_engine_configured()` if initialization state distinction is relevant; `is_running()` covers the active processing case.
