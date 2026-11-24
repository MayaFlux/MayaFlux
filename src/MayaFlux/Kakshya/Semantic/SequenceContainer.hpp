#pragma once

#include "MayaFlux/Kakshya/FrameSource.hpp"
#include "MayaFlux/Kakshya/Region/RegionSegment.hpp"
#include "MayaFlux/Kakshya/SemanticContainer.hpp"

namespace MayaFlux::Kakshya {

/**
 * @class SequenceContainer
 * @brief The container face of a FrameSource: an ordered sequence of immutable frames
 *        served as regions of one frame axis.
 *
 * A Region is an inclusive frame range on axis 0, so a point region is one frame and a
 * span is several. The get_*_data calls copy the frames of a range into a vector, one
 * value per frame. read() is the copy free path a player uses and returns the shared
 * frame itself. Frames never change after they are published, so set_region_data
 * refuses every write and any number of threads may read, given a source that allows
 * it. clear() must not run while another thread reads.
 *
 * Specializes SemanticContainer<Frame>, so it carries no ProcessingState and no
 * DataProcessor lifecycle. It knows nothing of the medium: a volume sequence, a spatial
 * cache and a mesh scene differ only in the source they wrap. A single frame is a
 * source of one frame, so a static thing needs no other container.
 *
 * The container holds the source it wraps and forwards count(), time_sampling() and
 * read() to it. clear() empties the container (count() is 0, every read is nullptr and
 * the region groups are dropped) without touching the source or any frame a reader still
 * holds.
 *
 * @tparam Frame The NDData struct one frame is, such as VolumeData.
 */
template <typename Frame>
class SequenceContainer final : public SemanticContainer<Frame>, public FrameSource<Frame> {
public:
    /**
     * @brief Wrap a source.
     * @param source The frames to serve. Null gives nullptr.
     * @param modality What the frames are, recorded in the structure metadata.
     */
    [[nodiscard]] static std::shared_ptr<SequenceContainer> create(
        std::shared_ptr<const FrameSource<Frame>> source,
        DataModality modality = DataModality::UNKNOWN)
    {
        if (!source) {
            return nullptr;
        }
        return std::shared_ptr<SequenceContainer>(new SequenceContainer(std::move(source), modality));
    }

    /** @brief The wrapped source. */
    [[nodiscard]] const std::shared_ptr<const FrameSource<Frame>>& source() const noexcept { return m_source; }

    [[nodiscard]] size_t count() const override { return m_cleared ? 0 : m_source->count(); }

    [[nodiscard]] const Kinesis::TimeSampling& time_sampling() const override { return m_source->time_sampling(); }

    [[nodiscard]] std::shared_ptr<const Frame> read(size_t index) const override
    {
        return m_cleared ? nullptr : m_source->read(index);
    }

    /** @brief One axis: the frames. */
    [[nodiscard]] std::vector<DataDimension> get_dimensions() const override
    {
        return { DataDimension("frames", count(), 1, DataDimension::Role::TIME) };
    }

    /** @brief The frame count. */
    [[nodiscard]] uint64_t get_total_elements() const override { return count(); }

    /** @brief Frames are not laid out in one block. */
    [[nodiscard]] MemoryLayout get_memory_layout() const override { return MemoryLayout::ROW_MAJOR; }

    /** @brief The layout is the source's own and is not changed here. */
    void set_memory_layout(MemoryLayout) override { }

    /** @brief One frame per step along the axis. */
    [[nodiscard]] uint64_t get_frame_size() const override { return 1; }

    /** @brief The frame count. */
    [[nodiscard]] uint64_t get_num_frames() const override { return count(); }

    /**
     * @brief Copy the frames a Region selects.
     * @return One value per frame in the range, in order. Empty when the region is not a
     * range on one axis, runs backwards, leaves the sequence or names a frame the source
     * could not produce.
     */
    [[nodiscard]] std::vector<Frame> get_region_data(const Region& region) const override
    {
        const auto range = frame_range(region);
        if (!range) {
            return {};
        }

        std::vector<Frame> copied;
        copied.reserve(range->second - range->first + 1);
        for (uint64_t i = range->first; i <= range->second; ++i) {
            const auto frame = read(i);
            if (!frame) {
                return {};
            }
            copied.push_back(*frame);
        }
        return copied;
    }

    /**
     * @brief Copy segments into separate values.
     *
     * A segment is the frame range that starts at offset_in_region inside its source
     * region and spans segment_size frames.
     * @return The frames of every valid segment, in segment order; malformed or
     * out of range segments are skipped.
     */
    [[nodiscard]] std::vector<Frame> get_segments_data(const std::vector<RegionSegment>& segments) const override
    {
        std::vector<Frame> result;
        for (const auto& segment : segments) {
            const auto& region = segment.source_region;
            if (region.start_coordinates.size() != 1 || region.end_coordinates.size() != 1
                || segment.offset_in_region.size() != 1 || segment.segment_size.size() != 1) {
                continue;
            }

            const uint64_t source_start = region.start_coordinates[0];
            const uint64_t source_end = region.end_coordinates[0];
            const uint64_t offset = segment.offset_in_region[0];
            const uint64_t size = segment.segment_size[0];
            if (source_start > source_end || offset > source_end - source_start || size == 0) {
                continue;
            }
            const uint64_t start = source_start + offset;
            if (size - 1 > source_end - start) {
                continue;
            }

            auto copied = get_region_data(Region({ start }, { start + size - 1 }));
            std::ranges::move(copied, std::back_inserter(result));
        }
        return result;
    }

    /** @brief Frames are immutable, so nothing is written. */
    void set_region_data(const Region&, const std::vector<Frame>&) override { }

    /** @brief Whether the region is a frame range inside the sequence. */
    [[nodiscard]] bool is_region_loaded(const Region& region) const override
    {
        return frame_range(region).has_value();
    }

    /** @brief The source decides what is resident. */
    void load_region(const Region&) override { }

    /** @brief The source decides what is resident. */
    void unload_region(const Region&) override { }

    /**
     * @brief The frame index a one axis coordinate names.
     * @return The index, or 0 when the coordinate has the wrong rank or leaves the sequence.
     */
    [[nodiscard]] uint64_t coordinates_to_linear_index(const std::vector<uint64_t>& coordinates) const override
    {
        return coordinates.size() == 1 && coordinates[0] < count() ? coordinates[0] : 0;
    }

    /**
     * @brief The coordinate of a frame index.
     * @return One coordinate, or empty when the index is past the last frame.
     */
    [[nodiscard]] std::vector<uint64_t> linear_index_to_coordinates(uint64_t linear_index) const override
    {
        return linear_index < count() ? std::vector<uint64_t> { linear_index } : std::vector<uint64_t> {};
    }

    /** @brief Become empty and drop all region groups. */
    void clear() override
    {
        m_cleared = true;
        this->clear_region_groups();
    }

    /** @brief Frames are separate allocations, so there is no block to borrow. */
    [[nodiscard]] const void* get_raw_data() const override { return nullptr; }

    /** @brief Whether the container holds any frame. */
    [[nodiscard]] bool has_data() const override { return count() > 0; }

private:
    SequenceContainer(std::shared_ptr<const FrameSource<Frame>> source, DataModality modality)
        : SemanticContainer<Frame>({ modality, OrganizationStrategy::INTERLEAVED })
        , m_source(std::move(source))
    {
    }

    [[nodiscard]] std::optional<std::pair<uint64_t, uint64_t>> frame_range(const Region& region) const
    {
        if (region.start_coordinates.size() != 1 || region.end_coordinates.size() != 1) {
            return std::nullopt;
        }
        const uint64_t first = region.start_coordinates[0];
        const uint64_t last = region.end_coordinates[0];
        if (first > last || last >= count()) {
            return std::nullopt;
        }
        return std::pair { first, last };
    }

    std::shared_ptr<const FrameSource<Frame>> m_source;
    bool m_cleared {};
};

}
