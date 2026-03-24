#pragma once

#include "MayaFlux/Buffers/BufferProcessor.hpp"
#include "MayaFlux/Kakshya/NDData/NDData.hpp"

namespace MayaFlux::Buffers {

class AudioBuffer;

/**
 * @class AudioWriteProcessor
 * @brief Unconditionally writes externally-supplied data into an AudioBuffer each cycle.
 *
 * Holds a snapshot of double-precision samples. On every processing cycle the
 * snapshot is copied into the attached buffer verbatim — no mixing, no gating,
 * no freshness check. If no snapshot has been set the buffer is zeroed.
 *
 * Timing and synchronisation with the supplier are the caller's responsibility.
 * The processor only guarantees that whatever was last set via set_data() will
 * appear in the buffer on the next cycle.
 *
 * Thread safety: set_data() and processing_function() may run concurrently.
 * A lock-free pending/active double-buffer swap ensures the audio thread never
 * blocks on the supplier thread.
 *
 * DataVariant overload converts to double via Kakshya::extract_from_variant<double>
 * using a thread-local staging vector, covering all active variant types without
 * allocation on the hot path after the first call.
 */
class MAYAFLUX_API AudioWriteProcessor : public BufferProcessor {
public:
    AudioWriteProcessor() = default;
    ~AudioWriteProcessor() override = default;

    /**
     * @brief Supply the next frame of double-precision samples.
     * @param data Samples to write into the buffer on the next cycle.
     *             If size differs from the attached buffer's frame count, the
     *             write is truncated or zero-padded to fit.
     */
    void set_data(std::vector<double> data);

    /**
     * @brief Supply data from any DataVariant — converted to double via EigenAccess.
     * @param variant  Source variant.
     *                 Arithmetic/complex types: converted via EigenAccess::to_vector()
     *                 (complex uses magnitude).
     *                 GLM vector/matrix types: converted via EigenAccess::to_matrix(),
     *                 then flattened column-major: [x0,y0,z0, x1,y1,z1, ...].
     */
    void set_data(const Kakshya::DataVariant& variant);

    /**
     * @brief Supply data as a raw float span — upconverted to double.
     * @param data Span of float samples.
     */
    void set_data(std::span<const float> data);

    /**
     * @brief Supply data as a raw double span — copied directly.
     * @param data Span of double samples.
     */
    void set_data(std::span<const double> data);

    /**
     * @brief Clear any pending snapshot. Buffer will be zeroed on the next cycle.
     */
    void clear();

    /**
     * @brief Returns true if a snapshot has been set and not yet consumed.
     */
    [[nodiscard]] bool has_pending() const noexcept;

    void processing_function(const std::shared_ptr<Buffer>& buffer) override;
    void on_attach(const std::shared_ptr<Buffer>& buffer) override;

    [[nodiscard]] bool is_compatible_with(const std::shared_ptr<Buffer>& buffer) const override;

private:
    // Pending slot: supplier writes here.
    std::vector<double> m_pending;
    // Active slot: audio thread reads from here.
    std::vector<double> m_active;
    // Set when m_pending contains unseen data.
    std::atomic_flag m_dirty { ATOMIC_FLAG_INIT };

    void commit_pending();
    void write_to_buffer(AudioBuffer& buf) const;
};

} // namespace MayaFlux::Buffers
