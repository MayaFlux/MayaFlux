#pragma once

#include "RootAudioBuffer.hpp"

namespace MayaFlux::Buffers {

/**
 * @class RootParallelAudioBuffer
 * @brief Root buffer of an AUDIO_PARALLEL unit
 *
 * Mixes its children exactly as RootAudioBuffer does, but prefers the
 * AUDIO_PARALLEL token and does not police the default processor token of its
 * children, so a CPU source and a GPU compute child can share the same root.
 *
 * The mix and any processors that follow it work on get_data(). When the
 * cycle completes the result is copied into a triple buffer, and read_block()
 * hands a reader on another thread the latest complete block. The reader never
 * sees a block that is being mixed, never waits, and the writer never waits.
 * One thread processes and one thread reads.
 */
class MAYAFLUX_API RootParallelAudioBuffer : public RootAudioBuffer {
public:
    /**
     * @brief Creates a parallel root buffer for a channel
     * @param channel_id Channel identifier
     * @param num_samples Buffer capacity in samples (default: 512)
     */
    RootParallelAudioBuffer(uint32_t channel_id, uint32_t num_samples = 512);

    /**
     * @brief Resizes the root and its published blocks, which become silent
     *
     * Not safe while a reader is active.
     */
    void resize(uint32_t num_samples) override;

    /**
     * @brief Publishes the finished mix as the latest complete block
     */
    void handle_cycle_complete() override;

    /**
     * @brief The latest complete block
     * @return A span valid until the next call to read_block
     *
     * Repeats the previous block if nothing was published since the last call.
     */
    [[nodiscard]] std::span<const double> read_block() override;

    /**
     * @brief Generation of the block last returned by read_block
     * @return Zero before the first publish; it increases with each publish and
     * is unchanged when read_block repeats a block
     */
    [[nodiscard]] uint64_t block_generation() const;

private:
    static constexpr uint32_t k_index_mask = 0x3;
    static constexpr uint32_t k_fresh = 0x4;

    std::array<std::vector<double>, 3> m_slots;
    std::array<uint64_t, 3> m_slot_generation {};
    std::atomic<uint32_t> m_middle { 1 };
    uint32_t m_write { 0 };
    uint32_t m_read { 2 };
    uint64_t m_produced { 0 };
};

}
