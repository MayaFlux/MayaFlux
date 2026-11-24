#pragma once

#include "Influence.hpp"

#include "MayaFlux/Kriya/FrameOperation.hpp"

namespace MayaFlux::Nexus {

/**
 * @struct FrameInfluenceContext
 * @brief What an influence over a reflow's frames is built from: the frame and the cycle it is in.
 *
 * Valid for the call only, like the contexts of an Emitter or an Expanse.
 */
template <typename Frame>
struct FrameInfluenceContext {
    std::shared_ptr<const Frame> frame;
    const Kriya::FrameContext& context;
};

/**
 * @brief A sink that hands each frame a reflow writes to an influence.
 *
 * The influence's targets are the ones every influence has, so a frame can become an Expanse's
 * bounds (bind_bounds), a parameter block (bind_params), a push constant block or a transform for
 * a slot, with the producer deciding what to read from the frame.
 */
template <typename T, typename Frame>
[[nodiscard]] std::function<void(const std::shared_ptr<const Frame>&, const Kriya::FrameContext&)> frame_sink(
    const std::shared_ptr<Influence<T, FrameInfluenceContext<Frame>>>& influence)
{
    return [influence](const std::shared_ptr<const Frame>& frame, const Kriya::FrameContext& context) {
        if (influence) {
            influence->invoke(FrameInfluenceContext<Frame> { frame, context });
        }
    };
}

/**
 * @brief An operation that hands each frame a lane or a reflow shows to an influence and changes nothing.
 *
 * For reading what a reflow shows into the world without being its sink.
 */
template <typename T, typename Frame>
[[nodiscard]] Kriya::FrameOperation<Frame> frame_derive(
    const std::shared_ptr<Influence<T, FrameInfluenceContext<Frame>>>& influence)
{
    return Kriya::FrameOperation<Frame>::derive(
        [influence](const Kriya::FrameContext& context, const std::shared_ptr<const Frame>& frame) {
            if (influence) {
                influence->invoke(FrameInfluenceContext<Frame> { frame, context });
            }
        });
}

} // namespace MayaFlux::Nexus
