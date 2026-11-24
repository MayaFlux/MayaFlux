#pragma once

#include "MayaFlux/Kriya/FrameOperation.hpp"

namespace MayaFlux::Kriya {

/**
 * @enum ReflowEventKind
 * @brief What a reflow announces.
 */
enum class ReflowEventKind : uint8_t {
    Written, ///< A frame went to a sink. value is the frames written so far.
    Held, ///< A cycle had no frame to show. value is the cycles held so far.
    Failed, ///< A callable threw and the cycle ended early. value is the failures so far.
    Finished ///< Every course finished. Announced once, again after one of them runs again.
};

/**
 * @struct ReflowEvent
 * @brief One announcement from a reflow.
 */
struct ReflowEvent {
    ReflowEventKind kind {};
    uint64_t cycle {}; ///< The cycle it happened in.
    uint64_t value {}; ///< A count, by kind.
};

/** @brief A stream of a reflow's events, awaited by one coroutine. */
using ReflowEvents = Vruta::BroadcastSource<ReflowEvent>;

/**
 * @struct ReflowControls
 * @brief What steers a reflow as a whole, written from any thread and read once per cycle.
 *
 * Empty members leave the reflow as it was built. `selection` is a FrameSelection and
 * `rewrite` a Rewrite. `enabled` set to zero stops cycles altogether, so no course steps
 * and nothing is written, until it is set again.
 */
struct ReflowControls {
    ControlValue selection;
    ControlValue rewrite;
    ControlValue enabled;
};

MF_DESCRIBE(ReflowControls)

/** @brief The store every reflow holds its controls in. */
using ReflowStore = ControlValueStore<ControlValue>;

} // namespace MayaFlux::Kriya

#include "ReflowControls.describe.hpp"
