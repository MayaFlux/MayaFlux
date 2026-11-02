#pragma once

#include "MayaFlux/Kakshya/Region/Region.hpp"
#include "MayaFlux/Kakshya/Source/DynamicSoundStream.hpp"

#include "MayaFlux/Kinesis/Tendency/Tendency.hpp"

#include <optional>

namespace MayaFlux::Kakshya {

/**
 * @struct StreamSlice
 * @brief A bounded region of a DynamicSoundStream with associated playback parameters.
 *
 * Describes a slice of audio data within a DynamicSoundStream as a Region.
 * region.start_coordinates[0] / end_coordinates[0] are the frame bounds;
 * region.start_coordinates[1] / end_coordinates[1] are the channel bounds.
 *
 * The stream length is read when the slice is loaded. Use a ring or a stream
 * that is fully written; frames appended to a growing stream afterwards are not
 * played until the slice is loaded again.
 *
 * The slice does not own a processor. The region is the contract; whatever
 * drives this slice resolves the appropriate processor at activation time.
 * Currently that is CursorAccessProcessor for sequential bounded reads, but
 * the region descriptor is equally valid input to RegionOrganizationProcessor
 * (non-linear navigation, selection patterns, transitions) or any future
 * region-aware processor against a DynamicSoundStream.
 *
 * speed is a playback rate: 2.0 plays twice as fast and an octave up, read with
 * linear interpolation, and it can change while the voice plays. scale is a
 * static gain. Both are applied by the driving processor.
 * looping and index are playback state and identity carried with the slice.
 *
 * time_map, when set, replaces speed: it gives the read position in frames as
 * a function of seconds since the voice was bound (see Kinesis::TimeMaps).
 * Like the other fields it can be changed while the voice plays.
 *
 * loop_count ends a looping voice after that many passes over the region. With
 * a time_map, each crossing of a region edge counts as a pass, whichever way
 * the map is moving, so a map that stays inside the region never ends. Zero
 * loops forever.
 *
 * source_channel picks which channel of the stream is played. Unset, the voice
 * plays the stream channel with the same number as its output channel.
 *
 * repeat, with a time_map, restarts the map over and over. Its value is how
 * many seconds each repeat lasts, read when the repeat begins, so a length that
 * shrinks over time gives a stutter or bouncing repeat.
 *
 * envelope is a gain applied to each output sample, on top of scale. It maps
 * seconds since the voice was bound to a gain, so a grain shape is a window
 * function over its duration. It acts on the output, so speed and time_map do
 * not stretch it. Unset means no envelope.
 */
struct StreamSlice {
    std::shared_ptr<DynamicSoundStream> stream;
    Region region;

    double speed { 1.0 };
    double cursor_remainder {};
    double scale { 1.0 };
    bool looping {};
    bool active {};
    uint8_t index {};
    size_t loop_count {};
    std::shared_ptr<const Kinesis::TimeMap> time_map;
    std::optional<uint32_t> source_channel;
    std::shared_ptr<const Kinesis::TimeMap> repeat;
    std::shared_ptr<const Kinesis::TimeMap> envelope;

    /**
     * @brief Construct a slice spanning the full stream across all channels.
     * @param stream Source stream.
     * @param index  Slot identity.
     */
    static StreamSlice from_stream(
        std::shared_ptr<DynamicSoundStream> stream,
        uint8_t index = 0)
    {
        const uint64_t end_frame = stream->get_num_frames() > 0
            ? stream->get_num_frames() - 1
            : 0;
        const uint64_t end_channel = stream->get_num_channels() > 0
            ? stream->get_num_channels() - 1
            : 0;
        return StreamSlice {
            .stream = std::move(stream),
            .region = Region::audio_span(0, end_frame, 0, end_channel),
            .index = index,
        };
    }

    /**
     * @brief Construct a slice spanning a frame sub-region across all channels.
     * @param stream      Source stream.
     * @param start_frame Inclusive start frame.
     * @param end_frame   Inclusive end frame.
     * @param index       Slot identity.
     */
    static StreamSlice from_frame_range(
        std::shared_ptr<DynamicSoundStream> stream,
        uint64_t start_frame,
        uint64_t end_frame,
        uint8_t index = 0)
    {
        const uint64_t end_channel = stream->get_num_channels() > 0
            ? stream->get_num_channels() - 1
            : 0;
        return StreamSlice {
            .stream = std::move(stream),
            .region = Region::audio_span(start_frame, end_frame, 0, end_channel),
            .index = index,
        };
    }

    /**
     * @brief Construct a slice spanning a frame and channel sub-region.
     * @param stream        Source stream.
     * @param start_frame   Inclusive start frame.
     * @param end_frame     Inclusive end frame.
     * @param start_channel Inclusive start channel.
     * @param end_channel   Inclusive end channel.
     * @param index         Slot identity.
     */
    static StreamSlice from_region(
        std::shared_ptr<DynamicSoundStream> stream,
        uint64_t start_frame,
        uint64_t end_frame,
        uint32_t start_channel,
        uint32_t end_channel,
        uint8_t index = 0)
    {
        return StreamSlice {
            .stream = std::move(stream),
            .region = Region::audio_span(start_frame, end_frame, start_channel, end_channel),
            .index = index,
        };
    }

    [[nodiscard]] uint64_t start_frame() const { return region.start_coordinates[0]; }
    [[nodiscard]] uint64_t end_frame() const { return region.end_coordinates[0]; }
    [[nodiscard]] uint32_t start_channel() const { return static_cast<uint32_t>(region.start_coordinates[1]); }
    [[nodiscard]] uint32_t end_channel() const { return static_cast<uint32_t>(region.end_coordinates[1]); }

    StreamSlice& with_speed(double s)
    {
        speed = s;
        return *this;
    }

    StreamSlice& with_looping(bool l)
    {
        looping = l;
        return *this;
    }

    StreamSlice& with_scale(double s)
    {
        scale = s;
        return *this;
    }

    StreamSlice& with_loop_count(size_t n)
    {
        loop_count = n;
        return *this;
    }

    StreamSlice& with_time_map(Kinesis::TimeMap map)
    {
        time_map = std::make_shared<const Kinesis::TimeMap>(std::move(map));
        return *this;
    }

    StreamSlice& with_source_channel(uint32_t channel)
    {
        source_channel = channel;
        return *this;
    }

    StreamSlice& with_repeat(Kinesis::TimeMap length)
    {
        repeat = std::make_shared<const Kinesis::TimeMap>(std::move(length));
        return *this;
    }

    StreamSlice& with_envelope(Kinesis::TimeMap gain)
    {
        envelope = std::make_shared<const Kinesis::TimeMap>(std::move(gain));
        return *this;
    }
};

} // namespace MayaFlux::Kakshya
