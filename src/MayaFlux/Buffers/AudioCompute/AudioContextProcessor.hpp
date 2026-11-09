#pragma once

#include "MayaFlux/Buffers/BufferProcessor.hpp"
#include "MayaFlux/Portal/Graphics/ShaderUtils.hpp"

namespace MayaFlux::Yantra {
class WordPairExecutionContext;
}

namespace MayaFlux::Buffers {

class AudioBuffer;

/**
 * @class AudioContextProcessor
 * @brief Runs a Yantra word pair context over a child AudioBuffer without blocking.
 *
 * Install as the default processor of a child AudioBuffer under AUDIO_PARALLEL.
 * Each call stages the child's samples into one of depth contexts, dispatches
 * it asynchronously and returns. The child receives the result of the call made
 * depth calls earlier, so the output lags the input by depth calls and nothing
 * waits on the GPU. A call whose result is not complete yields a silent child,
 * the late result is discarded, and an earlier block is never replayed.
 *
 * Contexts come from a factory so each in-flight job owns its buffers. The
 * default staging writes the samples to binding 0 as word pairs and reads the
 * primary output back at the same size. A stage function replaces it when the
 * shader takes other inputs.
 *
 * Usage:
 *   auto proc = std::make_shared<AudioContextProcessor>(make_context, 2);
 *   child->set_default_processor(proc);
 */
class MAYAFLUX_API AudioContextProcessor : public BufferProcessor {
public:
    using ContextFactory = std::function<std::shared_ptr<Yantra::WordPairExecutionContext>()>;

    /**
     * @brief Fills a context for one job from the child's samples and the call's sequence number.
     */
    using StageFunction = std::function<void(Yantra::WordPairExecutionContext&, std::span<const double>, uint64_t)>;

    /**
     * @param factory Builds one configured context per in-flight job. Called depth times.
     * @param depth Calls between a block's input and its output. At least 1.
     * @param stage Optional staging that replaces the default.
     */
    explicit AudioContextProcessor(const ContextFactory& factory, uint32_t depth = 2, StageFunction stage = {});

    ~AudioContextProcessor() override;

    AudioContextProcessor(const AudioContextProcessor&) = delete;
    AudioContextProcessor& operator=(const AudioContextProcessor&) = delete;
    AudioContextProcessor(AudioContextProcessor&&) = delete;
    AudioContextProcessor& operator=(AudioContextProcessor&&) = delete;

    void processing_function(const std::shared_ptr<Buffer>& buffer) override;
    void on_attach(const std::shared_ptr<Buffer>& buffer) override;
    void on_detach(const std::shared_ptr<Buffer>& buffer) override;
    [[nodiscard]] bool is_compatible_with(const std::shared_ptr<Buffer>& buffer) const override;

    [[nodiscard]] uint32_t depth() const { return static_cast<uint32_t>(m_slots.size()); }

    /**
     * @brief Calls whose output was silent although a result was due.
     *
     * A run of misses is logged once, when it starts. This is the total.
     */
    [[nodiscard]] uint64_t missed() const { return m_missed; }

private:
    struct Slot {
        std::shared_ptr<Yantra::WordPairExecutionContext> context;
        Portal::Graphics::FenceID fence { Portal::Graphics::INVALID_FENCE };
        uint64_t sequence {};
    };

    StageFunction m_stage;
    std::vector<Slot> m_slots;
    uint64_t m_next {};
    uint64_t m_missed {};
    bool m_missing {};

    void submit(Slot& slot, uint64_t sequence, std::span<const double> samples);
    void drain();
};

} // namespace MayaFlux::Buffers
