#pragma once

#include "Chain.hpp"
#include "SamplingPipeline.hpp"

namespace MayaFlux::Kriya {

/**
 * @class TapSet
 * @brief One stream read by several independent taps.
 *
 * Like several samplers sharing a stream, each tap has its own entry time,
 * speed, direction, level and output channels. Made by TapSetBuilder::start().
 * Taps are numbered in the order they were declared. Copies share the same
 * taps, so keep one alive for as long as the set should sound.
 */
class MAYAFLUX_API TapSet {
public:
    TapSet() = default;

    /**
     * @brief Number of taps.
     */
    [[nodiscard]] size_t tap_count() const { return m_taps.size(); }

    /**
     * @brief Change the level of a tap on all of its outputs.
     * @param tap  Tap number, in declaration order.
     * @param gain Linear gain.
     */
    void level(size_t tap, double gain);

    /**
     * @brief Change the speed of a tap while it plays.
     *
     * The tap carries on from where it is, so the change never jumps, and all
     * of its outputs change together. Zero holds the tap in place and a
     * negative ratio reads backwards.
     *
     * @param tap   Tap number, in declaration order.
     * @param ratio Speed relative to the source.
     */
    void speed(size_t tap, double ratio);

    /**
     * @brief The slice behind one output of a tap, for looping and region edits.
     *
     * Changes take effect on the next block and apply to that output only. Use
     * level() and speed() to change every output of a tap at once.
     *
     * @param tap    Tap number, in declaration order.
     * @param output Which of the tap's outputs, in the order its channels were given.
     */
    [[nodiscard]] Kakshya::StreamSlice& slice(size_t tap, size_t output = 0);

    /**
     * @brief Silence every tap and cancel entries that have not happened yet.
     */
    void stop();

private:
    friend class TapSetBuilder;

    struct Seat {
        std::shared_ptr<SamplingPipeline> sampler;
        size_t slot;
    };

    struct Tap {
        std::vector<Seat> seats;
        std::shared_ptr<double> velocity;
        double rate;
    };

    std::vector<std::shared_ptr<SamplingPipeline>> m_pipelines;
    std::vector<Tap> m_taps;
    std::shared_ptr<EventChain> m_entries;
};

/**
 * @class TapSetBuilder
 * @brief Describes a tap set one tap at a time, then starts it.
 *
 * Each tap() begins a tap with defaults: enters at once, normal speed, forward,
 * full level, output channel 0. The calls that follow describe that tap until
 * the next tap(). Calling one before any tap() starts the first. loop() and
 * region() apply to every tap.
 *
 * @code
 * auto taps = MayaFlux::create_tap_set("res/h.wav")
 *     .tap().speed(1.0)
 *     .tap().enters_after(2.0).speed(3.0 / 2.0).level(0.8)
 *     .tap().enters_after(4.0).speed(2.0).backward().level(0.6).on_channels({ 0, 1 })
 *     .start();
 *
 * taps.speed(0, 0.5);
 * taps.stop();
 * @endcode
 *
 * Two taps at speeds 1.0 and 1.001 drift slowly out of phase. Speeds change
 * pitch, since taps read with interpolation.
 *
 * A tap plays the source channel with the same number as each output channel,
 * wrapping when the source has fewer, so a mono source sounds on every output
 * and a stereo one keeps left and right. from_channel() picks one source
 * channel for all of a tap's outputs.
 */
class MAYAFLUX_API TapSetBuilder {
public:
    /**
     * @param stream    Source stream. Null gives an empty set from start().
     * @param mgr       BufferManager the taps are supplied through.
     * @param scheduler Scheduler for the pipelines and the entries.
     * @param buf_size  Engine buffer size in frames.
     */
    TapSetBuilder(
        std::shared_ptr<Kakshya::DynamicSoundStream> stream,
        Buffers::BufferManager& mgr,
        Vruta::TaskScheduler& scheduler,
        uint32_t buf_size);

    /**
     * @brief Begin a new tap.
     */
    TapSetBuilder& tap();

    /**
     * @brief Seconds after start() before the current tap enters.
     */
    TapSetBuilder& enters_after(double seconds);

    /**
     * @brief Speed of the current tap relative to the source. Negative also reads backwards.
     */
    TapSetBuilder& speed(double ratio);

    /**
     * @brief Read the current tap backwards from the end of the region.
     */
    TapSetBuilder& backward(bool enable = true);

    /**
     * @brief Linear gain of the current tap on each of its outputs.
     */
    TapSetBuilder& level(double gain);

    /**
     * @brief Output channel of the current tap.
     */
    TapSetBuilder& on_channel(uint32_t channel);

    /**
     * @brief Several output channels for the current tap, which stay in step.
     *
     * An empty list is ignored.
     */
    TapSetBuilder& on_channels(const std::vector<uint32_t>& channels);

    /**
     * @brief Source channel the current tap plays on every output.
     */
    TapSetBuilder& from_channel(uint32_t channel);

    /**
     * @brief Restart the current tap every @p seconds.
     *
     * Each repeat plays from the start of the region. Shorten it by giving a time
     * map instead: it is read at the start of each repeat, so a shrinking length
     * gives a stutter or bouncing repeat.
     */
    TapSetBuilder& repeat_every(double seconds);

    /**
     * @brief Restart the current tap with a repeat length that follows a time map.
     * @param seconds Map from seconds since start() to the length of the repeat
     *        that begins then, in seconds, for example TimeMaps::exponential.
     */
    TapSetBuilder& repeat_every(Kinesis::TimeMap seconds);

    /**
     * @brief Whether taps loop the region (default) or play it once.
     */
    TapSetBuilder& loop(bool enable = true);

    /**
     * @brief Restrict every tap to a region of the stream.
     * @param start_frame First frame.
     * @param end_frame   One past the last frame, 0 for the end of the stream.
     */
    TapSetBuilder& region(uint64_t start_frame, uint64_t end_frame = 0);

    /**
     * @brief Build the taps and start them.
     * @return The running set, empty if the stream or region is unusable.
     */
    [[nodiscard]] TapSet start();

private:
    struct Spec {
        double delay { 0.0 };
        double ratio { 1.0 };
        bool backward { false };
        double level { 1.0 };
        std::vector<uint32_t> channels { 0 };
        std::optional<uint32_t> source;
        std::shared_ptr<const Kinesis::TimeMap> repeat;
    };

    Spec& current();

    std::shared_ptr<Kakshya::DynamicSoundStream> m_stream;
    Buffers::BufferManager& m_mgr;
    Vruta::TaskScheduler& m_scheduler;
    uint32_t m_buf_size;

    std::vector<Spec> m_specs;
    bool m_looping { true };
    uint64_t m_start_frame {};
    uint64_t m_end_frame {};
};

} // namespace MayaFlux::Kriya
