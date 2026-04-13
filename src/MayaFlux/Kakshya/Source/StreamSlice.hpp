#pragma once

#include "MayaFlux/Kakshya/Region/Region.hpp"
#include "MayaFlux/Kakshya/Source/DynamicSoundStream.hpp"

namespace MayaFlux::Kakshya {

/**
 * @struct StreamSlice
 * @brief A bounded region of a DynamicSoundStream with associated playback parameters.
 *
 * Describes a slice of audio data within a DynamicSoundStream as a Region.
 * region.start_coordinates[0] / end_coordinates[0] are the frame bounds;
 * region.start_coordinates[1] / end_coordinates[1] are the channel bounds.
 *
 * The slice does not own a processor. The region is the contract; whatever
 * drives this slice resolves the appropriate processor at activation time.
 * Currently that is CursorAccessProcessor for sequential bounded reads, but
 * the region descriptor is equally valid input to RegionOrganizationProcessor
 * (non-linear navigation, selection patterns, transitions) or any future
 * region-aware processor against a DynamicSoundStream.
 *
 * speed and scale are playback parameters applied by the driving processor.
 * cursor_remainder accumulates sub-frame advancement for speed != 1.0.
 * looping and index are playback state and identity carried with the slice.
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
};

} // namespace MayaFlux::Kakshya
