#pragma once

#include "MayaFlux/Portal/Graphics/ShaderFoundry.hpp"

namespace MayaFlux::Yantra {

/**
 * @class DispatchRecording
 * @brief One graphics command buffer that several GpuResourceManagers append
 *        their dispatches to, submitted once when its results are needed.
 *
 * A manager given a recording records dispatches, buffer writes, buffer fills
 * and layout transitions into it instead of submitting each on its own. Every
 * dispatch is followed by a compute barrier, so later work in the recording
 * sees earlier writes. The recording is submitted when the host needs to read
 * a result, when its owner reaches a point it hands back to the caller, or
 * explicitly through submit(); it begins again on the next use.
 *
 * Releases requested while commands are recorded but not submitted are held
 * until the recording is submitted, then handed to
 * ShaderFoundry::release_after_submit, so nothing the recording uses is freed
 * before the GPU has run it.
 *
 * Owned by whoever groups the work (VisionGpuContexts, VisionExtractor) and
 * given once to each of its contexts.
 */
class MAYAFLUX_API DispatchRecording {
public:
    DispatchRecording() = default;
    ~DispatchRecording();

    DispatchRecording(const DispatchRecording&) = delete;
    DispatchRecording& operator=(const DispatchRecording&) = delete;
    DispatchRecording(DispatchRecording&&) = delete;
    DispatchRecording& operator=(DispatchRecording&&) = delete;

    /**
     * @brief The command buffer to record into, begun on first use.
     */
    [[nodiscard]] Portal::Graphics::CommandBufferID command();

    /**
     * @brief True while commands are recorded and not yet submitted.
     */
    [[nodiscard]] bool is_open() const { return m_cmd != Portal::Graphics::INVALID_COMMAND_BUFFER; }

    /**
     * @brief Run release once everything recorded so far has completed.
     *
     * Held until the next submit while the recording is open; otherwise handed
     * to ShaderFoundry::release_after_submit at once.
     */
    void release_after(std::function<void()> release);

    /**
     * @brief Submit what has been recorded, without waiting.
     *
     * Ends the recording with a barrier that orders later commands and host
     * reads behind its writes. Does nothing when nothing is recorded.
     *
     * @return The fence of the most recent submission, owned by the
     *         recording: poll it with is_complete(), never release it.
     *         INVALID_FENCE when nothing is outstanding.
     */
    Portal::Graphics::FenceID submit();

    /**
     * @brief True once the submission behind fence has completed.
     *
     * Every submission goes to the graphics queue in order and ends with the
     * closing barrier, so the latest one completes after all earlier ones.
     * Only the latest fence is kept; an earlier fence counts as complete once
     * the latest has signaled.
     */
    [[nodiscard]] bool is_complete(Portal::Graphics::FenceID fence);

    /**
     * @brief Submit what has been recorded and wait until every submission of
     *        this recording has completed.
     */
    void wait();

    /**
     * @brief Record the barrier every submission of Yantra dispatch work ends with.
     *
     * Orders every later command, and host reads after completion, behind the
     * compute and transfer writes recorded so far.
     */
    static void record_closing_barrier(Portal::Graphics::CommandBufferID cmd_id);

private:
    Portal::Graphics::CommandBufferID m_cmd { Portal::Graphics::INVALID_COMMAND_BUFFER };
    Portal::Graphics::FenceID m_last { Portal::Graphics::INVALID_FENCE };
    std::vector<std::function<void()>> m_releases;
};

} // namespace MayaFlux::Yantra
