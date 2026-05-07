MayaFlux 0.3.4 Release
======================

Targets correctness fixes in operator token release and ResonatorNetwork output, CMake library path resolution, operator shutdown safety, and operator collection access patterns. Adds parameterized path generation and operator accessors for external coroutine integration.

Fixes
-----

### PhysicsOperator and TopologyOperator token release

`PhysicsOperator::add_collection` and `TopologyOperator::add_topology` acquired `m_access_token` via CAS spin but never released it, causing subsequent `process()` calls to spin forever. Added `store(0)` after each `push_back` to release the token.

### ResonatorNetwork output mode and shadowed buffer

`ResonatorNetwork` defaulted to `OutputMode::NONE` at construction, causing `Creator::register_network` to fire a spurious warning and rely on the forced-AUDIO_SINK path. Set `AUDIO_SINK` explicitly in both constructors.

`ResonatorNetwork` declared a duplicate `m_last_audio_buffer` member shadowing the base class `NodeNetwork` field in the protected section. The shadow caused `apply_output_scale()` and `get_audio_buffer()` to operate on different vectors, producing incorrect output and potential memory corruption at scale. Removed the duplicate declaration.

### Operator shutdown hang on access token spin

The CAS spin on `m_access_token` in `PathOperator`, `TopologyOperator`, and `PhysicsOperator` had no exit path on engine shutdown, causing the graphics thread to hang indefinitely if destruction occurred while a spin was in progress. Added `std::atomic<bool> m_shutdown` to each operator. The destructor sets it with release ordering; the spin loop checks it with relaxed ordering on each failed CAS and returns immediately if set. The relaxed check is correct because the destructor's release store establishes the required happens-before relationship.

### CMake library path resolution on non-Fedora installs

`CMAKE_INSTALL_LIBDIR` expands to a relative segment ("lib") on most platforms and an absolute path ("/usr/lib64") on Fedora only. Relative paths caused `IMPORTED_LOCATION` to resolve relative to the build directory rather than the install root. Guarded with `IS_ABSOLUTE`: if relative, prepend `PACKAGE_PREFIX_DIR` to produce the correct absolute path. Fedora's absolute path is left unchanged. No platform branching required.

Features
--------

### Parameterized path generation

`PathOperator::add_path` previously hardcoded 1024 max control points and forwarded the operator's default samples per segment. Callers can now configure per-path capacity and interpolation parameters directly via new optional arguments. `initialize` and `initialize_paths` updated to forward `m_default_samples_per_segment`.

### Operator collection accessors

Added `get_path` and `get_topology` accessors on operator collections, exposing indexed access to `PathGeneratorNode` and `TopologyGeneratorNode` instances. External coroutines can now call `add_control_point` and `set_points` directly without routing through the network layer.

### TopologyOperator samples-per-segment dispatch

Added typed `set_samples_per_segment` on `TopologyOperator` forwarding to all topology nodes. `PointCloudNetwork::set_samples_per_segment` now dispatches to both `PathOperator` and `TopologyOperator` rather than warning on topology mode.

### Operator vertex count aggregate

`PathOperator`, `TopologyOperator`, and `PhysicsOperator` were returning the vertex count from collection 0 only in `get_vertex_layout`. With `NetworkGeometryBuffer` driving the draw call directly from the layout, the count must reflect the full aggregate across all collections.

### Nexus MouseTrigger capture safety

`Wiring` is a temporary builder destroyed after `finalise()`. The `MouseTrigger` lambda previously captured `[this, id]`, leaving a dangling reference to the builder. Snapshot `m_fabric` and `m_entity_id` into named locals before the lambda to outlive the builder lifetime.

### CompositeGeometryBuffer public registration

Added forward declaration and `ALL_BUFFER_REGISTRATION` entry for `CompositeGeometryBuffer` alongside the include in `MayaFlux.hpp`, enabling proxy-based instantiation.

Upgrade Notes
-------------

No API changes. Drop-in replacement for 0.3.3.

New optional parameters on `PathOperator::add_path` are backward compatible; existing calls without them use operator defaults as before. New accessors and dispatch methods are additive.
