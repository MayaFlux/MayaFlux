Updated 0.2.1 release notes:

---

MayaFlux 0.2.1
Patch release targeting two correctness bugs in the physical modelling and descriptor binding subsystems.

**Fixes**

Descriptor binding source dispatch crash `DescriptorBindingsProcessor::update_descriptor_from_node` was missing a `break` after the `NODE` source type case, causing fallthrough into the `AUDIO_BUFFER` branch. Any binding with a node source would attempt a `dynamic_pointer_cast` on a null buffer and crash. The function has been refactored into a dispatcher with three focused handlers (`update_from_node`, `update_from_buffer`, `update_from_network`), each with explicit null validation on entry.

NodeNetwork audio buffer data race `m_last_audio_buffer` in audio sink networks (`ModalNetwork`, `WaveguideNetwork`, `ResonatorNetwork`) was filled incrementally via `push_back` with no synchronisation between the audio thread and the graphics thread reading via `get_audio_buffer`. The read window was proportional to the fill duration, making torn reads probable under normal async operation rather than merely possible.
Each `process_batch` override now fills into a `thread_local` scratch vector, then acquires an `atomic_flag` spinlock to `assign` into `m_last_audio_buffer` and run `apply_output_scale` before releasing. `get_audio_buffer` acquires the same lock before copying. The lock is held only for the assign and scale pass, not during the fill. The `clear` + `reserve` + `push_back` pattern is replaced with `assign` throughout, eliminating any reallocation risk on the shared buffer.

SoundFileBridge incomplete initialisation `SoundFileBridge::setup_processors` was not calling `initialize()` before constructing `SoundStreamWriter`, leaving the bridge in an incomplete state. Fixed by inserting the missing call.

BufferOperation ROUTE semantics `route_to_buffer` and `route_to_container` now document that ROUTE is a batch operation for post-accumulation output, not live per-cycle routing. Live routing is handled by supply, clone, and per-buffer processing chains.

**Upgrade notes**

No API changes. Drop-in replacement for 0.2.0.
