#pragma once

#include "MayaFlux/Kakshya/DataProcessor.hpp"
#include "MayaFlux/Kakshya/NDimensionalContainer.hpp"

#include "MayaFlux/Kinesis/Tendency/Tendency.hpp"

namespace MayaFlux::Kakshya {

class DynamicSoundStream;

/**
 * @class CursorAccessProcessor
 * @brief Independent cursor reader for DynamicSoundStream, writing exclusively
 *        to a dynamic slot allocated at attach time.
 *
 * Intended for use cases where multiple independent cursors must read the same
 * DynamicSoundStream simultaneously -- for example StreamSliceProcessor holding
 * N slots over the same loaded buffer. Each instance allocates one dynamic slot
 * via DynamicSoundStream::allocate_dynamic_slot() and writes only into that slot
 * via get_dynamic_data(m_slot_index). The stream's processed_data, processing
 * state, and ready/consumed machinery are never touched. Callers read output
 * directly from get_dynamic_data(get_slot_index()).
 *
 * on_attach does three things only: cache the container structure, allocate the
 * dynamic slot, and initialise the cursor. It does not set the stream as ready
 * for processing or register any state callbacks. on_detach releases the slot.
 * All other container lifecycle is the caller's responsibility.
 *
 * For default single-cursor sequential reads driven by the container's own
 * processing state, use ContiguousAccessProcessor instead.
 *
 * The stream is treated as immutable memory after load. Block size is fixed at
 * construction or via set_frames_per_block(). When inactive, process() writes
 * silence without advancing the cursor. Activation and cursor reset are explicit
 * via reset(), keeping trigger logic decoupled from the processor.
 */
class MAYAFLUX_API CursorAccessProcessor : public DataProcessor {
public:
    /**
     * @brief Construct with a fixed output block size.
     * @param frames_per_block Number of frames extracted per process() call.
     *                         One frame = one sample per channel. Should match
     *                         the engine buffer size in frames.
     */
    explicit CursorAccessProcessor(uint64_t frames_per_block);

    ~CursorAccessProcessor() override = default;

    void on_attach(const std::shared_ptr<SignalSourceContainer>& container) override;
    void on_detach(const std::shared_ptr<SignalSourceContainer>& container) override;

    /**
     * @brief Extract one block of frames into container processed_data[0].
     *
     * If inactive, fills processed_data[0] with silence and returns.
     * Advances m_cursor by m_frames_per_block after each active read.
     * On reaching the loop end, wraps to m_loop_start if looping, otherwise
     * deactivates and fires m_on_end if set.
     *
     * @param container Must be the DynamicSoundStream passed to on_attach.
     */
    void process(const std::shared_ptr<SignalSourceContainer>& container) override;

    [[nodiscard]] bool is_processing() const override { return m_is_processing.load(); }

    /**
     * @brief Reset cursor to loop start and activate the processor.
     * Safe to call from any thread between process() calls.
     */
    void reset();

    /**
     * @brief Deactivate without resetting the cursor position.
     */
    void stop();

    /**
     * @brief Set the output block size in frames.
     * @param frames_per_block Must be > 0.
     */
    void set_frames_per_block(uint64_t frames_per_block);

    /**
     * @brief Enable or disable looping within the loop region.
     * @param enable True to loop, false for one-shot.
     */
    void set_looping(bool enable) { m_looping = enable; }

    /**
     * @brief Set the loop region in frames.
     *
     * Defaults to [0, total_frames) on attach. Both values are clamped to the
     * container's frame count on the next process() call. All units are frames
     * (one frame = num_channels samples); conversion to sample offsets for
     * get_region_data happens internally.
     *
     * @param start_frame Inclusive loop start, in frames.
     * @param end_frame   Exclusive loop end, in frames.
     */
    void set_loop_region(uint64_t start_frame, uint64_t end_frame);

    /**
     * @brief Set the number of loops to play before stopping.
     * Loop count is decremented on each loop completion; when it reaches zero,
     * the processor deactivates and fires m_on_end if set. Loop count is
     * ignored if looping is disabled.
     *
     * @param n Number of loops to play (0 for infinite).
     */
    void set_loop_count(size_t n);

    /**
     * @brief Register a callback fired when one-shot playback reaches the end.
     * @param cb Callback with no arguments. Called from the process() thread.
     */
    void set_on_end(std::function<void()> cb) { m_on_end = std::move(cb); }

    /**
     * @brief Set the playback speed relative to nominal rate.
     *
     * Speed is applied as a fractional frame accumulator: each process() call
     * advances the cursor by m_frames_per_block * m_speed frames, with the
     * sub-frame remainder carried in m_speed_remainder for the next call.
     * Speed 1.0 is the default (no accumulator overhead). Values <= 0.0 are
     * ignored.
     *
     * @param speed Playback speed multiplier (1.0 = normal).
     */
    void set_speed(double speed);

    /**
     * @brief Drive the read position from a function of time instead of speed.
     *
     * The map takes seconds since the last reset() and returns a source
     * position in frames, read with linear interpolation. Reverse, scrubbing
     * and a fixed position are all valid. Speed and loop count are ignored
     * while a map is set. Positions outside the loop region wrap if looping,
     * otherwise the voice ends. A different map restarts its clock at zero.
     *
     * @param map Time map, or null to go back to speed based reading.
     */
    void set_time_map(const std::shared_ptr<const Kinesis::TimeMap>& map);

    /**
     * @brief Restart the time map over and over, with a length that can change.
     *
     * Needs a time map. The length map takes seconds since the last reset() and
     * returns how long, in seconds, the repeat that starts then should last. It
     * is evaluated once per repeat. Each repeat restarts the time map from zero
     * and plays its first stretch of that length, with the jump back exact to
     * the frame, so a shrinking length gives a stutter or bouncing repeat.
     * Passing the map already set is a no-op.
     *
     * @param length Repeat length in seconds, or null for no repeating.
     */
    void set_repeat(const std::shared_ptr<const Kinesis::TimeMap>& length);

    [[nodiscard]] bool is_active() const { return m_active; }
    [[nodiscard]] uint64_t cursor() const { return m_cursor[0]; }
    [[nodiscard]] uint64_t loop_start() const { return m_loop_start; }
    [[nodiscard]] uint64_t loop_end() const { return m_loop_end; }
    [[nodiscard]] size_t loop_count() const { return m_loop_count; }
    [[nodiscard]] uint32_t get_slot_index() const { return m_slot_index; }

private:
    uint64_t m_frames_per_block;
    std::vector<uint64_t> m_cursor {};
    uint64_t m_loop_start {};
    uint64_t m_loop_end {};
    bool m_looping {};
    bool m_active {};
    size_t m_loop_count {};
    size_t m_loops_remaining {};
    double m_speed_remainder {};
    double m_speed { 1.0 };

    std::shared_ptr<const Kinesis::TimeMap> m_time_map;
    uint64_t m_clock_frames {};
    std::shared_ptr<const Kinesis::TimeMap> m_repeat;
    uint64_t m_repeat_start {};
    uint64_t m_repeat_length {};
    std::vector<uint64_t> m_gather_frames;
    std::vector<double> m_gather_weights;
    std::vector<double> m_gather_taps;

    void process_mapped(DynamicSoundStream& stream, std::vector<DataVariant>& pd);

    uint32_t m_slot_index { std::numeric_limits<uint32_t>::max() };

    std::atomic<bool> m_is_processing { false };

    ContainerDataStructure m_structure;

    std::function<void()> m_on_end;
};

} // namespace MayaFlux::Kakshya
